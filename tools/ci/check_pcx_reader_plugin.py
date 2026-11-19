"""Verify native PCX reading against an independent decoder and authored rows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
from PIL import Image, __version__ as pillow_version


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def header(width, height, planes, stride, origin=(0, 0)):
    data = bytearray(128)
    data[:4] = bytes((10, 5, 1, 8))
    x, y = origin
    struct.pack_into("<6H", data, 4, x, y, x + width - 1, y + height - 1, 72, 144)
    data[65] = planes
    struct.pack_into("<2H", data, 66, stride, 1)
    return bytes(data)


def encode_planar(codes, palette, version):
    height, width = codes.shape
    stride = 2 * ((width + 15) // 16)
    result = bytearray(header(width, height, 4, stride, (7, 17)))
    result[1] = version
    result[3] = 1
    result[16:64] = palette.tobytes()
    if version == 2:
        result[68] = 0
    for line in codes:
        for plane in range(4):
            packed = bytearray(np.packbits((line >> plane) & 1))
            if width % 8:
                packed[-1] |= 255 >> (width % 8)
            packed.extend(b"\xed" * (stride - len(packed)))
            for code in packed:
                if code >= 192:
                    result.append(193)
                result.append(code)
    return bytes(result)


def verify(region, manifest, output):
    output.mkdir(parents=True)
    entry, = json.loads(manifest.read_text(encoding="utf-8"))["plugins"]
    module = manifest.parent / entry["module"]["path"]
    artifacts = {path: sha(path) for path in (region, manifest, module)}
    assert artifacts[module] == entry["module"]["sha256"] and entry["dependencies"] == []
    folders = [Path(os.environ["SystemRoot"]) / name for name in ("System32", ".")]
    env = dict(os.environ, PATH=os.pathsep.join(map(str, folders)))
    configure = [region, "--reader-plugins", manifest]
    prefix = [*configure, "--reader", "org.qupathxx.pcx"]
    cases, checks = [], []

    def run(name, args, error=None):
        argv = list(map(str, args))
        result = subprocess.run(argv, cwd=output, env=env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        checks.append({"name": name, "argv": argv, "exit": result.returncode})
        if error:
            assert result.returncode == 2 and error in result.stderr, (name, result)
        else:
            assert result.returncode == 0, (name, result)
        return result.stdout

    def case(name, expected, palette=None, data=None, independent=True):
        if expected.ndim == 2:
            expected = expected[:, :, None]
        height, width, channels = expected.shape
        source = output / (name + ".pcx")
        if data is None:
            image = Image.fromarray(expected[:, :, 0] if channels == 1 else expected)
            if palette is not None:
                image.putpalette(palette.ravel().tolist())
            image.save(source)
        else:
            source.write_bytes(data)
        before = sha(source)
        rgba = np.full((height, width, 4), 255, dtype=np.uint8)
        rgba[:, :, :3] = palette[expected[:, :, 0]] if palette is not None else expected
        if independent:
            with Image.open(source) as decoded:
                np.testing.assert_array_equal(np.asarray(decoded).reshape(expected.shape), expected,
                                              strict=True)
                np.testing.assert_array_equal(np.asarray(decoded.convert("RGBA")), rgba, strict=True)
        raw = output / (name + ".npy")
        report = json.loads(run(name + "-raw", [*prefix, "--raw", source, raw, "--channels", channels]))
        np.testing.assert_array_equal(np.load(raw), expected, strict=True)
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == artifacts[module]
        meta = snapshot["raw"]
        assert meta["channel_names"] == (["Palette index"] if channels == 1 else ["Red", "Green", "Blue"])
        assert meta["pixel_width_um"] is None and meta["pixel_height_um"] is None
        assert report["valid_pixels"] == height * width and sha(source) == before
        pam = output / (name + ".pam")
        run(name + "-display", [*prefix, source, 0, 0, 0, width, height, pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
        cases.append({"name": name, "sha256": before, "raw_sha256": sha(raw),
                      "display_sha256": sha(pam), "independent": independent})
        return source

    assert "org.qupathxx.pcx" in run("catalog", [*configure, "--readers"])
    gray = (np.arange(7 * 11) * 127 % 256).astype(np.uint8).reshape(7, 11)
    palette = (np.arange(768).reshape(256, 3) * 35 % 256).astype(np.uint8)
    rgb = np.stack((gray, gray ^ 66, gray ^ 227), axis=2)
    rgb_path = case("producer-rgb", rgb)
    case("producer-indexed", gray, palette)
    case("producer-gray", gray)
    palette[254] = palette[253]
    footer = b"\x0c" + palette.tobytes()
    codes = np.array([[253, 254, 255], [255, 254, 253]], dtype=np.uint8)
    row_data = b"\xc1\xfd\xc1\xfe\xc1\xff\x03\xc1\xff\xc1\xfe\xc1\xfd\x05"
    indexed_path = case("duplicate-colors", codes, palette,
                        header(3, 2, 1, 4, (97, 12)) + row_data + footer)
    wide = np.full((2, 63), 222, dtype=np.uint8)
    case("max-length-run", wide, palette, header(63, 2, 1, 64) + b"\xff\xde\x17" * 2 + footer)
    # One run spans the green and blue planes in a complete padded RGB row.
    case("cross-plane", np.array([[[12, 44, 44], [35, 44, 76], [53, 44, 83]]], dtype=np.uint8),
         data=header(3, 1, 3, 4) + bytes((12, 35, 53, 90, 0xc5, 44, 76, 83, 91)))
    case("long-plane-padding", np.array([[[12, 33, 55]]], dtype=np.uint8),
         data=header(1, 1, 3, 6) + bytes((12, 0xc5, 15, 33, 0xc5, 36, 55, 0xc5, 57)),
         independent=False)
    crop = output / "rgb-crop.npy"
    run("rgb-crop", [*prefix, "--raw", rgb_path, crop, "--x", 7, "--y", 4, "--width", 4,
                     "--height", 3, "--channel", 1, "--channels", 2])
    np.testing.assert_array_equal(np.load(crop), rgb[4:, 7:, 1:], strict=True)
    crop = output / "index-crop.npy"
    run("index-crop", [*prefix, "--raw", indexed_path, crop, "--x", 1, "--width", 2])
    np.testing.assert_array_equal(np.load(crop), codes[:, 1:, None], strict=True)

    ega_palette = ((np.arange(48).reshape(16, 3) * 29 + 11) % 256).astype(np.uint8)
    ega_palette[4] = ega_palette[3]
    duplicate = np.array([[3, 4, 15], [15, 4, 3]], dtype=np.uint8)
    case("planar-duplicate-colors", duplicate, ega_palette, encode_planar(duplicate, ega_palette, 5))
    for width in (1, 13, 19, 32):
        planar = ((np.arange(5 * width) * 7 + 3) % 16).astype(np.uint8).reshape(5, width)
        for version in (2, 5):
            name = f"planar-v{version}-w{width}"
            case(name, planar, ega_palette, encode_planar(planar, ega_palette, version))
    # A single run can fill all four planes in one row.
    long_planar = bytearray(encode_planar(np.array([[15]], dtype=np.uint8), ega_palette, 5))
    case("planar-cross-plane", np.array([[15]], dtype=np.uint8), ega_palette,
         bytes(long_planar[:128]) + b"\xc8\x80")
    planar = ((np.arange(5 * 19) * 7 + 3) % 16).astype(np.uint8).reshape(5, 19)
    crop = output / "planar-crop.npy"
    run("planar-crop", [*prefix, "--raw", output / "planar-v5-w19.pcx", crop, "--x", 7,
                       "--y", 1, "--width", 11, "--height", 2])
    np.testing.assert_array_equal(np.load(crop), planar[1:3, 7:18, None], strict=True)
    planar_data = encode_planar(planar, ega_palette, 5)

    hdr = header(3, 2, 1, 4)
    data = hdr + row_data + footer
    invalid = {
        "short-header": (hdr[:120], "header is truncated"),
        "wrong-version": (b"\x0a\x03" + data[2:], "version or plane layout"),
        "wrong-encoding": (data[:2] + b"\x00" + data[3:], "signature and RLE"),
        "unsupported-bits": (data[:3] + b"\x01" + data[4:], "version or plane layout"),
        "unsupported-planes": (data[:65] + b"\x02" + data[66:], "version or plane layout"),
        "bad-bounds": (data[:4] + b"\x0f\x00" + data[6:], "bounds are reversed"),
        "odd-stride": (data[:66] + b"\x03\x00" + data[68:], "even plane stride"),
        "small-stride": (data[:66] + b"\x02\x00" + data[68:], "even plane stride"),
        "missing-palette": (hdr + row_data, "palette trailer"),
        "palette-tag": (hdr + row_data + b"\x0b" + footer[1:], "palette trailer"),
        "zero-count": (hdr + b"\xc0\x15" + row_data[2:] + footer, "run count or sample"),
        "missing-run-sample": (hdr + row_data[:-1] + b"\xc1" + footer, "run count or sample"),
        "row-overflow": (hdr + b"\xc5\x15" + row_data[2:] + footer, "crosses the scanline"),
        "trailing-byte": (hdr + row_data + b"\x10" + footer, "after the last scanline"),
        "excess-padding": (header(1, 65536, 3, 65534) + b"\x10", "padded raster exceeds"),
        "missing-planar-byte": (planar_data[:-1], "run count or sample"),
        "planar-trailing-palette": (planar_data + footer, "after the last scanline"),
        "short-planar-stride": (planar_data[:66] + b"\x02\x00" + planar_data[68:],
                                "even plane stride"),
    }
    for name, (bad, diagnostic) in invalid.items():
        source = output / (name + ".pcx")
        source.write_bytes(bad)
        target = output / (name + ".npy")
        run(name, [*prefix, "--raw", source, target], diagnostic)
        assert not target.exists() and source.read_bytes() == bad
    assert all(sha(path) == digest for path, digest in artifacts.items())
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.pcx-plugin-validation/1", "passed": True, "pillow": pillow_version,
        "module_sha256": artifacts[module], "manifest_sha256": artifacts[manifest],
        "region_sha256": artifacts[region], "positive": cases, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(cases)} exact PCX profiles and {len(checks)} native CLI checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("region", "manifest", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    verify(args.region.resolve(), args.manifest.resolve(), args.output.resolve())
