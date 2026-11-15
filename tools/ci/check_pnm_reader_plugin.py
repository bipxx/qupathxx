#!/usr/bin/env python3
"""Development-only authored PNM/PAM fixtures, independent oracles and native controls."""
import argparse
from decimal import Decimal, localcontext, ROUND_HALF_UP
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys


def display_code(value, maximum, transfer):
    """High-precision spec math, independent of the native double LUT."""
    with localcontext() as context:
        context.prec = 70
        x = Decimal(value) / Decimal(maximum)
        if transfer == "srgb":
            encoded = x
        else:
            if transfer == "bt709":
                linear = x / Decimal("4.5") if x < Decimal("0.081") else ((x + Decimal("0.099")) / Decimal("1.099")) ** (Decimal(20) / 9)
            elif transfer == "linear":
                linear = x
            else:
                raise ValueError(transfer)
            encoded = Decimal("12.92") * linear if linear <= Decimal("0.0031308") else Decimal("1.055") * linear ** (Decimal(5) / 12) - Decimal("0.055")
        return int((encoded * 255).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def rgba_oracle(values, channels, maximum, semantic, transfer):
    result = bytearray()
    for at in range(0, len(values), channels):
        pixel = values[at:at + channels]
        if semantic == "blackness":
            rgb = [255 * (1 - pixel[0])] * 3
        elif semantic in ("gray", "ga"):
            rgb = [display_code(pixel[0], maximum, transfer)] * 3
        else:
            rgb = [display_code(v, maximum, transfer) for v in pixel[:3]]
        alpha = int((Decimal(pixel[-1]) * 255 / maximum).quantize(Decimal(1), rounding=ROUND_HALF_UP)) if semantic in ("ga", "rgba") else 255
        result += bytes((*rgb, alpha))
    return bytes(result)


def write_fixtures(directory):
    from PIL import Image, __version__ as pillow_version
    directory.mkdir(parents=True, exist_ok=True)
    # Literal anchors include identity, opposite PBM polarity and exact half-up.
    assert [display_code(v, 255, "srgb") for v in (0, 1, 64, 128, 255)] == [0, 1, 64, 128, 255]
    assert [display_code(v, 255, "linear") for v in (0, 1, 64, 128, 255)] == [0, 13, 137, 188, 255]
    assert [display_code(v, 255, "bt709") for v in (0, 1, 9, 10, 35, 64, 128, 255)] == [0, 3, 22, 23, 51, 79, 140, 255]
    assert display_code(1, 2, "srgb") == 128
    cases, negatives = [], {}
    width, height = 5, 3
    rgb = [0, 1, 2, 9, 10, 32, 35, 127, 128, 254, 255, 64, 21, 46, 201,
           73, 99, 133, 200, 17, 240, 3, 222, 88, 155, 111, 42, 31, 63, 95,
           4, 8, 16, 32, 64, 128, 127, 129, 253, 89, 144, 233, 255, 0, 255]
    assert len(rgb) == width * height * 3

    def add(name, kind, values, maximum=255, *, w=width, h=height, semantic="gray", transfer="", payload=None, delimiter=b"\n", crlf=False, pillow=False):
        channels = {"blackness": 1, "gray": 1, "ga": 2, "rgb": 3, "rgba": 4}[semantic]
        assert len(values) == w * h * channels and all(0 <= v <= maximum for v in values)
        if kind in (1, 4):
            assert maximum == 1 and semantic == "blackness"
        if payload is None:
            if kind in (1, 2, 3):
                header = f"P{kind}\n# Authored literal samples\n{w} {h}\n" + (f"{maximum}\n" if kind != 1 else "")
                payload = header.encode() + (" ".join(str(v) for v in values) + "\n# permitted ASCII tail\n").encode()
            elif kind == 4:
                raster = bytearray()
                for y in range(h):
                    for x in range(0, w, 8):
                        byte = 0
                        for bit in range(8):
                            # One padding bit intentionally set: padding is not image data.
                            byte |= (values[y * w + x + bit] if x + bit < w else 1) << (7 - bit)
                        raster.append(byte)
                payload = f"P4\n{w} {h}".encode() + delimiter + raster
            elif kind in (5, 6):
                raster = bytes(values) if maximum <= 255 else struct.pack(">" + "H" * len(values), *values)
                payload = f"P{kind}\n{w} {h}\n{maximum}".encode() + delimiter + raster
            else:
                tuples = {"gray": "GRAYSCALE", "ga": "GRAYSCALE_ALPHA", "rgb": "RGB", "rgba": "RGB_ALPHA"}
                tuple_name = ("BLACKANDWHITE_ALPHA" if semantic == "ga" else "BLACKANDWHITE") if maximum == 1 and semantic in ("gray", "ga") else tuples[semantic]
                newline = "\r\n" if crlf else "\n"
                header = newline.join(["P7", "# Authored PAM semantics", f"WIDTH {w}", f"HEIGHT {h}", f"DEPTH {channels}", f"MAXVAL {maximum}", f"TUPLTYPE {tuple_name}", "ENDHDR", ""])
                payload = header.encode() + (bytes(values) if maximum <= 255 else struct.pack(">" + "H" * len(values), *values))
        extension = "pbm" if kind in (1, 4) else "pgm" if kind in (2, 5) else "ppm" if kind in (3, 6) else "pam"
        filename = name + "." + extension
        path = directory / filename
        path.write_bytes(payload)
        raw = bytes(values) if maximum <= 255 else struct.pack("=" + "H" * len(values), *values)
        rgba = rgba_oracle(values, channels, maximum, semantic, transfer or "bt709")
        if pillow:
            decoded = Image.open(path)
            decoded.load()
            if kind in (1, 4):
                assert list(decoded.convert("L").tobytes()) == [255 * (1 - v) for v in values], name
            else:
                assert decoded.size == (w, h) and list(decoded.get_flattened_data()) == (values if channels == 1 else [tuple(values[i:i + channels]) for i in range(0, len(values), channels)]), name
        (directory / (name + ".raw")).write_bytes(raw)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if channels == 3:
            (directory / (name + ".display-rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        cases.append({"name": name, "filename": filename, "width": w, "height": h, "channels": channels,
                      "sample_bytes": 1 if maximum <= 255 else 2, "semantic": semantic, "transfer": transfer or "default",
                      "maxval": maximum, "source_sha256": hashlib.sha256(payload).hexdigest(),
                      "raw_sha256": hashlib.sha256(raw).hexdigest(), "rgba_sha256": hashlib.sha256(rgba).hexdigest(),
                      "independent_pillow_raw_verified": pillow})

    pbm = [0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0]
    add("p1-spaced", 1, pbm, 1, semantic="blackness", pillow=True)
    add("p1-contiguous", 1, pbm, 1, semantic="blackness", payload=b"P1\n5 3\n010111001010100\n# tail\n", pillow=True)
    add("p1-comments", 1, pbm, 1, semantic="blackness", payload=b"P1\n# dimensions\n5 # width\n3\n01011# row0\n10010# row1\n10100\n", pillow=True)
    add("p4-padding", 4, [int((x + y) % 3 == 0) for y in range(3) for x in range(9)], 1, w=9, semantic="blackness", pillow=True)
    for byte in (9, 10, 32, 35):
        values = [(v >> (7 - bit)) & 1 for v in (byte, 0x55, 0xaa) for bit in range(8)]
        add("p4-leading-" + str(byte), 4, values, 1, w=8, semantic="blackness", pillow=True)
    for maximum in (1, 2, 15, 255, 256, 1000, 65535):
        gray = [0, maximum, 1, maximum // 2, max(0, maximum - 1)] * 3
        color = [0, maximum, 1, maximum // 2, max(0, maximum - 1)] * 9
        for kind in (2, 5):
            add(f"p{kind}-max{maximum}", kind, gray, maximum, pillow=maximum == 255 or kind == 5 and maximum == 65535)
        for kind in (3, 6):
            add(f"p{kind}-max{maximum}", kind, color, maximum, semantic="rgb", pillow=maximum == 255)
    add("rgb-main", 6, rgb, semantic="rgb", pillow=True)
    add("rgb-source-changed", 6, rgb[:-1] + [254], semantic="rgb", pillow=True,
        payload=b"P6\n# authored source-change control\n5 3\n255\n" + bytes(rgb[:-1] + [254]))
    gray16 = [0, 1, 255, 256, 257, 1000, 4095, 4096, 32767, 32768, 65280, 65534, 65535, 4660, 43981]
    add("gray16", 5, gray16, 65535, pillow=True)
    for transfer in ("bt709", "srgb", "linear"):
        add("rgb-main-" + transfer, 6, rgb, semantic="rgb", transfer=transfer, pillow=True)
        add("gray16-" + transfer, 5, gray16, 65535, transfer=transfer, pillow=True)
    add("srgb-half-up", 5, [0, 1, 2] * 5, 2, transfer="srgb")
    add("bt709-boundary", 5, [0, 140, 141, 809, 810, 811, 1000, 9999, 10000, 1, 2, 3, 4, 5, 6], 10000)
    add("linear-boundary", 5, [0, 1, 204, 205, 206, 257, 65535, 32768, 4095, 4096, 32767, 32769, 4660, 43981, 65534], 65535, transfer="linear", pillow=True)
    add("srgb-scaled-half-up", 6, [0, 127, 254] * 15, 254, semantic="rgb", transfer="srgb")
    for first in (9, 10, 32, 35):
        add("p5-leading-" + str(first), 5, [first] + list(range(14)), pillow=True)
        add("p6-leading-" + str(first), 6, [first] + list(range(44)), semantic="rgb", pillow=True)
    add("p5-delimiter-space", 5, [10] + list(range(14)), delimiter=b" ", pillow=True)
    add("p5-delimiter-cr-raster-lf", 5, [10] + list(range(14)), delimiter=b"\r")
    add("p5-delimiter-comment", 5, list(range(15)), payload=b"P5\n5 3\n255# single final delimiter comment\n" + bytes(range(15)))
    # Opposite PBM/PAM polarity and all six canonical tuples, including alpha0.
    add("pam-bw", 7, pbm, 1, semantic="gray")
    add("pam-bwa", 7, [v for i in range(15) for v in (i % 2, (i // 2) % 2)], 1, semantic="ga", crlf=True)
    add("pam-alpha-half-up", 7, [v for i in range(15) for v in (i % 3, (i // 3) % 3)], 2, semantic="ga")
    for maximum in (15, 255, 1000, 65535):
        for semantic, channels in (("gray", 1), ("ga", 2), ("rgb", 3), ("rgba", 4)):
            values = [((i * 137 + c * 71) % (maximum + 1)) for i in range(15) for c in range(channels)]
            if semantic in ("ga", "rgba"):
                values[channels - 1] = 0
                values[2 * channels - 1] = maximum
            add(f"pam-{semantic}-max{maximum}", 7, values, maximum, semantic=semantic, crlf=maximum in (255, 65535))

    def bad(name, payload, status):
        (directory / (name + ".pnm")).write_bytes(payload)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(payload).hexdigest()}

    bad("signature", b"not a PNM\n", 2)
    bad("p8", b"P8\n1 1\n255\n\0", 2)
    bad("ascii-truncated", b"P2\n2 1\n255\n1\n", 4)
    bad("ascii-overmax", b"P2\n1 1\n15\n16\n", 4)
    bad("ascii-negative", b"P3\n1 1\n255\n-1 0 0\n", 4)
    bad("ascii-decimal", b"P2\n1 1\n255\n1.0\n", 4)
    bad("ascii-nonnumeric", b"P2\n1 1\n255\nno\n", 4)
    bad("p1-notbit", b"P1\n1 1\n2\n", 4)
    bad("ascii-extra-sample", b"P2\n1 1\n255\n1 2\n", 4)
    bad("ascii-concatenated", b"P1\n1 1\n0\nP1\n1 1\n0\n", 4)
    bad("binary-truncated", b"P6\n1 1\n255\n\x01\x02", 4)
    bad("binary-overmax8", b"P5\n1 1\n15\n\x10", 4)
    bad("binary-overmax16", b"P5\n1 1\n256\n\x01\x01", 4)
    bad("binary-trailing-whitespace", b"P5\n1 1\n255\n\x01\n", 4)
    bad("binary-trailing-comment", b"P4\n1 1\n\x80#tail\n", 4)
    bad("binary-concatenated", b"P5\n1 1\n255\n\x01P5\n1 1\n255\n\x02", 4)
    bad("binary-no-delimiter", b"P5\n1 1\n255", 4)
    bad("zero-width", b"P5\n0 1\n255\n", 4)
    bad("maxval0", b"P5\n1 1\n0\n\0", 4)
    bad("maxval65536", b"P5\n1 1\n65536\n\0\0", 4)
    bad("dimension-overflow", b"P6\n9223372036854775807 2\n65535\n", 3)
    bad("classic-header-bound", b"P2\n#" + b"a" * 65536 + b"\n1 1\n255\n0\n", 3)
    pam = b"P7\nWIDTH 1\nHEIGHT 1\nDEPTH 1\nMAXVAL 255\nTUPLTYPE GRAYSCALE\nENDHDR\n\x01"
    bad("pam-no-tuple", pam.replace(b"TUPLTYPE GRAYSCALE\n", b""), 2)
    bad("pam-unknown-tuple", pam.replace(b"GRAYSCALE", b"AUTHORED_UNKNOWN"), 2)
    bad("pam-repeated-tuple", pam.replace(b"TUPLTYPE GRAYSCALE\n", b"TUPLTYPE GRAYSCALE\nTUPLTYPE ALPHA\n"), 2)
    bad("pam-unknown-field", pam.replace(b"ENDHDR", b"GAMMA 2.2\nENDHDR"), 2)
    for field in ("WIDTH", "HEIGHT", "DEPTH", "MAXVAL"):
        line = next(line for line in pam.splitlines() if line.startswith(field.encode() + b" "))
        bad("pam-duplicate-" + field.lower(), pam.replace(line + b"\n", line + b"\n" + line + b"\n"), 4)
    bad("pam-depth-mismatch", pam.replace(b"DEPTH 1", b"DEPTH 2"), 4)
    bad("pam-bw-maxval", pam.replace(b"GRAYSCALE", b"BLACKANDWHITE"), 4)
    bad("pam-no-height", pam.replace(b"HEIGHT 1\n", b""), 4)
    bad("pam-truncated", pam[:-1], 4)
    bad("pam-trailing", pam + b"\0", 4)
    bad("pam-header-bound", b"P7\n#" + b"a" * 65536 + b"\n" + pam[3:], 3)
    resource = b"P4\n256 256\n" + bytes(256 * 256 // 8)
    resource_path = directory / "budget-raster.pbm"
    resource_path.write_bytes(resource)
    resource_decoded = Image.open(resource_path)
    resource_decoded.load()
    assert resource_decoded.size == (256, 256) and resource_decoded.convert("L").tobytes() == bytes([255]) * (256 * 256)
    conservative_wrapper = 8192 + 8 * len(str(resource_path.resolve()).encode("utf8"))
    assert len(resource) + conservative_wrapper < 32768 < 256 * 256
    unicode_name = "PNM-\u03bc-\u56fe.ppm"
    (directory / unicode_name).write_bytes((directory / "rgb-main.ppm").read_bytes())
    manifest = {"schema": "qpx.pnm-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"Pillow": pillow_version, "decimal_precision": 70, "raw_byte_order": sys.byteorder,
                           "basis": "Authored original sample arrays; high-precision normative transfer math plus literal anchors; independent Pillow raw decode for representable profiles",
                           "literal_linear255_anchors": {"0": 0, "1": 13, "64": 137, "128": 188, "255": 255}},
                "unicode_filename": {"path": unicode_name, "control": "rgb-main", "sha256": hashlib.sha256((directory / unicode_name).read_bytes()).hexdigest()},
                "resource_fixtures": {"budget-raster": {"filename": "budget-raster.pbm", "encoded_bytes": len(resource), "raw_plus_rgba_bytes": 256 * 256 * 5, "conservative_wrapper_bytes": conservative_wrapper, "work_budget": 32768, "sha256": hashlib.sha256(resource).hexdigest(), "pillow_verified": True}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "filename", "width", "height", "channels", "sample_bytes", "semantic", "transfer")
    (directory / "fixtures.tsv").write_text("".join("\t".join(str(c[key]) for key in columns) + "\n" for c in cases), encoding="utf8")
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
    report = {"schema": "qpx.pnm-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]),
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

            def stage_manifest(directory, *, overlay=False, transfer=None):
                directory.mkdir()
                changed = json.loads(args.manifest.read_text(encoding="utf8"))
                for descriptor in changed["plugins"]:
                    target = directory / descriptor["module"]["path"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    data = (args.manifest.parent / descriptor["module"]["path"]).read_bytes()
                    if overlay:
                        data += b"QPX PNM inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    if transfer:
                        descriptor["options"] = {"transfer": transfer}
                    for dependency in descriptor["dependencies"]:
                        destination = directory / dependency["path"]
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(args.manifest.parent / dependency["path"], destination)
                manifest = directory / "reader-plugins.json"
                manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
                return manifest

            changed_manifest = stage_manifest(output / "changed-package", overlay=True)
            run(consumer + ["changed-implementation", str(changed_manifest), str(fixtures), str(work)], output, "project-changed-implementation", rows)
            options_manifest = stage_manifest(output / "options-package", transfer="srgb")
            run(consumer + ["changed-options", str(options_manifest), str(fixtures), str(work)], output, "project-changed-options", rows)
            run(consumer + ["changed-source", str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-changed-source", rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.pnm", str(fixtures / "rgb-main.ppm"), "0", "0", "0", "5", "3", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "rgb-main.rgba").read_bytes(), "Host PAM differs from independent transfer oracle"
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
