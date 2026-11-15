#!/usr/bin/env python3
"""Development-only literal scalar NRRD oracles and native plugin controls."""
import argparse
from decimal import Decimal, localcontext, ROUND_HALF_UP
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys


TYPES = {"u8": (1, 1, "B", "uint8"), "u16": (2, 2, "H", "uint16"),
         "i16": (4, 2, "h", "int16"), "f32": (9, 4, "I", "float")}


def scalar_decimal(value, kind):
    if kind != "f32":
        return Decimal(value)
    exponent, fraction = (value >> 23) & 255, value & 0x7fffff
    if exponent == 255:
        return None
    significand = fraction if exponent == 0 else (1 << 23) + fraction
    result = Decimal(significand) * Decimal(2) ** (-149 if exponent == 0 else exponent - 150)
    return -result if value >> 31 else result


def rgba_oracle(values, kind, width, height, window):
    with localcontext() as context:
        context.prec = 190
        low, high = (Decimal.from_float(float(v)) for v in window)
        result = bytearray()
        for value in values[:width * height]:
            numeric = scalar_decimal(value, kind)
            if numeric is None:
                result += bytes(4)
            else:
                normalized = min(Decimal(1), max(Decimal(0), (numeric - low) / (high - low)))
                code = int((normalized * 255).quantize(Decimal(1), rounding=ROUND_HALF_UP))
                result += bytes((code, code, code, 255))
        return bytes(result)


