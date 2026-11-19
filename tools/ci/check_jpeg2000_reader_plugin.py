"""Exercise the native JPEG 2000 reader with independently authored samples."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
from PIL import Image, features


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def jp2_box(kind, contents):
    return struct.pack(">I4s", 8 + len(contents), kind) + contents


def append_jp2_header(data, child):
    header = data.index(b"jp2h") - 4
    size, = struct.unpack_from(">I", data, header)
    return (data[:header] + struct.pack(">I", size + len(child)) +
            data[header + 4:header + size] + child + data[header + size:])


def tiled_codestream_parts(data):
    """Return the generated main header and its finite standalone tile parts."""
    offset = 2
    while data[offset:offset + 2] != b"\xff\x90":
        assert offset + 4 <= len(data)
        length = int.from_bytes(data[offset + 2:offset + 4], "big")
        assert length >= 2
        offset += 2 + length
    header = data[:offset]
    tiles = []
    while data[offset:offset + 2] == b"\xff\x90":
        size = int.from_bytes(data[offset + 6:offset + 10], "big")
        assert 14 < size <= len(data) - offset - 2
        tiles.append(data[offset:offset + size])
        offset += size
    assert data[offset:] == b"\xff\xd9"
    return header, tiles


def write_tiled_u16(ffmpeg, path, expected, tile_size, precision=16):
    height, width = expected.shape[:2]
    channels = expected.shape[2] if expected.ndim == 3 else 1
    rgb = channels >= 3
    alpha = channels in (2, 4)
    samples = expected.astype("<u2", copy=False)
    if alpha:
        assert precision == 16
        pixel_format = {2: "ya16le", 4: "rgba64le"}[channels]
    elif rgb and precision < 16:
        pixel_format = f"gbrp{precision}le"
        samples = samples[:, :, [1, 2, 0]].transpose(2, 0, 1)
    else:
        pixel_format = "rgb48le" if rgb else f"gray{precision}le"
    raw_path = path.with_suffix(".j2k")
    subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-f", "rawvideo",
                    "-pix_fmt", pixel_format, "-s", f"{width}x{height}", "-i", "pipe:0",
                    "-frames:v", "1", "-c:v", "jpeg2000", "-pred", "dwt53",
                    "-tile_width", str(tile_size[0]), "-tile_height", str(tile_size[1]),
                    "-format", "j2k", "-f", "image2", "-update", "1", str(raw_path)],
                   input=samples.tobytes(), capture_output=True,
                   timeout=30, check=True)
    if path != raw_path:
        # FFmpeg 7.1 can write the precision instead of precision minus one.
        # Author a minimal conforming wrapper over its unchanged codestream.
        def box(kind, data):
            return struct.pack(">I4s", 8 + len(data), kind) + data

        image = box(b"ihdr", struct.pack(">IIHBBBB", height, width, channels, precision - 1, 7, 0, 0))
        color = box(b"colr", struct.pack(">BBBI", 1, 0, 0, 16 if rgb else 17))
        definitions = b""
        if alpha:
            associations = b"".join(struct.pack(">HHH", c, 0, c + 1) for c in range(channels - 1))
            definitions = box(b"cdef", struct.pack(">H", channels) + associations +
                              struct.pack(">HHH", channels - 1, 1, 0))
        path.write_bytes(box(b"jP  ", bytes.fromhex("0d0a870a")) +
                         box(b"ftyp", b"jp2 " + bytes(4) + b"jp2 ") +
                         box(b"jp2h", image + color + definitions) + box(b"jp2c", raw_path.read_bytes()))


def check_ffmpeg_u16(ffmpeg, path, expected, precision):
    channels = expected.shape[2] if expected.ndim == 3 else 1
    pixel_format = {1: "gray16le", 2: "ya16le", 3: "rgb48le", 4: "rgba64le"}[channels]
    decoded = subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-i",
                              str(path), "-f", "rawvideo", "-pix_fmt", pixel_format, "pipe:1"],
                             capture_output=True, check=True, timeout=30)
    # FFmpeg returns left-aligned samples in these pixel formats.
    assert decoded.stdout == (expected << (16 - precision)).astype("<u2", copy=False).tobytes()


def verify(region, manifest, output, ffmpeg, openjpeg):
    output.mkdir(parents=True)
    entry, = json.loads(manifest.read_text(encoding="utf-8"))["plugins"]
    module = manifest.parent / entry["module"]["path"]
    artifacts = {path: sha(path) for path in (region, manifest, module)}
    assert artifacts[module] == entry["module"]["sha256"]
    assert entry["dependencies"] == []
    env = dict(os.environ, PATH=os.pathsep.join(
        str(Path(os.environ["SystemRoot"]) / path) for path in ("System32", ".")))
    checks = []
    plugin = [region, "--reader-plugins", manifest]
    prefix = [*plugin, "--reader", "org.qupathxx.jpeg2000"]

    def run(name, args, refusal=False):
        command = list(map(str, args))
        result = subprocess.run(command, cwd=output, env=env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        checks.append({"name": name, "argv": command, "exit": result.returncode})
        if refusal:
            assert result.returncode == 2 and result.stderr.strip(), (name, result)
            if isinstance(refusal, str):
                assert refusal in result.stderr, (name, result)
        else:
            assert result.returncode == 0, (name, result)
        return result.stdout

    assert "org.qupathxx.jpeg2000" in run("catalog", [*plugin, "--readers"])
    cases = []
    tiled_example = None
    profiles = ((8, 1, 1, None), (8, 61, 37, None), (16, 13, 9, None), (16, 257, 33, None),
                (8, 61, 37, (16, 16)), (16, 73, 53, (32, 24)), (8, 64, 64, (1, 1)),
                *((depth, 73, 53, (32, 24)) for depth in (9, 10, 12, 14)))
    for depth, width, height, tile in profiles:
        span = 1 << depth
        expected = ((np.arange(height * width) * 1237 + 173) % span).reshape(height, width)
        dtype = "uint8" if depth == 8 else "uint16"
        expected = expected.astype(dtype)
        if width > 1:
            expected[0, :2] = (0, span - 1)
        gray = ((expected.astype(np.uint32) * 255 + (span - 1) // 2) // (span - 1))
        rgba = np.full((height, width, 4), 255, dtype=np.uint8)
        rgba[:, :, :3] = gray[:, :, None]
        for suffix in ("jp2", "j2k"):
            tiled = "" if tile is None else f"-tiles{tile[0]}x{tile[1]}"
            name = f"u{depth}-{width}x{height}{tiled}-{suffix}"
            image = output / f"{name}.{suffix}"
            if tile is not None and depth > 8:
                write_tiled_u16(ffmpeg, image, expected, tile, depth)
            elif tile is not None:
                Image.fromarray(expected).save(image, irreversible=False, tile_size=tile,
                                               num_resolutions=1 if tile == (1, 1) else 3)
            else:
                Image.fromarray(expected).save(image, irreversible=False)
            source_hash = sha(image)
            if depth == 9:
                check_ffmpeg_u16(ffmpeg, image, expected, depth)
            else:
                with Image.open(image) as decoded:
                    aligned = expected << (16 - depth) if depth > 8 else expected
                    np.testing.assert_array_equal(np.asarray(decoded), aligned, strict=True)
            raw = output / (name + ".npy")
            result = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw]))
            np.testing.assert_array_equal(np.load(raw), expected[:, :, None], strict=True)
            implementation = result["reader_snapshot"]["implementation"]
            metadata = result["reader_snapshot"]["raw"]
            assert implementation["module_sha256"] == artifacts[module]
            assert metadata["channel_names"] == ["Gray"]
            assert metadata["pixel_width_um"] is None and metadata["pixel_height_um"] is None
            assert result["valid_pixels"] == width * height and result["dtype"] == dtype
            pam = output / (name + ".pam")
            run(name + "-display", [*prefix, image, 0, 0, 0, width, height, pam])
            assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
            if width > 1:
                crop = output / (name + "-crop.npy")
                run(name + "-crop", [*prefix, "--raw", image, crop, "--x", width - 3,
                                     "--y", height - 2, "--width", 3, "--height", 2])
                np.testing.assert_array_equal(np.load(crop), expected[-2:, -3:, None], strict=True)
            if tile and min(tile) > 1:
                x, y = tile[0] - 2, tile[1] - 2
                crossing = output / (name + "-crossing.npy")
                run(name + "-crossing", [*prefix, "--raw", image, crossing, "--x", x,
                                         "--y", y, "--width", 5, "--height", 5])
                np.testing.assert_array_equal(np.load(crossing), expected[y:y + 5, x:x + 5, None],
                                              strict=True)
                if suffix == "j2k" and depth == 16:
                    tiled_example = (image.read_bytes(), expected)
            assert sha(image) == source_hash
            cases.append({"image": image.name, "source_sha256": source_hash,
                          "raw_sha256": sha(raw), "display_sha256": sha(pam)})

    codestream = (output / "u16-13x9-j2k.j2k").read_bytes()
    boxed = (output / "u16-13x9-jp2.jp2").read_bytes()
    for depth, transform in ((8, 0), (8, 1), (9, 0), (10, 0), (12, 0), (14, 0), (16, 0)):
        name = f"rgb{depth}-mct{transform}"
        image = output / (name + ".jp2")
        y, x, channel = np.indices((53, 73, 3))
        span = 1 << depth
        dtype = "uint8" if depth == 8 else "uint16"
        expected = ((x * (71 + channel * 997) + y * (17 + channel * 311) +
                     channel * 2081) % span).astype(dtype)
        expected[0, :2] = ((0, span - 1, span // 2), (span - 1, 0, 1))
        if depth == 8:
            Image.fromarray(expected).save(image, irreversible=False, mct=transform,
                                           tile_size=(32, 24), num_resolutions=3)
            with Image.open(image) as decoded:
                np.testing.assert_array_equal(np.asarray(decoded), expected, strict=True)
        else:
            write_tiled_u16(ffmpeg, image, expected, (32, 24), depth)
            check_ffmpeg_u16(ffmpeg, image, expected, depth)
        data = image.read_bytes()
        j2k_offset = data.index(b"jp2c") + 4
        assert data[j2k_offset + 42:j2k_offset + 51] == bytes((depth - 1, 1, 1)) * 3
        main_header, _ = tiled_codestream_parts(data[j2k_offset:])
        assert main_header[main_header.index(b"\xff\x52") + 8] == transform
        source_hash = sha(image)
        raw = output / (name + ".npy")
        result = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw, "--channels", 3]))
        np.testing.assert_array_equal(np.load(raw), expected, strict=True)
        metadata = result["reader_snapshot"]["raw"]
        assert metadata["channel_names"] == ["Red", "Green", "Blue"]
        assert metadata["pixel_width_um"] is None and metadata["pixel_height_um"] is None
        assert result["valid_pixels"] == 73 * 53 and result["dtype"] == dtype
        crop = output / (name + "-crop.npy")
        run(name + "-crop", [*prefix, "--raw", image, crop, "--x", 30, "--y", 22,
                             "--width", 5, "--height", 5, "--channel", 1, "--channels", 2])
        np.testing.assert_array_equal(np.load(crop), expected[22:27, 30:35, 1:], strict=True)
        pam = output / (name + ".pam")
        run(name + "-display", [*prefix, image, 0, 0, 0, 73, 53, pam])
        rgba = np.full((53, 73, 4), 255, dtype=np.uint8)
        rgba[:, :, :3] = (expected.astype(np.uint32) * 255 + (span - 1) // 2) // (span - 1)
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
        assert sha(image) == source_hash
        cases.append({"image": image.name, "source_sha256": source_hash,
                      "raw_sha256": sha(raw), "display_sha256": sha(pam)})

    for channels, depth, transform in ((2, 8, 0), (2, 16, 0), (4, 8, 0), (4, 8, 1), (4, 16, 0)):
        name = f"alpha-c{channels}-u{depth}-mct{transform}"
        image = output / (name + ".jp2")
        y, x, channel = np.indices((53, 73, channels))
        max_code = (1 << depth) - 1
        dtype = f"uint{depth}"
        expected = ((x * (43 + channel * 913) + y * (27 + channel * 311) +
                     channel * 2051) % (max_code + 1)).astype(dtype)
        expected[0, 0, -1] = 0
        expected[0, 0, 0] = max_code
        expected[0, 1, -1] = max_code // 2 + 1
        expected[0, 2, -1] = max_code
        if depth == 8:
            encoded = expected
            if transform:
                # Pillow disables MCT for RGBA. Author its reversible transform
                # in the component planes and set the matching COD transform flag.
                expected[:, :, :3] = expected[:, :, :3] % 128 + 64
                r, g, b = expected[:, :, :3].astype(np.int16).transpose(2, 0, 1)
                encoded = expected.copy()
                encoded[:, :, 0] = (r + 2 * g + b) // 4
                encoded[:, :, 1] = b - g + 128
                encoded[:, :, 2] = r - g + 128
            Image.fromarray(encoded).save(image, irreversible=False, mct=0,
                                           tile_size=(32, 24), num_resolutions=3)
            if transform:
                data = bytearray(image.read_bytes())
                cod = data.index(b"\xff\x52", data.index(b"jp2c"))
                assert data[cod + 8] == 0
                data[cod + 8] = 1
                image.write_bytes(data)
            with Image.open(image) as decoded:
                np.testing.assert_array_equal(np.asarray(decoded), expected, strict=True)
        else:
            write_tiled_u16(ffmpeg, image, expected, (32, 24), depth)
            check_ffmpeg_u16(ffmpeg, image, expected, depth)
        data = image.read_bytes()
        j2k_offset = data.index(b"jp2c") + 4
        main_header, _ = tiled_codestream_parts(data[j2k_offset:])
        assert main_header[main_header.index(b"\xff\x52") + 8] == transform
        source_hash = sha(image)
        raw = output / (name + ".npy")
        metadata = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw, "--channels", channels]))
        np.testing.assert_array_equal(np.load(raw), expected, strict=True)
        names = ["Gray", "Alpha"] if channels == 2 else ["Red", "Green", "Blue", "Alpha"]
        assert metadata["reader_snapshot"]["raw"]["channel_names"] == names
        assert metadata["valid_pixels"] == 73 * 53 and metadata["dtype"] == dtype
        crop = output / (name + "-crop.npy")
        run(name + "-crop", [*prefix, "--raw", image, crop, "--x", 30, "--y", 22,
                             "--width", 5, "--height", 5, "--channel", channels - 1])
        np.testing.assert_array_equal(np.load(crop), expected[22:27, 30:35, -1:], strict=True)
        pam = output / (name + ".pam")
        run(name + "-display", [*prefix, image, 0, 0, 0, 73, 53, pam])
        codes = ((expected.astype(np.uint32) * 255 + max_code // 2) // max_code).astype("uint8")
        rgba = codes if channels == 4 else codes[:, :, [0, 0, 0, 1]]
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
        assert sha(image) == source_hash
        cases.append({"image": image.name, "source_sha256": source_hash,
                      "raw_sha256": sha(raw), "display_sha256": sha(pam)})

    with_alpha = (output / "alpha-c4-u8-mct1.jp2").read_bytes()
    definitions_offset = with_alpha.index(b"cdef") - 4
    definitions_end = definitions_offset + int.from_bytes(with_alpha[definitions_offset:definitions_offset + 4], "big")
    opacity_offset = definitions_end - 6
    header_offset = with_alpha.index(b"jp2h") - 4
    without_definitions = (with_alpha[:header_offset] + struct.pack(">I", definitions_offset - header_offset) +
                           with_alpha[header_offset + 4:definitions_offset] + with_alpha[definitions_end:])
    # The entries are a table keyed by component index and need not be listed in that order.
    reordered_definitions = output / "reordered-definitions.jp2"
    color_box = with_alpha.index(b"colr") - 4
    reordered_definitions.write_bytes(with_alpha[:color_box] + with_alpha[definitions_offset:definitions_offset + 10] +
                                       with_alpha[opacity_offset:definitions_end] +
                                       with_alpha[definitions_offset + 10:opacity_offset] +
                                       with_alpha[color_box:definitions_offset] + with_alpha[definitions_end:])
    reordered_raw = output / "reordered-definitions.npy"
    run("reordered-definitions", [*prefix, "--raw", reordered_definitions, reordered_raw, "--channels", 4])
    assert reordered_raw.read_bytes() == (output / "alpha-c4-u8-mct1.npy").read_bytes()

    rgb = (output / "rgb8-mct1.jp2").read_bytes()
    color_offset = rgb.index(b"colr") + 7
    rgb_offset = rgb.index(b"jp2c") + 4
    capture = jp2_box(b"resc", struct.pack(">HHHHbb", 6, 3, 8, 2, 6, 6))
    display = jp2_box(b"resd", struct.pack(">HHHHbb", 4, 1, 2, 1, 3, 3))
    signed = jp2_box(b"resc", struct.pack(">HHHHbb", 2, 1, 5, 1, -1, 1))
    for name, resolution, control, dimensions, spacing in (
        ("capture-resolution", capture, "u16-73x53-tiles32x24-jp2", (73, 53, 1), (.25, .5)),
        ("display-resolution", display, "u16-73x53-tiles32x24-jp2", (73, 53, 1), (None, None)),
        ("both-resolutions", display + capture, "alpha-c4-u8-mct1", (73, 53, 4), (.25, .5)),
        ("signed-resolution-exponent", signed, "u16-73x53-tiles32x24-jp2", (73, 53, 1), (20000, 5000000)),
    ):
        reference = output / (control + ".jp2")
        image = output / (name + ".jp2")
        image.write_bytes(append_jp2_header(reference.read_bytes(), jp2_box(b"res ", resolution)))
        source_hash = sha(image)
        with Image.open(reference) as expected, Image.open(image) as decoded:
            np.testing.assert_array_equal(np.asarray(decoded), np.asarray(expected), strict=True)
        width, height, channels = dimensions
        raw = output / (name + ".npy")
        result = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw, "--channels", channels]))
        assert raw.read_bytes() == (output / (control + ".npy")).read_bytes()
        for section in ("metadata", "raw"):
            metadata = result["reader_snapshot"][section]
            assert (metadata["pixel_width_um"], metadata["pixel_height_um"]) == spacing
        pam = output / (name + ".pam")
        run(name + "-display", [*prefix, image, 0, 0, 0, width, height, pam])
        assert pam.read_bytes() == (output / (control + ".pam")).read_bytes()
        assert sha(image) == source_hash
        cases.append({"image": image.name, "source_sha256": source_hash, "raw_sha256": sha(raw),
                      "display_sha256": sha(pam), "pixel_spacing_um": spacing})
    signed_profile = None
    for depth in (8, 9, 12, 16):
        width, height = 73, 53
        midpoint = 1 << (depth - 1)
        dtype = np.dtype("i1" if depth == 8 else "<i2")
        expected = (((np.arange(height * width) * 1237 + 173) % (2 * midpoint)) -
                    midpoint).astype(dtype).reshape(height, width)
        expected[0, :4] = (-midpoint, -1, 0, midpoint - 1)
        raw_source = output / f"signed-{depth}.raw"
        oracle_bytes = expected.astype(dtype.newbyteorder(">")).tobytes()
        raw_source.write_bytes(oracle_bytes)
        gray = (((expected.astype(np.int32) + midpoint) * 255 + midpoint - 1) //
                (2 * midpoint - 1)).astype("u1")
        rgba = np.full((height, width, 4), 255, dtype="u1")
        rgba[:, :, :3] = gray[:, :, None]
        for suffix in ("j2k", "jp2"):
            name = f"signed-{depth}-{suffix}"
            image = output / (name + "." + suffix)
            if depth in (8, 16):
                subprocess.run([openjpeg / "opj_compress.exe", "-i", raw_source, "-o", image,
                                "-F", f"{width},{height},1,{depth},s", "-n", "3", "-t", "32,24"],
                               check=True, capture_output=True, timeout=30)
            else:
                # Encode the same centered coefficients with the unsigned input
                # path, then declare signed samples in SIZ and, for JP2, IHDR.
                unsigned = (expected.astype(np.int32) + midpoint).astype("u2")
                write_tiled_u16(ffmpeg, image, unsigned, (32, 24), depth)
                data = bytearray(image.read_bytes())
                codestream_start = 0 if suffix == "j2k" else data.index(b"jp2c") + 4
                data[codestream_start + 42] |= 128
                if suffix == "jp2":
                    data[data.index(b"ihdr") + 14] |= 128
                image.write_bytes(data)
            calibrated = depth == 16 and suffix == "jp2"
            if calibrated:
                image.write_bytes(append_jp2_header(image.read_bytes(), jp2_box(b"res ", capture)))
                signed_profile = image.read_bytes()
            source_hash = sha(image)
            pgx = output / (name + "-oracle.pgx")
            subprocess.run([openjpeg / "opj_decompress.exe", "-i", image, "-o", pgx],
                           check=True, capture_output=True, timeout=30)
            pgx_header, pgx_data = pgx.with_stem(pgx.stem + "_0").read_bytes().split(b"\n", 1)
            assert pgx_header.split() == f"PG ML - {depth} {width} {height}".encode().split()
            assert pgx_data == oracle_bytes, name
            raw = output / (name + ".npy")
            report = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw]))
            np.testing.assert_array_equal(np.load(raw), expected[:, :, None], strict=True)
            metadata = report["reader_snapshot"]["raw"]
            assert metadata["channel_names"] == ["Gray"] and report["valid_pixels"] == width * height
            spacing = (.25, .5) if calibrated else (None, None)
            assert (metadata["pixel_width_um"], metadata["pixel_height_um"]) == spacing
            cropped = output / (name + "-crop.npy")
            run(name + "-crop", [*prefix, "--raw", image, cropped, "--x", 30, "--y", 22,
                                 "--width", 5, "--height", 5])
            np.testing.assert_array_equal(np.load(cropped), expected[22:27, 30:35, None], strict=True)
            pam = output / (name + ".pam")
            run(name + "-display", [*prefix, image, 0, 0, 0, width, height, pam])
            assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
            assert sha(image) == source_hash
            cases.append({"image": image.name, "source_sha256": source_hash, "raw_sha256": sha(raw),
                          "display_sha256": sha(pam), "pixel_spacing_um": spacing})
    assert signed_profile is not None
    signed_ihdr_sample = signed_profile.index(b"ihdr") + 14
    assert signed_profile[signed_ihdr_sample] == 0x8f

    irreversible_cases = []
    lossy_codestream = None
    y, x = np.indices((53, 73))
    for pattern, original, ratio, tile in (
        ("texture", ((x * 37 + y * 49 + (x * y) % 101) % 256).astype("u1"), 8, True),
        ("gradient", ((x * 3 + y * 2) % 256).astype("u1"), 2, False),
    ):
        source = output / (pattern + ".pgm")
        source.write_bytes(b"P5\n73 53\n255\n" + original.tobytes())
        for suffix in ("jp2", "j2k"):
            name = f"irreversible-{pattern}-{suffix}"
            image = output / (name + "." + suffix)
            subprocess.run([openjpeg / "opj_compress.exe", "-i", source, "-o", image,
                            "-I", "-n", "3", "-r", str(ratio), *(["-t", "32,24"] if tile else [])],
                           check=True, capture_output=True, timeout=30)
            calibration = (.25, .5) if suffix == "jp2" else (None, None)
            if suffix == "jp2":
                image.write_bytes(append_jp2_header(image.read_bytes(), jp2_box(b"res ", capture)))
            if tile and suffix == "j2k":
                lossy_codestream = image.read_bytes()
            source_hash = sha(image)
            pgx = output / (name + "-oracle.pgx")
            subprocess.run([openjpeg / "opj_decompress.exe", "-i", image, "-o", pgx],
                           check=True, capture_output=True, timeout=30)
            header, pgx_data = pgx.with_stem(pgx.stem + "_0").read_bytes().split(b"\n", 1)
            assert header.split() == b"PG ML + 8 73 53".split()
            reference = np.frombuffer(pgx_data, dtype="u1").reshape(53, 73, 1)
            assert not np.array_equal(reference[:, :, 0], original)
            raw = output / (name + ".npy")
            report = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw]))
            native = np.load(raw, allow_pickle=False)
            assert native.dtype == np.dtype("u1") and native.shape == (53, 73, 1)
            delta = np.abs(native.astype("i2") - reference)
            assert delta.max() <= 1, (name, int(delta.max()))
            assert report["valid_pixels"] == native.size and report["dtype"] == "uint8"
            snapshot = report["reader_snapshot"]
            assert snapshot["metadata"]["vendor"] == f"{suffix.upper()} (irreversible)"
            assert (snapshot["raw"]["pixel_width_um"], snapshot["raw"]["pixel_height_um"]) == calibration
            cropped = output / (name + "-crop.npy")
            run(name + "-crop", [*prefix, "--raw", image, cropped, "--x", 30, "--y", 22,
                                 "--width", 5, "--height", 5])
            np.testing.assert_array_equal(np.load(cropped), native[22:27, 30:35], strict=True)
            pam = output / (name + ".pam")
            run(name + "-display", [*prefix, image, 0, 0, 0, 73, 53, pam])
            display = np.frombuffer(pam.read_bytes().split(b"ENDHDR\n", 1)[1], dtype="u1").reshape(53, 73, 4)
            np.testing.assert_array_equal(display[:, :, :3], np.repeat(native, 3, axis=2), strict=True)
            assert (display[:, :, 3] == 255).all() and sha(image) == source_hash
            irreversible_cases.append({"image": image.name, "source_sha256": source_hash,
                                       "raw_sha256": sha(raw), "display_sha256": sha(pam),
                                       "pixel_spacing_um": calibration, "max_openjpeg_code_delta": int(delta.max()),
                                       "mean_openjpeg_code_delta": float(delta.mean())})
    cases.extend(irreversible_cases)
    assert lossy_codestream is not None
    cod = lossy_codestream.index(b"\xff\x52")
    qcd = lossy_codestream.index(b"\xff\x5c")

    layered = []
    explicit_grid = None
    marked_stream = None
    packet_table_case = None
    grayscale = ((x * 37 + y * 49 + (x * y) % 101) % 256).astype("u1")
    progressions = ("LRCP", "RLCP", "RPCL", "PCRL", "CPRL")
    rectangular = "[16,8],[8,4],[4,2]"
    for suffix, quality, irreversible, is_rgb, order, precincts, packet_markers in (
        ("jp2", "16,8,1", False, False, "LRCP", None, ()),
        ("j2k", ",".join(map(str, range(16, 0, -1))), False, False, "LRCP", None, ()),
        ("j2k", "16,10,6", True, False, "LRCP", None, ()),
        ("jp2", "16,8,1", False, True, "LRCP", None, ()),
        ("j2k", "16,8,1", False, False, "RLCP", None, ()),
        ("jp2", "16,10,6", True, False, "RPCL", None, ()),
        ("jp2", "16,8,1", False, True, "PCRL", None, ()),
        ("jp2", "16,8,1", False, True, "CPRL", None, ()),
        ("jp2", "16,8,1", False, False, "LRCP", "[8,8],[4,4],[1,1]", ()),
        ("j2k", "16,8,1", False, False, "RLCP", rectangular, ()),
        ("jp2", "16,10,6", True, False, "RPCL", rectangular, ()),
        ("jp2", "16,8,1", False, True, "PCRL", rectangular, ()),
        ("jp2", "16,8,1", False, True, "CPRL", rectangular, ()),
        ("jp2", "16,8,1", False, False, "RPCL", None, ("SOP",)),
        ("j2k", "16,8,1", False, False, "RLCP", None, ("EPH",)),
        ("jp2", "8,4,2", True, False, "RPCL", None, ("SOP", "EPH")),
        ("j2k", "16,8,1", False, False, "RPCL", None, ("PLT",)),
        ("jp2", "8,4,2", True, False, "RPCL", None, ("SOP", "EPH", "PLT")),
    ):
        layers = len(quality.split(","))
        profile = "rgb" if is_rgb else ("irreversible-gray" if irreversible else "reversible-gray")
        name = f"quality-{layers}-{profile}-{suffix}"
        if order != "LRCP":
            name += "-" + order.lower()
        if precincts:
            name += "-precincts"
        if packet_markers:
            name += "-" + "-".join(marker.lower() for marker in packet_markers)
        samples = (np.stack([grayscale, (x * 7 + y * 3) % 256, 255 - grayscale], axis=2)
                   .astype("u1") if is_rgb else grayscale[:, :, None])
        source = output / (name + (".ppm" if is_rgb else ".pgm"))
        source.write_bytes((b"P6" if is_rgb else b"P5") + b"\n73 53\n255\n" + samples.tobytes())
        image = output / (name + "." + suffix)
        tile = "31,23" if precincts else "32,24"
        subprocess.run([openjpeg / "opj_compress.exe", "-i", source, "-o", image, "-n", "3",
                        "-r", quality, "-t", tile, "-p", order, *(["-I"] if irreversible else []),
                        *(["-c", precincts] if precincts else []), *("-" + marker for marker in packet_markers)],
                       check=True, capture_output=True, timeout=30)
        if suffix == "jp2" and not is_rgb:
            image.write_bytes(append_jp2_header(image.read_bytes(), jp2_box(b"res ", capture)))
        data = image.read_bytes()
        offset = data.index(b"\xff\x52")
        assert int.from_bytes(data[offset+6:offset+8], "big") == layers
        assert data[offset + 5] == progressions.index(order)
        assert data[offset + 4] == (bool(precincts) + 2 * ("SOP" in packet_markers) + 4 * ("EPH" in packet_markers))
        if packet_markers == ("SOP", "EPH"):
            marked_stream = data, offset
        if packet_markers == ("PLT",):
            packet_table_case = data, name
        if precincts and suffix == "j2k":
            explicit_grid = data, offset
        if layers == 3:
            layered.append((data, offset))
        oracle_images = []
        for label, options in (("all", []), ("first", ["-l", "1"])):
            path = output / (name + "-" + label + (".ppm" if is_rgb else ".pgx"))
            subprocess.run([openjpeg / "opj_decompress.exe", "-i", image, "-o", path, *options],
                           check=True, capture_output=True, timeout=30)
            if is_rgb:
                with Image.open(path) as decoded:
                    oracle_images.append(np.array(decoded))
            else:
                header, pixels = path.with_stem(path.stem + "_0").read_bytes().split(b"\n", 1)
                assert header.split() == b"PG ML + 8 73 53".split()
                oracle_images.append(np.frombuffer(pixels, dtype="u1").reshape(samples.shape))
        all_layers, first = oracle_images
        assert not np.array_equal(all_layers, first), name
        if not irreversible:
            np.testing.assert_array_equal(all_layers, samples, strict=True)
        raw = output / (name + ".npy")
        report = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw, "--channels", samples.shape[2]]))
        native = np.load(raw, allow_pickle=False)
        assert native.shape == samples.shape and native.dtype == np.dtype("u1")
        delta = np.abs(native.astype("i2") - all_layers)
        assert delta.max() <= (1 if irreversible else 0), (name, int(delta.max()))
        meta = report["reader_snapshot"]["raw"]
        assert meta["channel_names"] == (["Red", "Green", "Blue"] if is_rgb else ["Gray"])
        spacing = (.25, .5) if suffix == "jp2" and not is_rgb else (None, None)
        assert (meta["pixel_width_um"], meta["pixel_height_um"]) == spacing
        crop = output / (name + "-crop.npy")
        run(name + "-crop", [*prefix, "--raw", image, crop, "--channels", samples.shape[2],
                            "--x", 30, "--y", 22, "--width", 5, "--height", 5])
        np.testing.assert_array_equal(np.load(crop), native[22:27, 30:35], strict=True)
        assert image.read_bytes() == data
        cases.append({"image": image.name, "source_sha256": sha(image), "raw_sha256": sha(raw),
                      "quality_layers": layers, "progression": order, "precincts": precincts, "packet_markers": packet_markers,
                      "max_openjpeg_code_delta": int(delta.max()),
                      "first_layer_differs": True})

    assert tiled_example is not None
    multipart = None
    for suffix, split, order, rates in (("j2k", "L", "LRCP", "16,8,1"),
                                        ("jp2", "R", "RLCP", "8,4,2")):
        name = f"tile-part-{split.lower()}-{suffix}"
        image = output / (name + "." + suffix)
        subprocess.run([openjpeg / "opj_compress.exe", "-i", output / "texture.pgm", "-o", image,
                        "-n", "3", "-r", rates, "-p", order, "-t", "31,23", "-TP", split,
                        "-PLT", *(["-I"] if suffix == "jp2" else [])],
                       check=True, capture_output=True, timeout=30)
        if suffix == "jp2":
            image.write_bytes(append_jp2_header(image.read_bytes(), jp2_box(b"res ", capture)))
        data = image.read_bytes()
        stream = data if suffix == "j2k" else data[data.index(b"jp2c") + 4:]
        main_header, parts = tiled_codestream_parts(stream)
        assert len(parts) == 27 and all(p[11] == 3 for p in parts)
        if suffix == "j2k":
            multipart = main_header, parts
        pgx = output / (name + "-oracle.pgx")
        subprocess.run([openjpeg / "opj_decompress.exe", "-i", image, "-o", pgx],
                       check=True, capture_output=True, timeout=30)
        hdr, pixels = pgx.with_stem(pgx.stem + "_0").read_bytes().split(b"\n", 1)
        assert hdr.split() == b"PG ML + 8 73 53".split()
        reference = np.frombuffer(pixels, dtype="u1").reshape(53, 73, 1)
        raw = output / (name + ".npy")
        report = json.loads(run(name + "-raw", [*prefix, "--raw", image, raw]))
        native = np.load(raw, allow_pickle=False)
        delta = np.abs(native.astype("i2") - reference)
        assert (native.shape, native.dtype) == (reference.shape, reference.dtype)
        assert delta.max() <= (1 if suffix == "jp2" else 0)
        meta = report["reader_snapshot"]["raw"]
        spacing = (.25, .5) if suffix == "jp2" else (None, None)
        assert (meta["pixel_width_um"], meta["pixel_height_um"]) == spacing
        crop = output / (name + "-crop.npy")
        run(name + "-crop", [*prefix, "--raw", image, crop, "--x", 30, "--y", 22,
                            "--width", 5, "--height", 5])
        np.testing.assert_array_equal(np.load(crop), native[22:27, 30:35], strict=True)
        cases.append({"image": image.name, "source_sha256": sha(image), "raw_sha256": sha(raw),
                      "tile_part_split": split, "max_openjpeg_code_delta": int(delta.max())})
    assert multipart is not None
    main_header, parts = multipart
    interleaved = output / "interleaved-tile-parts.j2k"
    interleaved.write_bytes(main_header + b"".join(sorted(parts, key=lambda part: part[10])) + b"\xff\xd9")
    interleaved_raw = output / "interleaved-tile-parts.npy"
    run("interleaved-tile-parts", [*prefix, "--raw", interleaved, interleaved_raw])
    assert interleaved_raw.read_bytes() == (output / "tile-part-l-j2k.npy").read_bytes()

    tiled_data, tiled_expected = tiled_example
    header, tiles = tiled_codestream_parts(tiled_data)
    assert len(tiles) == 9
    reordered = output / "reordered.j2k"
    reordered.write_bytes(header + b"".join(reversed(tiles)) + b"\xff\xd9")
    with Image.open(reordered) as decoded:
        np.testing.assert_array_equal(np.asarray(decoded), tiled_expected, strict=True)
    raw = output / "reordered.npy"
    run("reordered", [*prefix, "--raw", reordered, raw])
    np.testing.assert_array_equal(np.load(raw), tiled_expected[:, :, None], strict=True)

    at_tile_limit = (output / "u8-64x64-tiles1x1-j2k.j2k").read_bytes()
    invalid = {
        "bad-signature": b"not a raster",
        "missing-eoc": codestream[:-2],
        "trailing": codestream + b"\0",
        "truncated": codestream[:-20] + codestream[-2:],
        "signed-color": rgb[:rgb_offset + 42] + bytes((0x87,)) + rgb[rgb_offset + 43:],
        "signed-jp2-disagreement": (signed_profile[:signed_ihdr_sample] + bytes((15,)) +
                                    signed_profile[signed_ihdr_sample + 1:]),
        "irreversible-signed": lossy_codestream[:42] + b"\x87" + lossy_codestream[43:],
        "irreversible-high-depth": lossy_codestream[:42] + b"\x0f" + lossy_codestream[43:],
        "irreversible-implicit-scalar": lossy_codestream[:qcd+4] + b"\x41" + lossy_codestream[qcd+5:],
        "irreversible-quantizer-count": lossy_codestream[:cod+9] + b"\x01" + lossy_codestream[cod+10:],
        "irreversible-zero-exponent": lossy_codestream[:qcd+5] + b"\x00" + lossy_codestream[qcd+6:],
        "zero-quality-layers": lossy_codestream[:cod+6] + b"\0\0" + lossy_codestream[cod+8:],
        "above-quality-layer-limit": lossy_codestream[:cod+6] + b"\0\x11" + lossy_codestream[cod+8:],
        "unknown-progression": lossy_codestream[:cod+5] + b"\x05" + lossy_codestream[cod+6:],
        "precision-below-profile": codestream[:42] + bytes((6,)) + codestream[43:],
        "precision-above-profile": codestream[:42] + bytes((16,)) + codestream[43:],
        "nonzero-origin": codestream[:16] + struct.pack(">I", 1) + codestream[20:],
        "oversize-raster": codestream[:8] + struct.pack(">I", 8193) + codestream[12:],
        "missing-declared-tile": codestream[:24] + struct.pack(">I", 8) + codestream[28:],
        "empty-tile-width": codestream[:24] + bytes(4) + codestream[28:],
        "too-many-tiles": at_tile_limit[:8] + struct.pack(">I", 65) + at_tile_limit[12:],
        "duplicate-tile": header + tiles[0] + tiles[0] + b"".join(tiles[2:]) + b"\xff\xd9",
        "missing-final-tile": header + b"".join(tiles[:-1]) + b"\xff\xd9",
        "tile-outside-grid": header + tiles[0][:4] + b"\0\x09" + tiles[0][6:] +
                             b"".join(tiles[1:]) + b"\xff\xd9",
        "tile-extent-too-large": header + tiles[0][:6] + struct.pack(">I", len(tiles[0]) + 3) +
                                 tiles[0][10:] + b"".join(tiles[1:]) + b"\xff\xd9",
        "unexpected-tile-part": header + tiles[0][:10] + b"\x01\x02" + tiles[0][12:] +
                                b"".join(tiles[1:]) + b"\xff\xd9",
        "wrong-jp2-shape": boxed[:44] + struct.pack(">I", 8) + boxed[48:],
        "wrong-jp2-precision": boxed[:58] + bytes((16,)) + boxed[59:],
        "jp2-trailing-box": boxed + b"\0\0\0\x08free",
        "jp2-wrong-rgb-colorspace": rgb[:color_offset] + struct.pack(">I", 18) + rgb[color_offset + 4:],
        "jp2-mixed-rgb-depth": rgb[:rgb_offset + 45] + bytes((15,)) + rgb[rgb_offset + 46:],
        "jp2-subsampled-rgb": rgb[:rgb_offset + 46] + bytes((2,)) + rgb[rgb_offset + 47:],
        "jp2-undeclared-opacity": without_definitions,
        "jp2-premultiplied-opacity": with_alpha[:opacity_offset + 2] + b"\0\x02" + with_alpha[opacity_offset + 4:],
        "jp2-component-opacity": with_alpha[:opacity_offset + 4] + b"\0\x01" + with_alpha[opacity_offset + 6:],
        "jp2-duplicate-channel": with_alpha[:opacity_offset] + b"\0\x02" + with_alpha[opacity_offset + 2:],
        "jp2-channel-outside-grid": with_alpha[:opacity_offset] + b"\0\x04" + with_alpha[opacity_offset + 2:],
        "jp2-channel-definition-count": with_alpha[:definitions_offset + 8] + b"\0\x03" +
                                       with_alpha[definitions_offset + 10:],
        "jp2-empty-resolution": append_jp2_header(boxed, jp2_box(b"res ", b"")),
        "jp2-duplicate-resolutions": append_jp2_header(boxed, jp2_box(b"res ", capture) * 2),
        "jp2-duplicate-capture": append_jp2_header(boxed, jp2_box(b"res ", capture * 2)),
        "jp2-zero-resolution-ratio": append_jp2_header(boxed, jp2_box(b"res ", capture[:10] + b"\0\0" + capture[12:])),
        "jp2-truncated-resolution": append_jp2_header(boxed, jp2_box(b"res ", capture[:-1])),
    }
    diagnostics = {
        "too-many-tiles": "grid exceeds 4096 tiles",
        "duplicate-tile": "tile-part sequence is incomplete, repeated or inconsistent",
        "tile-outside-grid": "tile ID is outside the grid",
        "unexpected-tile-part": "tile-part sequence is incomplete, repeated or inconsistent",
        "wrong-jp2-precision": "image header disagrees with the codestream",
        "signed-color": "signed samples require grayscale without alpha",
        "signed-jp2-disagreement": "image header disagrees with the codestream",
        "irreversible-implicit-scalar": "explicit scalar quantization",
        "irreversible-quantizer-count": "quantization count",
        "irreversible-zero-exponent": "quantization exponent",
        "zero-quality-layers": "1 to 16 quality layers",
        "above-quality-layer-limit": "1 to 16 quality layers",
        "unknown-progression": "progression order is outside Part 1",
    }
    for index, (data, offset) in enumerate(layered):
        name = f"missing-quality-layer-packets-{index}"
        invalid[name] = data[:offset+6] + b"\0\x04" + data[offset+8:]
        diagnostics[name] = "codec refused the codestream"
    assert explicit_grid is not None
    data, offset = explicit_grid
    invalid["short-precinct-table"] = data[:offset+9] + b"\x03" + data[offset+10:]
    invalid["zero-high-resolution-precinct-exponent"] = data[:offset+15] + b"\x20" + data[offset+16:]
    excessive = bytearray(data)
    for position in (8, 12, 24, 28):
        struct.pack_into(">I", excessive, position, 2048)
    excessive[offset+7] = 16
    excessive[offset+14:offset+17] = b"\0\x11\x11"
    invalid["excessive-precinct-packet-grid"] = excessive
    diagnostics.update({
        "short-precinct-table": "precinct table length",
        "zero-high-resolution-precinct-exponent": "precinct exponent must be positive",
        "excessive-precinct-packet-grid": "precinct grid exceeds 2097152 packets",
    })
    assert marked_stream is not None
    data, offset = marked_stream
    first_tile = data.index(b"\xff\x90", offset)
    sop = first_tile + 14
    eph = data.index(b"\xff\x92", sop + 6)
    assert data[sop:sop+6] == b"\xff\x91\0\x04\0\0"
    invalid["undeclared-sop"] = data[:offset+4] + b"\x04" + data[offset+5:]
    invalid["undeclared-eph"] = data[:offset+4] + b"\x02" + data[offset+5:]
    invalid["bad-sop-extent"] = data[:sop+3] + b"\x03" + data[sop+4:]
    invalid["bad-sop-sequence"] = data[:sop+5] + b"\x01" + data[sop+6:]
    invalid["missing-eph"] = data[:eph] + b"\0\0" + data[eph+2:]
    diagnostics.update({
        "undeclared-sop": "unexpected marker",
        "undeclared-eph": "unexpected marker",
        "bad-sop-extent": "SOP marker segment has an invalid extent",
        "bad-sop-sequence": "codec refused the codestream",
        "missing-eph": "codec refused the codestream",
    })
    assert packet_table_case is not None
    data, name = packet_table_case
    tile = data.index(b"\xff\x90")
    plt = tile + 12
    assert data[plt:plt+2] == b"\xff\x58" and data[plt+4] == 0
    table_end = plt + 2 + int.from_bytes(data[plt+2:plt+4], "big")
    assert data[table_end:table_end+2] == b"\xff\x93"
    table = data[plt+5:table_end]
    split_at = next(index + 1 for index, byte in enumerate(table) if byte < 128)
    assert len(table) > split_at
    def segment(index, contents):
        return b"\xff\x58" + struct.pack(">HB", len(contents) + 3, index) + contents
    split_table = segment(0, table[:split_at]) + segment(1, table[split_at:])
    new_extent = int.from_bytes(data[tile+6:tile+10], "big") + 5
    split_image = output / "split-plt.j2k"
    split_image.write_bytes(data[:tile+6] + struct.pack(">I", new_extent) + data[tile+10:plt] +
                            split_table + data[table_end:])
    split_npy = output / "split-plt.npy"
    run("split-plt", [*prefix, "--raw", split_image, split_npy])
    assert split_npy.read_bytes() == (output / (name + ".npy")).read_bytes()
    invalid["plt-marker-extent"] = data[:plt+2] + b"\xff\xff" + data[plt+4:]
    invalid["plt-marker-index"] = data[:plt+4] + b"\x01" + data[plt+5:]
    last = table_end - 1
    assert data[last] < 128
    invalid["plt-unterminated-length"] = data[:last] + bytes([data[last] | 128]) + data[last+1:]
    bumped = (data[last] + 1) if data[last] < 127 else data[last] - 1
    invalid["plt-wrong-total"] = data[:last] + bytes([bumped]) + data[last+1:]
    diagnostics.update({
        "plt-marker-extent": "PLT segment extent",
        "plt-marker-index": "PLT segment indices must be consecutive",
        "plt-unterminated-length": "PLT lengths must cover",
        "plt-wrong-total": "PLT lengths must cover",
    })
    assert [p[10] for p in parts[:3]] == [0, 1, 2]
    invalid["missing-declared-tile-part"] = main_header + b"".join(parts[:2] + parts[3:]) + b"\xff\xd9"
    shortened = [p[:11] + b"\x02" + p[12:] for p in parts[:2]]
    invalid["missing-packets-at-declared-final-part"] = main_header + b"".join(shortened + parts[3:]) + b"\xff\xd9"
    inconsistent = parts[1][:11] + b"\x02" + parts[1][12:]
    invalid["changed-tile-part-count"] = main_header + b"".join([parts[0], inconsistent, *parts[2:]]) + b"\xff\xd9"
    diagnostics.update({
        "missing-declared-tile-part": "complete tile-part grid",
        "missing-packets-at-declared-final-part": "codec refused the codestream",
        "changed-tile-part-count": "tile-part sequence is incomplete, repeated or inconsistent",
    })
    for name, data in invalid.items():
        image = output / (name + ".jp2")
        image.write_bytes(data)
        raw = output / (name + ".npy")
        run(name, [*prefix, "--raw", image, raw], refusal=diagnostics.get(name, True))
        assert not raw.exists() and image.read_bytes() == data
    for name, mode, options in (("rgb", "RGB", {}), ("irreversible-color", "RGB", {"irreversible": True})):
        image = output / (name + ".j2k")
        if options:
            image = image.with_suffix(".jp2")
        Image.new(mode, (32, 32)).save(image, **options)
        raw = output / (name + ".npy")
        run(name, [*prefix, "--raw", image, raw], refusal=True)
        assert not raw.exists()
    for name, args in (("other-channel", ["--channel", 1]), ("other-plane", ["--z", 1]),
                       ("outside", ["--x", -1, "--width", 2])):
        raw = output / (name + ".npy")
        run(name, [*prefix, "--raw", output / "u16-13x9-jp2.jp2", raw, *args], refusal=True)
        assert not raw.exists()

    assert all(sha(path) == digest for path, digest in artifacts.items())
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.jpeg2000-plugin-validation/1", "passed": True,
        "openjpeg_version": features.version_codec("jpg_2000"),
        "module_sha256": artifacts[module], "manifest_sha256": artifacts[manifest],
        "region_sha256": artifacts[region], "positive": cases, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(cases)} producer profiles and {len(checks)} native CLI checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("region", "manifest", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--ffmpeg", type=Path, required=True, help="Development fixture producer")
    parser.add_argument("--openjpeg-bin", type=Path, required=True, help="OpenJPEG 2.5.4 fixture tools")
    args = parser.parse_args()
    verify(args.region.resolve(), args.manifest.resolve(), args.output.resolve(), args.ffmpeg.resolve(),
           args.openjpeg_bin.resolve())
