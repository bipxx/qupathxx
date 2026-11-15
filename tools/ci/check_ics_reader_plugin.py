#!/usr/bin/env python3
"""Development-only literal ICS/IDS fixtures, native libics oracle and controls."""
import argparse
from decimal import Decimal, localcontext, ROUND_HALF_UP
import hashlib
import itertools
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys


TYPES = {"u8": (1, 1, "B", "integer", "unsigned", "uint8"), "u16": (2, 2, "H", "integer", "unsigned", "uint16"),
         "i16": (4, 2, "h", "integer", "signed", "int16"), "f32": (9, 4, "I", "real", "signed", "float32")}


def rgba_oracle(values, kind, width, height, channels, selected, window):
    with localcontext() as context:
        context.prec = 190
        low, high = (Decimal.from_float(float(v)) for v in window)
        result = bytearray()
        for pixel in range(width * height):
            word = values[pixel * channels + selected]
            if kind == "f32":
                exponent, fraction = (word >> 23) & 255, word & 0x7fffff
                if exponent == 255:
                    result += bytes(4)
                    continue
                numeric = Decimal(fraction if exponent == 0 else (1 << 23) + fraction) * Decimal(2) ** (-149 if exponent == 0 else exponent - 150)
                if word >> 31:
                    numeric = -numeric
            else:
                numeric = Decimal(word)
            normalized = min(Decimal(1), max(Decimal(0), (numeric - low) / (high - low)))
            code = int((normalized * 255).quantize(Decimal(1), rounding=ROUND_HALF_UP))
            result += bytes((code, code, code, 255))
        return bytes(result)


