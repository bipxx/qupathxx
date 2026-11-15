"""Development-only JPEG fixtures/oracles and fresh-process plugin validation.

JPEG expectations are decoded pixels, never the pre-compression source raster.
All source rasters/metadata and analytic JPEG coefficients are authored here.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess


def segment(marker: int, payload: bytes) -> bytes:
    return bytes((255, marker)) + struct.pack(">H", len(payload) + 2) + payload


def headers(data: bytes):
    assert data[:2] == b"\xff\xd8"
    result, offset = [], 2
    while offset < len(data):
        assert data[offset] == 255
        start = offset
        while data[offset] == 255:
            offset += 1
        marker = data[offset]
        offset += 1
        if marker in (0xda, 0xd9):
            return result, data[start:]
        size = struct.unpack_from(">H", data, offset)[0]
        result.append((marker, data[offset + 2:offset + size]))
        offset += size
    raise ValueError("JPEG has no scan")


def jfif(unit=0, x=1, y=1):
    return b"JFIF\0\x01\x02" + bytes((unit,)) + struct.pack(">HHBB", x, y, 0, 0)


def decorate(data, *, density=None, exif=None, extra=()):
    existing, scan = headers(data)
    kept = [(m, p) for m, p in existing if not (m == 0xe0 and p.startswith(b"JFIF\0") and density is not None)]
    prefix = [(0xe0, density)] if density is not None else []
    if exif is not None:
        prefix.append((0xe1, exif))
    return b"\xff\xd8" + b"".join(segment(m, p) for m, p in prefix + list(extra) + kept) + scan


def exif_ifd(endian="II", orientation=1, x=None, y=None, unit=None, *, duplicate=False,
             color_space=None, interoperability=False, malformed_pointer=False):
    order = "<" if endian == "II" else ">"
    fields = [(0x112, 3, struct.pack(order + "H", orientation))]
    if duplicate:
        fields.append((0x112, 3, struct.pack(order + "H", orientation)))
    if x is not None:
        fields.append((0x11a, 5, struct.pack(order + "II", *x)))
    if y is not None:
        fields.append((0x11b, 5, struct.pack(order + "II", *y)))
    if unit is not None:
        fields.append((0x128, 3, struct.pack(order + "H", unit)))
    if color_space is not None or interoperability:
        fields.append((0x8769, 4, b"CHLD"))
    fields.sort(key=lambda value: value[0])
    base = 8 + 2 + 12 * len(fields) + 4
    payload = bytearray()
    entries = bytearray()
    child_pointer_offset = None
    for tag, kind, value in fields:
        entries += struct.pack(order + "HHI", tag, kind, 1)
        if tag == 0x8769:
            child_pointer_offset = len(entries)
            entries += bytes(4)
        elif len(value) <= 4:
            entries += value + bytes(4 - len(value))
        else:
            entries += struct.pack(order + "I", base + len(payload))
            payload += value
    child = bytearray()
    if child_pointer_offset is not None:
        pointer = 0xfffffff0 if malformed_pointer else base + len(payload)
        struct.pack_into(order + "I", entries, child_pointer_offset, pointer)
        child_fields = []
        if color_space is not None:
            child_fields.append((0xa001, 3, struct.pack(order + "H", color_space)))
        if interoperability:
            child_fields.append((0xa005, 4, struct.pack(order + "I", 8)))
        child += struct.pack(order + "H", len(child_fields))
        for tag, kind, value in child_fields:
            child += struct.pack(order + "HHI", tag, kind, 1) + value + bytes(4 - len(value))
        child += bytes(4)
    tiff = endian.encode("ascii") + struct.pack(order + "HIH", 42, 8, len(fields)) + entries + bytes(4) + payload + child
    return b"Exif\0\0" + tiff


def constant_jpeg(width=8, height=8, color="gray"):
    """Analytic128 raster: all DC and AC coefficients zero, DQT entries1.

    A one-symbol DC-zero and AC-EOB Huffman table encodes each block as00.
    This is independent of Pillow's JPEG encoder and any input sample raster.
    """
    channels = 1 if color == "gray" else 3
    ids = [1] if channels == 1 else ([ord("R"), ord("G"), ord("B")] if color == "rgb" else [1, 2, 3])
    out = b"\xff\xd8"
    out += segment(0xe0, jfif()) if color != "rgb" else segment(0xee, b"Adobe" + struct.pack(">HHHB", 100, 0, 0, 0))
    out += segment(0xdb, b"\0" + bytes([1]) * 64)
    out += segment(0xc0, bytes((8,)) + struct.pack(">HHB", height, width, channels) + b"".join(bytes((i, 0x11, 0)) for i in ids))
    table = bytes((1,)) + bytes(15) + b"\0"
    out += segment(0xc4, b"\0" + table + b"\x10" + table)
    out += segment(0xda, bytes((channels,)) + b"".join(bytes((i, 0)) for i in ids) + bytes((0, 63, 0)))
    zero_bits = ((width + 7) // 8) * ((height + 7) // 8) * channels * 2
    entropy = bytes(zero_bits // 8)
    if zero_bits % 8:
        entropy += bytes(((1 << (8 - zero_bits % 8)) - 1,))
    return out + entropy.replace(b"\xff", b"\xff\0") + b"\xff\xd9"


def unequal_rgb_jpeg():
    # For DQT1, DC coefficients -512/0/+512 produce exact64/128/192 pixels.
    # DC category0 has Huffman code0; category10 code10; AC EOB code0.
    existing, scan = headers(constant_jpeg(color="rgb"))
    dc = bytes((1, 1)) + bytes(14) + bytes((0, 10))
    ac = bytes((1,)) + bytes(15) + b"\0"
    changed = [(m, b"\0" + dc + b"\x10" + ac) if m == 0xc4 else (m, p) for m, p in existing]
    bits = "10" + "0111111111" + "0" + "0" + "0" + "10" + "1000000000" + "0"
    bits += "1" * (-len(bits) % 8)
    entropy = bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8)).replace(b"\xff", b"\xff\0")
    sos_bytes = 2 + struct.unpack_from(">H", scan, 2)[0]
    return b"\xff\xd8" + b"".join(segment(m, p) for m, p in changed) + scan[:sos_bytes] + entropy + b"\xff\xd9"


def write_fixtures(directory: Path) -> dict:
    from PIL import Image, ImageOps, features
    import PIL

    directory.mkdir(parents=True, exist_ok=True)
    width, height = 17, 11
    rgb = bytes((x * 17 + y * 31 + c * 67) & 255 for y in range(height) for x in range(width) for c in range(3))
    gray = bytes((x * 37 + y * 19) & 255 for y in range(height) for x in range(width))
    rgb_image, gray_image = Image.frombytes("RGB", (width, height), rgb), Image.frombytes("L", (width, height), gray)

    def encoded(image, progressive=False, subsampling=0, keep_rgb=False, restart_blocks=0):
        buffer = io.BytesIO()
        image.save(buffer, "JPEG", quality=83, optimize=False, progressive=progressive,
                   subsampling=subsampling, keep_rgb=keep_rgb, restart_marker_blocks=restart_blocks)
        return buffer.getvalue()

    base = encoded(rgb_image, subsampling=2)
    cases = []

    def add(name, data, *, flags=0, px=0., py=0., analytic=False):
        path = directory / (name + ".jpg")
        path.write_bytes(data)
        image = Image.open(path)
        image.load()
        oriented = ImageOps.exif_transpose(image)
        mode = "L" if oriented.mode == "L" else "RGB"
        raw = oriented.convert(mode).tobytes()
        channels = 1 if mode == "L" else 3
        rgba = oriented.convert("RGBA").tobytes()
        if analytic:
            pixel = bytes([128] * channels) if analytic is True else bytes(analytic)
            assert len(pixel) == channels and raw == pixel * (oriented.width * oriented.height), name
        (directory / (name + ".raw")).write_bytes(raw)
        (directory / (name + ".rgba")).write_bytes(rgba)
        cases.append({"name": name, "width": oriented.width, "height": oriented.height, "channels": channels,
                      "calibration_flags": flags, "pixel_width_um": px, "pixel_height_um": py,
                      "jpeg_sha256": hashlib.sha256(data).hexdigest(), "raw_sha256": hashlib.sha256(raw).hexdigest(),
                      "rgba_sha256": hashlib.sha256(rgba).hexdigest(), "analytic_constant128": analytic is True,
                      "analytic_expected_pixel": list(pixel) if analytic else None})

    for progressive in (False, True):
        suffix = "progressive" if progressive else "baseline"
        add("gray-" + suffix, encoded(gray_image, progressive))
        add("rgb-" + suffix, encoded(rgb_image, progressive, keep_rgb=True))
        for sampling in (0, 1, 2):
            add(f"ycbcr-{suffix}-s{sampling}", encoded(rgb_image, progressive, sampling))
    add("rgb-base", base)
    restart = encoded(rgb_image, subsampling=2, restart_blocks=1)
    assert b"\xff\xdd" in restart and b"\xff\xd0" in restart, "Restart fixture lacks actual DRI/RST markers"
    add("ycbcr-restart-block1", restart)
    add("rgb-jfif-inch", decorate(base, density=jfif(1, 50800, 25400)), flags=3, px=.5, py=1.)
    add("rgb-jfif-cm", decorate(base, density=jfif(2, 40000, 20000)), flags=3, px=.25, py=.5)
    add("rgb-jfif-ratio", decorate(base, density=jfif(0, 40000, 20000)))
    for endian in ("II", "MM"):
        for orientation in range(1, 9):
            swapped = orientation >= 5
            add(f"exif-{endian}-o{orientation}", decorate(base, exif=exif_ifd(endian, orientation, (40000, 1), (20000, 1), 3)),
                flags=3, px=.5 if swapped else .25, py=.25 if swapped else .5)
    add("exif-matching-rational", decorate(base, density=jfif(2, 40000, 20000), exif=exif_ifd("MM", 1, (80000, 2), (40000, 2), 3)), flags=3, px=.25, py=.5)
    add("exif-unknown-unit", decorate(base, exif=exif_ifd("II", 1, (40000, 1), (20000, 1), 1)))
    add("exif-x-only-o6", decorate(base, exif=exif_ifd("II", 6, (40000, 1), None, 3)), flags=2, py=.25)
    add("exif-y-only-o8", decorate(base, exif=exif_ifd("MM", 8, None, (20000, 1), 3)), flags=1, px=.5)
    add("exif-srgb-child", decorate(base, exif=exif_ifd("II", 1, color_space=1)))
    for color in ("gray", "rgb", "ycbcr"):
        add("analytic-" + color, constant_jpeg(color=color), analytic=True)
    add("analytic-rgb-order", unequal_rgb_jpeg(), analytic=(64, 128, 192))
    unicode_name = "JPEG-\u03bc-\u56fe.jpg"
    (directory / unicode_name).write_bytes((directory / "rgb-jfif-cm.jpg").read_bytes())
    negatives = {}

    def bad(name, content, status):
        (directory / (name + ".jpg")).write_bytes(content)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(content).hexdigest()}

    bad("signature", b"not a JPEG\n", 2)
    bad("truncated", base[:-2], 4)
    base_headers, base_scan = headers(base)
    sos_bytes = 2 + struct.unpack_from(">H", base_scan, 2)[0]
    bad("premature-entropy-eoi", b"\xff\xd8" + b"".join(segment(m, p) for m, p in base_headers) + base_scan[:sos_bytes] + b"\0\xff\xd9", 4)
    bad("trailing", base + b"\0", 4)
    bad("concatenated", base + base, 4)
    bad("wrong-restart", restart.replace(b"\xff\xd0", b"\xff\xd3", 1), 4)
    existing, scan = headers(base)
    for name, marker, precision in (("precision12", 0xc0, 12), ("lossless", 0xc3, 8), ("arithmetic", 0xc9, 8)):
        altered = [(marker, bytes((precision,)) + p[1:]) if m == 0xc0 else (m, p) for m, p in existing]
        bad(name, b"\xff\xd8" + b"".join(segment(m, p) for m, p in altered) + scan, 2)
    cmyk = io.BytesIO()
    Image.new("CMYK", (width, height), (17, 65, 129, 193)).save(cmyk, "JPEG")
    bad("cmyk", cmyk.getvalue(), 2)
    cmyk_headers, cmyk_scan = headers(cmyk.getvalue())
    ycck_headers = [(m, p[:-1] + b"\x02") if m == 0xee and p.startswith(b"Adobe") else (m, p) for m, p in cmyk_headers]
    bad("ycck", b"\xff\xd8" + b"".join(segment(m, p) for m, p in ycck_headers) + cmyk_scan, 2)
    for name, marker, payload in (("icc", 0xe2, b"ICC_PROFILE\0\x01\x01" + bytes(132)),
                                  ("xmp", 0xe1, b"http://ns.adobe.com/xap/1.0/\0<authored/>"),
                                  ("mpo", 0xe2, b"MPF\0II*\0\x08\0\0\0\0\0\0\0\0\0")):
        bad(name, decorate(base, extra=((marker, payload),)), 2)
    for name, payload in (("orientation0", exif_ifd(orientation=0)), ("orientation9", exif_ifd(orientation=9)),
                          ("exif-duplicate-tag", exif_ifd(duplicate=True)),
                          ("resolution-denominator0", exif_ifd(x=(40000, 0), unit=3)),
                          ("resolution-numerator0", exif_ifd(y=(0, 1), unit=3)),
                          ("exif-bad-child", exif_ifd(color_space=1, malformed_pointer=True))):
        bad(name, decorate(base, exif=payload), 4)
    bad("exif-duplicate-app", decorate(base, exif=exif_ifd(), extra=((0xe1, exif_ifd()),)), 4)
    bad("resolution-conflict", decorate(base, density=jfif(2, 40000, 20000), exif=exif_ifd(x=(20000, 1), y=(20000, 1), unit=3)), 4)
    bad("jfif-unit3", decorate(base, density=jfif(3, 1, 1)), 4)
    bad("jfif-zero-density", decorate(base, density=jfif(1, 0, 1)), 4)
    bad("exif-unknown-colorspace", decorate(base, exif=exif_ifd(color_space=65535)), 2)
    bad("exif-interop", decorate(base, exif=exif_ifd(color_space=1, interoperability=True)), 2)
    bad("app13", decorate(base, extra=((0xed, b"Photoshop 3.0\0Authored metadata"),)), 2)
    whitepoint = b"Exif\0\0II" + struct.pack("<HIH", 42, 8, 1)
    whitepoint += struct.pack("<HHII", 0x013e, 5, 2, 26) + bytes(4) + struct.pack("<4I", 3127, 10000, 329, 1000)
    bad("exif-whitepoint", decorate(base, exif=whitepoint), 2)
    # This ordinary Exif record would be valid before SOS. After entropy it must
    # be refused before libjpeg releases its saved marker pool at finish.
    bad("late-exif", base[:-2] + segment(0xe1, exif_ifd("II", 8)) + base[-2:], 2)
    # The encoded analytic raster is12KiB; its canonical decoded RGB is3MiB.
    resource = constant_jpeg(1024, 1024, "rgb")
    (directory / "budget-raster.jpg").write_bytes(resource)
    decoded = Image.open(directory / "budget-raster.jpg")
    decoded.load()
    assert decoded.size == (1024, 1024) and decoded.convert("RGB").tobytes() == bytes([128]) * (1024 * 1024 * 3)
    assert len(resource) < 32768 < 1024 * 1024 * 3
    manifest = {"schema": "qpx.jpeg-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"Pillow": PIL.__version__, "jpeg_api": features.version_codec("jpg"),
                           "libjpeg_turbo": features.version_feature("libjpeg_turbo"),
                           "basis": "Canonical decoded pixels after Pillow ImageOps.exif_transpose; analytic zero-coefficient JPEGs independently require128"},
                "unicode_filename": {"path": unicode_name, "control": "rgb-jfif-cm", "sha256": hashlib.sha256((directory / unicode_name).read_bytes()).hexdigest()},
                "resource_fixtures": {"budget-raster": {"encoded_bytes": len(resource), "decoded_raw_bytes": 1024 * 1024 * 3,
                                       "work_budget": 32768, "sha256": hashlib.sha256(resource).hexdigest(), "pillow_verified": True}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "width", "height", "channels", "calibration_flags", "pixel_width_um", "pixel_height_um")
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
    report = {"schema": "qpx.jpeg-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]),
              "checks": rows, "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        run([str(args.abi_test.resolve()), str(args.module.resolve()), str(fixtures)], output, "direct-abi", rows)
        if args.consumer and args.manifest:
            work = output / "project"
            work.mkdir(exist_ok=True)
            consumer = [str(args.consumer.resolve())]
            for mode in ("create", "reopen", "missing"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)
            package = output / "changed-package"
            package.mkdir(exist_ok=True)
            changed = json.loads(args.manifest.read_text(encoding="utf8"))
            for descriptor in changed["plugins"]:
                target = package / descriptor["module"]["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((args.manifest.parent / descriptor["module"]["path"]).read_bytes() + b"QPX JPEG inert implementation identity check\n")
                descriptor["module"]["sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
                for dependency in descriptor["dependencies"]:
                    shutil.copy2(args.manifest.parent / dependency["path"], package / dependency["path"])
            changed_manifest = package / "reader-plugins.json"
            changed_manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
            run(consumer + ["changed-implementation", str(changed_manifest), str(fixtures), str(work)], output, "project-changed-implementation", rows)
            run(consumer + ["changed-source", str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-changed-source", rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.jpeg", str(fixtures / "rgb-jfif-cm.jpg"), "0", "0", "0", "17", "11", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "rgb-jfif-cm.rgba").read_bytes(), "Host PAM differs from independent decoded JPEG oracle"
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