def write_fixtures(directory):
    import nrrd
    import numpy as np
    directory.mkdir(parents=True, exist_ok=True)
    assert nrrd.__version__ == "1.1.3", "Use the admitted pinned development oracle"
    cases, negatives = [], {}
    literals = {
        "u8": [0, 9, 10, 32, 35, 127, 128, 129, 254, 255, 1, 2, 3, 4, 5],
        "u16": [0, 1, 127, 128, 129, 254, 255, 256, 257, 1000, 4095, 4096, 32767, 32768, 65535],
        "i16": [-32768, -257, -256, -255, -129, -128, -1, 0, 1, 127, 128, 255, 256, 32766, 32767],
        "f32": [0x00000000, 0x80000000, 0x3f000000, 0x3f800000, 0xbf800000, 0x437f0000, 0x43800000,
                0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345, 0xffa54321, 0x00000001, 0x80000001],
    }
    floats_second = [0x3e800000, 0x3f400000, 0x3fc00000, 0x40000000, 0xc0000000, 0x7f7fffff, 0xff7fffff,
                     0x00800000, 0x80800000, 0x7fc00001, 0xffc00002, 0x7fa00003, 0xffa00004, 0x42fe0000, 0x43000000]

    def values_for(kind, dimensions):
        result = []
        for z in range(dimensions[2] if len(dimensions) == 3 else 1):
            plane = literals[kind]
            if z:
                plane = floats_second if kind == "f32" else [v ^ 0x5555 for v in plane] if kind == "u16" else list(reversed(plane))
            result += plane
        assert len(result) == math.prod(dimensions)
        return result

    def authored(kind, dimensions, values, *, endian="little", alias=None, spacings=None, units=None, endings="lf", extras=(), ci=False):
        _, sample, pack, canonical = TYPES[kind]
        lines = ["NRRD0005", "# Repository-authored scalar codes", "type: " + (alias or canonical),
                 "dimension: " + str(len(dimensions)), "sizes: " + " ".join(map(str, dimensions)),
                 "kinds: " + " ".join("domain" if i % 2 == 0 else "space" for i in range(len(dimensions))), "encoding: raw"]
        if sample > 1:
            lines.append("endian: " + endian)
        if spacings is not None:
            lines.append("spacings: " + " ".join(map(str, spacings)))
        if units is not None:
            lines.append("units: " + " ".join(json.dumps(v, ensure_ascii=True) for v in units))
        lines += ["content: literal scalar test"] + list(extras)
        if ci:
            lines = [line if line.startswith(("NRRD", "#")) else line.split(": ", 1)[0].upper() + ": " + (line.split(": ", 1)[1].upper() if line.split(": ", 1)[0] in ("type", "encoding", "endian", "kinds") else line.split(": ", 1)[1]) for line in lines]
        header = b"".join(line.encode("ascii") + (b"\r\n" if endings == "crlf" or endings == "mixed" and i % 2 else b"\n") for i, line in enumerate(lines))
        header += b"\r\n" if endings == "crlf" else b"\n"
        raw = struct.pack(("<" if endian == "little" else ">") + pack * len(values), *values)
        return header + raw

    def add(name, kind, dimensions=(5, 3), *, values=None, options="", window=(0, 255), oracle_seed=None, **header_options):
        values = values_for(kind, dimensions) if values is None else list(values)
        width, height = dimensions[:2]
        assert len(values) == math.prod(dimensions)
        data = authored(kind, dimensions, values, **header_options)
        path = directory / (name + ".nrrd")
        path.write_bytes(data)
        abi_type, sample, pack, _ = TYPES[kind]
        expected = struct.pack("=" + pack * len(values), *values)
        limitation = None
        try:
            array, _ = nrrd.read(str(path), index_order="C")
        except (KeyError, ValueError, nrrd.NRRDError) as error:
            assert oracle_seed is not None, (name, error)
            array, _ = nrrd.read(str(directory / (oracle_seed + ".nrrd")), index_order="C")
            limitation = str(error)
        assert array.shape == tuple(reversed(dimensions)) and array.dtype.itemsize == sample, name
        if array.dtype.byteorder not in ("=", "|") and (array.dtype.byteorder == "<") != (sys.byteorder == "little"):
            array = array.byteswap().view(array.dtype.newbyteorder("="))
        assert array.tobytes(order="C") == expected, name
        if oracle_seed:
            assert (directory / (oracle_seed + ".raw")).read_bytes() == expected
        rgba = rgba_oracle(values, kind, width, height, window)
        (directory / (name + ".raw")).write_bytes(expected)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if rgba[3::4] == bytes([255]) * (width * height):
            (directory / (name + ".rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        flags, physical = 0, [0., 0., 0.]
        spacings, units = header_options.get("spacings"), header_options.get("units")
        if spacings is not None and units is not None:
            for axis, (spacing, unit) in enumerate(zip(spacings, units)):
                if unit and math.isfinite(float(spacing)):
                    physical[axis] = float(spacing) * {"nm": .001, "um": 1., "mm": 1000., "cm": 10000., "m": 1000000.}[unit]
                    flags |= 1 << axis
        cases.append({"name": name, "kind": kind, "width": width, "height": height, "z_planes": dimensions[2] if len(dimensions) == 3 else 1,
                      "sample_type": abi_type, "sample_bytes": sample, "dimension_order": "XY" if len(dimensions) == 2 else "XYZ",
                      "options": options, "window": list(window), "calibration_flags": flags, "pixel_width_um": physical[0],
                      "pixel_height_um": physical[1], "z_spacing_um": physical[2],
                      "source_sha256": hashlib.sha256(data).hexdigest(), "raw_sha256": hashlib.sha256(expected).hexdigest(),
                      "rgba_sha256": hashlib.sha256(rgba).hexdigest(), "pynrrd_bit_exact": True,
                      "pynrrd_header_limitation": limitation, "canonical_oracle_seed": oracle_seed})
        return data

    for kind in TYPES:
        for endian in (("little",) if kind == "u8" else ("little", "big")):
            for dims in ((5, 3), (5, 3, 2)):
                add(f"{kind}-{endian}-{len(dims)}d", kind, dims, endian=endian)
    aliases = {"u8": ("uchar", "unsigned char", "uint8_t"),
               "u16": ("ushort", "unsigned short", "unsigned short int", "uint16_t"),
               "i16": ("short", "short int", "signed short", "signed short int", "int16_t")}
    for kind, variants in aliases.items():
        for i, alias in enumerate(variants):
            add(f"alias-{kind}-{i}", kind, alias=alias)
    main_kwargs = {"spacings": (.25, .5, 1.5), "units": ("um", "um", "um")}
    main = add("u16-main", "u16", (5, 3, 2), **main_kwargs)
    source_changed = main.replace(b"\n\n", b"\n# authored source identity mutation\n\n", 1)
    source_changed_path = directory / "u16-source-changed.nrrd"
    source_changed_path.write_bytes(source_changed)
    changed_array, _ = nrrd.read(str(source_changed_path), index_order="C")
    assert changed_array.tobytes(order="C") == (directory / "u16-main.raw").read_bytes()
    add("float-volume", "f32", (5, 3, 2), endian="big")
    add("case-insensitive", "u16", (5, 3, 2), ci=True, oracle_seed="u16-little-3d")
    add("header-crlf", "u16", endings="crlf")
    add("header-mixed-endings", "u16", endings="mixed")
    for first in (9, 10, 32, 35):
        add("raster-leading-" + str(first), "u8", values=[first] + literals["u8"][1:])
    for unit, spacing in (("nm", (250, 500)), ("um", (.25, .5)), ("mm", (.00025, .0005)), ("cm", (.000025, .00005)), ("m", (.00000025, .0000005))):
        add("units-" + unit, "u16", spacings=spacing, units=(unit, unit))
    add("spacing-without-units", "u16", spacings=(.25, .5))
    add("units-without-spacing", "u16", units=("um", "mm"))
    add("calibration-partial", "u16", (5, 3, 2), spacings=(.25, "nan", 1.5), units=("um", "um", ""))
    add("u16-main-wide", "u16", (5, 3, 2), options='{"window":[0,65535]}', window=(0, 65535), **main_kwargs)
    add("i16-signed-window", "i16", options='{"window":[-32768,32767]}', window=(-32768, 32767))
    add("float-unit-window", "f32", (5, 3, 2), options='{"window":[0,1]}', window=(0, 1))
    add("window-exponents", "u16", options='{"window":[0e0,2.55E+2]}')
    long_window = '{"window":[0,1.' + '0' * (512 - len('{"window":[0,1.]}')) + ']}'
    assert len(long_window) == 512
    add("window-512bytes", "f32", options=long_window, window=(0, 1))
    add("float-nonfinite-only", "f32", values=[(0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345)[i % 5] for i in range(15)])
    add("type-word-tabs", "u16", alias="unsigned\tshort\tint", oracle_seed="u16-little-2d")

    def bad(name, data, status):
        (directory / (name + ".nrrd")).write_bytes(data)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(data).hexdigest()}

    bad("signature", b"NOPE0005" + main[8:], 2)
    bad("version4", main.replace(b"NRRD0005", b"NRRD0004", 1), 2)
    for name, old, new, status in (("missing-type", b"type: uint16\n", b"", 4), ("missing-dimension", b"dimension: 3\n", b"", 4),
                                   ("missing-sizes", b"sizes: 5 3 2\n", b"", 4), ("missing-encoding", b"encoding: raw\n", b"", 4),
                                   ("missing-endian", b"endian: little\n", b"", 4), ("missing-kinds", b"kinds: domain space domain\n", b"", 2),
                                   ("kinds-count", b"kinds: domain space domain", b"kinds: domain domain", 4),
                                   ("kinds-time", b"kinds: domain space domain", b"kinds: domain space time", 2),
                                   ("kinds-vector", b"kinds: domain space domain", b"kinds: vector space domain", 2),
                                   ("sizes-count", b"sizes: 5 3 2", b"sizes: 5 3", 4), ("size-zero", b"sizes: 5 3 2", b"sizes: 5 0 2", 4),
                                   ("dimension1", b"dimension: 3", b"dimension: 1", 2), ("dimension4", b"dimension: 3", b"dimension: 4", 2),
                                   ("type-double", b"type: uint16", b"type: double", 2), ("type-u32", b"type: uint16", b"type: uint32", 2),
                                   ("encoding-gzip", b"encoding: raw", b"encoding: gzip", 2), ("encoding-ascii", b"encoding: raw", b"encoding: ascii", 2),
                                   ("endian-invalid", b"endian: little", b"endian: middle", 4), ("leading-field-space", b"type: uint16", b" type: uint16", 4),
                                   ("missing-colon-space", b"type: uint16", b"type:uint16", 4),
                                   ("spacing-negative", b"spacings: 0.25 0.5 1.5", b"spacings: -0.25 0.5 1.5", 2),
                                   ("spacing-zero", b"spacings: 0.25 0.5 1.5", b"spacings: 0 0.5 1.5", 4),
                                   ("spacing-inf", b"spacings: 0.25 0.5 1.5", b"spacings: inf 0.5 1.5", 4),
                                   ("spacing-count", b"spacings: 0.25 0.5 1.5", b"spacings: 0.25 0.5", 4),
                                   ("units-unknown", b'units: "um" "um" "um"', b'units: "microns" "um" "um"', 2),
                                   ("units-case-sensitive", b'units: "um" "um" "um"', b'units: "UM" "um" "um"', 2),
                                   ("units-count", b'units: "um" "um" "um"', b'units: "um" "um"', 4),
                                   ("units-unquoted", b'units: "um" "um" "um"', b'units: um "um" "um"', 4)):
        assert old in main, name
        bad(name, main.replace(old, new, 1), status)
    bad("duplicate-type", main.replace(b"type: uint16\n", b"type: uint16\nTYPE: uint16\n", 1), 4)
    bad("duplicate-sizes", main.replace(b"sizes: 5 3 2\n", b"sizes: 5 3 2\nsizes: 5 3 2\n", 1), 4)
    bad("header-nonascii", main.replace(b"literal scalar test", b"literal\xc2\xb5", 1), 4)
    bad("header-nul", main.replace(b"literal scalar test", b"literal\0test", 1), 4)
    bad("missing-separator", main.replace(b"\n\n", b"\n", 1), 4)
    bad("payload-truncated", main[:-1], 4)
    bad("payload-trailing", main + b"\0", 4)
    bad("concatenated", main + main, 4)
    bad("header-bound", main.replace(b"# Repository-authored scalar codes\n", b"#" + b"a" * 65536 + b"\n", 1), 3)
    bad("size-arithmetic-bound", main.replace(b"sizes: 5 3 2", b"sizes: 9223372036854775807 2 2", 1), 3)
    bad("calibration-overflow", main.replace(b"spacings: 0.25 0.5 1.5", b"spacings: 1e308 0.5 1.5", 1).replace(b'units: "um"', b'units: "m"', 1), 2)
    for name, field in (("space", "space: right-anterior-superior"), ("space-origin", "space origin: (0,0,0)"),
                        ("space-directions", "space directions: (1,0,0) (0,1,0) (0,0,1)"), ("measurement-frame", "measurement frame: (1,0,0) (0,1,0) (0,0,1)"),
                        ("axis-mins", "axis mins: 0 0 0"), ("axis-maxs", "axis maxs: 5 3 2"), ("centers", "centers: cell cell cell"),
                        ("thicknesses", "thicknesses: 1 1 1"), ("sample-units", "sample units: counts"), ("labels", 'labels: "x" "y" "z"'),
                        ("line-skip", "line skip: 0"), ("byte-skip", "byte skip: 0"), ("detached", "data file: authored.raw"),
                        ("custom-key", "authored:=uninterpreted"), ("unknown-field", "authored meaning: unknown")):
        bad(name, main.replace(b"\n\n", b"\n" + field.encode("ascii") + b"\n\n", 1), 2)

    resource_values = [17] * (256 * 256)
    resource = authored("u8", (256, 256), resource_values)
    resource_path = directory / "budget-input.nrrd"
    resource_path.write_bytes(resource)
    array, _ = nrrd.read(str(resource_path), index_order="C")
    assert array.shape == (256, 256) and array.tobytes() == bytes(resource_values)
    assert len(resource) > 32768 and len(resource) < 256 * 1024 * 1024
    unicode_name = "NRRD-\u03bc-\u56fe.nrrd"
    (directory / unicode_name).write_bytes(main)
    manifest = {"schema": "qpx.nrrd-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"pynrrd": nrrd.__version__, "numpy": np.__version__, "index_order": "C", "native_byte_order": sys.byteorder,
                           "basis": "Authored integer/F32 bitwords; pynrrd z,y,x array byte checks with byte-swap/view only; Decimal190 display math, no NaN/Inf/negative-zero float conversion",
                           "header_adapter_limit": "pynrrd field/type tokens are case-sensitive and type-word tabs are not normalized; normative variants use a canonical-header seed with identical literal payload."},
                "unicode_filename": {"path": unicode_name, "control": "u16-main", "sha256": hashlib.sha256(main).hexdigest()},
                "source_change_control": {"filename": "u16-source-changed.nrrd", "sha256": hashlib.sha256(source_changed).hexdigest(),
                                          "pynrrd_bit_exact": True, "raw_codes_unchanged": True},
                "resource_fixtures": {"budget-input": {"filename": "budget-input.nrrd", "encoded_bytes": len(resource), "work_budget": 32768,
                                       "sha256": hashlib.sha256(resource).hexdigest(), "pynrrd_bit_exact": True,
                                       "scope": "One owned inline raw input exceeds caller budget; no expanded decoder/RGBA cache claim"}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "width", "height", "z_planes", "sample_type", "sample_bytes", "dimension_order", "calibration_flags", "pixel_width_um", "pixel_height_um", "z_spacing_um")
    (directory / "fixtures.tsv").write_text("".join("\t".join(str(c[key]) for key in columns) + "\t" + (c["options"] or "-") + "\n" for c in cases), encoding="utf8")
    (directory / "negative.tsv").write_text("".join(f"{name}\t{row['status']}\n" for name, row in negatives.items()), encoding="utf8")
    return manifest

def run(command, directory, name, rows):
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8", errors="strict")
    log = directory / (name + ".log")
    log.write_text(result.stdout + result.stderr, encoding="utf8")
    rows.append({"name": name, "command": command, "exit": result.returncode, "log": str(log)})
    if result.returncode:
        raise RuntimeError(f"{name} failed: {result.returncode}\n{result.stdout}{result.stderr}")


def validate(args):
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Validation requires a new or empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    fixtures = output / "fixtures"
    fixture_manifest = write_fixtures(fixtures)
    rows = []
    artifacts = {name: str(getattr(args, name).resolve()) if getattr(args, name) else None
                 for name in ("module", "abi_test", "consumer", "region", "manifest")}

    def hashes():
        result = {}
        for name, path in artifacts.items():
            try:
                result[name] = hashlib.sha256(Path(path).read_bytes()).hexdigest() if path else None
            except OSError as error:
                result[name] = {"error": str(error)}
        return result

    before = hashes()
    assert all(value is None or isinstance(value, str) for value in before.values()), before
    report = {"schema": "qpx.nrrd-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]),
              "negative_cases": len(fixture_manifest["negative"]), "checks": rows,
              "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        run([str(args.abi_test.resolve()), str(args.module.resolve()), str(fixtures)], output, "direct-abi", rows)
        if args.consumer and args.manifest:
            work = output / "project"
            work.mkdir()
            consumer = [str(args.consumer.resolve())]
            for mode in ("create", "reopen", "missing"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)

            def stage_manifest(directory, *, overlay=False, window=None):
                directory.mkdir()
                changed = json.loads(args.manifest.read_text(encoding="utf8"))
                for descriptor in changed["plugins"]:
                    target = directory / descriptor["module"]["path"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    data = (args.manifest.parent / descriptor["module"]["path"]).read_bytes()
                    if overlay:
                        data += b"QPX NRRD inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    if window:
                        descriptor["options"] = {"window": window}
                    for dependency in descriptor["dependencies"]:
                        destination = directory / dependency["path"]
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(args.manifest.parent / dependency["path"], destination)
                manifest = directory / "reader-plugins.json"
                manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
                return manifest

            changed_manifest = stage_manifest(output / "changed-package", overlay=True)
            run(consumer + ["changed-implementation", str(changed_manifest), str(fixtures), str(work)], output, "project-changed-implementation", rows)
            options_manifest = stage_manifest(output / "options-package", window=[0, 65535])
            run(consumer + ["changed-options", str(options_manifest), str(fixtures), str(work)], output, "project-changed-options", rows)
            run(consumer + ["changed-source", str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-changed-source", rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.nrrd", str(fixtures / "u16-main.nrrd"), "0", "0", "0", "5", "3", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "u16-main.rgba").read_bytes(), "Host PAM differs from independent scalar-window oracle"
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = hashes()
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation input module/executable/manifest bytes changed; see validation.json")
    print(json.dumps({"passed": True, "fixture_cases": len(fixture_manifest["cases"]), "native_checks": len(rows), "report": str(output / "validation.json")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="mode", required=True)
    fixtures = commands.add_parser("fixtures")
    fixtures.add_argument("output", type=Path)
    validation = commands.add_parser("validate")
    for argument in ("module", "abi-test", "output"):
        validation.add_argument("--" + argument, required=True, type=Path)
    for argument in ("consumer", "manifest", "region"):
        validation.add_argument("--" + argument, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        manifest = write_fixtures(args.output.resolve())
        print(json.dumps({"cases": len(manifest["cases"]), "negatives": len(manifest["negative"]), "output": str(args.output.resolve())}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
