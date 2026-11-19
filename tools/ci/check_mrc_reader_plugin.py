#!/usr/bin/env python3
"""Author open MRC2014 fixtures and check the ABI, SDK and CLI consumers."""

import argparse
import bz2
from decimal import Decimal, localcontext
import gzip
import hashlib
import itertools
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess

import mrcfile
import numpy as np


# Canonical oracle arrays use Z,Y,X index order. Keep Float32 bit patterns in
# unsigned storage until the display oracle explicitly interprets a finite value.
TYPES = {
    "i8": (0, 3, "i1", "i1", (-128, 127), (-128, -127, -13, -1, 0, 1, 13, 64, 126, 127)),
    "i16": (1, 4, "i2", "i2", (-32768, 32767), (-32768, -257, -256, -1, 0, 1, 128, 256, 32766, 32767)),
    "u16": (6, 2, "u2", "u2", (0, 65535), (0, 1, 254, 255, 256, 257, 32767, 32768, 65534, 65535)),
    "f32": (2, 9, "u4", "f4", (-1, 1), (0, 0x80000000, 0x3f800000, 0xbf800000, 0x3e800000,
                                         0xbe800000, 0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678,
                                         0x7fa12345, 0xffa54321, 1, 0x80000001, 0x40000000, 0xc0000000)),
    "f16": (12, 9, "u2", "f2", (-1, 1), (0, 0x8000, 0x3c00, 0xbc00, 0x3800, 0xb800,
                                         0x7c00, 0xfc00, 0x7e42, 0xfe19, 0x7c01, 0xfc21,
                                         1, 0x8001, 0x03ff, 0x83ff, 0x0400, 0x7bff)),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def scalar_decimal(value, kind):
    if kind != "f32":
        return Decimal(value)
    exponent = (value >> 23) & 255
    fraction = value & 0x7fffff
    if exponent == 255:
        return None
    significand = fraction if exponent == 0 else (1 << 23) + fraction
    magnitude = Decimal(significand) * Decimal(2) ** (-149 if exponent == 0 else exponent - 150)
    return -magnitude if value >> 31 else magnitude


def display_bytes(array, kind, window):
    result = bytearray()
    with localcontext() as context:
        context.prec = 180
        low, high = map(float, window)
        for code in array[0].ravel():
            value = scalar_decimal(int(code), kind)
            if value is None:
                result.extend(b"\0\0\0\0")
                continue
            # The declared display projection uses binary64. Round after the
            # projection, so tiny signed Float32 samples near a byte tie agree
            # with the binary64 display contract while raw bytes stay exact.
            number = float(value)
            normalized = min(1., max(0., (number - low) / (high - low)))
            gray = math.floor(normalized * 255 + .5)
            result.extend((gray, gray, gray, 255))
    return bytes(result)


def codes(kind, shape):
    _, _, storage, _, _, values = TYPES[kind]
    length = int(np.prod(shape))
    # Use both a distinct varying sequence and nonperiodic Z-plane boundaries.
    # Distinct lengths on X, Y and Z make incorrect axis maps visible.
    return np.array([values[(index + index // 7) % len(values)] for index in range(length)],
                    dtype=storage).reshape(shape)


def encode(array, kind, axes, endian, cell, grid, ispg, *, version=20140, little_stamp=b"DD\0\0"):
    mode, _, storage, _, _, _ = TYPES[kind]
    stored = array.transpose(tuple(3 - axis for axis in reversed(axes)))
    header = bytearray(1024)
    struct.pack_into(endian + "4i", header, 0, *reversed(stored.shape), mode)
    struct.pack_into(endian + "3i", header, 28, *grid)
    struct.pack_into(endian + "6f", header, 40, *cell, 90, 90, 90)
    struct.pack_into(endian + "3i", header, 64, *axes)
    struct.pack_into(endian + "3f", header, 76, 1, 0, -1)  # Statistics intentionally unset.
    struct.pack_into(endian + "i", header, 88, ispg)
    struct.pack_into(endian + "i", header, 108, version)
    header[208:216] = b"MAP " + (little_stamp if endian == "<" else b"\x11\x11\0\0")
    struct.pack_into(endian + "f", header, 216, -1)
    return bytes(header) + stored.astype(endian + storage).tobytes(order="C")


def read_reference(path, array, kind, axes, endian, cell, grid, ispg):
    _, _, storage, dtype, _, _ = TYPES[kind]
    opener = mrcfile.open if path.suffix.lower() in {".gz", ".bz2"} else mrcfile.mrcfile.MrcFile
    with opener(path) as source:
        assert source.data.dtype.kind == np.dtype(dtype).kind
        assert source.data.dtype.itemsize == np.dtype(dtype).itemsize
        assert (int(source.header.mapc), int(source.header.mapr), int(source.header.maps)) == axes
        assert int(source.header.ispg) == ispg
        header_grid = tuple(int(getattr(source.header, axis)) for axis in ("mx", "my", "mz"))
        assert header_grid == grid
        assert [float(value) for value in source.header.cella.tolist()] == list(cell)
        assert source.voxel_size.tolist() == tuple(
            float(np.float32(length / samples)) for length, samples in zip(cell, grid)
        ), path.name
        stored = source.data.reshape(tuple(array.shape[3 - axis] for axis in reversed(axes)))
        # View as integer storage first. A float endian conversion must not
        # canonicalize a payload or quiet a signaling NaN.
        stored_bits = stored.view(np.dtype(endian + storage))
        stored_axes = tuple(reversed(axes))
        restored_bits = stored_bits.transpose(tuple(stored_axes.index(axis) for axis in (3, 2, 1)))
        assert restored_bits.astype(np.dtype(storage)).tobytes() == array.tobytes(), path.name


def write_fixtures(directory):
    assert mrcfile.__version__ == "1.5.4", "Use the admitted CCP-EM development oracle"
    directory.mkdir(parents=True, exist_ok=True)
    cases = {}
    negative = {}

    def add(name, kind, *, shape=(2, 3, 5), axes=(1, 2, 3), endian="<", cell=(20, 18, 40),
            grid=None, ispg=1, version=20140, little_stamp=b"DD\0\0", window=None, producer=False,
            array=None, compression_suffix=""):
        array = codes(kind, shape) if array is None else array
        assert array.shape == shape
        grid = grid or tuple(reversed(shape))
        path = directory / (name + ".mrc" + compression_suffix)
        if producer:
            assert kind == "i16" and axes == (1, 2, 3) and endian == "<" and ispg == 1
            with mrcfile.new(path, compression="bzip2" if compression_suffix else None) as volume:
                volume.set_data(array)
                volume.set_volume()
                volume.voxel_size = tuple(length / samples for length, samples in zip(cell, grid))
            assert mrcfile.validate(path)
        else:
            payload = encode(array, kind, axes, endian, cell, grid, ispg, version=version,
                             little_stamp=little_stamp)
            if compression_suffix.lower() == ".gz":
                payload = gzip.compress(payload, mtime=0)
            elif compression_suffix.lower() == ".bz2":
                payload = bz2.compress(payload, compresslevel=1)
            path.write_bytes(payload)
        read_reference(path, array, kind, axes, endian, cell, grid, ispg)
        _, type_id, storage, dtype, default_window, _ = TYPES[kind]
        native = array
        render_kind = kind
        if kind == "f16":
            native = array.view("f2").astype("f4").view("u4")
            storage, dtype, render_kind = "u4", "f4", "f32"
        raw = directory / (name + ".raw")
        raw.write_bytes(native.tobytes())
        rgba = directory / (name + ".rgba")
        rgba.write_bytes(display_bytes(native, render_kind, window or default_window))
        pixel = rgba.read_bytes()
        if all(value == 255 for value in pixel[3::4]):
            (directory / (name + ".rgb")).write_bytes(bytes(v for index, v in enumerate(pixel) if index % 4 != 3))
        spacing = tuple(length / samples * 1e-4 if ispg or axis < 2 else 0.
                        for axis, (length, samples) in enumerate(zip(cell, grid)))
        cases[name] = {
            "name": name, "filename": path.name, "width": shape[2], "height": shape[1], "z_planes": shape[0],
            "sample_type": type_id, "sample_bytes": np.dtype(storage).itemsize, "dtype": dtype,
            "dimension_order": "XYZ" if ispg else "XY",
            "calibration_flags": sum(1 << index for index, value in enumerate(spacing) if value),
            "pixel_width_um": spacing[0], "pixel_height_um": spacing[1], "z_spacing_um": spacing[2],
            "option": json.dumps({"window": list(window)}, separators=(",", ":")) if window else "-",
            "source_sha256": sha256(path), "raw_sha256": sha256(raw), "rgba_sha256": sha256(rgba),
            "axes": axes, "byte_order": "little" if endian == "<" else "big", "oracle_bit_exact": True,
        }
        return path.read_bytes()

    for kind, endian, axes in itertools.product(TYPES, ("<", ">"), itertools.permutations((1, 2, 3))):
        tag = "".join(map(str, axes))
        add(f"{kind}-{tag}-{'le' if endian == '<' else 'be'}", kind, axes=axes, endian=endian)
    main = add("i16-main", "i16", producer=True)
    add("i16-window", "i16", window=(-128, 127))
    add("float-volume", "f32", axes=(3, 1, 2), endian=">", cell=(0, 0, 0))
    add("half-volume", "f16", axes=(3, 1, 2), endian=">", cell=(0, 0, 0))
    add("half-all-bits", "f16", shape=(1, 256, 256), ispg=0, version=20141,
        array=np.arange(65536, dtype="u2").reshape(1, 256, 256))
    add("partial-calibration", "u16", cell=(20, 0, 40), grid=(10, 6, 4))
    xy = add("xy-image", "i8", shape=(1, 3, 5), ispg=0)
    add("xy-swapped", "u16", shape=(1, 3, 5), axes=(2, 1, 3), endian=">", ispg=0)
    add("mrc20141", "i16", version=20141, little_stamp=b"DA\0\0")
    add("float-window", "f32", window=(-2, 2))
    add("budget-input", "i8", shape=(2, 255, 137))
    for kind in TYPES:
        kwargs = {}
        if kind == "f16":
            kwargs = dict(axes=(3, 1, 2), endian=">", cell=(0, 0, 0))
        elif kind != "i16":
            kwargs = dict(axes=(2, 3, 1), endian=">")
        add(f"{kind}-gzip", kind, compression_suffix=".GZ" if kind == "u16" else ".gz", **kwargs)
        if kind == "i16":
            add("i16-bzip2", kind, compression_suffix=".bz2", producer=True)
        else:
            add(f"{kind}-bzip2", kind, compression_suffix=".BZ2" if kind == "u16" else ".bz2", **kwargs)
    add("half-all-bits-gzip", "f16", shape=(1, 256, 256), ispg=0, version=20141,
        array=np.arange(65536, dtype="u2").reshape(1, 256, 256), compression_suffix=".gz")
    add("budget-gzip", "i8", shape=(2, 255, 137), compression_suffix=".gz")
    images = np.arange(24,dtype="i2").reshape(3,2,4)
    volumes = np.arange(48,dtype="i2").reshape(3,2,2,4)
    for name, data in (("stack-images", images), ("stack-volumes", volumes)):
        path = directory / (name + ".mrc")
        with mrcfile.new(path) as stack:
            stack.set_data(data)
            if data.ndim == 3:
                stack.set_image_stack()
            stack.voxel_size = (2,3,4)
        assert mrcfile.validate(path)

    stack_file = directory / "stack-volumes.mrc"
    gzip_file = directory / "stack-volumes.mrc.gz"
    bzip_file = directory / "stack-volumes.mrc.bz2"
    gzip_file.write_bytes(gzip.compress(stack_file.read_bytes(),mtime=0))
    bzip_file.write_bytes(bz2.compress(stack_file.read_bytes()))

    def series_case(name, filename, source, series, option):
        path = directory / filename
        with mrcfile.open(path) as reference:
            assert np.array_equal(reference.data,source)
        volume = source.ndim == 4
        member = source[series] if volume else source[series:series+1]
        raw = directory / (name + ".raw")
        rgba = directory / (name + ".rgba")
        raw.write_bytes(member.tobytes())
        options = json.loads(option) if option != "-" else {}
        rgba.write_bytes(display_bytes(member,"i16",options.get("window",(-32768,32767))))
        cases[name] = dict(name=name,filename=filename,width=4,height=2,z_planes=member.shape[0],
            sample_type=TYPES["i16"][1],sample_bytes=2,dtype="i2",dimension_order="XYZ" if volume else "XY",
            calibration_flags=7 if volume else 3,pixel_width_um=2e-4,pixel_height_um=3*1e-4,
            z_spacing_um=4e-4 if volume else 0,option=option,series=series,series_count=3,
            source_sha256=sha256(path),raw_sha256=sha256(raw),rgba_sha256=sha256(rgba),oracle_bit_exact=True)

    series_case("stack-image-first","stack-images.mrc",images,0,"-")
    series_case("stack-image-last","stack-images.mrc",images,2,'{"series":2}')
    series_case("stack-volume-first","stack-volumes.mrc",volumes,0,"-")
    series_case("stack-volume-middle",gzip_file.name,volumes,1,'{"series":1,"window":[0,48]}')
    series_case("stack-volume-last",bzip_file.name,volumes,2,'{"series":2}')

    def packed_case(name, array, *, axes=(1,2,3), endian="<", ispg=1, suffix="", series=0):
        grid = (array.shape[2],array.shape[1],array.shape[0] if ispg else 1)
        cell = tuple(step * samples for step,samples in zip((2,3,4),grid))
        full = encode(array,"i8",axes,endian,cell,grid,ispg,version=20141)
        header = bytearray(full[:1024])
        struct.pack_into(endian + "i",header,12,101)
        assert struct.unpack_from(endian + "i",header)[0] % 2 == 0
        samples = np.frombuffer(full[1024:],dtype="u1")
        assert np.all(samples <= 15)
        bits = np.unpackbits(samples[:,None],axis=1,bitorder="little")[:,:4]
        encoded = bytes(header) + np.packbits(bits.ravel(),bitorder="little").tobytes()
        source = directory / (name + ".mrc" + suffix)
        codec = {".gz": lambda data: gzip.compress(data,mtime=0), ".bz2": bz2.compress}
        source.write_bytes(codec[suffix](encoded) if suffix else encoded)
        member = array if ispg else array[series:series+1]
        raw = directory / (name + ".raw")
        rgba = directory / (name + ".rgba")
        raw.write_bytes(member.tobytes())
        window = (0,12) if series else (0,15)
        rgba.write_bytes(display_bytes(member,"i8",window))
        options = json.dumps({"series":series,"window":window},separators=(",",":")) if series else "-"
        cases[name] = dict(name=name,filename=source.name,width=member.shape[2],height=member.shape[1],
            z_planes=member.shape[0],sample_type=1,sample_bytes=1,dtype="u1",dimension_order="XYZ" if ispg else "XY",
            calibration_flags=7 if ispg else 3,pixel_width_um=2e-4,pixel_height_um=3*1e-4,
            z_spacing_um=4e-4 if ispg else 0,option=options,series=series,series_count=1 if ispg else array.shape[0],
            source_sha256=sha256(source),raw_sha256=sha256(raw),rgba_sha256=sha256(rgba),
            axes=axes,byte_order="big" if endian==">" else "little",oracle="Literal UInt8 and independent NumPy bit packing")
        return encoded

    packed_image = packed_case("packed-image",np.arange(18,dtype="u1").reshape(1,3,6)%16,ispg=0)
    packed_volume = np.arange(40,dtype="u1").reshape(2,4,5)%16
    packed_case("packed-be-volume",packed_volume,axes=(2,3,1),endian=">")
    packed_case("packed-gzip-volume",packed_volume,axes=(2,3,1),endian=">",suffix=".gz")
    packed_case("packed-stack-bzip2",np.arange(36,dtype="u1").reshape(3,2,6)%16,ispg=0,
                suffix=".bz2",series=2)
    shape = (2, 257, 263)
    random_bits = np.random.default_rng(7).integers(0, 256, size=shape, dtype="u1")
    add("multiblock-bzip2", "i8", shape=shape, array=random_bits.view("i1"), compression_suffix=".bz2")
    add("plain-gzip-magic", "i8", shape=(1, 1, 0x8b1f), ispg=0)

    unicode_name = "\u03bc-\u56fe.mrc"
    (directory / unicode_name).write_bytes(main)
    original = (directory / cases["i16-gzip"]["filename"]).read_bytes()
    # Re-encoding with a different block size preserves the entire inner MRC.
    bzip_original = (directory / cases["i16-bzip2"]["filename"]).read_bytes()
    altered = bz2.compress(bz2.decompress(bzip_original), compresslevel=1)
    assert bzip_original != altered and bz2.decompress(bzip_original) == bz2.decompress(altered)
    (directory / "i16-source-changed.mrc.bz2").write_bytes(altered)

    def bad(name, content, status, suffix=".mrc"):
        path = directory / (name + suffix)
        path.write_bytes(content)
        negative[path.name] = {"status": status, "sha256": sha256(path)}

    def replace(name, offset, layout, value, status):
        altered = bytearray(main)
        struct.pack_into("<" + layout, altered, offset, value)
        bad(name, altered, status)

    bad("bad-signature", main[:208] + b"MISS" + main[212:], 2)
    bad("bad-machine-stamp", main[:212] + b"\0\0\0\0" + main[216:], 4)
    bad("bad-stamp-reserved", main[:212] + b"DD\0\1" + main[216:], 4)
    bad("short-header", main[:1023], 4)
    bad("short-raster", main[:-1], 4)
    bad("long-raster", main + b"\0", 4)
    bad("extended-type", main[:104] + b"SERI" + main[108:], 2)
    bad("wrong-image-sampling", xy[:36] + struct.pack("<i", 2) + xy[40:], 2)
    for name, content, status in (
        ("crc", original[:-8] + bytes([original[-8] ^ 1]) + original[-7:], 4),
        ("isize", original[:-4] + struct.pack("<I", len(gzip.decompress(original)) + 1), 4),
        ("concatenated", original + original, 4),
        ("trailing", original + b"extra" + original[-4:], 4),
        ("truncated", original[:-9] + original[-4:], 4),
        ("inner-short", gzip.compress(main[:-1], mtime=0), 4),
        ("inner-long", gzip.compress(main + b"\0", mtime=0), 4),
        ("inner-signature", gzip.compress(main[:208] + b"MISS" + main[212:], mtime=0), 2),
        ("oversized", original[:-4] + struct.pack("<I", 0xffffffff), 3),
    ):
        bad("gzip-" + name, content, status, suffix=".mrc.gz")
    for name, content, status in (
        ("crc", bzip_original[:10] + bytes([bzip_original[10] ^ 1]) + bzip_original[11:], 4),
        ("concatenated", bzip_original + bzip_original, 4),
        ("trailing", bzip_original + b"extra", 4),
        ("truncated", bzip_original[:-3], 4),
        ("inner-header", bz2.compress(main[:1000]), 4),
        ("inner-short", bz2.compress(main[:-1]), 4),
        ("inner-long", bz2.compress(main + b"\0"), 4),
        ("inner-signature", bz2.compress(main[:208] + b"MISS" + main[212:]), 2),
        ("oversized", bz2.compress(struct.pack("<I", 0x7fffffff) + main[4:]), 3),
    ):
        bad("bzip2-" + name, content, status, suffix=".mrc.bz2")
    for name, offset, layout, value, status in (
        ("zero-width", 0, "i", 0, 4),
        ("negative-width", 0, "i", -1, 4),
        ("invalid-mode", 12, "i", 3, 2),
        ("complex-float-mode", 12, "i", 4, 2),
        ("origin-start", 16, "i", -1, 2),
        ("invalid-sample-grid", 28, "i", 0, 4),
        ("negative-sample-grid", 36, "i", -1, 4),
        ("negative-cell", 44, "f", -1, 4),
        ("nonfinite-cell", 48, "f", float("inf"), 4),
        ("oblique-cell", 52, "f", 80, 2),
        ("nonfinite-angle", 52, "f", float("nan"), 4),
        ("duplicate-axis", 64, "i", 2, 4),
        ("invalid-axis", 72, "i", 4, 4),
        ("image-stack", 88, "i", 0, 2),
        ("nontrivial-volume-stack", 88, "i", 402, 2),
        ("nontrivial-symmetry", 88, "i", 19, 2),
        ("extended-header", 92, "i", 4, 2),
        ("negative-extended-header", 92, "i", -4, 4),
        ("unsupported-version", 108, "i", 20000, 2),
        ("nonzero-origin", 196, "f", -1, 2),
        ("nonfinite-origin", 196, "f", float("nan"), 4),
    ):
        replace(name, offset, layout, value, status)

    for name, offset, value, status in (("stack-section-count",8,5,4),("stack-zero-depth",36,0,4),
                                         ("stack-wrong-section-axis",72,2,2)):
        altered = bytearray(stack_file.read_bytes())
        struct.pack_into("<i",altered,offset,value)
        if name == "stack-wrong-section-axis":
            struct.pack_into("<i",altered,68,3)
        bad(name,altered,status)
    odd = bytearray(packed_image)
    struct.pack_into("<i",odd,0,5)
    bad("packed-odd-columns",odd,2)
    bad("packed-short",packed_image[:-1],4)
    bad("packed-padded",packed_image+b"\0",4)
    bad("packed-gzip-short",gzip.compress(packed_image[:-1],mtime=0),4,suffix=".mrc.gz")

    columns = ("name", "filename", "width", "height", "z_planes", "sample_type", "sample_bytes", "dimension_order",
               "calibration_flags", "pixel_width_um", "pixel_height_um", "z_spacing_um", "option","series","series_count")
    for case in cases.values():
        case.setdefault("series",0)
        case.setdefault("series_count",1)
    (directory / "fixtures.tsv").write_text(
        "".join("\t".join(str(case[column]) for column in columns) + "\n" for case in cases.values()),
        encoding="utf8",
    )
    (directory / "negative.tsv").write_text(
        "".join(f"{name}\t{item['status']}\n" for name, item in negative.items()), encoding="utf8",
    )
    manifest = {
        "schema": "qpx.mrc-test-fixtures/1", "cases": cases, "negative": negative,
        "oracle": {"mrcfile": mrcfile.__version__, "numpy": np.__version__,
                   "compare": "Literal ZYX codes, CCP-EM readback/endian/dtype/axis checks, independently evaluated display"},
        "unicode_filename": unicode_name,
    }
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    return manifest


def run(command, directory, name, checks):
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8")
    log = directory / (name + ".log")
    log.write_text(result.stdout + result.stderr, encoding="utf8")
    checks.append({"name": name, "command": command, "exit": result.returncode, "log": str(log)})
    if result.returncode:
        raise RuntimeError(f"{name} failed ({result.returncode})\n{result.stdout}{result.stderr}")


def validate(args):
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Validation requires a new or empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    fixture_path = output / "fixtures"
    fixtures = write_fixtures(fixture_path)
    artifact_names = ("module", "abi_test", "consumer", "manifest", "region")
    artifacts = {name: path.resolve() for name in artifact_names if (path := getattr(args, name))}
    before = {name: sha256(path) for name, path in artifacts.items()}
    checks = []
    report = {"schema": "qpx.mrc-plugin-validation/1", "checks": checks, "artifacts_before": before,
              "artifact_paths": {name: str(path) for name, path in artifacts.items()},
              "fixture_cases": len(fixtures["cases"]), "negative_cases": len(fixtures["negative"]),
              **{name + "_sha256": digest for name, digest in before.items()}}
    try:
        run([str(artifacts["abi_test"]), str(artifacts["module"]), str(fixture_path)],
            output, "direct-abi", checks)
        if args.consumer and args.manifest:
            work = output / "project"
            for mode in ("create", "reopen", "missing", "changed-source"):
                run([str(artifacts["consumer"]), mode, str(artifacts["manifest"]), str(fixture_path), str(work)],
                    output, "project-" + mode, checks)
            for family,series,high in (("stack",1,48),("packed",2,12)):
                selected_manifest = output / ("selected-"+family) / "reader-plugins.json"
                selected_manifest.parent.mkdir()
                manifest = json.loads(artifacts["manifest"].read_text(encoding="utf8"))
                for provider in manifest["plugins"]:
                    module_path = Path(provider["module"]["path"])
                    shutil.copy2(artifacts["manifest"].parent / module_path, selected_manifest.parent / module_path)
                    provider["options"] = {"series":series,"window":[0,high]}
                selected_manifest.write_text(json.dumps(manifest,indent=2) + "\n",encoding="utf8")
                for action,entry in (("create",selected_manifest),("reopen",artifacts["manifest"])):
                    mode = family+"-"+action
                    run([str(artifacts["consumer"]),mode,str(entry),str(fixture_path),str(output / (family+"-project"))],
                        output,"project-"+mode,checks)
        if args.region and args.manifest:
            cli = [str(artifacts["region"]), "--reader-plugins", str(artifacts["manifest"])]
            run(cli + ["--readers"], output, "host-catalog", checks)
            assert "org.qupathxx.mrc" in (output / "host-catalog.log").read_text(encoding="utf8")
            command = cli + ["--reader", "org.qupathxx.mrc"]
            for name in ("i16-main", "i8-321-be", "u16-231-le", "float-volume", "half-volume",
                         "i16-gzip", "f16-gzip", "i16-bzip2", "f16-bzip2"):
                case = fixtures["cases"][name]
                region = ("5", "3")
                assert (case["width"], case["height"]) == tuple(map(int, region))
                source = fixture_path / case["filename"]
                npy = output / (name + "-z1.npy")
                run(command + ["--raw", str(source), str(npy), "--width", region[0], "--height", region[1], "--z", "1"],
                    output, "host-raw-" + name, checks)
                data = np.load(npy, allow_pickle=False)
                assert data.dtype == np.dtype(case["dtype"]) and data.shape == (3, 5, 1), name
                stride = 5 * 3 * case["sample_bytes"]
                assert data.tobytes() == (fixture_path / (name + ".raw")).read_bytes()[stride:2*stride], name
            for name in ("i16-main", "float-volume", "half-volume", "i16-gzip", "f16-gzip", "i16-bzip2", "f16-bzip2"):
                source = fixture_path / fixtures["cases"][name]["filename"]
                pam = output / (name + ".pam")
                run(command + [str(source), "0", "0", "0", "5", "3", str(pam)], output, "host-display-" + name, checks)
                assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == (fixture_path / (name + ".rgba")).read_bytes(), name
            for name in ("stack-image-last","stack-volume-middle","stack-volume-last"):
                case = fixtures["cases"][name]
                npy = output / (name + ".npy")
                options = ["--reader-options",case["option"]]
                selected = case["z_planes"]-1
                run(command + options + ["--raw", str(fixture_path / case["filename"]), str(npy),
                                         "--z", str(selected)], output, "host-raw-" + name, checks)
                samples = np.load(npy,allow_pickle=False)
                assert samples.dtype==np.dtype("i2") and samples.shape==(2,4,1),name
                assert samples.tobytes()==(fixture_path / (name + ".raw")).read_bytes()[-16:],name
                report_data = json.loads((output / ("host-raw-" + name + ".log")).read_text(encoding="utf8"))
                raw = report_data["reader_snapshot"]["raw"]
                assert (raw["series"],raw["series_count"],raw["z_planes"],raw["timepoints"])==(
                    case["series"],3,case["z_planes"],1),name
            for name in ("packed-gzip-volume","packed-stack-bzip2"):
                case = fixtures["cases"][name]
                npy = output / (name + ".npy")
                options = ["--reader-options",case["option"]] if case["option"] != "-" else []
                run(command + options + ["--raw",str(fixture_path / case["filename"]),str(npy),
                                         "--z",str(case["z_planes"]-1)],output,"host-raw-"+name,checks)
                samples = np.load(npy,allow_pickle=False)
                stride = case["width"]*case["height"]
                assert samples.dtype == np.dtype("u1") and samples.shape==(case["height"],case["width"],1)
                assert samples.tobytes() == (fixture_path / (name+".raw")).read_bytes()[-stride:]
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = {name: sha256(path) for name, path in artifacts.items()}
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation module, manifest or consumer changed during execution")
    print(json.dumps({"passed": True, "fixture_cases": len(fixtures["cases"]),
                      "native_checks": len(checks), "report": str(output / "validation.json")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="mode", required=True)
    output = commands.add_parser("fixtures")
    output.add_argument("output", type=Path)
    validate_parser = commands.add_parser("validate")
    for name in ("module", "abi-test", "output"):
        validate_parser.add_argument("--" + name, type=Path, required=True)
    for name in ("consumer", "manifest", "region"):
        validate_parser.add_argument("--" + name, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        result = write_fixtures(args.output.resolve())
        print(json.dumps({"cases": len(result["cases"]), "negatives": len(result["negative"])}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
