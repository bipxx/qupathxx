"""Development-only authored PNG fixtures, independent oracles and native runner.

No function in this file is used by a shipped reader or scientific workflow.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib


CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
PASSES = ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8),
          (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def packed(samples: list[int], depth: int) -> bytes:
    if depth == 16:
        return struct.pack(">" + "H" * len(samples), *samples)
    if depth == 8:
        return bytes(samples)
    result = bytearray((len(samples) * depth + 7) // 8)
    for i, value in enumerate(samples):
        result[i * depth // 8] |= value << (8 - depth - (i * depth % 8))
    return bytes(result)


def encode(width: int, height: int, depth: int, color: int, values: list[int],
           before: tuple[tuple[bytes, bytes], ...] = (), interlaced: bool = False) -> bytes:
    channels = CHANNELS[color]
    scan = bytearray()
    for x0, y0, dx, dy in PASSES if interlaced else ((0, 0, 1, 1),):
        if x0 >= width or y0 >= height:
            continue
        for y in range(y0, height, dy):
            row = [values[(y * width + x) * channels + c]
                   for x in range(x0, width, dx) for c in range(channels)]
            scan.append(0)
            scan.extend(packed(row, depth))
    result = b"\x89PNG\r\n\x1a\n"
    result += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, int(interlaced)))
    result += b"".join(chunk(kind, data) for kind, data in before)
    return result + chunk(b"IDAT", zlib.compress(scan)) + chunk(b"IEND", b"")


def fixture_samples(width: int, height: int, channels: int, depth: int) -> list[int]:
    maximum = (1 << depth) - 1
    edge16 = (0, 1, 255, 256, 257, 32769, 65535, 65280, 511, 4097)
    values = []
    for y in range(height):
        for x in range(width):
            for c in range(channels):
                values.append(edge16[(x + y * 3 + c * 2) % len(edge16)] if depth == 16
                              else (x * 37 + y * 19 + c * 53) % (maximum + 1))
    return values


def write_fixtures(directory: Path) -> dict:
    from PIL import Image  # Independent development oracle; not a product dependency.

    directory.mkdir(parents=True, exist_ok=True)
    cases = []
    # All PNG color/depth combinations, each with and without Adam7.
    profiles = [(0, d) for d in (1, 2, 4, 8, 16)] + [(2, d) for d in (8, 16)]
    profiles += [(3, d) for d in (1, 2, 4, 8)] + [(4, d) for d in (8, 16)] + [(6, d) for d in (8, 16)]

    def add(name, color, depth, interlaced=False, extra=(), calibrated=False, trns=True):
        width, height, channels = 9, 5, CHANNELS[color]
        values = fixture_samples(width, height, channels, depth)
        maximum = (1 << depth) - 1
        before = []
        palette = []
        transparency = []
        transparent_value = None
        if color == 3:
            palette = [(13 + i * 17 & 255, 7 + i * 83 & 255, 29 + i * 61 & 255)
                       for i in range(1 << depth)]
            transparency = [(i * 71) & 255 for i in range(max(1, len(palette) - 1))]
            before.append((b"PLTE", bytes(v for rgb in palette for v in rgb)))
            if trns:
                before.append((b"tRNS", bytes(transparency)))
        elif color == 0 and trns:
            transparent_value = values[0]
            before.append((b"tRNS", struct.pack(">H", transparent_value)))
        elif color == 2 and trns:
            transparent_value = tuple(values[:3])
            before.append((b"tRNS", struct.pack(">HHH", *transparent_value)))
        if calibrated:
            before.append((b"pHYs", struct.pack(">IIB", 4_000_000, 2_000_000, 1)))
        before.extend(extra)
        data = encode(width, height, depth, color, values, tuple(before), interlaced)
        rgba = bytearray()
        byte_value = (lambda v: v >> 8) if depth == 16 else (lambda v: v * 255 // maximum)
        for i in range(width * height):
            sample = values[i * channels:(i + 1) * channels]
            if color == 0:
                rgb = [byte_value(sample[0])] * 3
                alpha = 0 if sample[0] == transparent_value else 255
            elif color == 2:
                rgb = [byte_value(v) for v in sample]
                alpha = 0 if tuple(sample) == transparent_value else 255
            elif color == 3:
                rgb = palette[sample[0]]
                alpha = transparency[sample[0]] if trns and sample[0] < len(transparency) else 255
            elif color == 4:
                rgb, alpha = [byte_value(sample[0])] * 3, byte_value(sample[1])
            else:
                rgb, alpha = [byte_value(v) for v in sample[:3]], byte_value(sample[3])
            rgba.extend((*rgb, alpha))
        raw = struct.pack("<" + "H" * len(values), *values) if depth == 16 else bytes(values)
        (directory / (name + ".png")).write_bytes(data)
        (directory / (name + ".raw")).write_bytes(raw)
        (directory / (name + ".rgba")).write_bytes(rgba)
        image = Image.open(directory / (name + ".png"))
        image.load()
        assert image.size == (width, height), name
        # Pillow's 16-bit RGB conversion is not a raw scalar oracle. Authored
        # scalar values above remain the independent expected uint16 values.
        if depth <= 8:
            actual = image.convert("RGBA").tobytes()
            assert actual == bytes(rgba), (name, "independent RGBA oracle differs")
        case = {"name": name, "width": width, "height": height, "depth": depth,
                "color": color, "channels": channels, "sample_type": 2 if depth == 16 else 1,
                "calibration_flags": 3 if calibrated else 0,
                "pixel_width_um": .25 if calibrated else 0,
                "pixel_height_um": .5 if calibrated else 0, "interlaced": interlaced,
                "png_sha256": hashlib.sha256(data).hexdigest(),
                "raw_sha256": hashlib.sha256(raw).hexdigest(),
                "rgba_sha256": hashlib.sha256(rgba).hexdigest(),
                "pillow_rgba_verified": depth <= 8}
        cases.append(case)

    for color, depth in profiles:
        for interlaced in (False, True):
            add(f"c{color}-d{depth}" + ("-adam7" if interlaced else ""), color, depth, interlaced,
                calibrated=(color == 2 and depth == 8))
    add("rgb8-opaque", 2, 8, trns=False, calibrated=True)
    unicode_filename = "PNG-\u03bc-\u56fe.png"
    (directory / unicode_filename).write_bytes((directory / "rgb8-opaque.png").read_bytes())
    add("rgb8-unknown-units", 2, 8, trns=False,
        extra=((b"pHYs", struct.pack(">IIB", 4, 2, 0)),))
    standard_chrm = struct.pack(">8I", 31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000)
    add("rgb8-srgb", 2, 8, extra=((b"gAMA", struct.pack(">I", 45455)),
                                (b"cHRM", standard_chrm), (b"sRGB", b"\0")))
    base_values = fixture_samples(9, 5, 3, 8)
    base = encode(9, 5, 8, 2, base_values)
    negatives = {}

    def negative(name, content, status):
        (directory / (name + ".png")).write_bytes(content)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(content).hexdigest()}

    negative("bad-signature", b"not-a-PNG\n", 2)
    negative("truncated", base[:-7], 4)
    bad_crc = bytearray(base)
    bad_crc[-13] ^= 1  # IDAT CRC, immediately before IEND's length.
    negative("bad-crc", bytes(bad_crc), 4)
    bad_filter = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
    negative("bad-filter", bad_filter + chunk(b"IDAT", zlib.compress(b"\x05\0\0\0")) + chunk(b"IEND", b""), 4)
    negative("huge-dimensions", b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 0x7fffffff, 0x7fffffff, 16, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(b"\0")) + chunk(b"IEND", b""), 3)
    for name, kind, payload in (
        ("animation", b"acTL", struct.pack(">II", 2, 0)),
        ("exif", b"eXIf", b"II*\0\x08\0\0\0\x01\0\x12\x01\x03\0\x01\0\0\0\x06\0\0\0\0\0\0\0"),
        ("icc", b"iCCP", b"Authored profile\0\0" + zlib.compress(bytes(132))),
        ("gamma", b"gAMA", struct.pack(">I", 100000)),
        ("chroma", b"cHRM", struct.pack(">8I", 31270, 32900, 63000, 33000, 30000, 60000, 15000, 6000)),
    ):
        negative(name, encode(9, 5, 8, 2, base_values, ((kind, payload),)), 2)
    # A valid, highly compressed raster fits the encoded budget but its decoded
    # rows cannot fit. This reaches the custom decoder allocation budget after
    # open/metadata overhead, unlike a work budget below sizeof(Decode).
    bomb_width = bomb_height = 1024
    bomb = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", bomb_width, bomb_height, 8, 2, 0, 0, 0))
    bomb += chunk(b"IDAT", zlib.compress(bytes((bomb_width * 3 + 1) * bomb_height))) + chunk(b"IEND", b"")
    budget_raster = directory / "budget-raster.png"
    budget_raster.write_bytes(bomb)
    bomb_oracle = Image.open(budget_raster)
    bomb_oracle.load()
    assert bomb_oracle.mode == "RGB" and bomb_oracle.size == (bomb_width, bomb_height)
    assert bomb_oracle.tobytes() == bytes(bomb_width * bomb_height * 3)
    assert len(bomb) < 32768 < bomb_width * bomb_height * 3
    resources = {"budget-raster": {"width": bomb_width, "height": bomb_height,
                 "encoded_bytes": len(bomb), "work_budget": 32768,
                 "decoded_raw_bytes": bomb_width * bomb_height * 3,
                 "sha256": hashlib.sha256(bomb).hexdigest(), "pillow_verified": True}}
    manifest = {"schema": "qpx.png-test-fixtures/1", "cases": cases, "negative": negatives,
                "resource_fixtures": resources,
                "unicode_filename": {"path": unicode_filename, "control": "rgb8-opaque",
                                     "sha256": hashlib.sha256((directory / unicode_filename).read_bytes()).hexdigest()},
                "origin": "Repository-authored scalar values, PNG chunk writer and independent Pillow display oracle"}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "width", "height", "depth", "color", "channels", "sample_type",
               "calibration_flags", "pixel_width_um", "pixel_height_um")
    (directory / "fixtures.tsv").write_text("".join("\t".join(str(case[key]) for key in columns) + "\n" for case in cases), encoding="utf8")
    (directory / "negative.tsv").write_text("".join(f"{name}\t{row['status']}\n" for name, row in negatives.items()), encoding="utf8")
    return manifest


def run(command: list[str], directory: Path, name: str, rows: list[dict]) -> None:
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8", errors="strict")
    log = directory / (name + ".log")
    log.write_text(result.stdout + result.stderr, encoding="utf8")
    rows.append({"name": name, "command": command, "exit": result.returncode, "log": str(log)})
    if result.returncode:
        raise RuntimeError(f"{name} failed: {result.returncode}\n{result.stdout}{result.stderr}")


def validate(args) -> None:
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixture_dir = output / "fixtures"
    fixture_manifest = write_fixtures(fixture_dir)
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
    report = {"schema": "qpx.png-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]),
              "checks": rows, "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        run([str(args.abi_test.resolve()), str(args.module.resolve()), str(fixture_dir)], output, "direct-abi", rows)
        if args.consumer and args.manifest:
            project_work = output / "project"
            project_work.mkdir(exist_ok=True)
            base = [str(args.consumer.resolve())]
            for mode in ("create", "reopen", "missing"):
                run(base + [mode, str(args.manifest.resolve()), str(fixture_dir), str(project_work)], output, "project-" + mode, rows)
            # Mutating the module only changes this separate disposable package;
            # PE overlay is inert executable data but changes implementation identity.
            original_manifest = json.loads(args.manifest.read_text(encoding="utf8"))
            package = output / "changed-package"
            package.mkdir(exist_ok=True)
            changed = json.loads(json.dumps(original_manifest))
            for descriptor in changed["plugins"]:
                original = args.manifest.parent / descriptor["module"]["path"]
                target = package / descriptor["module"]["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(original.read_bytes() + b"QPX PNG inert implementation identity test\n")
                descriptor["module"]["sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
                for dependency in descriptor["dependencies"]:
                    shutil.copy2(args.manifest.parent / dependency["path"], package / dependency["path"])
            changed_manifest = package / "reader-plugins.json"
            changed_manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
            run(base + ["changed-implementation", str(changed_manifest), str(fixture_dir), str(project_work)], output, "project-changed-implementation", rows)
            run(base + ["changed-source", str(args.manifest.resolve()), str(fixture_dir), str(project_work)], output, "project-changed-source", rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.png",
                 str(fixture_dir / "rgb8-opaque.png"), "0", "0", "0", "9", "5", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixture_dir / "rgb8-opaque.rgba").read_bytes(), "Host PAM pixels differ from independent authored fixture"
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = hashes()
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation input module/executable/manifest bytes changed during checks; see validation.json")
    print(json.dumps({"passed": True, "fixture_cases": len(fixture_manifest["cases"]), "native_checks": len(rows), "report": str(output / "validation.json")}))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    fixtures = sub.add_parser("fixtures")
    fixtures.add_argument("output", type=Path)
    validation = sub.add_parser("validate")
    for argument in ("module", "abi-test", "output"):
        validation.add_argument("--" + argument, required=True, type=Path)
    for argument in ("consumer", "manifest", "region"):
        validation.add_argument("--" + argument, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        manifest = write_fixtures(args.output.resolve())
        print(json.dumps({"cases": len(manifest["cases"]), "negative": len(manifest["negative"]), "output": str(args.output.resolve())}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
