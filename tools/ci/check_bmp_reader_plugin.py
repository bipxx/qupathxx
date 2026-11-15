#!/usr/bin/env python3
"""Development-only authored BMP fixtures, independent Pillow oracle and native controls."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess


def encode_bmp(width, height, bpp, values, *, palette=None, top_down=False,
               xppm=0, yppm=0, zero_image_size=False, used=None, important=0):
    """Author a BITMAPINFOHEADER file from literal canonical upper-left samples."""
    channels = 1 if bpp <= 8 else 3
    assert len(values) == width * height * channels
    stride = ((width * bpp + 31) // 32) * 4
    raster = bytearray()
    rows = range(height) if top_down else reversed(range(height))
    for y in rows:
        row = bytearray()
        if bpp in (1, 4):
            per_byte = 8 // bpp
            for x in range(0, width, per_byte):
                byte = 0
                for slot in range(per_byte):
                    value = values[y * width + x + slot] if x + slot < width else (1 << bpp) - 1
                    byte |= value << (8 - bpp * (slot + 1))
                row.append(byte)
        elif bpp == 8:
            row += bytes(values[y * width:(y + 1) * width])
        else:
            for x in range(width):
                at = (y * width + x) * 3
                red, green, blue = values[at:at + 3]
                row += bytes((blue, green, red))
                if bpp == 32:
                    row.append((0, 17, 255)[(y * width + x) % 3])
        row += bytes([0xa5]) * (stride - len(row))
        assert len(row) == stride
        raster += row
    palette = palette or []
    colors = b"".join(bytes((b, g, r, 0)) for r, g, b in palette)
    used = (0 if bpp > 8 or len(palette) == 1 << bpp else len(palette)) if used is None else used
    offset = 54 + len(colors)
    dib = struct.pack("<IiiHHIIiiII", 40, width, -height if top_down else height, 1, bpp, 0,
                      0 if zero_image_size else len(raster), xppm, yppm, used, important)
    return struct.pack("<2sIHHI", b"BM", offset + len(raster), 0, 0, offset) + dib + colors + raster


def write_fixtures(directory):
    from PIL import Image, __version__ as pillow_version
    directory.mkdir(parents=True, exist_ok=True)
    cases, negatives = [], {}

    def palette_for(count):
        return [((i * 53 + 17) & 255, (i * 71 + 33) & 255, (i * 97 + 65) & 255) for i in range(count)]

    def add(name, width, height, bpp, values, **options):
        data = encode_bmp(width, height, bpp, values, **options)
        path = directory / (name + ".bmp")
        path.write_bytes(data)
        palette = options.get("palette")
        expected_rgb = bytes(v for index in values for v in palette[index]) if bpp <= 8 else bytes(values)
        expected_rgba = bytes(v for i in range(width * height) for v in (*expected_rgb[i * 3:i * 3 + 3], 255))
        decoded = Image.open(path)
        decoded.load()
        assert decoded.size == (width, height) and decoded.convert("RGBA").tobytes() == expected_rgba, name
        (directory / (name + ".raw")).write_bytes(bytes(values))
        (directory / (name + ".rgba")).write_bytes(expected_rgba)
        xppm, yppm = options.get("xppm", 0), options.get("yppm", 0)
        cases.append({"name": name, "width": width, "height": height, "channels": 1 if bpp <= 8 else 3,
                      "semantic": "index" if bpp <= 8 else "rgb", "bpp": bpp, "top_down": options.get("top_down", False),
                      "calibration_flags": int(xppm > 0) | (2 * int(yppm > 0)),
                      "pixel_width_um": 1e6 / xppm if xppm else 0., "pixel_height_um": 1e6 / yppm if yppm else 0.,
                      "source_sha256": hashlib.sha256(data).hexdigest(), "raw_sha256": hashlib.sha256(bytes(values)).hexdigest(),
                      "rgba_sha256": hashlib.sha256(expected_rgba).hexdigest(), "pillow_display_exact": True})
        return data

    for bpp, widths in ((1, (1, 7, 8, 9, 31, 32, 33)), (4, (1, 2, 3, 7, 8, 9)), (8, (1, 3, 4, 5))):
        palette = palette_for(1 << bpp)
        for width in widths:
            values = [(x * 7 + y * 3) % len(palette) for y in range(3) for x in range(width)]
            for top_down in (False, True):
                add(f"indexed{bpp}-w{width}-" + ("top" if top_down else "bottom"), width, 3, bpp, values,
                    palette=palette, top_down=top_down, zero_image_size=top_down)
    for bpp, widths in ((24, (1, 2, 3, 4, 5, 7)), (32, (1, 3, 5))):
        for width in widths:
            values = [(x * 29 + y * 73 + c * 101) & 255 for y in range(3) for x in range(width) for c in range(3)]
            for top_down in (False, True):
                add(f"rgb{bpp}-w{width}-" + ("top" if top_down else "bottom"), width, 3, bpp, values,
                    top_down=top_down, zero_image_size=top_down)
    rgb = [0, 1, 2, 9, 10, 32, 35, 127, 128, 254, 255, 64, 21, 46, 201,
           73, 99, 133, 200, 17, 240, 3, 222, 88, 155, 111, 42, 31, 63, 95,
           4, 8, 16, 32, 64, 128, 127, 129, 253, 89, 144, 233, 255, 0, 255]
    base = add("rgb-main", 5, 3, 24, rgb, xppm=4000000, yppm=2000000)
    add("rgb-source-changed", 5, 3, 32, rgb[:-1] + [254], top_down=True, xppm=4000000, yppm=2000000)
    indexed = add("indexed-main", 5, 3, 8, [0, 1, 2, 3, 0, 3, 2, 1, 0, 1, 2, 3, 1, 2, 0], palette=palette_for(4), important=3)
    add("density-x", 5, 3, 24, rgb, xppm=4000000)
    add("density-y", 5, 3, 24, rgb, yppm=2000000)
    add("density-int32max", 5, 3, 24, rgb, xppm=2147483647, yppm=2147483647)
    add("indexed1-single-entry", 5, 3, 1, [0] * 15, palette=palette_for(1), important=1)
    add("indexed4-compact", 5, 3, 4, [i % 3 for i in range(15)], palette=palette_for(3), important=2)
    add("indexed8-compact", 5, 3, 8, [i % 3 for i in range(15)], palette=palette_for(3), top_down=True, important=3)
    add("indexed4-explicit-full", 5, 3, 4, [i % 16 for i in range(15)], palette=palette_for(16), used=16)

    def bad(name, content, status):
        (directory / (name + ".bmp")).write_bytes(content)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(content).hexdigest()}

    def changed(content, offset, kind, value):
        result = bytearray(content)
        struct.pack_into("<" + kind, result, offset, value)
        return bytes(result)

    def resized(content):
        return changed(content, 2, "I", len(content))

    bad("signature", b"ZZ" + base[2:], 2)
    # Keep the recognized 40-byte DIB type and actual bfSize while truncating
    # its required fields; an all-zero DIB would correctly be unsupported.
    bad("short-bm-header", resized(base[:22]), 4)
    for field, offset, kind, value in (("bfsize-small", 2, "I", len(base) - 1), ("bfsize-large", 2, "I", len(base) + 1),
                                       ("bfsize-zero", 2, "I", 0), ("reserved1", 6, "H", 1), ("reserved2", 8, "H", 1),
                                       ("planes0", 26, "H", 0), ("planes2", 26, "H", 2), ("width0", 18, "i", 0),
                                       ("width-negative", 18, "i", -1), ("height0", 22, "i", 0),
                                       ("offset-before-header", 10, "I", 53), ("sizeimage-wrong", 34, "I", 47),
                                       ("density-x-negative", 38, "i", -1), ("density-y-negative", 42, "i", -1)):
        bad(field, changed(base, offset, kind, value), 4)
    bad("offset-gap", changed(resized(base[:54] + b"\0" + base[54:]), 10, "I", 55), 4)
    bad("palette-offset", changed(indexed, 10, "I", 54), 4)
    palette_reserved = bytearray(indexed)
    palette_reserved[57] = 1
    bad("palette-reserved", palette_reserved, 4)
    bad("palette-too-many", changed(indexed, 46, "I", 257), 4)
    bad("important-too-many", changed(indexed, 50, "I", 5), 4)
    out_of_range = bytearray(indexed)
    out_of_range[struct.unpack_from("<I", indexed, 10)[0]] = 4
    bad("index-outside-palette", out_of_range, 4)
    bad("truncated-palette-or-raster", resized(indexed[:69] + indexed[70:]), 4)
    bad("truncated-raster", resized(base[:-1]), 4)
    bad("trailing", resized(base + b"\0"), 4)
    bad("concatenated", resized(base + base), 4)
    bad("truecolor-optimization-palette", changed(base, 46, "I", 1), 2)
    for bpp in (2, 16):
        bad("depth" + str(bpp), changed(base, 28, "H", bpp), 2)
    for name, compression in (("rle8", 1), ("rle4", 2), ("bitfields", 3), ("embedded-jpeg", 4), ("embedded-png", 5), ("alpha-bitfields", 6)):
        bad(name, changed(base, 30, "I", compression), 2)
    for name, size in (("v4", 108), ("v5-icc", 124)):
        extended = bytearray(resized(base[:54] + bytes(size - 40) + base[54:]))
        struct.pack_into("<I", extended, 14, size)
        struct.pack_into("<I", extended, 10, 14 + size)
        if size == 124:
            struct.pack_into("<I", extended, 70, 0x4d424544)  # PROFILE_EMBEDDED, excluded with V5.
        bad(name, extended, 2)
    core = struct.pack("<2sIHHI", b"BM", 30, 0, 0, 26) + struct.pack("<IHHHH", 12, 1, 1, 1, 24) + bytes(4)
    bad("os2-core", core, 2)
    huge = changed(changed(changed(base, 18, "i", 2147483647), 22, "i", 2147483647), 28, "H", 32)
    bad("decoded-size-bound", changed(huge, 34, "I", 0), 3)
    bad("intmin-height-bound", changed(changed(base, 22, "i", -2147483648), 34, "I", 0), 3)

    resource = encode_bmp(256, 256, 1, [0] * (256 * 256), palette=[(0, 0, 0), (255, 255, 255)])
    resource_path = directory / "budget-raster.bmp"
    resource_path.write_bytes(resource)
    decoded = Image.open(resource_path)
    decoded.load()
    assert decoded.size == (256, 256) and decoded.convert("RGB").tobytes() == bytes(256 * 256 * 3)
    margin = 8192 + 8 * len(str(resource_path.resolve()).encode("utf8"))
    assert len(resource) + margin < 32768 < 256 * 256
    unicode_name = "BMP-\u03bc-\u56fe.bmp"
    (directory / unicode_name).write_bytes(base)
    manifest = {"schema": "qpx.bmp-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"Pillow": pillow_version, "basis": "Repository-authored literal RGB/index samples and BMP byte encoding; Pillow independently verifies every positive display raster exactly"},
                "unicode_filename": {"path": unicode_name, "control": "rgb-main", "sha256": hashlib.sha256(base).hexdigest()},
                "resource_fixtures": {"budget-raster": {"filename": "budget-raster.bmp", "encoded_bytes": len(resource), "raw_plus_rgba_bytes": 256 * 256 * 5,
                                       "conservative_wrapper_bytes": margin, "work_budget": 32768, "sha256": hashlib.sha256(resource).hexdigest(), "pillow_verified": True}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "width", "height", "channels", "semantic", "calibration_flags", "pixel_width_um", "pixel_height_um")
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
    report = {"schema": "qpx.bmp-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]),
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

            def stage_manifest(directory, *, overlay=False):
                directory.mkdir()
                changed = json.loads(args.manifest.read_text(encoding="utf8"))
                for descriptor in changed["plugins"]:
                    target = directory / descriptor["module"]["path"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    data = (args.manifest.parent / descriptor["module"]["path"]).read_bytes()
                    if overlay:
                        data += b"QPX BMP inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    for dependency in descriptor["dependencies"]:
                        destination = directory / dependency["path"]
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(args.manifest.parent / dependency["path"], destination)
                manifest = directory / "reader-plugins.json"
                manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
                return manifest

            changed_manifest = stage_manifest(output / "changed-package", overlay=True)
            run(consumer + ["changed-implementation", str(changed_manifest), str(fixtures), str(work)], output, "project-changed-implementation", rows)
            run(consumer + ["changed-source", str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-changed-source", rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.bmp", str(fixtures / "rgb-main.bmp"), "0", "0", "0", "5", "3", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "rgb-main.rgba").read_bytes(), "Host PAM differs from independent exact BMP oracle"
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
