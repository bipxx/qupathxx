"""Check independently produced and authored native TGA rasters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
from PIL import Image, __version__ as pillow_version

SIGNATURE = b"TRUEVISION-XFILE.\0"
FOOTER = bytes(8) + SIGNATURE


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def authored(expected, descriptor, *, image_id=b"", packet_data=None, extension=None):
    height, width, channels = expected.shape
    kind = 2 if packet_data is None else 10
    header = struct.pack("<BBBHHBHHHHBB", len(image_id), 0, kind, 0, 0, 0, 0, 0,
                         width, height, channels * 8, descriptor | (8 if channels == 4 else 0))
    ordered = expected if descriptor & 32 else expected[::-1, :, :]
    if descriptor & 16:
        ordered = ordered[:, ::-1, :]
    raster = ordered[:, :, [2, 1, 0, 3] if channels == 4 else [2, 1, 0]].tobytes()
    body = header + image_id + (raster if packet_data is None else packet_data)
    if extension is None:
        return body
    return body + extension + struct.pack("<II", len(body), 0) + SIGNATURE


def partial_palette(expected, first, palette, *, packet=None, footer=b""):
    height, width = expected.shape
    header = struct.pack("<BBBHHBHHHHBB", 0, 1, 9 if packet else 1, first, len(palette), 24,
                         0, 0, width, height, 8, 32)
    data = palette[:, ::-1].tobytes()
    return header + data + (packet if packet else expected.tobytes()) + footer


def verify(region, manifest, output):
    output.mkdir(parents=True)
    entry, = json.loads(manifest.read_text(encoding="utf-8"))["plugins"]
    module = manifest.parent / entry["module"]["path"]
    artifacts = {path: sha(path) for path in (region, manifest, module)}
    assert artifacts[module] == entry["module"]["sha256"] and entry["dependencies"] == []
    env = dict(os.environ, PATH=os.pathsep.join(
        str(Path(os.environ["SystemRoot"]) / piece) for piece in ("System32", ".")))
    cases, checks = [], []
    plugin = [region, "--reader-plugins", manifest]
    prefix = [*plugin, "--reader", "org.qupathxx.tga"]

    def run(name, args, expected=0, diagnostic=None):
        argv = list(map(str, args))
        result = subprocess.run(argv, cwd=output, env=env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        checks.append({"name": name, "argv": argv, "exit": result.returncode})
        assert result.returncode == expected, (name, result)
        if diagnostic is not None:
            assert diagnostic in result.stderr, (name, result.stderr)
        return result.stdout

    def case(name, source, expected, *, palette=None, independent=True):
        height, width, channels = expected.shape
        before = sha(source)
        rgba = expected
        channel_names = ["Red", "Green", "Blue", "Alpha"][:channels]
        if channels != 4:
            rgba = np.full((height, width, 4), 255, dtype=np.uint8)
            rgba[:, :, :3] = palette[expected[:, :, 0]] if palette is not None else expected
        if channels == 1:
            channel_names = ["Palette index" if palette is not None else "Gray"]
        if independent:
            with Image.open(source) as decoded:
                np.testing.assert_array_equal(np.asarray(decoded).reshape(expected.shape), expected,
                                              strict=True)
                if channels == 1:
                    np.testing.assert_array_equal(np.asarray(decoded.convert("RGBA")), rgba,
                                                  strict=True)
        raw = output / (name + ".npy")
        report = json.loads(run(name + "-raw", [*prefix, "--raw", source, raw,
                                               "--channels", channels]))
        np.testing.assert_array_equal(np.load(raw), expected, strict=True)
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == artifacts[module]
        assert snapshot["raw"]["channel_names"] == channel_names
        assert snapshot["raw"]["pixel_width_um"] is None and snapshot["raw"]["pixel_height_um"] is None
        assert report["valid_pixels"] == width * height
        pam = output / (name + ".pam")
        run(name + "-display", [*prefix, source, 0, 0, 0, width, height, pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
        assert sha(source) == before
        cases.append({"name": name, "source_sha256": before, "raw_sha256": sha(raw),
                      "display_sha256": sha(pam), "pillow_oracle": independent})

    assert "org.qupathxx.tga" in run("catalog", [*plugin, "--readers"])
    rgb, rgba = ((np.arange(11 * 7 * n) * 43 % 256).astype(np.uint8).reshape(7, 11, n)
                 for n in (3, 4))
    rgb[2, 2:8] = rgb[2, 2]
    rgba[2, 2:8] = rgba[2, 2]
    rgba[0, 0, 3] = 0
    for expected in (rgb, rgba):
        channels = expected.shape[2]
        for orientation in (-1, 1):
            for rle in (False, True):
                name = f"pillow-{channels}c-orientation{orientation}-rle{int(rle)}"
                source = output / (name + ".tga")
                Image.fromarray(expected).save(source, rle=rle, orientation=orientation,
                                               id_section=b"qupathxx-authored")
                case(name, source, expected)
        for descriptor in (0, 16, 32, 48):
            name = f"legacy-{channels}c-origin{descriptor}"
            source = output / (name + ".tga")
            source.write_bytes(authored(expected, descriptor, image_id=bytes(range(255))))
            case(name, source, expected)

    scalar = (np.arange(77).reshape(7, 11) * 43 % 256).astype(np.uint8)
    scalar[2, 2:8] = scalar[2, 2]
    palette = (np.arange(768).reshape(256, 3) * 35 % 256).astype(np.uint8)
    for indexed in (False, True):
        image = Image.fromarray(scalar)
        if indexed:
            image.putpalette(palette.ravel().tolist())
        for orientation in (-1, 1):
            for rle in (False, True):
                name = f"scalar-mode{image.mode}-orientation{orientation}-rle{int(rle)}"
                source = output / (name + ".tga")
                image.save(source, rle=rle, orientation=orientation, id_section=b"scalar")
                case(name, source, scalar[:, :, None], palette=palette if indexed else None)

    codes = np.array([[253, 254, 255, 253], [255, 255, 254, 253]], dtype=np.uint8)
    colors = palette[253:]
    # Two distinct indices deliberately map to the same RGB.
    colors[1] = colors[0]
    partial = output / "partial-palette.tga"
    partial.write_bytes(partial_palette(codes, 253, colors, footer=FOOTER))
    case("partial-palette", partial, codes[:, :, None], palette=palette)
    partial_rle = output / "partial-palette-rle.tga"
    partial_rle.write_bytes(partial_palette(codes, 253, colors, footer=FOOTER,
                                            packet=b"\x03\xfd\xfe\xff\xfd\x81\xff\x01\xfe\xfd"))
    case("partial-palette-rle", partial_rle, codes[:, :, None], palette=palette)
    scalar_crop = output / "partial-palette-crop.npy"
    run("index-crop", [*prefix, "--raw", partial_rle, scalar_crop, "--x", 1, "--y", 0,
                       "--width", 2, "--height", 2])
    np.testing.assert_array_equal(np.load(scalar_crop), codes[:, 1:3, None], strict=True)

    exact = output / "right-top-rgba-extension.tga"
    extension = bytearray(495)
    struct.pack_into("<H", extension, 0, 495)
    extension[2:9] = b"QPX-TGA"
    extension[-1] = 3
    exact.write_bytes(authored(rgba, 48, extension=extension))
    case("extension", exact, rgba)
    crop = output / "crop.npy"
    run("channel-crop", [*prefix, "--raw", exact, crop, "--x", 2, "--y", 1, "--width", 5,
                         "--height", 3, "--channel", 1, "--channels", 2])
    np.testing.assert_array_equal(np.load(crop), rgba[1:4, 2:7, 1:3], strict=True)

    long_rgb = np.tile(np.array([11, 101, 235], dtype=np.uint8), (2, 129, 1))
    packet_data = (b"\xff\xeb\x65\x0b\x00\xeb\x65\x0b") * 2
    long_run = output / "max-run.tga"
    long_run.write_bytes(authored(long_rgb, 32, packet_data=packet_data) + FOOTER)
    case("max-run", long_run, long_rgb)
    repeated = np.tile(np.array([11, 101, 235], dtype=np.uint8), (2, 3, 1))
    legacy = output / "legacy-repeat.tga"
    legacy.write_bytes(authored(repeated, 32, packet_data=b"\x85\xeb\x65\x0b"))
    case("legacy-repeat", legacy, repeated, independent=False)

    plain = authored(rgb, 32)
    enclosed = exact.read_bytes()
    extension_start = len(enclosed) - 26 - 495
    indexed_data = partial.read_bytes()
    negative = {
        "truncated-header": (plain[:17], "header is truncated"),
        "truncated-id": (bytes([255]) + plain[1:22], "image ID is truncated"),
        "plain-truncated": (plain[:-1], "extent differs"),
        "plain-trailing": (plain + b"x", "extent differs"),
        "invalid-size": (plain[:12] + bytes(2) + plain[14:], "dimensions must be positive"),
        "palette": (plain[:1] + b"\x01" + plain[2:], "without a palette"),
        "interleaved": (plain[:17] + b"\x60" + plain[18:], "non-interleaved"),
        "alpha-ambiguous": (enclosed[:17] + b"\x30" + enclosed[18:], "depth or attributes"),
        "coordinate-offset": (plain[:8] + b"\x01" + plain[9:], "origin offsets"),
        "v2-cross-row-repeat": (legacy.read_bytes() + FOOTER, "repeat packet crosses a row"),
        "missing-packet": (legacy.read_bytes()[:18], "packet stream cannot fill"),
        "short-packet": (legacy.read_bytes()[:18] + b"\x05\xeb\x65\x0b", "packet payload is truncated"),
        "oversize-packet": (legacy.read_bytes()[:18] + b"\x86" + legacy.read_bytes()[19:],
                            "packet exceeds the raster size"),
        "trailing-packet": (legacy.read_bytes() + b"x", "trailing data"),
        "premultiplied": (enclosed[:-27] + b"\x04" + enclosed[-26:], "alpha semantics"),
        "extension-alias": (enclosed[:-26] + struct.pack("<II", 18, 0) + SIGNATURE,
                            "contiguous standard block"),
        "extension-setting": (enclosed[:extension_start + 474] + b"\x02" +
                              enclosed[extension_start + 475:], "unsupported raster settings"),
        "developer-area": (plain + struct.pack("<II", 0, 18) + SIGNATURE, "developer area"),
        "footer-overlaps-id": (b"\x12" + plain[1:18] + FOOTER, "footer overlaps"),
        "no-index-map": (indexed_data[:1] + b"\x00" + indexed_data[2:], "24-bit color map"),
        "wrong-map-depth": (indexed_data[:7] + b"\x20" + indexed_data[8:], "24-bit color map"),
        "empty-map": (indexed_data[:5] + bytes(2) + indexed_data[7:], "color map range"),
        "overflow-map-domain": (indexed_data[:3] + b"\xfe\x00" + indexed_data[5:],
                                "color map range"),
        "short-map": (indexed_data[:26] + FOOTER, "color map is truncated"),
        "index-below-map": (indexed_data[:27] + b"\xfc" + indexed_data[28:],
                            "index is outside the declared color map"),
        "repeat-index-below-map": (partial_rle.read_bytes()[:33] + b"\xfc" +
                                  partial_rle.read_bytes()[34:], "index is outside the declared color map"),
    }
    for name, (data, diagnostic) in negative.items():
        source = output / (name + ".tga")
        source.write_bytes(data)
        target = output / (name + ".npy")
        run(name, [*prefix, "--raw", source, target], 2, diagnostic)
        assert not target.exists() and source.read_bytes() == data
    target = output / "bad-channel.npy"
    run("bad-channel", [*prefix, "--raw", output / "legacy-3c-origin32.tga", target,
                        "--channel", 3], 2)
    assert not target.exists()
    assert all(sha(path) == digest for path, digest in artifacts.items())
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.tga-plugin-validation/1", "passed": True, "pillow": pillow_version,
        "module_sha256": artifacts[module], "manifest_sha256": artifacts[manifest],
        "region_sha256": artifacts[region], "positive": cases, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(cases)} exact TGA profiles and {len(checks)} native CLI checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("region", "manifest", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    verify(args.region.resolve(), args.manifest.resolve(), args.output.resolve())
