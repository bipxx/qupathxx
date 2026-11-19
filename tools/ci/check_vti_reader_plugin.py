"""Check the installed VTI module against VTK output and exact authored controls."""
import argparse
import base64
import hashlib
import json
import lzma
import os
from pathlib import Path
import shutil
import struct
import subprocess
from xml.etree import ElementTree
import zlib

import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("region", "probe", "manifest", "producer", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    parser.add_argument("--compressed-producer", type=Path, required=True)
    parser.add_argument("--default-writer-file", type=Path, required=True)
    parser.add_argument("--raw-appended-producer", type=Path, required=True)
    parser.add_argument("--raw-appended-baseline", type=Path, required=True)
    parser.add_argument("--streamed-producer", type=Path, required=True)
    parser.add_argument("--shifted-producer", type=Path)
    for codec in ("lz4", "lzma"):
        for suffix in ("", "-raw", "-streamed"):
            parser.add_argument("--" + codec + suffix, type=Path, required=True)
    for case in ("scalars", "zlib", "lz4-raw", "lzma"):
        parser.add_argument("--component-" + case, type=Path, required=True)
    for case in ("plain", "zlib", "raw", "lzma"):
        parser.add_argument("--selection-" + case, type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir()
    manifest = args.manifest.resolve()
    manifest_hash = sha(manifest)
    config = json.loads(manifest.read_bytes())
    entry, = config["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    module_hash = sha(module)
    assert entry["module"]["sha256"] == module_hash
    rows = {"raw": [], "refusals": [], "display": [], "project": []}
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(str(Path(env["SYSTEMROOT"]) / suffix) for suffix in ("System32", ""))
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=root, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == code, (cmd, result.returncode, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def raw(source, expected, unit=None, z=0, spacing=(None, None, None), name="samples",
            shape=(2, 3), component_names=None, first=0, count=1, point_array=None):
        options = {}
        if unit:
            options["spacing_unit"] = unit
        if point_array:
            options["point_array"] = point_array
        cmd = command + (["--reader-options", json.dumps(options)] if options else [])
        selected = ("-" + point_array.replace(" ", "-") if point_array else "")
        if component_names:
            selected += f"-c{first}-{count}"
        target = root / (source.stem + f"-z{z}{selected}.npy")
        stdout, _ = run(cmd + ["--raw", source, target, "--z", str(z),
                               "--channel", str(first), "--channels", str(count)])
        report = json.loads(stdout)
        actual = np.load(target, allow_pickle=False)
        assert actual.shape == (*shape, count) and actual.dtype == expected.dtype
        assert actual.tobytes() == expected.tobytes() and report["valid_pixels"] == np.prod(shape) and report["z"] == z
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == module_hash
        assert snapshot["implementation"]["options"] == options
        assert snapshot["observed"]["scope"] == "single_file"
        meta = snapshot["raw"]
        assert meta["channel_names"] == (component_names or [name])
        if component_names:
            assert meta["dimension_order"] == "CXYZ"
        assert tuple(meta[key] for key in ("pixel_width_um", "pixel_height_um", "z_spacing_um")) == spacing
        rows["raw"].append(source.stem)

    producer_files = sorted(args.producer.resolve().glob("*.vti"))
    assert len(producer_files) == 6
    for source in producer_files:
        raw(source, np.arange(6, 12, dtype="<u2"), "mm", 1, (0.5, 1.5, 3), "Research samples")
    shifted = []
    if args.shifted_producer:
        shifted = sorted(args.shifted_producer.resolve().glob("*.vti"))
        assert len(shifted) == 6
        for source in shifted:
            local = root / f"shifted-{source.name}"
            shutil.copyfile(source, local)
            for z in (0, 1):
                raw(local, np.arange(6*z, 6*(z+1), dtype="<u2"), "mm", z, (250, 500, 2000),
                    "Research samples")

    pixels = np.arange(12, dtype="<u2")
    components = np.column_stack((pixels*257, pixels+8192, 60000-pixels*17))
    unnamed = [f"Research samples [{i}]" for i in (1, 2, 3)]
    named = ["Horizontal", unnamed[1], "Depth"]
    selections = {}
    selection_vectors = [f"Displacement [{i}]" for i in (1, 2, 3)]
    for label, count in (("plain", 6), ("zlib", 4), ("raw", 2), ("lzma", 4)):
        folder = getattr(args, "selection_" + label).resolve()
        files = sorted(folder.glob("*.vti"))
        assert len(files) == count
        for source in files:
            local = root / f"selection-{label}-{source.name}"
            shutil.copyfile(source, local)
            for name, values in (("Research samples", pixels[6:]), ("Auxiliary", pixels[6:] * 9 + 1000)):
                raw(local, values, "mm", 1, (0.5, 1.5, 3), name=name, point_array=name)
            raw(local, components[6:, 1:3], "mm", 1, (0.5, 1.5, 3), component_names=selection_vectors,
                first=1, count=2, point_array="Displacement")
            selections[local.name] = sha(source)
    component_inputs = {}
    for label, folder, expect_count, names in (
        ("plain", args.component_scalars, 6, unnamed), ("zlib", args.component_zlib, 4, named),
        ("lz4-raw", args.component_lz4_raw, 2, named), ("lzma", args.component_lzma, 4, unnamed),
    ):
        files = sorted(folder.resolve().glob("*.vti"))
        assert len(files) == expect_count
        for source in files:
            local = root / f"components-{label}-{source.name}"
            shutil.copyfile(source, local)
            for first, count in ((0, 3), (1, 2)):
                raw(local, components[6:, first:first+count], "mm", 1, (0.5, 1.5, 3),
                    component_names=names, first=first, count=count)
            component_inputs[local.name] = sha(source)
    component_preview = root / "component-preview.pam"
    vector_case = args.component_zlib.resolve() / "binary-BigEndian-UInt64.vti"
    run(command + ["--reader-options", '{"spacing_unit":"mm"}', vector_case, "0", "0", "0", "3", "2",
                   component_preview])
    expected_rgba = bytes(channel for pixel in range(6) for channel in (pixel, pixel, pixel, 255))
    assert component_preview.read_bytes().split(b"ENDHDR\n", 1)[1] == expected_rgba
    rows["display"].append(component_preview.stem)
    compressed_files = sorted(args.compressed_producer.resolve().glob("*.vti"))
    assert len(compressed_files) == 4
    for source in compressed_files:
        local = root / ("compressed-" + source.name)
        shutil.copyfile(source, local)
        raw(local, np.arange(6, 12, dtype="<u2"), "mm", 1, (0.5, 1.5, 3), "Research samples")
    raw(args.default_writer_file.resolve(), np.arange(6, 12, dtype="<u2"), z=1, name="Research samples")
    raw_appended = args.raw_appended_producer.resolve()
    raw_producers = {}
    for encoding in ("plain", "zlib"):
        baseline = args.raw_appended_baseline.resolve() / (encoding + ".vti")
        raw(baseline, np.arange(6, 12, dtype="<u2"), z=1, name="Research samples")
        raw_producers[f"baseline/{encoding}.vti"] = sha(baseline)
        cases = sorted((raw_appended / encoding).glob("*.vti"))
        assert len(cases) == 2
        for source in cases:
            local = root / f"raw-appended-{encoding}-{source.name}"
            shutil.copyfile(source, local)
            raw(local, np.arange(6, 12, dtype="<u2"), "mm", 1, (0.5, 1.5, 3), "Research samples")
            raw_producers[f"{encoding}/{source.name}"] = sha(source)
    streamed = args.streamed_producer.resolve()
    expected_streamed = np.load(streamed / "expected.npy", allow_pickle=False)
    assert expected_streamed.shape == (5, 2, 3)
    stream_files = sorted(streamed.glob("*.vti"))
    assert len(stream_files) == 4
    for original_stream in stream_files:
        source = root / ("streamed-" + original_stream.name)
        shutil.copyfile(original_stream, source)
        for z in range(5):
            raw(source, expected_streamed[z], z=z, name="RTData")
    codec_inputs = {}
    for codec in ("lz4", "lzma"):
        inputs = codec_inputs[codec] = {}
        for suffix, count in (("", 4), ("_raw", 2)):
            folder = getattr(args, codec + suffix).resolve()
            cases = sorted(folder.glob("*.vti"))
            assert len(cases) == count
            for source in cases:
                local = root / f"{codec}{suffix}-{source.name}"
                shutil.copyfile(source, local)
                raw(local, np.arange(6, 12, dtype="<u2"), "mm", 1, (0.5, 1.5, 3), "Research samples")
                inputs[local.name] = sha(source)
        streamed_folder = getattr(args, codec + "_streamed").resolve()
        codec_expected = np.load(streamed_folder / "expected.npy", allow_pickle=False)
        assert np.array_equal(codec_expected, expected_streamed)
        codec_stream_files = sorted(streamed_folder.glob("*.vti"))
        assert len(codec_stream_files) == 4
        for source in codec_stream_files:
            local = root / f"{codec}-streamed-{source.name}"
            shutil.copyfile(source, local)
            for z in range(5):
                raw(local, codec_expected[z], z=z, name="RTData")
            inputs[local.name] = sha(source)
    micron_source = root / "explicit-micrometres.vti"
    shutil.copyfile(producer_files[0], micron_source)
    raw(micron_source, np.arange(6, 12, dtype="<u2"), "um", 1, (0.0005, 0.0015, 0.003), "Research samples")

    def fixture(name, scalar, data, fmt="ascii", compression="", extent="0 2 0 1 0 0"):
        source = root / (name + ".vti")
        source.write_text(
            f'<VTKFile type="ImageData" version="0.1" byte_order="LittleEndian"{compression}>'
            f'<ImageData WholeExtent="{extent}" Origin="0 0 0" Spacing="1 1 1">'
            f'<Piece Extent="{extent}"><PointData Scalars="samples">'
            f'<DataArray type="{scalar}" Name="samples" format="{fmt}">{data}</DataArray>'
            '</PointData><CellData/></Piece></ImageData></VTKFile>', encoding="ascii")
        return source

    for scalar, dtype in (("Int8", "i1"), ("UInt8", "u1"), ("Int16", "i2"), ("UInt16", "u2"),
                          ("Int32", "i4"), ("UInt32", "u4"), ("Int64", "i8"), ("UInt64", "u8")):
        limits = np.iinfo(dtype)
        expected = np.array([limits.min, limits.max, 0, 1, 7, limits.max - 1], dtype="<" + dtype)
        source = fixture(scalar, scalar, " ".join(str(value) for value in expected))
        raw(source, expected)

    for scalar, dtype, bits in (
        ("Float32", "<u4", [0xbf800000, 0x80000000, 0x3f800000, 0x7f800000, 0x7f800001, 0x3f000000]),
        ("Float64", "<u8", [0xbff0000000000000, 0x8000000000000000, 0x3ff0000000000000,
                             0x7ff0000000000000, 0x7ff0000000000001, 0x3fe0000000000000]),
    ):
        expected = np.array(bits, dtype=dtype)
        data = expected.tobytes()
        encoded = base64.b64encode(struct.pack("<I", len(data)) + data).decode("ascii")
        source = fixture(scalar, scalar, encoded, "binary")
        raw(source, expected.view("<f" + dtype[-1]))
        pam = root / (scalar + ".pam")
        run(command + [source, "0", "0", "0", "3", "2", pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == bytes([
            0, 0, 0, 255, 128, 128, 128, 255, 255, 255, 255, 255,
            0, 0, 0, 0, 0, 0, 0, 0, 191, 191, 191, 255,
        ])
        rows["display"].append(scalar)
        ascii_source = fixture(scalar + "-ascii", scalar, "-1 -0 1 0.5 0.125 0.0009765625")
        raw(ascii_source, np.array([-1, -0., 1, 0.5, 0.125, 0.0009765625], dtype="<f" + dtype[-1]))

    def refuse(name, source, message, options=None):
        target = root / (name + ".npy")
        before = sha(source)
        flags = ["--reader-options", json.dumps(options)] if options else []
        stdout, stderr = run(command + flags + ["--raw", source, target], 2)
        assert message in stdout + stderr and not target.exists() and sha(source) == before, (name, stdout, stderr)
        rows["refusals"].append(name)

    # Replace just the first compressed block of an independently produced array.
    # Recompute its table length so each control reaches the codec.
    for codec in ("LZ4", "LZMA"):
        reference = ElementTree.parse(getattr(args, codec.lower()) / "binary-LittleEndian-UInt32.vti")
        encoded = reference.find(".//DataArray").text.strip()
        block_count, = struct.unpack("<I", base64.b64decode(encoded[:8])[:4])
        split = 4 * ((4 * (3 + block_count) + 2) // 3)
        words = list(struct.unpack("<" + "I" * (3 + block_count), base64.b64decode(encoded[:split])))
        payload = base64.b64decode(encoded[split:])
        block, rest = payload[:words[3]], payload[words[3]:]
        assert words[:3] == [3, 8, 0] and sum(words[3:]) == len(payload)
        diagnostic = f"Invalid or incorrectly sized VTI {codec} block"
        invalid = [("truncated-block", block[:-1], diagnostic), ("trailing-byte", block + b"\x00", diagnostic)]
        if codec == "LZ4":
            invalid.extend((("short-output", b"\x70" + bytes(7), diagnostic),
                            ("invalid-sequence", bytes(len(block)), diagnostic)))
        else:
            assert lzma.decompress(block) == np.arange(4, dtype="<u2").tobytes()
            index_bytes = 4 * (1 + int.from_bytes(block[-8:-4], "little"))
            crc_at = len(block) - 12 - index_bytes - 4
            corrupted = bytearray(block)
            corrupted[crc_at] ^= 1
            invalid.extend((
                ("short-output", lzma.compress(bytes(7), check=lzma.CHECK_CRC32), diagnostic),
                ("bad-checksum", corrupted, diagnostic),
                ("concatenated", block + block, diagnostic),
                ("unchecked", lzma.compress(bytes(8), check=lzma.CHECK_NONE), "requires an XZ CRC32 stream"),
            ))
        for case, bad, message in invalid:
            name = f"{codec.lower()}-{case}"
            words[3] = len(bad)
            table = base64.b64encode(struct.pack("<" + "I" * len(words), *words))
            data = (table + base64.b64encode(bad + rest)).decode()
            source = fixture(name, "UInt16", data, "binary", f' compressor="vtk{codec}DataCompressor"', "0 2 0 1 0 1")
            refuse(name, source, message)

    original = fixture("original", "UInt16", "0 1 2 3 4 5").read_text(encoding="ascii")
    for name, old, new, expected in (
        ("cell-data", "<CellData/>", "<CellData><DataArray/></CellData>", "only text"),
        ("direction", ' Origin=', ' Direction="0 1 0 1 0 0 0 0 1" Origin=', "direction must be identity"),
        ("origin", 'Origin="0 0 0"', 'Origin="1 0 0"', "first sample must be at physical zero"),
        ("extent", 'WholeExtent="0 2', 'WholeExtent="1 2', "first sample must be at physical zero"),
        ("reversed-extent", 'WholeExtent="0 2', 'WholeExtent="3 2', "extent is reversed"),
        ("wide-extent", 'WholeExtent="0 2', 'WholeExtent="-2147483648 2147483647', "exceeds reader limits"),
        ("partial-piece", 'Piece Extent="0 2', 'Piece Extent="0 1', "must equal the whole"),
        ("zero-components", 'type="UInt16"', 'type="UInt16" NumberOfComponents="0"', "Invalid VTI component count"),
        ("excess-components", 'type="UInt16"', 'type="UInt16" NumberOfComponents="65537"', "Invalid VTI component count"),
        ("out-of-range-component-name", 'type="UInt16"', 'type="UInt16" ComponentName1="other"', "Invalid VTI component name"),
        ("vector-with-one-component", 'Scalars="samples"', 'Vectors="samples"', "require three components"),
        ("short-components", 'type="UInt16"', 'type="UInt16" NumberOfComponents="2"', "Missing or oversized VTI number"),
        ("active", 'Scalars="samples"', 'Scalars="different"', "must match its active"),
        ("truncated", "0 1 2 3 4 5", "0 1 2 3 4", "Missing or oversized VTI number"),
        ("extra", "0 1 2 3 4 5", "0 1 2 3 4 5 6", "count differs from geometry"),
        ("overflow", "0 1 2 3 4 5", "0 1 2 3 4 65536", "Invalid VTI number"),
        ("xml-declaration", "<VTKFile", '<!DOCTYPE VTKFile><VTKFile', "Unsupported or oversized"),
        ("entity-reference", "0 1 2 3 4 5", "0 1 2 3 4 &#53;", "entity references"),
        ("compressor", "<VTKFile ", '<VTKFile compressor="unknown" ', "Unsupported VTI compressor"),
    ):
        assert old in original
        path = root / (name + ".vti")
        path.write_text(original.replace(old, new, 1), encoding="ascii")
        refuse(name, path, expected)
    refuse("missing-unit", producer_files[0], "requires explicit spacing_unit")
    for name, blob, diagnostic in (
        ("binary-count", struct.pack("<I", 10) + bytes(12), "byte count differs"),
        ("binary-extra", struct.pack("<I", 12) + bytes(13), "exceeds declared extent"),
        ("binary-short", struct.pack("<I", 12) + bytes(11), "byte count differs"),
    ):
        path = fixture(name, "UInt16", base64.b64encode(blob).decode("ascii"), "binary")
        refuse(name, path, diagnostic)
    path = fixture("bad-padding", "UInt16", "Rx=A", "binary")
    refuse("bad-padding", path, "Invalid VTI base64 padding")

    def packed(name, size, tail, blocks, sizes=None, extra=b"", count=None, extent="0 2 0 1 0 0"):
        sizes = sizes if sizes is not None else [len(block) for block in blocks]
        count = len(sizes) if count is None else count
        header = struct.pack("<" + "I" * (len(sizes) + 3), count, size, tail, *sizes)
        encoded = base64.b64encode(header) + base64.b64encode(b"".join(blocks) + extra)
        return fixture(name, "UInt16", encoded.decode("ascii"), "binary",
                       ' compressor="vtkZLibDataCompressor"', extent)

    member = zlib.compress(bytes(12))
    valid_path = packed("compressed-partial-only", 32768, 12, [member])
    raw(valid_path, np.zeros(6, dtype="<u2"))
    large = np.random.default_rng(152).integers(0, 65536, size=(193, 257), dtype="<u2")
    large_member = zlib.compress(large.tobytes())
    assert len(large_member) > 65536
    valid_path = packed("compressed-large-block", 131072, large.nbytes, [large_member], extent="0 256 0 192 0 0")
    raw(valid_path, large, shape=large.shape)
    refuse("bad-block-count", packed("bad-block-count", 12, 0, [member], count=65537), "block geometry")
    refuse("bad-block-geometry", packed("bad-block-geometry", 12, 12, [member]), "block geometry")
    refuse("bad-block-size", packed("bad-block-size", 12, 0, [member], [0]), "compressed block size")
    refuse("truncated-table", packed("truncated-table", 6, 0, [], [], count=2), "byte count differs")
    refuse("oversized-table-entry", packed("oversized-table-entry", 12, 0, [member], [1000]), "exceed the encoded payload")
    refuse("bad-adler", packed("bad-adler", 12, 0, [member[:-1] + b"\0"]), "incorrectly sized VTI zlib block")
    refuse("extra-member", packed("extra-member", 12, 0, [member + member]), "incorrectly sized VTI zlib block")
    refuse("short-member-output", packed("short-member-output", 12, 0, [zlib.compress(bytes(11))]), "incorrectly sized VTI zlib block")
    refuse("long-member-output", packed("long-member-output", 12, 0, [zlib.compress(bytes(13))]), "incorrectly sized VTI zlib block")
    refuse("unreferenced-compressed-bytes", packed("unreferenced-compressed-bytes", 12, 0, [member], extra=b"\0"),
           "exceeds declared extent")

    # The raw body includes strings that look like XML syntax and trailing
    # whitespace that is part of the declared raster rather than the XML tail.
    marker_bytes = b"</AppendedData>\x00\xff</VTKFile>\r\n "
    if len(marker_bytes) % 2:
        marker_bytes += b" "
    width = len(marker_bytes) // 2
    extent = f"0 {width - 1} 0 1 0 0"
    prefix = (f'<VTKFile type="ImageData" version="0.1" byte_order="LittleEndian">\n'
              f'<ImageData WholeExtent="{extent}" Origin="0 0 0" Spacing="1 1 1">'
              f'<Piece Extent="{extent}"><PointData Scalars="samples">'
              '<DataArray type="UInt8" Name="samples" format="appended" offset="0"/>'
              '</PointData></Piece></ImageData>\n  <AppendedData encoding="raw">\n   _').encode("ascii")
    suffix = b"\n  </AppendedData>\n</VTKFile>\n"
    size_word = struct.pack("<I", len(marker_bytes))
    source = root / "raw-xml-marker-samples.vti"
    source.write_bytes(prefix + size_word + marker_bytes + suffix)
    expected = np.frombuffer(marker_bytes, dtype=np.uint8)
    raw(source, expected, shape=(2, width))
    source = root / "raw-crlf-metadata.vti"
    source.write_bytes(prefix.replace(b"\n", b"\r\n") + size_word + marker_bytes +
                       suffix.replace(b"\n", b"\r\n"))
    raw(source, expected, shape=(2, width))
    for name, data, diagnostic in (
        ("raw-truncated", prefix + size_word + marker_bytes[:-1] + suffix, "payload is truncated"),
        ("raw-extra-whitespace", prefix + size_word + marker_bytes + b" " + suffix, "Extra bytes"),
        ("raw-short-header", prefix + size_word[:3] + suffix, "payload is truncated"),
        ("raw-wrong-count", prefix + struct.pack("<I", 1) + marker_bytes + suffix, "byte count differs"),
        ("raw-missing-marker", prefix[:-1] + b"X" + size_word + marker_bytes + suffix, "marker is missing"),
        ("raw-missing-close", prefix + size_word + marker_bytes + b"\n</VTKFile>\n", "section close is missing"),
        ("raw-trailing-content", prefix + size_word + marker_bytes + suffix + b"X", "root close is missing"),
        ("raw-wrong-offset", prefix.replace(b'offset="0"', b'offset="1"') + size_word + marker_bytes + suffix,
         "offset zero"),
    ):
        source = root / (name + ".vti")
        source.write_bytes(data)
        refuse(name, source, diagnostic)

    for filename, mutation, diagnostic in (
        ("raw-zlib-bad-checksum", lambda data: data[:-1] + bytes([data[-1] ^ 1]), "incorrectly sized VTI zlib block"),
        ("raw-zlib-truncated", lambda data: data[:-1], "block sizes exceed the encoded payload"),
    ):
        source = root / (filename + ".vti")
        compressed_word = zlib.compress(marker_bytes)
        header = struct.pack("<IIII", 1, 32768, len(marker_bytes), len(compressed_word))
        compressed_prefix = prefix.replace(b'<VTKFile ', b'<VTKFile compressor="vtkZLibDataCompressor" ', 1)
        source.write_bytes(compressed_prefix + header + mutation(compressed_word) + suffix)
        refuse(filename, source, diagnostic)

    def simple_piece(extent, values, name="Research samples", dtype="UInt16", offset=None):
        attributes = f'format="appended" offset="{offset}"' if offset is not None else 'format="ascii"'
        return (f'<Piece Extent="{extent}"><PointData Scalars="{name}">'
                f'<DataArray type="{dtype}" Name="{name}" {attributes}>{values}</DataArray>'
                '</PointData><CellData/></Piece>')

    left = simple_piece("0 1 0 1 0 1", "0 1 3 4 6 7 9 10")
    right = simple_piece("1 2 0 1 0 1", "1 2 4 5 7 8 10 11")

    def assembled_file(name, pieces, spacing="1 1 1", appended="", extent="0 2 0 1 0 1", origin="0 0 0"):
        path = root / (name + ".vti")
        path.write_text(
            '<VTKFile type="ImageData" version="0.1" byte_order="LittleEndian">\n'
            f'<ImageData WholeExtent="{extent}" Origin="{origin}" Spacing="{spacing}">'
            + "".join(pieces) + '</ImageData>' + appended + '</VTKFile>', encoding="ascii", newline="")
        return path

    assembled = assembled_file("assembled", [right, left], spacing="0.0005 0.0015 0.003")
    raw(assembled, np.arange(6, 12, dtype="<u2"), "mm", 1, (0.5, 1.5, 3), "Research samples")
    shifted_left = left.replace('Extent="0 1 0 1 0 1"', 'Extent="10 11 -5 -4 7 8"')
    shifted_right = right.replace('Extent="1 2 0 1 0 1"', 'Extent="11 12 -5 -4 7 8"')
    shifted_pieces = assembled_file("assembled-shifted", [shifted_right, shifted_left],
                                    spacing="0.25 0.5 2", extent="10 12 -5 -4 7 8", origin="-2.5 2.5 -14")
    raw(shifted_pieces, np.arange(6, 12, dtype="<u2"), "mm", 1, (250, 500, 2000), "Research samples")
    for label, pieces, message in (
        ("piece-overlap-conflict", [left, right.replace(">1 2", ">9 2")], "overlapping pieces disagree"),
        ("piece-hole", [left, left], "do not cover the whole extent"),
        ("piece-outside", [left, right.replace('Extent="1 2', 'Extent="1 3')], "outside the whole extent"),
        ("piece-sample-type", [left, right.replace("UInt16", "Int16")], "same active scalar name and type"),
        ("piece-scalar-name", [left, right.replace("Research samples", "Other")], "same active scalar name and type"),
        ("piece-limit", [left] * 65, "piece limit exceeded"),
    ):
        refuse(label, assembled_file(label, pieces), message)

    def tuple_piece(extent, indices):
        values = " ".join(map(str, components[indices, :].ravel()))
        return simple_piece(extent, values).replace('type="UInt16"', 'type="UInt16" NumberOfComponents="3"')

    left_tuples = tuple_piece("0 1 0 1 0 1", [0, 1, 3, 4, 6, 7, 9, 10])
    right_tuples = tuple_piece("1 2 0 1 0 1", [1, 2, 4, 5, 7, 8, 10, 11])
    raw(assembled_file("assembled-components", [right_tuples, left_tuples]), components[6:], z=1,
        component_names=unnamed, count=3)
    for label, mutated, message in (
        ("tuple-overlap-disagree", right_tuples.replace("8193", "8194", 1), "overlapping pieces disagree"),
        ("piece-component-count", right_tuples.replace('NumberOfComponents="3"', 'NumberOfComponents="2"'),
         "piece component descriptions differ"),
        ("piece-component-name", right_tuples.replace('NumberOfComponents="3"',
         'NumberOfComponents="3" ComponentName2="Depth"'), "piece component descriptions differ"),
    ):
        refuse(label, assembled_file(label, [left_tuples, mutated]), message)

    wrong_channel = root / "component-range.npy"
    _, error = run(command + ["--reader-options", '{"spacing_unit":"mm"}', "--raw", vector_case, wrong_channel,
                              "--channel", "2", "--channels", "2"], 2)
    assert "channel" in error.lower() and not wrong_channel.exists()
    rows["refusals"].append("component-range")

    multi = args.selection_plain.resolve() / "appended-BigEndian-UInt64.vti"
    refuse("ambiguous-point-array", multi, "specify point_array", {"spacing_unit": "mm"})
    refuse("missing-point-array", multi, "missing from a piece", {"spacing_unit": "mm", "point_array": "Missing"})
    refuse("mistyped-point-array", multi, "Expected JSON string", {"spacing_unit": "mm", "point_array": 4})
    selection_text = multi.read_text(encoding="ascii")
    array_options = {"spacing_unit": "mm", "point_array": "Research samples"}
    for label, replacement, message in (
        ("duplicate-point-array", ('Name="Auxiliary"', 'Name="Research samples"'), "duplicate VTI point array name"),
        ("duplicate-unselected-offset", ('offset="44"', 'offset="0"'), "offsets must be distinct"),
        ("selected-appended-extent", ('offset="44"', 'offset="40"'), "VTI"),
    ):
        old, new = replacement
        assert old in selection_text
        modified = root / f"{label}.vti"
        modified.write_text(selection_text.replace(old, new, 1), encoding="ascii", newline="")
        refuse(label, modified, message, array_options)

    default_active = root / "sole-active-with-inactive-arrays.vti"
    default_active.write_text(selection_text.replace(' Vectors="Displacement"', '', 1), encoding="ascii", newline="")
    raw(default_active, pixels[6:], "mm", 1, (0.5, 1.5, 3), name="Research samples")

    arrays = []
    combined = []
    for extent, indices in (("0 1 0 1 0 1", [0, 1, 3, 4, 6, 7, 9, 10]),
                            ("1 2 0 1 0 1", [1, 2, 4, 5, 7, 8, 10, 11])):
        positions = []
        for values in (indices, [9 * i + 1000 for i in indices]):
            positions.append(sum(map(len, arrays)))
            arrays.append(base64.b64encode(struct.pack("<I8H", 16, *values)).decode())
        extra = f'<DataArray type="UInt16" Name="Auxiliary" format="appended" offset="{positions[1]}"/>'
        combined.append(simple_piece(extent, "", offset=positions[0]).replace("</PointData>", extra + "</PointData>"))
    multi_piece = assembled_file("selected-appended-pieces", combined[::-1],
                                 appended='<AppendedData encoding="base64">_' + "".join(arrays) + "</AppendedData>")
    raw(multi_piece, pixels[6:] * 9 + 1000, z=1, name="Auxiliary", point_array="Auxiliary")

    # Appended offsets name positions in the original file, including CRLF.
    # Reverse the element order to ensure array offsets and placement stay linked.
    left_data = base64.b64encode(struct.pack("<I8H", 16, 0, 1, 3, 4, 6, 7, 9, 10)).decode("ascii")
    right_data = base64.b64encode(struct.pack("<I8H", 16, 1, 2, 4, 5, 7, 8, 10, 11)).decode("ascii")
    appended = f'<AppendedData encoding="base64">_{left_data}\r\n{right_data}</AppendedData>'
    stored_left = simple_piece("0 1 0 1 0 1", "", offset=0)
    stored_right = simple_piece("1 2 0 1 0 1", "", offset=len(left_data) + 2)
    source = assembled_file("piece-offset-order", [stored_right, stored_left], appended=appended)
    raw(source, np.arange(6, 12, dtype="<u2"), z=1, name="Research samples")
    for label, offset, diagnostic in (
        ("piece-offset-repeat", 0, "offsets must be distinct"),
        ("piece-offset-outside", len(appended), "offsets must be distinct"),
        ("piece-offset-overlap", len(left_data) - 4, "byte count differs"),
    ):
        bad = simple_piece("1 2 0 1 0 1", "", offset=offset)
        refuse(label, assembled_file(label, [stored_left, bad], appended=appended), diagnostic)

    project_catalog = root / "reader-plugins-mm.json"
    config["plugins"][0]["options"] = {"spacing_unit": "mm"}
    project_catalog.write_text(json.dumps(config), encoding="ascii")
    shutil.copyfile(module, root / module.name)
    for fixture_dir, encoding in ((args.producer, "base64"), (args.compressed_producer, "zlib"),
                                  (raw_appended / "zlib", "raw-appended-zlib"), (None, "pieces"),
                                  (args.lz4, "lz4"), (args.lz4_raw, "raw-appended-lz4"),
                                  (args.lzma, "lzma"), (args.lzma_raw, "raw-appended-lzma")):
        project = root / (encoding + ".qpxproj")
        source = fixture_dir.resolve() / "appended-BigEndian-UInt64.vti" if fixture_dir else assembled
        for mode, catalog in (("create", project_catalog), ("reopen", manifest)):
            stdout, _ = run([args.probe.resolve(), mode, catalog, project, source])
            rows["project"].append(encoding + ": " + stdout.strip())

    vector_project = root / "vector.qpxproj"
    source = args.component_lz4_raw.resolve() / "appended-BigEndian-UInt64.vti"
    for mode, catalog in (("create", project_catalog), ("reopen", manifest)):
        stdout, _ = run([args.probe.resolve(), mode, catalog, vector_project, source, "--vector"])
        rows["project"].append("vector: " + stdout.strip())

    selected_catalog = root / "selected-array-manifest.json"
    config["plugins"][0]["options"] = array_options
    selected_catalog.write_text(json.dumps(config), encoding="ascii")
    selected_project = root / "selected-array.qpxproj"
    for mode, catalog in (("create", selected_catalog), ("reopen", manifest)):
        stdout, _ = run([args.probe.resolve(), mode, catalog, selected_project, multi])
        rows["project"].append("selected array: " + stdout.strip())

    assert (sha(module), sha(manifest)) == (module_hash, manifest_hash)
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": module_hash, "manifest_sha256": manifest_hash,
        "producer_inputs_sha256": {
            "uncompressed": {source.name: sha(source) for source in producer_files},
            "shifted": {source.name: sha(source) for source in shifted},
            "compressed": {source.name: sha(source) for source in compressed_files},
            "default": sha(args.default_writer_file),
            "raw_appended": raw_producers,
            "streamed": {source.name: sha(source) for source in (*stream_files, streamed / "expected.npy")},
            **codec_inputs,
            "components": component_inputs,
            "point_arrays": selections,
        }, **rows,
    }, indent=2) + "\n", encoding="utf-8")
    print({kind: len(cases) for kind, cases in rows.items()})


if __name__ == "__main__":
    main()
