"""Exercise the installed Windows MetaImage module using authored binary fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zlib

import numpy as np


PROVIDER = "org.qupathxx.metaimage"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("region", "probe", "manifest", "output", "baseline"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--producer-inputs", type=Path, required=True)
    parser.add_argument("--compressed-producer-inputs", type=Path, required=True)
    parser.add_argument("--vector-inputs", type=Path, required=True)
    parser.add_argument("--compressed-vector-inputs", type=Path, required=True)
    for label in ("wide", "wide-compressed", "wide-vector", "wide-vector-compressed"):
        parser.add_argument("--" + label + "-inputs", type=Path, required=True)
    parser.add_argument("--slice-list-inputs", type=Path, required=True)
    parser.add_argument("--compressed-slice-list-inputs", type=Path, required=True)
    parser.add_argument("--numbered-inputs", type=Path, required=True)
    parser.add_argument("--compressed-numbered-inputs", type=Path, required=True)
    parser.add_argument("--spaced-inputs", type=Path, required=True)
    parser.add_argument("--compressed-spaced-inputs", type=Path, required=True)
    args = parser.parse_args()
    manifest = args.manifest.resolve()
    entry, = json.loads(manifest.read_bytes())["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    hashes = (digest(module), digest(manifest))
    assert entry["provider_id"] == PROVIDER and entry["module"]["sha256"] == hashes[0]
    root = args.output.resolve()
    root.mkdir()
    inputs = root / "inputs with spaces"
    inputs.mkdir()
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", PROVIDER]
    rows = {"raw": [], "display": [], "refusals": [], "project": []}
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))

    def run(command, code=0):
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=20)
        assert result.returncode == code, (command, result.returncode, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def source(name, element="USHORT", dimensions="3 2", fields="", payload=b"",
               locator="LOCAL", order="BinaryDataByteOrderMSB = False\n", binary="True"):
        path = inputs / name
        rank = len(dimensions.split())
        header = (f"ObjectType = Image\nNDims = {rank}\nDimSize = {dimensions}\n"
                  f"BinaryData = {binary}\n{order}{fields}ElementType = MET_{element}\n"
                  f"ElementDataFile = {locator}\n")
        path.write_bytes(header.encode("ascii") + payload)
        return path

    def raw(path, expected, z=0, spacing=(None, None, None), companion=None, label=None,
            shape=(2, 3), components=1, first=0, count=1, order=None):
        label = label or path.stem
        target = root / (label + ".npy")
        stdout, _ = run(command + ["--raw", path, target, "--z", str(z),
                                   "--channel", str(first), "--channels", str(count)])
        report = json.loads(stdout)
        samples = np.load(target, allow_pickle=False)
        assert samples.dtype == expected.dtype and samples.tobytes() == expected.tobytes()
        assert samples.shape == (*shape, count) and report["valid_pixels"] == np.prod(shape) and report["z"] == z
        snapshot = report["reader_snapshot"]
        meta = snapshot["raw"]
        names = ["Scalar"] if components == 1 else [f"Component {i+1}" for i in range(components)]
        assert meta["channel_names"] == names
        if order:
            assert meta["dimension_order"] == order
        assert tuple(meta[key] for key in ("pixel_width_um", "pixel_height_um", "z_spacing_um")) == spacing
        observed = snapshot["observed"]
        assert observed["scope"] == ("compound_files" if companion else "single_file")
        if companion:
            companions = (companion,) if isinstance(companion, Path) else companion
            assert len(observed["companions"]) == len(companions)
            assert all(any(Path(item["path"]).samefile(path) for item in observed["companions"])
                       for path in companions)
        assert snapshot["implementation"]["module_sha256"] == hashes[0]
        rows["raw"].append(label)

    raw(args.baseline.resolve(), np.arange(6, 12, dtype="<u2"), 1, (0.5, 1.5, 3.0))
    integer_cases = (("CHAR", "i1"), ("UCHAR", "u1"), ("SHORT", "i2"),
                     ("USHORT", "u2"), ("INT", "i4"), ("UINT", "u4"))
    for element, dtype in integer_cases:
        info = np.iinfo(dtype)
        original = np.array([info.min, info.max, 0, 1, 4, 9], dtype="<" + dtype)
        for endian, msb in (("<", "False"), (">", "True")):
            path = source(f"{element}-{msb}.mha", element,
                          payload=original.astype(endian + dtype).tobytes(),
                          order=f"BinaryDataByteOrderMSB = {msb}\n")
            raw(path, original)

    for element, dtype, bits in (
        ("FLOAT", "f4", [0xbf800000, 0x80000000, 0x3f800000, 0x7f800000, 0x7f800001, 0x3f000000]),
        ("DOUBLE", "f8", [0xbff0000000000000, 0x8000000000000000, 0x3ff0000000000000,
                          0x7ff0000000000000, 0x7ff0000000000001, 0x3fe0000000000000]),
    ):
        unsigned = "u" + dtype[1:]
        for endian, msb in (("<", "False"), (">", "True")):
            original = np.array(bits, dtype="<" + unsigned).view("<" + dtype)
            path = source(f"{element}-{msb}.mha", element,
                          payload=np.array(bits, dtype=endian + unsigned).tobytes(),
                          order=f"ElementByteOrderMSB = {msb}\n")
            raw(path, original)
            pam = root / (path.stem + ".pam")
            run(command + [path, "0", "0", "0", "3", "2", pam])
            assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == bytes([
                0, 0, 0, 255, 128, 128, 128, 255, 255, 255, 255, 255,
                0, 0, 0, 0, 0, 0, 0, 0, 191, 191, 191, 255,
            ])
            rows["display"].append(path.stem)

    payload = inputs / "study.raw"
    prefix = b"fixture preamble!"
    volume = np.arange(12, dtype="<u2")
    payload.write_bytes(prefix + volume.astype(">u2").tobytes())
    calibration = "ElementSize = 0.0005 0.0015 0.003\n"
    geometry = "TransformMatrix = 1 0 0 0 1 0 0 0 1\nOrigin = 0 0 0\n"
    for name, skip in (("study.mhd", -1), ("offset.mhd", len(prefix))):
        path = source(name, dimensions="3 2 2", locator=payload.name,
                      fields=f"HeaderSize = {skip}\n" + geometry + calibration,
                      order="BinaryDataByteOrderMSB = True\nElementByteOrderMSB = True\n")
        raw(path, volume[6:], 1, (0.5, 1.5, 3.0), payload)

    producer_hashes = {}
    for encoding, folder, extension in (("raw", args.producer_inputs, ".raw"),
                                         ("zlib", args.compressed_producer_inputs, ".zraw")):
        hashes_by_name = producer_hashes[encoding] = {}
        for rank in (2, 3):
            z = rank - 2
            for suffix in ("mha", "mhd"):
                path = folder.resolve() / f"producer-{rank}d.{suffix}"
                companion = path.with_suffix(extension) if suffix == "mhd" else None
                raw(path, volume[6*z:6*z+6], z, (0.5, 1.5, 3.0 if z else None),
                    companion, encoding + "-" + path.name)
                hashes_by_name[path.name] = digest(path)
                if companion:
                    hashes_by_name[companion.name] = digest(companion)

    vectors = volume[:, None] * 257 + np.array([0, 8192, 32768, 60000], dtype="<u2")
    for encoding, folder, extension in (("raw-vector", args.vector_inputs, ".raw"),
                                         ("zlib-vector", args.compressed_vector_inputs, ".zraw")):
        hashes_by_name = producer_hashes[encoding] = {}
        for rank in (2, 3):
            z = rank - 2
            expected = vectors[6*z:6*z+6]
            spacing = (0.5, 1.5, 3.0 if z else None)
            for suffix in ("mha", "mhd"):
                path = folder.resolve() / f"producer-{rank}d.{suffix}"
                companion = path.with_suffix(extension) if suffix == "mhd" else None
                for first, count in ((0, 4), (1, 2), (3, 1)):
                    raw(path, expected[:, first:first+count], z, spacing, companion,
                        f"{encoding}-{path.name}-c{first}-{count}", components=4, first=first, count=count,
                        order="CXY" if rank == 2 else "CXYZ")
                hashes_by_name[path.name] = digest(path)
                if companion:
                    hashes_by_name[companion.name] = digest(companion)

    preview_source = args.vector_inputs.resolve() / "producer-2d.mha"
    preview = root / "vector-first-component.pam"
    run(command + [preview_source, "0", "0", "0", "3", "2", preview])
    gray = (vectors[:6, 0].astype(float)*255/65535 + 0.5).astype(np.uint8)
    rgba = np.column_stack((gray, gray, gray, np.full(6, 255, dtype=np.uint8)))
    assert preview.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes()
    rows["display"].append(preview.stem)
    for first, count in ((4, 1), (3, 2)):
        name = f"channel-range-{first}-{count}"
        target = root / (name + ".npy")
        stdout, stderr = run(command + ["--raw", preview_source, target, "--channel", str(first),
                                        "--channels", str(count)], 2)
        assert "channel" in (stdout + stderr).lower() and not target.exists()
        rows["refusals"].append(name)

    wide_values = {
        "i64": np.array([-(1 << 63), -(1 << 53) - 1, -1, 0, 1, (1 << 53) + 1, (1 << 63) - 1], dtype="<i8"),
        "u64": np.array([0, 1, (1 << 53) + 1, (1 << 63) - 1, 1 << 63, (1 << 64) - 2, (1 << 64) - 1], dtype="<u8"),
    }
    for label in ("wide", "wide_compressed", "wide_vector", "wide_vector_compressed"):
        folder = getattr(args, label + "_inputs").resolve()
        components = 4 if "vector" in label else 1
        extension = ".zraw" if "compressed" in label else ".raw"
        hashes_by_name = producer_hashes[label] = {}
        for kind, pattern in wide_values.items():
            for rank in (2, 3):
                z = rank - 2
                samples = np.resize(pattern, (6 * (1 + z), components))[6*z:]
                spacing = (0.5, 1.5, 3.0 if z else None)
                for suffix in ("mha", "mhd"):
                    path = folder / f"{kind}-{rank}d.{suffix}"
                    companion = path.with_suffix(extension) if suffix == "mhd" else None
                    ranges = ((0, 4), (1, 2)) if components > 1 else ((0, 1),)
                    for first, count in ranges:
                        raw(path, samples[:, first:first+count], z, spacing, companion,
                            f"{label}-{path.name}-c{first}-{count}", components=components, first=first, count=count)
                    hashes_by_name[path.name] = digest(path)
                    if companion:
                        hashes_by_name[companion.name] = digest(companion)

    for kind, element in (("i64", "LONG_LONG"), ("u64", "ULONG_LONG")):
        expected = wide_values[kind][[0, 6, 2, 3, 4, 5]]
        encoded = expected.astype(">i8" if kind == "i64" else ">u8").tobytes()
        path = source(f"{kind}-big-endian.mha", element, payload=encoded,
                      order="BinaryDataByteOrderMSB = True\n")
        raw(path, expected)
        preview = root / f"{kind}-display.pam"
        run(command + [path, "0", "0", "0", "3", "2", preview])
        pixels = preview.read_bytes().split(b"ENDHDR\n", 1)[1]
        assert pixels[:8] == bytes((0, 0, 0, 255, 255, 255, 255, 255))
        rows["display"].append(preview.stem)

    multi = np.random.default_rng(929).integers(0, 65536, size=(193, 257), dtype=np.uint16)
    encoded = zlib.compress(multi.astype(">u2").tobytes())
    assert len(encoded) > 65536
    path = source("compressed-multi.mha", dimensions="257 193", fields="CompressedData = True\n",
                  order="ElementByteOrderMSB = True\n", payload=encoded)
    raw(path, multi, shape=multi.shape)

    packed_project = inputs / "compressed project"
    packed_project.mkdir()
    packed_payload = packed_project / "study.raw"
    encoded = zlib.compress(volume.astype(">u2").tobytes())
    packed_payload.write_bytes(prefix + encoded)
    fields = (f"CompressedData = True\nCompressedDataSize = {len(encoded)}\n" +
              "HeaderSize = -1\n" + geometry + calibration)
    source("compressed-study.mhd", dimensions="3 2 2", locator=packed_payload.name, fields=fields,
           order="BinaryDataByteOrderMSB = True\n").replace(packed_project / "study.mhd")
    raw(packed_project / "study.mhd", volume[6:], 1, (0.5, 1.5, 3.0), packed_payload, "compressed-study")
    vector_project = inputs / "vector project"
    vector_project.mkdir()
    vector_payload = vector_project / "study.raw"
    vector_encoded = zlib.compress(vectors.astype(">u2").tobytes())
    vector_payload.write_bytes(prefix + vector_encoded)
    vector_fields = (f"CompressedData = True\nCompressedDataSize = {len(vector_encoded)}\n" +
                     "ElementNumberOfChannels = 4\nHeaderSize = -1\n" + geometry + calibration)
    source("vector-study.mhd", dimensions="3 2 2", locator=vector_payload.name, fields=vector_fields,
           order="BinaryDataByteOrderMSB = True\n").replace(vector_project / "study.mhd")
    raw(vector_project / "study.mhd", vectors[6:, 1:3], 1, (0.5, 1.5, 3.0), vector_payload,
        "compressed-big-endian-vector", components=4, first=1, count=2, order="CXYZ")
    wide_project = inputs / "wide vector project"
    wide_project.mkdir()
    wide_payload = wide_project / "study.raw"
    wide_vectors = np.resize(wide_values["u64"], (12, 4))
    wide_encoded = zlib.compress(wide_vectors.astype(">u8").tobytes())
    wide_payload.write_bytes(prefix + wide_encoded)
    wide_fields = (f"CompressedData = True\nCompressedDataSize = {len(wide_encoded)}\n" +
                   "ElementNumberOfChannels = 4\nHeaderSize = -1\n" + geometry + calibration)
    source("wide-study.mhd", element="ULONG_LONG", dimensions="3 2 2", locator=wide_payload.name,
           fields=wide_fields, order="BinaryDataByteOrderMSB = True\n").replace(wide_project / "study.mhd")
    raw(wide_project / "study.mhd", wide_vectors[6:, 1:3], 1, (0.5, 1.5, 3.0), wide_payload,
        "compressed-big-endian-wide-vector", components=4, first=1, count=2, order="CXYZ")

    slice_projects = []
    slice_cases = (
        ("slice-list", args.slice_list_inputs, args.compressed_slice_list_inputs, ("tail", "head"), "--slice-list"),
        ("numbered", args.numbered_inputs, args.compressed_numbered_inputs, ("slice-03", "slice002"), "--numbered-slices"),
        ("spaced-list", args.spaced_inputs / "listed", args.compressed_spaced_inputs / "listed",
         ("last section", "first section"), "--spaced-list"),
        ("spaced-numbered", args.spaced_inputs / "numbered", args.compressed_spaced_inputs / "numbered",
         ("scan 002 raw", "scan 005 raw"), "--spaced-numbered"),
    )
    for label, folder, extension, stems, flag in (
        (prefix + name, directory, extension, stems, flag)
        for name, plain, packed, stems, flag in slice_cases
        for prefix, directory, extension in (("", plain, ".raw"), ("zlib-", packed, ".zraw"))
    ):
        work = inputs / label
        work.mkdir()
        names = ("study.mhd", *(stem + extension for stem in stems))
        for name in names:
            shutil.copyfile(folder / name, work / name)
        producer_hashes[label] = {name: digest(folder / name) for name in names}
        companions = tuple(work / name for name in names[1:])
        for z in (0, 1):
            raw(work / names[0], volume[z*6:z*6+6], z, (0.5, 1.5, 3.0), companions, f"{label}-z{z}")
        slice_projects.append((work, companions[1], [flag]))

    for label, folder, extension in (("spaced-detached", args.spaced_inputs, ".raw"),
                                      ("zlib-spaced-detached", args.compressed_spaced_inputs, ".zraw")):
        work = inputs / label
        work.mkdir()
        names = ("study data.mhd", "study data" + extension)
        for name in names:
            shutil.copyfile(folder / "detached" / name, work / name)
        producer_hashes[label] = {name: digest(folder / "detached" / name) for name in names}
        raw(work / names[0], volume[6:], 1, (0.5, 1.5, 3.0), work / names[1], label)

    list_text = (args.slice_list_inputs / "study.mhd").read_text(encoding="ascii")
    list_header = list_text.split("ElementDataFile = ", 1)[0]
    raw_tail = inputs / "tail.raw"
    raw_head = inputs / "head.raw"
    raw_tail.write_bytes(b"preamble" + volume[:6].astype(">u2").tobytes())
    raw_head.write_bytes(b"another-preamble" + volume[6:].astype(">u2").tobytes())
    bare = inputs / "bare-list.mhd"
    bare.write_text(list_header.replace("ByteOrderMSB = False", "ByteOrderMSB = True") +
                    "HeaderSize = -1\nElementDataFile = LIST\ntail.raw\nhead.raw", encoding="ascii")
    raw(bare, volume[6:], 1, (0.5, 1.5, 3.0), (raw_tail, raw_head))

    unpadded = inputs / "unpadded-numbered.mhd"
    first_slice = inputs / "frame9.raw"
    last_slice = inputs / "frame10.raw"
    first_slice.write_bytes(volume[:6].astype("<u2").tobytes())
    last_slice.write_bytes(volume[6:].astype("<u2").tobytes())
    unpadded.write_text(list_header + "ElementDataFile = frame%d.raw 9 10", encoding="ascii")
    raw(unpadded, volume[6:], 1, (0.5, 1.5, 3.0), (first_slice, last_slice))

    for name, locator, message in (
        ("numbered-missing-range", "slice%d.raw", "decimal pattern and range"),
        ("numbered-missing-end", "slice%d.raw 1", "start and end indices"),
        ("numbered-negative-step", "slice%d.raw 1 2 -1", "positive step"),
        ("numbered-zero-step", "slice%d.raw 1 2 0", "positive step"),
        ("numbered-descending", "slice%d.raw 2 1", "ascending range"),
        ("numbered-too-many", "slice%d.raw 1 3", "does not match Z size"),
        ("numbered-too-few", "slice%d.raw 1 1", "does not match Z size"),
        ("numbered-index-overflow", "slice%d.raw 2147483648 2147483649", "Invalid MetaImage number"),
        ("numbered-width", "slice%012d.raw 1 2", "width exceeds"),
        ("numbered-wrong-type", "slice%x.raw 1 2", "decimal pattern"),
        ("numbered-two-conversions", "slice%d-%d.raw 1 2", "same-directory"),
        ("numbered-escape", "other/slice%d.raw 1 2", "same-directory"),
        ("numbered-missing-file", "frame%d.raw 10 11", "companion is missing"),
        ("numbered-trailing-entries", "frame%d.raw 9 10\nextra.raw", "trailing entries"),
        ("numbered-spaced-without-step", "scan %d.raw 2 3", "decimal pattern"),
        ("numbered-device-with-space", "COM%d .raw 1 2 1", "reserved device"),
        ("numbered-alternate-stream", "frame %d.raw:notes 1 2 1", "same-directory"),
    ):
        primary = inputs / f"{name}.mhd"
        primary.write_text(list_header + "ElementDataFile = " + locator + "\n", encoding="ascii")
        target = root / f"{name}.npy"
        stdout, stderr = run(command + ["--raw", primary, target], 2)
        assert message in stdout + stderr and not target.exists(), (name, stdout, stderr)
        rows["refusals"].append(name)

    for name, text, message in (
        ("list-short", list_header + "ElementDataFile = LIST\ntail.raw\n", "fewer names than slices"),
        ("list-long", list_text + "extra.raw\n", "trailing entries"),
        ("list-duplicate", list_text.replace("head.raw", "TAIL.raw"), "duplicate slice name"),
        ("list-escape", list_text.replace("head.raw", "../head.raw"), "same-directory"),
        ("list-missing", list_text.replace("head.raw", "absent.raw"), "companion is missing"),
        ("list-63-limit", list_text.replace("DimSize = 3 2 2", "DimSize = 3 2 64"), "at most 63 slices"),
        ("list-truncated-payload", list_text.replace("tail.raw", "plain-tail.raw").replace("head.raw", "short.raw"),
         "wrong extent"),
        ("list-hard-link", list_text.replace("head.raw", "alias-slice.raw"), "duplicate or unordered"),
    ):
        assert text != list_text
        if name == "list-hard-link":
            os.link(raw_tail, inputs / "alias-slice.raw")
        if name == "list-truncated-payload":
            (inputs / "plain-tail.raw").write_bytes(volume[:6].astype("<u2").tobytes())
            (inputs / "short.raw").write_bytes(bytes(11))
        primary = inputs / (name + ".mhd")
        primary.write_text(text, encoding="ascii")
        target = root / (name + ".npy")
        before = digest(primary)
        stdout, stderr = run(command + ["--raw", primary, target], 2)
        assert message in stdout + stderr and not target.exists() and digest(primary) == before, (name, stdout, stderr)
        rows["refusals"].append(name)

    packed_list = args.compressed_slice_list_inputs
    packed_list_text = (packed_list / "study.mhd").read_text(encoding="ascii")
    shutil.copyfile(packed_list / "tail.zraw", inputs / "tail.zraw")
    (inputs / "head.zraw").write_bytes((packed_list / "head.zraw").read_bytes() + b"\0")
    primary = inputs / "list-zlib-trailing.mhd"
    primary.write_text(packed_list_text, encoding="ascii")
    target = root / "list-zlib-trailing.npy"
    stdout, stderr = run(command + ["--raw", primary, target], 2)
    assert "trailing" in stdout + stderr and not target.exists(), (stdout, stderr)
    rows["refusals"].append("list-zlib-trailing")

    for name, fields, order, locator, data, message in (
        ("missing-order", "", "", "LOCAL", bytes(12), "explicit byte order"),
        ("order-conflict", "", "BinaryDataByteOrderMSB = False\nElementByteOrderMSB = True\n",
         "LOCAL", bytes(12), "fields conflict"),
        ("rotation", "TransformMatrix = 0 1 1 0\n", "", "LOCAL", bytes(12), "transform must be identity"),
        ("origin", "Position = 1 0\n", "", "LOCAL", bytes(12), "must be zero"),
        ("ascii", "", "", "LOCAL", bytes(12), "requires a binary image"),
        ("short-vector", "ElementNumberOfChannels = 3\n", "", "LOCAL", bytes(12), "wrong extent"),
        ("zero-channels", "ElementNumberOfChannels = 0\n", "", "LOCAL", b"", "Invalid MetaImage channel count"),
        ("excess-channels", "ElementNumberOfChannels = 65537\n", "", "LOCAL", b"", "Invalid MetaImage channel count"),
        ("short", "", "", "LOCAL", bytes(11), "wrong extent"),
        ("trailing", "", "", "LOCAL", bytes(13), "wrong extent"),
        ("escape", "", "", "../study.raw", b"", "same-directory"),
        ("device", "", "", "NUL.raw", b"", "reserved device"),
        ("device-with-space", "", "", "NUL .raw", b"", "reserved device"),
        ("list", "", "", "LIST", b"", "LIST requires XYZ"),
        ("unproved-orientation", "AnatomicalOrientation = ??\n", "", "LOCAL", bytes(12),
         "anatomical orientation requires"),
        ("conflicting-orientation", "TransformMatrix = 1 0 0 1\nAnatomicalOrientation = RA\n",
         "", "LOCAL", bytes(12), "anatomical orientation requires"),
        ("unknown", "CustomField = 1\n", "", "LOCAL", bytes(12), "unsupported field"),
        ("duplicate", "NDims = 2\n", "", "LOCAL", bytes(12), "duplicate field"),
        ("oversized-header", "#" + "A" * 65536 + "\n", "", "LOCAL", bytes(12), "past 64 KiB"),
        ("encoded-size-with-raw", "CompressedDataSize = 12\n", "", "LOCAL", bytes(12), "Invalid MetaImage CompressedDataSize"),
        ("unproved-compressed-tail", "CompressedData = True\nHeaderSize = -1\n", "",
         payload.name, b"", "requires CompressedDataSize"),
    ):
        if not order and name != "missing-order":
            order = "BinaryDataByteOrderMSB = False\n"
        path = source(name + ".mha", fields=fields, order=order, locator=locator, payload=data,
                      binary="False" if name == "ascii" else "True")
        target = root / f"{name}.npy"
        before = digest(path)
        stdout, stderr = run(command + ["--raw", path, target], 2)
        assert message in stdout + stderr and not target.exists() and digest(path) == before, (name, stdout, stderr)
        rows["refusals"].append(name)

    alias = source("alias.mhd", locator="alias.raw")
    os.link(alias, inputs / "alias.raw")
    stdout, stderr = run(command + ["--raw", alias, root / "alias.npy"], 2)
    assert "distinct regular file" in stdout + stderr and not (root / "alias.npy").exists()
    rows["refusals"].append("primary-hard-link")

    encoded = zlib.compress(bytes(12))
    for name, data, size, message in (
        ("compressed-extent", encoded, len(encoded) + 1, "wrong extent"),
        ("compressed-zero-size", encoded, 0, "Invalid MetaImage CompressedDataSize"),
        ("compressed-short", encoded[:-1], None, "corrupt or truncated"),
        ("compressed-checksum", encoded[:-1] + bytes([encoded[-1] ^ 1]), None, "corrupt or truncated"),
        ("compressed-trailing", encoded + bytes(1), None, "trailing data"),
        ("compressed-concatenated", encoded + encoded, None, "trailing data"),
        ("compressed-small-raster", zlib.compress(bytes(11)), None, "wrong decoded extent"),
        ("compressed-large-raster", zlib.compress(bytes(13)), None, "exceeds the declared size"),
        ("compressed-gzip-wrapper", zlib.compress(bytes(12), wbits=31), None, "corrupt or truncated"),
    ):
        fields = "CompressedData = True\n"
        if size is not None:
            fields += f"CompressedDataSize = {size}\n"
        path = source(name + ".mha", fields=fields, payload=data)
        target = root / (name + ".npy")
        stdout, stderr = run(command + ["--raw", path, target], 2)
        assert message in stdout + stderr and not target.exists(), (name, stdout, stderr)
        rows["refusals"].append(name)

    probe = args.probe.resolve()
    for work, companion, flags in ((inputs, payload, []), (packed_project, packed_payload, []),
                                   (vector_project, vector_payload, ["--vector"]),
                                   (wide_project, wide_payload, ["--wide-vector"]), *slice_projects):
        for mode in ("create", "reopen", "changed"):
            if mode == "changed":
                replacement = companion.with_name("replacement.raw")
                replacement.write_bytes(companion.read_bytes())
                replacement.replace(companion)
            stdout, _ = run([probe, mode, manifest, work, *flags])
            rows["project"].append(f"{work.name}: {stdout.strip()}")
    assert hashes == (digest(module), digest(manifest))
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": hashes[0], "manifest_sha256": hashes[1],
        "producer_inputs_sha256": producer_hashes, **rows,
    }, indent=2) + "\n", encoding="utf-8")
    print({name: len(results) for name, results in rows.items()})


if __name__ == "__main__":
    main()