def write_fixtures(directory, oracle=None):
    oracle_before = hashlib.sha256(oracle.read_bytes()).hexdigest() if oracle else None
    directory.mkdir(parents=True, exist_ok=True)
    reference_dir = directory / ".reference"
    reference_dir.mkdir(exist_ok=True)
    cases, negatives = [], {}
    literals = {"u8": [0, 9, 10, 13, 32, 35, 127, 128, 129, 254, 255, 1, 2, 3, 4],
                "u16": [0, 1, 127, 128, 129, 254, 255, 256, 257, 1000, 4095, 4096, 32767, 32768, 65535],
                "i16": [-32768, -257, -256, -255, -129, -128, -1, 0, 1, 127, 128, 255, 256, 32766, 32767],
                "f32": [0, 0x80000000, 0x3f000000, 0x3f800000, 0xbf800000, 0x437f0000, 0x43800000,
                        0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345, 0xffa54321, 1, 0x80000001]}

    def values_for(kind, dimensions, channels):
        width, height = dimensions[:2]
        return [literals[kind][(x + y * width + ch * 3 + z * 7) % 15] for z in range(dimensions[2] if len(dimensions) == 3 else 1)
                for y in range(height) for x in range(width) for ch in range(channels)]

    def pair(name, kind, dimensions, channels, values, *, version=1, channel_axis=None, ranks=None, scales=None, units=None,
             endings="lf", labels=True, history="Literal repository-authored values", end_tab=True, filename=None, blank=False):
        _, sample, pack, form, sign, _ = TYPES[kind]
        channel_axis = channels > 1 if channel_axis is None else channel_axis
        ranks = tuple(range(1, sample + 1)) if ranks is None else tuple(ranks)
        assert sorted(ranks) == list(range(1, sample + 1))
        axes = (["ch"] if channel_axis else []) + ["x", "y"] + (["z"] if len(dimensions) == 3 else [])
        sizes = ([channels] if channel_axis else []) + list(dimensions)
        scales = tuple(scales or [1] * len(dimensions))
        units = tuple(["undefined"] * len(dimensions) if units is None else units)
        origins = [0] * (len(axes) + 1)
        all_scales = [1] + ([1] if channel_axis else []) + list(scales)
        all_units = ["relative"] + (["undefined"] if channel_axis else []) + list(units)
        rows = [["ics_version", str(version) + ".0"], ["filename", filename or name], ["layout", "parameters", str(len(axes) + 1)],
                ["layout", "order", "bits"] + axes, ["layout", "sizes", str(sample * 8)] + list(map(str, sizes)),
                ["layout", "coordinates", "video"], ["layout", "significant_bits", str(sample * 8)],
                ["representation", "format", form], ["representation", "sign", sign],
                ["representation", "compression", "uncompressed"], ["representation", "byte_order"] + list(map(str, ranks)),
                ["parameter", "origin"] + list(map(str, origins)), ["parameter", "scale"] + list(map(str, all_scales)),
                ["parameter", "units"] + all_units]
        if labels:
            rows.append(["parameter", "labels", "intensity"] + axes)
        rows.append(["history", "qpx", history])
        if blank:
            rows.insert(6, [""])
        if version == 2:
            rows.append(["end", ""] if end_tab else ["end"])
        def newline(index):
            return b"\r\n" if endings == "crlf" or endings == "mixed" and index % 2 else b"\n"
        header = b"\t" + newline(0) + b"".join("\t".join(row).encode("ascii") + newline(i + 1) for i, row in enumerate(rows))
        little = struct.pack("<" + pack * len(values), *values)
        payload = bytes(little[at + rank - 1] for at in range(0, len(little), sample) for rank in ranks)
        return header, payload, axes, sizes, all_scales, all_units

    def gate(name, kind, expected, axes, sizes, scales, units):
        if oracle is None:
            return {"pending": True}
        prefix = ".reference/" + name
        command = [str(oracle.resolve()), name + ".ics", prefix]
        result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8", errors="strict")
        (reference_dir / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
        raw_path, meta_path = directory / (prefix + ".raw"), directory / (prefix + ".json")
        assert raw_path.read_bytes() == expected, (name, "libics decoded bytes differ from literal original codes")
        metadata = json.loads(meta_path.read_text(encoding="utf8"))
        assert metadata["type"] == TYPES[kind][5] and metadata["sample_bytes"] == TYPES[kind][1]
        assert metadata["data_bytes"] == len(expected) and metadata["dimensions"] == sizes
        assert [a["order"] for a in metadata["axes"]] == axes
        assert metadata["significant_bits"] == TYPES[kind][1] * 8 and metadata["coordinates"] == "video"
        assert metadata["scalar_origin"] == 0 and metadata["scalar_scale"] == 1 and metadata["scalar_units"] == "relative"
        for actual, scale, unit in zip(metadata["axes"], scales[1:], units[1:]):
            assert actual["origin"] == 0 and actual["scale"] == float(scale)
            assert actual["unit"] == unit or not unit and actual["unit"] == "undefined"
        return {"passed": True, "command": command, "raw_sha256": hashlib.sha256(raw_path.read_bytes()).hexdigest(),
                "metadata_sha256": hashlib.sha256(meta_path.read_bytes()).hexdigest()}

    def add(name, kind, dimensions=(5, 3, 2), channels=3, *, values=None, options="", selected=0, window=(0, 255), **kwargs):
        values = values_for(kind, dimensions, channels) if values is None else list(values)
        assert len(values) == math.prod(dimensions) * channels
        header, payload, axes, sizes, scales, units = pair(name, kind, dimensions, channels, values, **kwargs)
        version = kwargs.get("version", 1)
        primary = header + (payload if version == 2 else b"")
        (directory / (name + ".ics")).write_bytes(primary)
        if version == 1:
            (directory / (name + ".ids")).write_bytes(payload)
        raw = struct.pack("=" + TYPES[kind][2] * len(values), *values)
        rgba = rgba_oracle(values, kind, *dimensions[:2], channels, selected, window)
        (directory / (name + ".raw")).write_bytes(raw)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if rgba[3::4] == bytes([255]) * math.prod(dimensions[:2]):
            (directory / (name + ".rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        physical, flags = [0., 0., 0.], 0
        spatial_scale = kwargs.get("scales", [1] * len(dimensions))
        spatial_units = kwargs.get("units", ["undefined"] * len(dimensions))
        for axis, (scale, unit) in enumerate(zip(spatial_scale, spatial_units)):
            if unit not in ("", "undefined"):
                physical[axis] = float(scale) * {"nm": .001, "um": 1., "mm": 1000., "cm": 10000., "m": 1000000.}[unit]
                flags |= 1 << axis
        reference = gate(name, kind, raw, axes, sizes, scales, units)
        cases.append({"name": name, "kind": kind, "version": version, "width": dimensions[0], "height": dimensions[1],
                      "z_planes": dimensions[2] if len(dimensions) == 3 else 1, "channels": channels,
                      "sample_type": TYPES[kind][0], "sample_bytes": TYPES[kind][1], "dimension_order": "".join(axes).upper().replace("CH", "C"),
                      "calibration_flags": flags, "pixel_width_um": physical[0], "pixel_height_um": physical[1], "z_spacing_um": physical[2],
                      "options": options, "selected_channel": selected, "window": list(window), "byte_order": list(kwargs.get("ranks", range(1, TYPES[kind][1] + 1))),
                      "primary_sha256": hashlib.sha256(primary).hexdigest(), "companion_sha256": hashlib.sha256(payload).hexdigest() if version == 1 else None,
                      "raw_sha256": hashlib.sha256(raw).hexdigest(), "rgba_sha256": hashlib.sha256(rgba).hexdigest(), "reference": reference})
        return header, payload

    for kind in TYPES:
        for version in (1, 2):
            add(f"{kind}-v{version}-channels", kind, version=version)
        add(kind + "-scalar-xy", kind, (5, 3), 1)
        add(kind + "-scalar-xyz", kind, (5, 3, 2), 1, version=2)
    main_args = {"scales": (.25, .5, 1.5), "units": ("um", "um", "um")}
    main_header, main_payload = add("channels-main", "u16", **main_args)
    inline_header, inline_payload = add("float-inline", "f32", channels=2, version=2)
    for ranks in itertools.permutations((1, 2, 3, 4)):
        add("f32-perm-" + "".join(map(str, ranks)), "f32", channels=2, version=2, ranks=ranks)
    add("u16-big-endian", "u16", ranks=(2, 1))
    add("i16-big-endian", "i16", ranks=(2, 1))
    add("header-crlf", "u16", endings="crlf")
    add("header-mixed", "u16", endings="mixed", blank=True)
    for first in (9, 10, 13, 32, 35):
        add("inline-leading-" + str(first), "u8", (5, 3), 1, version=2, values=[first] + literals["u8"][1:])
    add("inline-end-no-tab", "u8", (5, 3), 1, version=2, end_tab=False)
    for unit, scales in (("nm", (250, 500, 1500)), ("um", (.25, .5, 1.5)), ("mm", (.00025, .0005, .0015)),
                         ("cm", (.000025, .00005, .00015)), ("m", (.00000025, .0000005, .0000015))):
        add("units-" + unit, "u16", channels=2, scales=scales, units=(unit,) * 3)
    add("empty-units", "u16", units=("", "", ""))
    add("partial-calibration", "u16", scales=(.25, .5, 1.5), units=("um", "undefined", "um"))
    add("no-optional-labels", "u16", labels=False)
    add("history-inert", "u16", history="reversed=true; do not infer a pixel transform from history")
    add("channel-limit256", "u8", (1, 1), 256, options='{"channel":255,"window":[0,255]}', selected=255)
    add("channels-main-view", "u16", options='{"channel":1,"window":[0,65535]}', selected=1, window=(0, 65535), **main_args)
    add("channels-main-ch2", "u16", options='{"channel":2,"window":[0,255]}', selected=2, **main_args)
    add("float-unit-window", "f32", channels=2, version=2, options='{"channel":1,"window":[0,1]}', selected=1, window=(0, 1))
    prefix = '{"channel":0,"window":[0,1.'
    long_options = prefix + '0' * (512 - len(prefix) - 2) + ']}'
    assert len(long_options) == 512
    add("options-512bytes", "f32", channels=2, version=2, options=long_options, window=(0, 1))

    def bad(name, header, status, payload=main_payload, version=1, companion=True):
        (directory / (name + ".ics")).write_bytes(header + (payload if version == 2 else b""))
        if version == 1 and companion:
            (directory / (name + ".ids")).write_bytes(payload)
        negatives[name] = {"status": status, "primary_sha256": hashlib.sha256(header + (payload if version == 2 else b"")).hexdigest()}

    for name, old, new, status in (("version", b"ics_version\t1.0", b"ics_version\t3.0", 2),
                                   ("missing-filename", b"filename\tchannels-main\n", b"", 4),
                                   ("empty-filename", b"filename\tchannels-main", b"filename\t", 4),
                                   ("parameters-count", b"parameters\t5", b"parameters\t4", 4),
                                   ("sizes-count", b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t3\t5\t3", 4),
                                   ("channel-zero", b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t0\t5\t3\t2", 4),
                                   ("channel-limit257", b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t257\t5\t3\t2", 3),
                                   ("order-reordered", b"order\tbits\tch\tx\ty\tz", b"order\tbits\tx\tch\ty\tz", 2),
                                   ("coordinates", b"coordinates\tvideo", b"coordinates\tcartesian", 2),
                                   ("partial-significant", b"significant_bits\t16", b"significant_bits\t12", 2),
                                   ("compression", b"compression\tuncompressed", b"compression\tgzip", 2),
                                   ("byte-order-zero", b"byte_order\t1\t2", b"byte_order\t0\t2", 4),
                                   ("byte-order-duplicate", b"byte_order\t1\t2", b"byte_order\t1\t1", 4),
                                   ("byte-order-range", b"byte_order\t1\t2", b"byte_order\t1\t3", 4),
                                   ("byte-order-count", b"byte_order\t1\t2", b"byte_order\t1", 4),
                                   ("nonzero-origin", b"origin\t0\t0\t0\t0\t0", b"origin\t0\t0\t1\t0\t0", 2),
                                   ("scaled-scalar", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t2\t1\t0.25\t0.5\t1.5", 2),
                                   ("scaled-channel", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t2\t0.25\t0.5\t1.5", 2),
                                   ("negative-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\t-0.25\t0.5\t1.5", 2),
                                   ("zero-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\t0\t0.5\t1.5", 4),
                                   ("nan-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\tnan\t0.5\t1.5", 4),
                                   ("inf-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\tinf\t0.5\t1.5", 4),
                                   ("unknown-units", b"units\trelative\tundefined\tum\tum\tum", b"units\trelative\tundefined\tmicrons\tum\tum", 2),
                                   ("units-case", b"units\trelative\tundefined\tum\tum\tum", b"units\trelative\tundefined\tUM\tum\tum", 2),
                                   ("scalar-units", b"units\trelative\tundefined", b"units\tcounts\tundefined", 2),
                                   ("channel-units", b"units\trelative\tundefined", b"units\trelative\tum", 2),
                                   ("labels-noncanonical", b"labels\tintensity\tch\tx\ty\tz", b"labels\tintensity\twavelength\tx\ty\tz", 2)):
        assert old in main_header, name
        bad(name, main_header.replace(old, new, 1), status)
    for token in (b"layout\tparameters\t5\n", b"layout\torder\tbits\tch\tx\ty\tz\n", b"layout\tsizes\t16\t3\t5\t3\t2\n",
                  b"layout\tcoordinates\tvideo\n", b"layout\tsignificant_bits\t16\n", b"representation\tformat\tinteger\n",
                  b"representation\tsign\tunsigned\n", b"representation\tcompression\tuncompressed\n", b"representation\tbyte_order\t1\t2\n",
                  b"parameter\torigin\t0\t0\t0\t0\t0\n", b"parameter\tscale\t1\t1\t0.25\t0.5\t1.5\n", b"parameter\tunits\trelative\tundefined\tum\tum\tum\n"):
        bad("missing-" + token.split(b"\t")[1].decode(), main_header.replace(token, b"", 1), 4)
    unsupported_u32 = main_header.replace(b"sizes\t16\t", b"sizes\t32\t", 1).replace(b"significant_bits\t16", b"significant_bits\t32", 1).replace(b"byte_order\t1\t2", b"byte_order\t1\t2\t3\t4", 1)
    bad("unsupported-u32", unsupported_u32, 2)
    unsupported_f64 = unsupported_u32.replace(b"sizes\t32\t", b"sizes\t64\t", 1).replace(b"significant_bits\t32", b"significant_bits\t64", 1).replace(b"format\tinteger", b"format\treal", 1).replace(b"sign\tunsigned", b"sign\tsigned", 1).replace(b"byte_order\t1\t2\t3\t4", b"byte_order\t1\t2\t3\t4\t5\t6\t7\t8", 1)
    bad("unsupported-f64", unsupported_f64, 2)
    bad("duplicate-sizes", main_header.replace(b"layout\tsizes", b"layout\tsizes\t16\t3\t5\t3\t2\nlayout\tsizes", 1), 4)
    bad("header-no-final-newline", main_header[:-1], 4)
    bad("header-nonascii", main_header.replace(b"Literal", b"Literal\xc2\xb5", 1), 4)
    bad("header-nul", main_header.replace(b"Literal", b"Literal\0", 1), 4)
    bad("header-bound", main_header + b"history\tqpx\t" + b"a" * 65536 + b"\n", 3)
    bad("size-arithmetic-bound", main_header.replace(b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t3\t9223372036854775807\t3\t2", 1), 3)
    overflow = main_header.replace(b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\t1e308\t0.5\t1.5", 1).replace(b"units\trelative\tundefined\tum", b"units\trelative\tundefined\tm", 1)
    bad("calibration-overflow", overflow, 2)
    bad("ids-truncated", main_header, 4, payload=main_payload[:-1])
    bad("ids-trailing", main_header, 4, payload=main_payload + b"\0")
    bad("ids-concatenated", main_header, 4, payload=main_payload + main_payload)
    bad("inline-truncated", inline_header, 4, payload=inline_payload[:-1], version=2)
    bad("inline-trailing", inline_header, 4, payload=inline_payload + b"\0", version=2)
    bad("inline-end-value", inline_header.replace(b"end\t\n", b"end\tunexpected\n", 1), 4, payload=inline_payload, version=2)
    bad("missing-derived-companion", main_header, 4, companion=False)
    (directory / "missing-derived-companion.ids.gz").write_bytes(main_payload)
    assert not (directory / "missing-derived-companion.ids").exists()
    for name, row in (("sensor", b"sensor\ttype\tIncohConfocal\n"), ("scil", b"layout\tscil_type\tg3d\n"),
                      ("source-file", b"source\tfile\tchannels-main.ids\n"), ("source-offset", b"source\toffset\t0\n"),
                      ("unknown", b"authored\tmeaning\tunknown\n")):
        bad(name, main_header + row, 2)

    changed_header = main_header + b"history\tqpx\tprimary-only exact identity mutation\n"
    (directory / "primary-changed.ics").write_bytes(changed_header)
    changed_payload = bytes((main_payload[0] ^ 1,)) + main_payload[1:]
    (directory / "companion-changed.ids").write_bytes(changed_payload)
    file_set_header, file_set_payload, *_ = pair("file-set-changed", "u16", (5, 3, 2), 3, values_for("u16", (5, 3, 2), 3), version=2, **main_args)
    (directory / "file-set-changed.ics").write_bytes(file_set_header + file_set_payload)
    resource_values = [17] * (256 * 256)
    resource_header, resource_payload, *_ = pair("budget-input", "u8", (256, 256), 1, resource_values)
    (directory / "budget-input.ics").write_bytes(resource_header)
    (directory / "budget-input.ids").write_bytes(resource_payload)
    assert len(resource_header) < 4096 and len(resource_header) + len(resource_payload) > 32768
    unicode_name = "ICS-\u03bc-\u56fe"
    (directory / (unicode_name + ".ics")).write_bytes(main_header)
    (directory / (unicode_name + ".ids")).write_bytes(main_payload)
    oracle_after = hashlib.sha256(oracle.read_bytes()).hexdigest() if oracle else None
    assert oracle_before == oracle_after, "Reference executable changed while qualifying fixtures"
    manifest = {"schema": "qpx.ics-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"kind": "Native libics1.7.2 public API helper; test-only",
                           "primary_authority": "Literal original integer/F32 bitwords plus Decimal190 channel/window expectations",
                           "source_pin": "c1a1ae6f46ed1167001c0a14d9ef767c37ff4833", "pending": oracle is None,
                           "executable_sha256": oracle_before, "executable_unchanged": oracle_before == oracle_after,
                           "ascii_relative_paths": True, "scope": "No libics code/dependency linked into the native provider"},
                "unicode_filename": {"stem": unicode_name, "control": "channels-main"},
                "resource_fixtures": {"budget-input": {"primary_bytes": len(resource_header), "companion_bytes": len(resource_payload),
                                       "work_budget": 32768, "discovery_budget": 16384, "scope": "Owned primary+IDS input bound; discovery reads only primary"}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "version", "width", "height", "z_planes", "channels", "sample_type", "sample_bytes", "dimension_order", "calibration_flags", "pixel_width_um", "pixel_height_um", "z_spacing_um")
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
    rows = []
    artifacts = {name: str(getattr(args, name).resolve()) if getattr(args, name) else None
                 for name in ("module", "abi_test", "consumer", "region", "manifest", "oracle")}

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
    report = {"schema": "qpx.ics-plugin-validation/1", "checks": rows,
              "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        fixture_manifest = write_fixtures(fixtures, args.oracle)
        report.update(fixture_cases=len(fixture_manifest["cases"]), negative_cases=len(fixture_manifest["negative"]),
                      independent_reference=fixture_manifest["oracle"])
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
                        data += b"QPX ICS inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    if window:
                        descriptor["options"] = {"channel": 1, "window": window}
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
            for mode in ("changed-source", "changed-companion", "changed-file-set"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.ics", str(fixtures / "channels-main.ics"), "0", "0", "0", "5", "3", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "channels-main.rgba").read_bytes(), "Host PAM differs from independent scalar-window oracle"
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = hashes()
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation input module/executable/manifest bytes changed; see validation.json")
    print(json.dumps({"passed": True, "fixture_cases": len(fixture_manifest["cases"]), "native_checks": len(rows), "oracle_exe_sha256": before["oracle"], "report": str(output / "validation.json")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="mode", required=True)
    fixtures = commands.add_parser("fixtures")
    fixtures.add_argument("output", type=Path)
    fixtures.add_argument("--oracle", type=Path)
    validation = commands.add_parser("validate")
    for argument in ("module", "abi-test", "output", "oracle"):
        validation.add_argument("--" + argument, required=True, type=Path)
    for argument in ("consumer", "manifest", "region"):
        validation.add_argument("--" + argument, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        manifest = write_fixtures(args.output.resolve(), args.oracle)
        print(json.dumps({"cases": len(manifest["cases"]), "negatives": len(manifest["negative"]), "output": str(args.output.resolve())}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
