"""Generate independent NIfTI oracles and exercise the standalone reader and native SDK."""

import argparse
from decimal import Decimal, ROUND_HALF_UP, localcontext
import gzip
from hashlib import sha256 as sha256_hash
from io import BytesIO
import json
from pathlib import Path
import struct
import subprocess

import nibabel
import numpy as np


TYPES = {
    # NIfTI datatype, qpx_reader_sample_type_v1, NumPy dtype
    "u8": (2, 1, "u1"), "i8": (256, 3, "i1"), "u16": (512, 2, "u2"),
    "i16": (4, 4, "i2"), "u32": (768, 5, "u4"), "i32": (8, 6, "i4"),
    "u64": (1280, 7, "u8"), "i64": (1024, 8, "i8"),
    "f32": (16, 9, "f4"), "f64": (64, 10, "f8"),
}
SPACING = (0.25, 0.5, 2.0)
WIDE_SPACING = (0.2500000001, 0.5000000002, 2.0000000003)
UNIT_NAMES = ("unknown", "meter", "mm", "micron")
UNIT_FACTORS = (0, 1_000_000, 1_000, 1)


def sha256(path):
    return sha256_hash(path.read_bytes()).hexdigest()


def scalar_values(kind, count):
    dtype = np.dtype(TYPES[kind][2])
    if dtype.kind == "f":
        # IEEE raw comparisons verify quiet payloads, signed zero, infinities and subnormals.
        ieee = {
            4: [0x7FC12345, 0xFFC54321, 0x7F800000, 0xFF800000, 0x80000000, 1, 2],
            8: [0x7FF8123456789ABC, 0xFFF8ABCDEF123456, 0x7FF0000000000000,
                0xFFF0000000000000, 0x8000000000000000, 1, 2],
        }
        floats = np.array([0.0, -1.0, 1.0, -2.0, 2.0, -0.5, 0.75, 0.5], dtype=dtype)
        values = np.concatenate((np.array(ieee[dtype.itemsize], dtype=f"u{dtype.itemsize}").view(dtype), floats))
    else:
        bounds = np.iinfo(dtype)
        span = int(bounds.max) - int(bounds.min)
        mid = (int(bounds.min) + int(bounds.max)) // 2
        values = np.array([int(bounds.min), int(bounds.max), mid, mid + 1, 0, 1,
                           int(bounds.min) + 1, int(bounds.max) - 1,
                           *(int(bounds.min) + span * n // 255 for n in (3, 33, 127, 199, 254)),
                           *([2**53 + 1, 2**53 + 2] if dtype.itemsize == 8 else [2, 3])],
                          dtype=dtype)
    second_plane = np.roll(values[::-1], 5)
    if dtype.kind == "f":
        # A negative subnormal is an exact raw oracle on Z1, away from the Z0 midpoint.
        sign_bit = 1 << (dtype.itemsize * 8 - 1)
        second_plane.view(f"u{dtype.itemsize}")[-1] = sign_bit | 1
    return np.concatenate((values, second_plane))[:count]


TIME_UNITS = {0: "unknown", 8: "sec", 16: "msec", 24: "usec"}
TIME_DIVISORS = {8: 1, 16: 1000, 24: 1000000}


def encoded(values, shape, endian, unit, form, *, ignored_intercept=None, time_unit=0, step=0):
    header = bytearray(352)

    def put(offset, format, *fields):
        struct.pack_into(endian + format, header, offset, *fields)

    dtype = values.dtype
    datatype = next(code for code, _, name in TYPES.values() if np.dtype(name) == dtype)
    put(0, "i", 348)
    put(40, "8h", len(shape), *shape, *([1] * (7 - len(shape))))
    put(70, "2h", datatype, dtype.itemsize * 8)
    pixdim = (11.0, 12.0, 13.0) if form == "s" else SPACING
    put(76, "8f", int("q" in form), *pixdim, step, 0, 0, 0)
    put(108, "3f", 352, float(ignored_intercept is None), ignored_intercept or 0)
    header[123] = unit | time_unit
    put(252, "2h", 1 if "q" in form else 0, 2 if "s" in form else 0)
    if "s" in form:
        put(280, "12f", SPACING[0], 0, 0, 0, 0, SPACING[1], 0, 0, 0, 0, SPACING[2], 0)
    header[344:348] = b"n+1\0"
    return bytes(header) + values.astype(dtype.newbyteorder(endian)).tobytes()


def encoded_nifti2(values, shape, endian, unit, form, spacing, *, time_unit=0, step=0):
    header = nibabel.Nifti2Header(endianness=endian)
    header.set_data_shape(shape)
    header.set_data_dtype(values.dtype)
    header.set_xyzt_units(UNIT_NAMES[unit], TIME_UNITS[time_unit])
    header.set_slope_inter(1, 0)
    header["vox_offset"] = 544
    header.set_zooms((*spacing, step)[:len(shape)])
    transform = np.diag([*spacing, 1])
    if "q" in form:
        header.set_qform(transform, 1)
    if "s" in form:
        header.set_sform(transform, 2)
    return header.binaryblock + bytes(4) + values.astype(values.dtype.newbyteorder(endian)).tobytes()


def expected_display(values, width, height, window):
    dtype = values.dtype
    if window is None:
        bounds = np.iinfo(dtype) if dtype.kind != "f" else None
        window = (int(bounds.min), int(bounds.max)) if bounds else (-1, 1)
    lo, hi = (Decimal(str(v)) for v in window)
    rgba = bytearray()
    with localcontext() as context:
        context.prec = 100
        for value in values[:width * height]:
            if dtype.kind == "f" and not np.isfinite(value):
                rgba.extend((0, 0, 0, 0))
                continue
            scalar = Decimal.from_float(float(value)) if dtype.kind == "f" else Decimal(int(value))
            intensity = (min(hi, max(lo, scalar)) - lo) * 255 / (hi - lo)
            gray = int(intensity.to_integral_value(rounding=ROUND_HALF_UP))
            rgba.extend((gray, gray, gray, 255))
    return bytes(rgba)


def check_nibabel(path, values, shape, endian, unit, form, spacing, *, pair=None, nifti2=False,
                  time_unit=0, step=0):
    if pair is None:
        disk = nibabel.load(path)
    else:
        # NiBabel's filename-derived pair assumes the same gzip suffix on both.
        reader = nibabel.Nifti2Pair if nifti2 else nibabel.Nifti1Pair
        disk = reader.from_file_map(reader.make_file_map({"header": str(path), "image": str(pair)}))
    assert disk.shape == shape and disk.header.endianness == endian, path
    assert disk.header.get_xyzt_units() == (UNIT_NAMES[unit], TIME_UNITS[time_unit]), path
    assert disk.dataobj.slope == 1 and disk.dataobj.inter == 0, path
    raw = np.asarray(disk.dataobj.get_unscaled()).ravel(order="F")
    expected = values.astype(values.dtype.newbyteorder(endian))
    assert raw.dtype == expected.dtype and raw.tobytes() == expected.tobytes(), path
    if form:
        np.testing.assert_array_equal(disk.affine, np.diag([*spacing, 1]))
    else:
        assert disk.header.get_zooms() == (*spacing, step)[:len(shape)], path
    if len(shape) == 4:
        assert disk.header.get_zooms()[3] == step, path
    return disk


def write_fixtures(directory):
    if nibabel.__version__ != "5.3.2":
        raise RuntimeError(f"Expected the independent NiBabel 5.3.2 oracle, found {nibabel.__version__}")
    directory.mkdir(parents=True)
    cases, negatives, manifest = {}, {}, []

    def positive(name, kind, endian, depth, unit, form, window=None, *, ignored_intercept=None,
                 pair_offset=None, extender=False, nifti2=False, spacing=SPACING, width=5,
                 mixed=False, frames=None, time_unit=0, step=0, comments=()):
        shape = (width, 3, 2) if depth else (width, 3)
        count = int(np.prod(shape))
        values = np.resize(scalar_values(kind, count), count)
        if frames is not None:
            shape = (*shape, frames) if depth else (*shape, 1, frames)
            values = np.concatenate([np.roll(values, 7 * t) for t in range(frames)])
        data = (encoded_nifti2(values, shape, endian, unit, form, spacing, time_unit=time_unit, step=step)
                if nifti2 else encoded(values, shape, endian, unit, form, ignored_intercept=ignored_intercept,
                                       time_unit=time_unit, step=step))
        length = 540 if nifti2 else 348
        extension = b""
        if comments:
            blocks = BytesIO()
            nibabel.nifti1.Nifti1Extensions(
                [nibabel.nifti1.Nifti1Extension(6, text) for text in comments]
            ).write_to(blocks, byteswap=endian != "<")
            extension = blocks.getvalue()
        source = directory / name
        def write(path, value):
            path.write_bytes(gzip.compress(value, mtime=0) if path.suffix == ".gz" else value)

        pair = None
        if pair_offset is None:
            if extension:
                header = bytearray(data[:length])
                struct.pack_into(endian + ("q" if nifti2 else "f"), header,
                                 168 if nifti2 else 108, length + 4 + len(extension))
                data = header + b"\x01\0\0\0" + extension + data[length+4:]
            write(source, data)
        else:
            header = bytearray(data[:length + (4 if extender else 0)])
            if nifti2:
                header[4:8] = b"ni2\0"
                struct.pack_into(endian + "q", header, 168, pair_offset)
            else:
                header[344:348] = b"ni1\0"
                struct.pack_into(endian + "f", header, 108, pair_offset)
            if extension:
                header[length:] = b"\x01\0\0\0" + extension
            write(source, header)
            pair_name = name.replace(".hdr", ".img")
            if mixed:
                pair_name = pair_name.removesuffix(".gz") if pair_name.endswith(".gz") else pair_name + ".gz"
            pair = directory / pair_name
            write(pair, b"p" * pair_offset + data[length+4:])
        disk = check_nibabel(source, values, shape, endian, unit, form, spacing,
                             pair=pair if mixed else None, nifti2=nifti2, time_unit=time_unit, step=step)
        assert [value.get_content() for value in disk.header.extensions] == list(comments)
        if pair_offset is not None:
            assert disk.dataobj.offset == pair_offset
        rgba = expected_display(values, *shape[:2], window)
        (directory / (name + ".rgba")).write_bytes(rgba)
        (directory / (name + ".rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        dtype = values.dtype.newbyteorder("<")
        (directory / (name + ".raw")).write_bytes(values.astype(dtype).tobytes())
        volume = len(shape) >= 3
        flags = ((7 if volume else 3) if unit else 0) + (8 if time_unit else 0)
        calibration = tuple(x * UNIT_FACTORS[unit] if use else 0
                            for x, use in zip(spacing, (unit, unit, unit and volume)))
        seconds = step / TIME_DIVISORS[time_unit] if time_unit else 0
        opts = json.dumps({"window": window}, separators=(",", ":")) if window is not None else "-"
        manifest.append((name, *shape[:2], shape[2] if volume else 1, frames or 1, TYPES[kind][1],
                         dtype.itemsize, "NIfTI-2" if nifti2 else "NIfTI-1", "XYZT"[:len(shape)],
                         flags, *calibration, seconds, opts))
        cases[name] = {"shape": shape, "dtype": dtype.str, "endian": endian, "unit": UNIT_NAMES[unit],
                       "form": form, "spacing_um": calibration, "window": window,
                       "sample_bytes": dtype.itemsize, "sha256": sha256(source), "seconds": seconds}
        return data

    plain = positive("i16-main.nii", "i16", "<", True, 2, "qs")
    positive("i16-window.nii", "i16", "<", True, 2, "qs", [-200, 400])
    positive("f32-main.nii.gz", "f32", ">", True, 0, "")
    positive("f64-window.nii.gz", "f64", ">", False, 3, "q", [-0.5, 1.25])
    for index, kind in enumerate(TYPES):
        for endian, suffix, shift in (("<", "le.nii", 0), (">", "be.nii.gz", 1)):
            if (kind, endian) in (("i16", "<"), ("f32", ">")):
                continue
            unit = (index + shift) % 4
            form = ("", "q", "s", "qs")[(index + 2 * shift) % 4]
            positive(f"{kind}-{suffix}", kind, endian, bool((index + shift) % 2), unit, form,
                     ignored_intercept=37.0 if kind == "u8" else None)

    positive("i16-pair.hdr", "i16", "<", True, 2, "qs", pair_offset=0)
    positive("f32-pair.hdr.gz", "f32", ">", True, 0, "", pair_offset=17)
    positive("u64-pair.hdr.gz", "u64", ">", True, 3, "qs", pair_offset=129, extender=True)
    positive("f64-pair.hdr", "f64", ">", False, 3, "q", [-0.5, 1.25], pair_offset=13, extender=True)
    positive("u8-pair.hdr.gz", "u8", "<", False, 0, "", pair_offset=0)
    positive("i32-pair.hdr", "i32", ">", False, 1, "s", pair_offset=0)
    v2 = positive("v2-u64.nii", "u64", "<", True, 3, "s", nifti2=True)
    positive("v2-f64.nii.gz", "f64", ">", True, 2, "qs", [-0.5, 1.25], nifti2=True, spacing=WIDE_SPACING)
    positive("v2-i16.hdr", "i16", "<", True, 2, "qs", pair_offset=0, nifti2=True)
    positive("v2-f32.hdr.gz", "f32", ">", True, 0, "", pair_offset=32, extender=True, nifti2=True)
    positive("v2-wide.nii", "u8", "<", False, 0, "", nifti2=True, width=40000)
    positive("v2-2d.hdr", "u16", ">", False, 1, "q", pair_offset=16, nifti2=True, spacing=WIDE_SPACING)
    positive("i16-mixed.hdr", "i16", "<", True, 2, "qs", pair_offset=0, mixed=True)
    positive("f32-mixed.hdr.gz", "f32", ">", True, 0, "", pair_offset=17, mixed=True)
    positive("v2-i16-mixed.hdr", "i16", "<", True, 2, "qs", pair_offset=0, nifti2=True, mixed=True)
    positive("v2-f32-mixed.hdr.gz", "f32", ">", True, 0, "", pair_offset=32,
             extender=True, nifti2=True, mixed=True)
    (directory / "i16-pair.img.gz").write_bytes(b"unselected gzip")
    (directory / "f32-pair.img").write_bytes(b"unselected plain")
    time_v1 = positive("time-v1.nii", "i16", "<", True, 2, "qs", frames=3, time_unit=8, step=0.5)
    positive("time-v1-pair.hdr.gz", "f32", ">", False, 0, "", pair_offset=16, mixed=True,
             frames=2, time_unit=16, step=125)
    time_v2 = positive("time-v2.nii.gz", "u64", ">", True, 3, "s", nifti2=True,
                       spacing=WIDE_SPACING, frames=2, time_unit=24, step=123456.125)
    positive("time-v2-pair.hdr", "f64", "<", True, 0, "", nifti2=True, pair_offset=32,
             frames=2, step=1.5)

    comment = positive("comment-v1.nii", "i16", "<", True, 2, "s",
                       comments=(b"two blocks", b"voxel order preserved\n"))
    positive("comment-v2.nii.gz", "u64", ">", True, 3, "s", nifti2=True,
             spacing=WIDE_SPACING, comments=(b"big endian gzip",))
    positive("comment-v1.hdr.gz", "f32", ">", False, 0, "", pair_offset=16, mixed=True,
             comments=(b"paired header",))
    positive("comment-v2.hdr", "f64", "<", True, 0, "", nifti2=True, pair_offset=32,
             comments=(b"header and raster have independent offsets",))
    (directory / "\u03bc-\u56fe.nii.gz").write_bytes(gzip.compress(plain, mtime=0))
    changed = bytearray(plain)
    changed[-1] ^= 2
    (directory / "i16-source-changed.nii").write_bytes(changed)
    budget = encoded(np.zeros(256 * 256, dtype="u1"), (256, 256), "<", 0, "")
    (directory / "budget-input.nii.gz").write_bytes(gzip.compress(budget, mtime=0))

    def negative(name, data, status):
        (directory / name).write_bytes(data)
        negatives[name] = status

    def mutate(name, offset, fmt, value, status):
        data = bytearray(plain)
        struct.pack_into("<" + fmt, data, offset, value)
        negative(name + ".nii", data, status)

    mutate("bad-header-size", 0, "i", 347, 4)
    mutate("nifti2-magic", 344, "4s", b"n+2\0", 2)
    mutate("pair-magic", 344, "4s", b"ni1\0", 2)
    mutate("unknown-magic", 344, "4s", b"BAD!", 2)
    negative("short-header.nii", plain[:300], 4)
    negative("short-data.nii", plain[:-1], 4)
    negative("trailing-data.nii", plain + b"x", 4)
    mutate("unsupported-rank", 40, "h", 5, 2)
    mutate("zero-width", 42, "h", 0, 4)
    mutate("negative-height", 44, "h", -3, 4)
    mutate("unsupported-datatype", 70, "h", 32, 2)
    mutate("wrong-bitpix", 72, "h", 32, 4)
    mutate("offset-384", 108, "f", 384, 2)
    mutate("offset-nan", 108, "f", float("nan"), 4)
    mutate("extension", 348, "B", 1, 4)
    for label, offset, fmt, value, status in [
        ("short-record", 352, "i", 8, 4), ("unaligned", 352, "i", 17, 4),
        ("overrun", 352, "i", 65536, 4), ("unsupported-code", 356, "i", 2, 2),
        ("reserved-extender", 350, "B", 1, 2), ("non-ascii", 360, "B", 255, 4),
        ("embedded-nul", 361, "B", 0, 4), ("truncated-area", 108, "f", len(comment) + 16, 4),
    ]:
        data = bytearray(comment)
        struct.pack_into("<" + fmt, data, offset, value)
        negative(f"comment-bad-{label}.nii", data, status)
    mutate("intent-code", 68, "h", 1, 2)
    mutate("intent-parameter", 56, "f", 2, 2)
    mutate("scaling", 112, "f", 2, 2)
    mutate("intercept", 116, "f", 2, 2)
    mutate("time-unit", 123, "B", 10, 2)
    mutate("unknown-unit", 123, "B", 7, 2)
    mutate("slice-code", 122, "B", 1, 2)
    mutate("time-offset", 136, "f", 2, 2)
    mutate("qform-code", 252, "h", 3, 2)
    mutate("sform-code", 254, "h", 3, 2)
    mutate("negative-qfac", 76, "f", -1, 2)
    mutate("nonpositive-spacing", 80, "f", 0, 4)
    mutate("nonfinite-spacing", 84, "f", float("inf"), 4)
    mutate("qform-rotation", 256, "f", 0.5, 2)
    mutate("qform-offset", 268, "f", 1, 2)
    mutate("sform-shear", 284, "f", 0.5, 2)
    mutate("sform-offset", 292, "f", 1, 2)
    mutate("different-forms", 280, "f", 1, 2)
    gz = gzip.compress(plain, mtime=0)
    negative("gzip-truncated.nii.gz", gz[:18] + gz[-8:], 4)
    bad_crc = bytearray(gz)
    bad_crc[-8] ^= 1
    negative("gzip-crc.nii.gz", bad_crc, 4)
    negative("gzip-concatenated.nii.gz", gz + gz, 4)
    negative("gzip-trailing.nii.gz", gz + b"trailing!" + gz[-4:], 4)
    negative("gzip-oversized-isize.nii.gz", gz[:-4] + struct.pack("<I", 0xF0000000), 3)

    pair_header = (directory / "i16-pair.hdr").read_bytes()
    pair_data = (directory / "i16-pair.img").read_bytes()
    negative("pair-missing.hdr", pair_header, 4)
    negative("pair-short.hdr", pair_header, 4)
    (directory / "pair-short.img").write_bytes(pair_data[:-1])
    negative("pair-extra.hdr", pair_header, 4)
    (directory / "pair-extra.img").write_bytes(pair_data + b"x")
    negative("pair-single-format.hdr", plain[:348], 2)
    negative("pair-analyze.hdr", pair_header[:344] + bytes(4), 2)
    negative("pair-extender.hdr", pair_header + b"\x01\0\0\0", 4)
    for name, offset in [("negative", -1), ("fractional", .5), ("overflow", 1e30)]:
        broken = bytearray(pair_header)
        struct.pack_into("<f", broken, 108, offset)
        negative(f"pair-{name}-offset.hdr", broken, 4)
    paired_gzip_header = gzip.compress(pair_header, mtime=0)
    paired_gzip_data = gzip.compress(pair_data, mtime=0)
    negative("pair-concatenated-header.hdr.gz", paired_gzip_header * 2, 4)
    negative("pair-plain-header.hdr.gz", pair_header, 4)
    for name, broken in [("crc", paired_gzip_data[:-8] + bytes([paired_gzip_data[-8] ^ 1]) + paired_gzip_data[-7:]),
                         ("concat", paired_gzip_data * 2), ("plain", pair_data),
                         ("extent", gzip.compress(pair_data[:-1], mtime=0))]:
        negative(f"pair-{name}-payload.hdr.gz", paired_gzip_header, 4)
        (directory / f"pair-{name}-payload.img.gz").write_bytes(broken)
    negative("mixed-bad-crc.hdr", pair_header, 4)
    (directory / "mixed-bad-crc.img.gz").write_bytes(paired_gzip_data[:-8] + bytes([paired_gzip_data[-8] ^ 1])
                                                     + paired_gzip_data[-7:])
    negative("mixed-bad-plain.hdr.gz", paired_gzip_header, 4)
    (directory / "mixed-bad-plain.img").write_bytes(pair_data[:-1])
    negative("mixed-preferred-corrupt.hdr", pair_header, 4)
    (directory / "mixed-preferred-corrupt.img").write_bytes(pair_data[:-1])
    (directory / "mixed-preferred-corrupt.img.gz").write_bytes(paired_gzip_data)
    for name, offset, fmt, value, status in [
        ("nonzero-origin", 136, "f", 0.25, 2),
        ("frequency-units", 123, "B", 34, 2),
        ("nonfinite-step", 92, "f", float("nan"), 4),
        ("zero-step", 92, "f", 0, 4),
        ("zero-extent", 48, "h", 0, 4),
    ]:
        data = bytearray(time_v1)
        struct.pack_into("<" + fmt, data, offset, value)
        negative(f"time-bad-{name}.nii", data, status)
    negative("time-truncated-raster.nii", time_v1[:-1], 4)
    underflow = bytearray(time_v2)
    struct.pack_into(">d", underflow, 136, float.fromhex("0x0.0000000000001p-1022"))
    negative("time-v2-underflow.nii", underflow, 4)

    for label, offset, fmt, value, status in [
        ("rank", 16, "q", 5, 2), ("negative-width", 24, "q", -1, 4),
        ("width", 24, "q", 2**32, 4), ("extent", 24, "q", 2**31, 3),
        ("offset", 168, "q", -1, 4), ("nondefault", 168, "q", 560, 2),
        ("magic", 4, "4s", b"n+1\0", 2), ("eol", 8, "B", 1, 2),
        ("extension", 540, "B", 1, 4), ("nonfinite", 400, "d", float("nan"), 4),
        ("scale", 176, "d", 1.0000000001, 2), ("units", 500, "i", 257, 2),
    ]:
        data = bytearray(v2)
        struct.pack_into("<" + fmt, data, offset, value)
        negative(f"v2-bad-{label}.nii", data, status)
    overflow = bytearray(v2)
    struct.pack_into("<d", overflow, 400, 1e308)
    struct.pack_into("<i", overflow, 500, 1)
    negative("v2-bad-converted-spacing.nii", overflow, 4)

    (directory / "fixtures.tsv").write_text(
        "".join("\t".join(map(str, row)) + "\n" for row in manifest), encoding="utf8")
    (directory / "negative.tsv").write_text(
        "".join(f"{name}\t{status}\n" for name, status in negatives.items()), encoding="utf8")
    report = {"schema": "qpx.nifti-fixtures/1", "oracle": {"nibabel": nibabel.__version__},
              "cases": cases, "negative": negatives}
    (directory / "fixtures.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
    return report


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
    report = {"schema": "qpx.nifti-plugin-validation/1", "checks": checks, "artifacts_before": before,
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
        if args.region and args.manifest:
            cli = [str(artifacts["region"]), "--reader-plugins", str(artifacts["manifest"])]
            run(cli + ["--readers"], output, "host-catalog", checks)
            assert "org.qupathxx.nifti" in (output / "host-catalog.log").read_text(encoding="utf8")
            command = cli + ["--reader", "org.qupathxx.nifti"]
            for name in ("i16-main.nii", "f32-main.nii.gz", "u64-be.nii.gz", "f64-window.nii.gz",
                         "i16-pair.hdr", "f32-pair.hdr.gz", "u64-pair.hdr.gz",
                         "v2-u64.nii", "v2-f64.nii.gz", "v2-f32.hdr.gz", "i16-mixed.hdr",
                         "f32-mixed.hdr.gz", "v2-i16-mixed.hdr", "v2-f32-mixed.hdr.gz",
                         "time-v1.nii", "time-v1-pair.hdr.gz", "time-v2.nii.gz", "time-v2-pair.hdr",
                         "comment-v1.nii", "comment-v2.nii.gz", "comment-v1.hdr.gz", "comment-v2.hdr"):
                case = fixtures["cases"][name]
                width, height = case["shape"][:2]
                z = 1 if len(case["shape"]) >= 3 and case["shape"][2] > 1 else 0
                t = case["shape"][3] - 1 if len(case["shape"]) == 4 else 0
                source = fixture_path / name
                npy = output / (name + ".npy")
                run(command + ["--raw", str(source), str(npy), "--width", str(width), "--height",
                               str(height), "--z", str(z), "--time", str(t)], output, "host-raw-" + name, checks)
                data = np.load(npy, allow_pickle=False)
                assert data.dtype == np.dtype(case["dtype"]) and data.shape == (height, width, 1), name
                stride = width * height * case["sample_bytes"]
                plane = t * (case["shape"][2] if len(case["shape"]) >= 3 else 1) + z
                assert data.tobytes() == (fixture_path / (name + ".raw")).read_bytes()[plane*stride:(plane+1)*stride], name
            wide_name = "v2-wide.nii"
            wide_width, wide_height = fixtures["cases"][wide_name]["shape"]
            wide_crop = output / "wide-edge.npy"
            run(command + ["--raw", str(fixture_path / wide_name), str(wide_crop), "--x", str(wide_width-3),
                           "--width", "3", "--height", "2"], output, "host-wide-edge", checks)
            wide_pixels = np.frombuffer((fixture_path / (wide_name + ".raw")).read_bytes(), dtype="u1")
            np.testing.assert_array_equal(np.load(wide_crop, allow_pickle=False),
                                          wide_pixels.reshape(wide_height, wide_width)[:2, -3:, None])
            for name in ("i16-main.nii", "f32-main.nii.gz", "f32-pair.hdr.gz", "v2-i16.hdr"):
                source = fixture_path / name
                pam = output / (name + ".pam")
                run(command + [str(source), "0", "0", "0", "5", "3", str(pam)], output, "host-display-" + name, checks)
                assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == (fixture_path / (name + ".rgba")).read_bytes(), name
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
