#!/usr/bin/env python3
"""Development-only literal scalar NRRD oracles and native plugin controls."""
import argparse
import bz2
from decimal import Decimal, localcontext, ROUND_HALF_UP
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import zlib


TYPES = {"u8": (1, 1, "B", "uint8"), "u16": (2, 2, "H", "uint16"),
         "i16": (4, 2, "h", "int16"), "f32": (9, 4, "I", "float"),
         "i8": (3, 1, "b", "int8"), "u32": (5, 4, "I", "uint32"),
         "i32": (6, 4, "i", "int32"), "u64": (7, 8, "Q", "uint64"),
         "i64": (8, 8, "q", "int64"), "f64": (10, 8, "Q", "double")}
FLOAT_FORMAT = {"f32": "f4", "f64": "f8"}
COMPRESSED = {"gzip": (lambda data: gzip.compress(data, mtime=0), gzip.decompress),
              "bzip2": (bz2.compress, bz2.decompress)}


def scalar_decimal(value, kind):
    if kind not in FLOAT_FORMAT:
        return Decimal.from_float(float(value)) if kind in ("i64", "u64") else Decimal(value)
    fraction_bits, exponent_bits = (23, 8) if kind == "f32" else (52, 11)
    max_exponent = (1 << exponent_bits) - 1
    fraction_mask = (1 << fraction_bits) - 1
    exponent, fraction = (value >> fraction_bits) & max_exponent, value & fraction_mask
    if exponent == max_exponent:
        return None
    significand = fraction if exponent == 0 else (1 << fraction_bits) + fraction
    bias = max_exponent // 2
    result = Decimal(significand) * Decimal(2) ** (max(exponent, 1) - bias - fraction_bits)
    return -result if value >> (fraction_bits + exponent_bits) else result


def rgba_oracle(values, kind, width, height, window):
    with localcontext() as context:
        context.prec = 190
        low, high = (Decimal.from_float(float(v)) for v in window)
        result = bytearray()
        for value in values[:width * height]:
            numeric = scalar_decimal(value, kind)
            if numeric is None:
                result += bytes(4)
            else:
                normalized = min(Decimal(1), max(Decimal(0), (numeric - low) / (high - low)))
                code = int((normalized * 255).quantize(Decimal(1), rounding=ROUND_HALF_UP))
                result += bytes((code, code, code, 255))
        return bytes(result)


def write_fixtures(directory):
    import nrrd
    import numpy as np
    directory.mkdir(parents=True, exist_ok=True)
    assert nrrd.__version__ == "1.1.3", "Use the admitted pinned development oracle"
    cases, negatives = [], {}
    literals = {
        "u8": [0, 9, 10, 32, 35, 127, 128, 129, 254, 255, 1, 2, 3, 4, 5],
        "u16": [0, 1, 127, 128, 129, 254, 255, 256, 257, 1000, 4095, 4096, 32767, 32768, 65535],
        "i16": [-32768, -257, -256, -255, -129, -128, -1, 0, 1, 127, 128, 255, 256, 32766, 32767],
        "f32": [0x00000000, 0x80000000, 0x3f000000, 0x3f800000, 0xbf800000, 0x437f0000, 0x43800000,
                0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345, 0xffa54321, 0x00000001, 0x80000001],
        "i8": [-128, -127, -100, -10, -1, 0, 1, 5, 17, 31, 63, 64, 100, 126, 127],
        "u32": [0, 1, 2, 127, 128, 254, 255, 256, 65535, 65536, 2**31-1, 2**31, 2**31+1, 2**32-2, 2**32-1],
        "i32": [-2**31, -2**31+1, -65536, -257, -1, 0, 1, 127, 128, 255, 256, 65536, 2**30, 2**31-2, 2**31-1],
        "u64": [0, 1, 2, 127, 255, 256, 2**53-1, 2**53, 2**53+1, 2**53+2, 2**63-1, 2**63, 2**63+1, 2**64-2, 2**64-1],
        "i64": [-2**63, -2**63+1, -2**53-1, -2**53, -1, 0, 1, 127, 255, 256, 2**53-1, 2**53, 2**53+1, 2**63-2, 2**63-1],
        "f64": [0, 0x8000000000000000, 0x3fe0000000000000, 0x3ff0000000000001, 0xbff0000000000001,
                0x406fe00000000000, 0x4070000000000000, 0x7ff0000000000000, 0xfff0000000000000,
                0x7ff8000000001234, 0xfff8000000004321, 0x7ff0000000000001, 0xfff0000000000001, 1, 0x8000000000000001],
    }
    floats_second = [0x3e800000, 0x3f400000, 0x3fc00000, 0x40000000, 0xc0000000, 0x7f7fffff, 0xff7fffff,
                     0x00800000, 0x80800000, 0x7fc00001, 0xffc00002, 0x7fa00003, 0xffa00004, 0x42fe0000, 0x43000000]

    def values_for(kind, dimensions):
        result = []
        for z in range(dimensions[2] if len(dimensions) == 3 else 1):
            plane = literals[kind]
            if z:
                plane = floats_second if kind == "f32" else [v ^ 0x5555 for v in plane] if kind == "u16" else list(reversed(plane))
            result += plane
        assert len(result) == math.prod(dimensions)
        return result

    def authored(kind, dimensions, values, *, endian="little", alias=None, spacings=None, units=None, endings="lf",
                 extras=(), ci=False, encoding="raw", ascii_tokens=None):
        _, sample, pack, canonical = TYPES[kind]
        lines = ["NRRD0005", "# Repository-authored scalar codes", "type: " + (alias or canonical),
                 "dimension: " + str(len(dimensions)), "sizes: " + " ".join(map(str, dimensions)),
                 "kinds: " + " ".join("domain" if i % 2 == 0 else "space" for i in range(len(dimensions))),
                 "encoding: " + encoding]
        text_mode = encoding.lower() in ("ascii", "text", "txt")
        if sample > 1 and not text_mode:
            lines.append("endian: " + endian)
        if spacings is not None:
            lines.append("spacings: " + " ".join(map(str, spacings)))
        if units is not None:
            lines.append("units: " + " ".join(json.dumps(v, ensure_ascii=True) for v in units))
        lines += ["content: literal scalar test"] + list(extras)
        if ci:
            lines = [line if line.startswith(("NRRD", "#")) else line.split(": ", 1)[0].upper() + ": " + (line.split(": ", 1)[1].upper() if line.split(": ", 1)[0] in ("type", "encoding", "endian", "kinds") else line.split(": ", 1)[1]) for line in lines]
        header = b"".join(line.encode("ascii") + (b"\r\n" if endings == "crlf" or endings == "mixed" and i % 2 else b"\n") for i, line in enumerate(lines))
        header += b"\r\n" if endings == "crlf" else b"\n"
        if text_mode:
            assert ascii_tokens is not None or kind not in FLOAT_FORMAT
            spellings = ascii_tokens if ascii_tokens is not None else list(map(str, values))
            assert len(spellings) == len(values)
            whitespace = (" ", "\t", "\r\n", "\v", "\f")
            payload = "".join(whitespace[i % len(whitespace)] + value
                              for i, value in enumerate(spellings))
            return header + payload.encode("ascii") + b"\t "
        raw = struct.pack(("<" if endian == "little" else ">") + pack * len(values), *values)
        if encoding.lower() in ("gz", "gzip"):
            return header + gzip.compress(raw, mtime=0)
        if encoding.lower() in ("bz2", "bzip2"):
            return header + bz2.compress(raw)
        return header + raw

    def add(name, kind, dimensions=(5, 3), *, values=None, options="", window=(0, 255), oracle_seed=None,
            detached=False, detached_header="standard", **header_options):
        values = values_for(kind, dimensions) if values is None else list(values)
        width, height = dimensions[:2]
        assert len(values) == math.prod(dimensions)
        abi_type, sample, pack, canonical = TYPES[kind]
        expected = struct.pack("=" + pack * len(values), *values)
        companion = None
        if detached:
            path = directory / (name + "-header.nhdr")
            order = ">" if header_options.get("endian") == "big" else "<"
            dtype = np.dtype(order + FLOAT_FORMAT.get(kind, pack))
            volume = np.frombuffer(struct.pack(order + pack * len(values), *values), dtype=dtype)
            volume = volume.reshape(tuple(reversed(dimensions)))
            field_values = {"type": canonical, "encoding": header_options.get("encoding", "raw"),
                            "kinds": ["domain"] * len(dimensions)}
            for key in ("spacings", "units"):
                if key in header_options:
                    field_values[key] = header_options[key]
            nrrd.write(str(path), volume, field_values, index_order="C")
            data = path.read_bytes()
            reference = nrrd.read(str(path), index_order="C")
            companion = field_values["data file"]
            if detached_header == "eof":
                data = data.rstrip(b"\n").replace(b"data file:", b"datafile:", 1)
            elif detached_header == "lf":
                data = data.rstrip(b"\n") + b"\n"
            elif detached_header == "dot-relative":
                data = data.replace(b"data file: ", b"data file: ./", 1)
            path.write_bytes(data)
        else:
            data = authored(kind, dimensions, values, **header_options)
            path = directory / (name + ".nrrd")
            path.write_bytes(data)
        limitation = None
        try:
            array, _ = nrrd.read(str(path), index_order="C")
        except (KeyError, ValueError, nrrd.NRRDError) as error:
            if detached and detached_header != "standard":
                array, _ = reference
            else:
                assert oracle_seed is not None, (name, error)
                array, _ = nrrd.read(str(directory / (oracle_seed + ".nrrd")), index_order="C")
            limitation = str(error)
        assert array.shape == tuple(reversed(dimensions)) and array.dtype.itemsize == sample, name
        if array.dtype.byteorder not in ("=", "|") and (array.dtype.byteorder == "<") != (sys.byteorder == "little"):
            array = array.byteswap().view(array.dtype.newbyteorder("="))
        assert array.tobytes(order="C") == expected, name
        if oracle_seed:
            assert (directory / (oracle_seed + ".raw")).read_bytes() == expected
        rgba = rgba_oracle(values, kind, width, height, window)
        (directory / (name + ".raw")).write_bytes(expected)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if rgba[3::4] == bytes([255]) * (width * height):
            (directory / (name + ".rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        flags, physical = 0, [0., 0., 0.]
        spacings, units = header_options.get("spacings"), header_options.get("units")
        if spacings is not None and units is not None:
            for axis, (spacing, unit) in enumerate(zip(spacings, units)):
                if unit and math.isfinite(float(spacing)):
                    physical[axis] = float(spacing) * {"nm": .001, "um": 1., "mm": 1000., "cm": 10000., "m": 1000000.}[unit]
                    flags |= 1 << axis
        cases.append({"name": name, "kind": kind, "width": width, "height": height, "z_planes": dimensions[2] if len(dimensions) == 3 else 1,
                      "sample_type": abi_type, "sample_bytes": sample, "dimension_order": "XY" if len(dimensions) == 2 else "XYZ",
                      "options": options, "window": list(window), "calibration_flags": flags, "pixel_width_um": physical[0],
                      "pixel_height_um": physical[1], "z_spacing_um": physical[2],
                      "source_sha256": hashlib.sha256(data).hexdigest(), "raw_sha256": hashlib.sha256(expected).hexdigest(),
                      "rgba_sha256": hashlib.sha256(rgba).hexdigest(), "pynrrd_bit_exact": True,
                      "pynrrd_header_limitation": limitation, "canonical_oracle_seed": oracle_seed,
                      "source_file": path.name, "companion": companion,
                      "companion_sha256": hashlib.sha256((directory / companion).read_bytes()).hexdigest() if companion else None})
        return data

    for kind in TYPES:
        for endian in (("little",) if TYPES[kind][1] == 1 else ("little", "big")):
            for dims in ((5, 3), (5, 3, 2)):
                add(f"{kind}-{endian}-{len(dims)}d", kind, dims, endian=endian)
    aliases = {"u8": ("uchar", "unsigned char", "uint8_t"),
               "u16": ("ushort", "unsigned short", "unsigned short int", "uint16_t"),
               "i16": ("short", "short int", "signed short", "signed short int", "int16_t"),
               "i8": ("signed char", "int8_t"), "u32": ("uint", "unsigned int", "uint32_t"),
               "i32": ("int", "signed int", "int32_t"),
               "u64": ("ulonglong", "unsigned long long", "unsigned long long int", "uint64_t"),
               "i64": ("longlong", "long long", "long long int", "signed long long", "signed long long int", "int64_t")}
    for kind, variants in aliases.items():
        for i, alias in enumerate(variants):
            add(f"alias-{kind}-{i}", kind, alias=alias)
    main_kwargs = {"spacings": (.25, .5, 1.5), "units": ("um", "um", "um")}
    main = add("u16-main", "u16", (5, 3, 2), **main_kwargs)
    source_changed = main.replace(b"\n\n", b"\n# authored source identity mutation\n\n", 1)
    source_changed_path = directory / "u16-source-changed.nrrd"
    source_changed_path.write_bytes(source_changed)
    changed_array, _ = nrrd.read(str(source_changed_path), index_order="C")
    assert changed_array.tobytes(order="C") == (directory / "u16-main.raw").read_bytes()
    add("float-volume", "f32", (5, 3, 2), endian="big")
    add("case-insensitive", "u16", (5, 3, 2), ci=True, oracle_seed="u16-little-3d")
    add("header-crlf", "u16", endings="crlf")
    add("header-mixed-endings", "u16", endings="mixed")
    for first in (9, 10, 32, 35):
        add("raster-leading-" + str(first), "u8", values=[first] + literals["u8"][1:])
    for unit, spacing in (("nm", (250, 500)), ("um", (.25, .5)), ("mm", (.00025, .0005)), ("cm", (.000025, .00005)), ("m", (.00000025, .0000005))):
        add("units-" + unit, "u16", spacings=spacing, units=(unit, unit))
    add("spacing-without-units", "u16", spacings=(.25, .5))
    add("units-without-spacing", "u16", units=("um", "mm"))
    add("calibration-partial", "u16", (5, 3, 2), spacings=(.25, "nan", 1.5), units=("um", "um", ""))
    add("u16-main-wide", "u16", (5, 3, 2), options='{"window":[0,65535]}', window=(0, 65535), **main_kwargs)
    add("i16-signed-window", "i16", options='{"window":[-32768,32767]}', window=(-32768, 32767))
    add("float-unit-window", "f32", (5, 3, 2), options='{"window":[0,1]}', window=(0, 1))
    add("window-exponents", "u16", options='{"window":[0e0,2.55E+2]}')
    long_window = '{"window":[0,1.' + '0' * (512 - len('{"window":[0,1.]}')) + ']}'
    assert len(long_window) == 512
    add("window-512bytes", "f32", options=long_window, window=(0, 1))
    add("float-nonfinite-only", "f32", values=[(0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345)[i % 5] for i in range(15)])
    add("type-word-tabs", "u16", alias="unsigned\tshort\tint", oracle_seed="u16-little-2d")
    add("gzip-u16", "u16", (5, 3, 2), encoding="gzip", **main_kwargs)
    add("gzip-i16", "i16", (5, 3, 2), encoding="gz", endian="big", **main_kwargs)
    add("gzip-f32", "f32", (5, 3, 2), encoding="gzip", endian="big")
    add("gzip-uppercase", "u16", (5, 3, 2), encoding="GZIP", oracle_seed="gzip-u16", **main_kwargs)
    multiblock = add("gzip-multiblock", "u8", (512, 320), encoding="gzip",
                     values=random.Random(117).randbytes(512 * 320))
    assert len(multiblock) > 2 * 65536
    add("detached-u16", "u16", (5, 3, 2), endian="big", detached=True, **main_kwargs)
    add("detached-i16", "i16", (5, 3, 2), endian="big", encoding="gzip", detached=True, **main_kwargs)
    add("detached-f32", "f32", (5, 3, 2), endian="big", encoding="gzip", detached=True, detached_header="eof")
    add("detached spaced u16", "u16", (5, 3, 2), endian="big", encoding="gzip", detached=True,
        detached_header="dot-relative", **main_kwargs)
    add("ascii-u8", "u8", encoding="ascii")
    add("text-u16", "u16", (5, 3, 2), encoding="text", **main_kwargs)
    add("txt-i16", "i16", encoding="txt", ascii_tokens=[
        "-32768", "-257", "-256", "-255", "-129", "-128", "-1", "-0", "+1", "+127",
        "128", "255", "256", "32766", "+32767"])
    add("ascii-f32", "f32", encoding="ascii",
        values=[0, 0x80000000, 0x3f000000, 0x3f800000, 0xbf800000, 0x437f0000,
                0x43800000, 0x7f800000, 0xff800000, 0x7fc00000, 0x7fc00000, 1,
                0x80000001, 0x7f7fffff, 0xff7fffff],
        extras=("endian: big",), ascii_tokens=["0.0", "-0e0", ".5", "+1.", "-1", "255",
            "256", "+iNF", "-Infinity", "NaN", "-nan", "1.40129846e-45", "-1.40129846e-45",
            "3.402823466e38", "-3.402823466e38"])
    add("detached-ascii-i16", "i16", (5, 3, 2), encoding="ascii", detached=True, **main_kwargs)
    add("detached-ascii-u16", "u16", (5, 3, 2), encoding="ascii", detached=True, **main_kwargs)
    text_float_values = [0x7fc00000 if bits & 0x7fffffff > 0x7f800000 else bits
                         for bits in values_for("f32", (5, 3, 2))]
    add("detached-ascii-f32", "f32", (5, 3, 2), values=text_float_values, encoding="ascii", detached=True)
    add("detached-lf", "u8", detached=True, detached_header="lf")
    for kind in ("i8", "u32", "i32", "u64", "i64"):
        add("ascii-" + kind, kind, encoding="ascii")
    add("ascii-f64", "f64", encoding="ascii",
        values=[0, 0x8000000000000000, 0x3fe0000000000000, 0x3ff0000000000001, 0xbff0000000000001,
                0x406fe00000000000, 0x4070000000000000, 0x7ff0000000000000, 0xfff0000000000000,
                0x7ff8000000000000, 0x7ff8000000000000, 1, 0x8000000000000001,
                0x7fefffffffffffff, 0xffefffffffffffff],
        ascii_tokens=["0", "-0e0", ".5", "1.0000000000000002", "-1.0000000000000002", "255", "256",
                      "+inf", "-infinity", "NaN", "-nan", "4.9406564584124654e-324",
                      "-4.9406564584124654e-324", "1.7976931348623157e308", "-1.7976931348623157e308"])
    add("detached-gzip-f64", "f64", (5, 3, 2), encoding="gzip", endian="big", detached=True)
    add("u64-high-window", "u64", options='{"window":[9007199254740992,9007199254740996]}',
        window=(2**53, 2**53+4))

    def add_skip(name, kind, *, skip_from_end=False, trailer=b"", codec="raw", detached=False,
                 prefix=b"external\0\xff\r\n", preamble=b"", lines=None, line_key="line skip", **options):
        add(name, kind, (5, 3, 2), encoding=codec, detached=detached, **options)
        case = cases[-1]
        path = directory / case["source_file"]
        head, inline = path.read_bytes().split(b"\n\n", 1)
        sidecar = directory / case["companion"] if detached else None
        original = sidecar.read_bytes() if sidecar else inline
        if codec in COMPRESSED:
            encode, decode = COMPRESSED[codec]
            decoded = decode(original)
            payload = encode(prefix + decoded)
            assert decode(payload)[len(prefix):] == decoded
        else:
            payload = prefix + original + trailer
            assert payload[len(prefix):len(prefix) + len(original)] == original
        fields = []
        if prefix or skip_from_end:
            fields.append("byteskip: -1" if skip_from_end else f"byte skip: {len(prefix)}")
        if lines is not None:
            fields.append(f"{line_key}: {lines}")
        written_header = head + ("\n" + "\n".join(fields) + "\n\n").encode("ascii")
        payload = preamble + payload
        updated = written_header if sidecar else written_header + payload
        if sidecar:
            sidecar.write_bytes(payload)
            case["companion_sha256"] = hashlib.sha256(payload).hexdigest()
        path.write_bytes(updated)
        case["source_sha256"] = hashlib.sha256(updated).hexdigest()
        if skip_from_end and lines:
            assert payload[-len(original):] == original
            case["pynrrd_bit_exact"] = False
            case["pynrrd_header_limitation"] = "pynrrd loops through line skips before seeking to EOF"
        elif (codec in COMPRESSED and prefix) or trailer:
            case["pynrrd_bit_exact"] = False
            case["pynrrd_header_limitation"] = (
                "pynrrd also seeks the compressed input before decoding" if codec in COMPRESSED
                else "pynrrd materializes the trailing raw bytes")
        else:
            reopened, _ = nrrd.read(path, index_order="C")
            assert reopened.astype(reopened.dtype.newbyteorder("="), copy=False).tobytes() == (directory / (name + ".raw")).read_bytes()

    add_skip("attached-skip-u16", "u16", endian="big", **main_kwargs)
    add_skip("attached-eof-i16", "i16", skip_from_end=True)
    add_skip("detached-skip-u16", "u16", endian="big", trailer=b"unrelated suffix", detached=True, **main_kwargs)
    add_skip("detached-eof-i16", "i16", endian="big", skip_from_end=True, detached=True, **main_kwargs)
    add_skip("attached-skip-text", "i16", codec="ascii")
    add_skip("detached-skip-f32", "f32", endian="big", codec="gzip", detached=True)
    add_skip("attached-line-zero", "u16", lines=0, prefix=b"", **main_kwargs)
    add_skip("attached-line-ascii", "i16", codec="ascii", preamble=b"first\r\n\n", lines=2)
    add_skip("attached-line-raw", "i16", prefix=b"", preamble=b"first\r\n\n", lines=2,
             trailer=b"extra bytes")
    add_skip("attached-line-gzip", "u16", codec="gzip", preamble=b"first\r\nsecond\n", lines=2)
    add_skip("detached-line-u16", "u16", detached=True, endian="big", lines=2, line_key="lineskip",
             preamble=b"a" * 4095 + b"\r\n\n", **main_kwargs)
    add_skip("detached-line-f32", "f32", detached=True, endian="big", codec="gzip", lines=2,
             prefix=b"", preamble=b"outside gzip\r\n\n")
    add_skip("detached-line-eof-i16", "i16", detached=True, endian="big", skip_from_end=True,
             lines=2147483647, **main_kwargs)
    add("attached-bzip2-i16", "i16", (5, 3, 2), encoding="bzip2", endian="big", **main_kwargs)
    add("attached-bz2-u64", "u64", (5, 3, 2), encoding="bz2", endian="big")
    add("detached-bzip2-u16", "u16", (5, 3, 2), encoding="bzip2", endian="big", detached=True, **main_kwargs)
    add_skip("detached-bzip2-f32", "f32", codec="bzip2", detached=True, endian="big",
             preamble=b"prefix line\r\n", lines=1)
    add("bzip2-multiblock", "u8", (1024, 1024), encoding="bzip2", values=random.Random(918).randbytes(1024*1024))

    def hex_text(data, mixed=False):
        cases = data.hex().encode("ascii")
        if mixed:
            space = (b" ", b"\t", b"\r\n", b"\v", b"\f")
            cases = b"".join(bytes([c]).upper() + space[i % len(space)] if i % 2 == 0 else bytes([c])
                             for i, c in enumerate(cases))
        else:
            cases = b"\n".join(cases[i:i+70] for i in range(0, len(cases), 70))
        assert bytes.fromhex(b"".join(cases.split()).decode("ascii")) == data
        return cases + b"\n"

    def add_hex(name, kind, *, detached=False, mixed=False, preamble=b"", offset=b"", **options):
        add(name, kind, (5, 3, 2), detached=detached, **options)
        case = cases[-1]
        path = directory / case["source_file"]
        head, inline = path.read_bytes().split(b"\n\n", 1)
        companion = directory / case["companion"] if detached else None
        original = companion.read_bytes() if companion else inline
        head = head.replace(b"encoding: raw", b"encoding: HeX" if mixed else b"encoding: hex", 1)
        if preamble:
            head += b"\nline skip: " + str(preamble.count(b"\n")).encode()
        if offset:
            head += b"\nbyte skip: " + str(len(offset)).encode()
        payload = preamble + offset + hex_text(original, mixed)
        if companion:
            companion.write_bytes(payload)
            case["companion_sha256"] = hashlib.sha256(payload).hexdigest()
        rewritten = head + b"\n\n" + (b"" if detached else payload)
        path.write_bytes(rewritten)
        case.update(source_sha256=hashlib.sha256(rewritten).hexdigest(), pynrrd_bit_exact=False,
                    pynrrd_header_limitation="pynrrd checked the exact raw raster before independent hex encoding")

    add_hex("hex-u8", "u8")
    add_hex("hex-offset-i16", "i16", endian="big", mixed=True, preamble=b"header\r\n\n", offset=b"XX\0")
    add_hex("hex-f64", "f64", endian="little", mixed=True)
    add_hex("detached-hex-u16", "u16", detached=True, endian="big", **main_kwargs)
    add_hex("detached-hex-f32", "f32", detached=True, endian="big", mixed=True, offset=b"start")
    add_hex("detached-hex-f64", "f64", detached=True, endian="big")

    def add_list(name, kind, dims, count, *, subdim=None, codec="raw", prefix=b"", pattern=None,
                 pattern_first=False, relative_first=False, **options):
        data = add(name, kind, dims, encoding="raw" if codec == "hex" else codec, **options)
        case = cases[-1]
        header, payload = data.split(b"\n\n", 1)
        if codec == "hex":
            header = header.replace(b"encoding: raw", b"encoding: hex", 1)
        if codec in COMPRESSED:
            payload = COMPRESSED[codec][1](payload)
        elif codec == "ascii":
            payload = payload.split()
        assert len(payload) % count == 0
        extent = len(payload) // count
        if pattern:
            fmt, first, last, step = pattern
            names = [fmt % i for i in range(first, last + (1 if step > 0 else -1), step)]
            assert len(names) == count
        else:
            names = [f"{name}-{i:02d}.part" for i in reversed(range(1, count + 1))]
        for i, part in enumerate(names):
            piece = payload[i * extent:(i + 1) * extent]
            if codec == "ascii":
                piece = b" ".join(piece)
            if codec in COMPRESSED:
                piece = COMPRESSED[codec][0](prefix + piece)
            elif codec == "hex":
                piece = prefix + hex_text(piece)
            else:
                piece = prefix + piece
            (directory / part).write_bytes(piece)
        field = b"\nbyte skip: " + str(len(prefix)).encode() if prefix else b""
        form = " ".join(map(str, pattern)) if pattern else "LIST"
        if subdim is not None:
            form += f" {subdim}"
        declaration = f"\ndata file: {form}\n".encode()
        if pattern_first:
            magic, fields = header.split(b"\n", 1)
            listed = magic + declaration + fields + field + b"\n"
        else:
            listed_names = ["./" + names[0], *names[1:]] if relative_first else names
            tail = b"\n" if pattern else "\n".join(listed_names).encode()
            listed = header + field + declaration + tail
        path = directory / (name + ".nhdr")
        path.write_bytes(listed)
        case.update(source_file=path.name, source_sha256=hashlib.sha256(listed).hexdigest(),
                    companions=names, companion_hashes={part: hashlib.sha256((directory / part).read_bytes()).hexdigest() for part in names},
                    pynrrd_bit_exact=False, pynrrd_header_limitation="pynrrd checked the equivalent single raster before splitting")

    add_list("list-project-u16", "u16", (5, 3, 2), 2, endian="big", **main_kwargs)
    add_list("list spaced u16", "u16", (5, 3, 2), 2, endian="big", relative_first=True, **main_kwargs)
    add_list("list-slices-i16", "i16", (5, 3, 2), 2, subdim=2, endian="big", **main_kwargs)
    add_list("list-rows-u8", "u8", (5, 3), 3, codec="ascii")
    add_list("list-volume-rows-i16", "i16", (5, 3, 2), 6, subdim=1)
    add_list("list-slabs-f32", "f32", (5, 3, 2), 2, subdim=3, codec="gzip", prefix=b"XYZ", endian="big")
    add_list("list-single-slab-u16", "u16", (5, 3, 2), 1, subdim=3, **main_kwargs)
    add_list("list-63-rows", "u8", (1, 63), 63, values=list(range(63)))
    add_list("numbered-project-u16", "u16", (5, 3, 2), 2, endian="big",
             pattern=("numbered-project-u16-%02d.part", 1, 2, 1), **main_kwargs)
    add_list("numbered-descending-i16", "i16", (5, 3, 2), 2, endian="big",
             pattern=("numbered-descending-%03d.part", 1, -2, -2), **main_kwargs)
    add_list("numbered-extreme-i16", "i16", (5, 3, 2), 2,
             pattern=("numbered-extreme-%i.part", 0, -2147483648, -2147483648))
    add_list("numbered-slabs-f32", "f32", (5, 3, 2), 2, subdim=3, codec="gzip", prefix=b"XYZ",
             pattern=("numbered-slabs-%d.part", 2147483646, 2147483647, 1))
    add_list("numbered-text-rows", "u8", (5, 3), 3, codec="ascii",
             pattern=("numbered-rows-%d.part", 1, 8, 3), pattern_first=True)
    add_list("numbered-63-rows", "u8", (1, 63), 63, values=list(range(63)),
             pattern=("numbered-63-row-%03d.part", 62, 0, -1))
    add_list("list-u64", "u64", (5, 3, 2), 2, endian="big")
    add_list("list-bzip2-f64", "f64", (5, 3, 2), 2, endian="big", codec="bzip2")
    add_list("list-hex-u64", "u64", (5, 3, 2), 2, endian="big", codec="hex")
    add_list("numbered-hex-i16", "i16", (5, 3, 2), 2, endian="big", codec="hex", prefix=b"hdr",
             pattern=("hex-series-%d.part", 2, 1, -1), **main_kwargs)

    def bad(name, data, status, *, extension=".nrrd"):
        filename = name + extension
        (directory / filename).write_bytes(data)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(data).hexdigest(), "source_file": filename}

    bad("signature", b"NOPE0005" + main[8:], 2)
    bad("version2", main.replace(b"NRRD0005", b"NRRD0002", 1), 2)
    ascii_prefix = authored("u16", (2, 2), [0, 1, 2, 3], encoding="ascii").split(b"\n\n", 1)[0] + b"\n\n"
    for name, payload in (
        ("ascii-short", b"1 2 3"), ("ascii-extra", b"1 2 3 4 5"),
        ("ascii-suffix", b"1 2 3 4junk"), ("ascii-range", b"1 2 3 65536"),
        ("ascii-negative-unsigned", b"1 2 3 -1"), ("ascii-sign", b"1 2 3 +-4"),
        ("ascii-long-token", b"1 2 3 " + b"0" * 129),
        ("ascii-nul", b"1 2 3 4\0"), ("ascii-comma", b"1, 2 3 4"),
    ):
        bad(name, ascii_prefix + payload, 4)
    bad("ascii-float-overflow", ascii_prefix.replace(b"type: uint16", b"type: float") + b"1 2 3 1e40", 4)
    bad("ascii-float-nan-payload", ascii_prefix.replace(b"type: uint16", b"type: float") + b"1 2 3 nan(1)", 4)
    for kind, token in (("int8", b"128"), ("uint32", b"4294967296"), ("int32", b"-2147483649"),
                        ("uint64", b"18446744073709551616"), ("int64", b"9223372036854775808"),
                        ("double", b"1e309")):
        bad("ascii-overflow-" + kind, ascii_prefix.replace(b"type: uint16", b"type: " + kind.encode()) + b"1 2 3 " + token, 4)
    for name, old, new, status in (("missing-type", b"type: uint16\n", b"", 4), ("missing-dimension", b"dimension: 3\n", b"", 4),
                                   ("missing-sizes", b"sizes: 5 3 2\n", b"", 4), ("missing-encoding", b"encoding: raw\n", b"", 4),
                                   ("missing-endian", b"endian: little\n", b"", 4), ("missing-kinds", b"kinds: domain space domain\n", b"", 2),
                                   ("kinds-count", b"kinds: domain space domain", b"kinds: domain domain", 4),
                                   ("kinds-time", b"kinds: domain space domain", b"kinds: domain space time", 2),
                                   ("kinds-vector", b"kinds: domain space domain", b"kinds: vector space domain", 2),
                                   ("sizes-count", b"sizes: 5 3 2", b"sizes: 5 3", 4), ("size-zero", b"sizes: 5 3 2", b"sizes: 5 0 2", 4),
                                   ("dimension1", b"dimension: 3", b"dimension: 1", 2), ("dimension4", b"dimension: 3", b"dimension: 4", 4),
                                   ("type-block", b"type: uint16", b"type: block", 2), ("type-f128", b"type: uint16", b"type: float128", 2),
                                   ("encoding-gzip-raw-payload", b"encoding: raw", b"encoding: gzip", 4), ("encoding-unknown", b"encoding: raw", b"encoding: xyz", 2),
                                   ("endian-invalid", b"endian: little", b"endian: middle", 4), ("leading-field-space", b"type: uint16", b" type: uint16", 4),
                                   ("missing-colon-space", b"type: uint16", b"type:uint16", 4),
                                   ("spacing-negative", b"spacings: 0.25 0.5 1.5", b"spacings: -0.25 0.5 1.5", 2),
                                   ("spacing-zero", b"spacings: 0.25 0.5 1.5", b"spacings: 0 0.5 1.5", 4),
                                   ("spacing-inf", b"spacings: 0.25 0.5 1.5", b"spacings: inf 0.5 1.5", 4),
                                   ("spacing-count", b"spacings: 0.25 0.5 1.5", b"spacings: 0.25 0.5", 4),
                                   ("units-unknown", b'units: "um" "um" "um"', b'units: "microns" "um" "um"', 2),
                                   ("units-case-sensitive", b'units: "um" "um" "um"', b'units: "UM" "um" "um"', 2),
                                   ("units-count", b'units: "um" "um" "um"', b'units: "um" "um"', 4),
                                   ("units-unquoted", b'units: "um" "um" "um"', b'units: um "um" "um"', 4)):
        assert old in main, name
        bad(name, main.replace(old, new, 1), status)
    bad("duplicate-type", main.replace(b"type: uint16\n", b"type: uint16\nTYPE: uint16\n", 1), 4)
    bad("duplicate-sizes", main.replace(b"sizes: 5 3 2\n", b"sizes: 5 3 2\nsizes: 5 3 2\n", 1), 4)
    bad("header-nonascii", main.replace(b"literal scalar test", b"literal\xc2\xb5", 1), 4)
    bad("header-nul", main.replace(b"literal scalar test", b"literal\0test", 1), 4)
    bad("missing-separator", main.replace(b"\n\n", b"\n", 1), 4)
    bad("payload-truncated", main[:-1], 4)
    bad("payload-trailing", main + b"\0", 4)
    bad("concatenated", main + main, 4)
    header, raw = main.split(b"\n\n", 1)
    hex_header = authored("u8", (2, 2), [0, 1, 2, 3]).split(b"\n\n", 1)[0].replace(b"encoding: raw", b"encoding: hex")
    for name, encoded in (("empty", b""), ("short", b"00 01 02"), ("odd", b"00 01 02 3"),
                          ("long", b"00 01 02 03 04"), ("digit", b"00 01 02 0G"),
                          ("nul", b"00 01\0 02 03"), ("terminator", b"00 01 02 03>")):
        bad("hex-" + name, hex_header + b"\n\n" + encoded, 4)
    bad("hex-no-endian", header.replace(b"encoding: raw", b"encoding: hex").replace(b"\nendian: little", b"") + b"\n\n" + hex_text(raw), 4)
    bad("hex-eof-skip", hex_header + b"\nbyte skip: -1\n\n00 01 02 03", 2)
    gzip_header = header.replace(b"encoding: raw", b"encoding: gzip", 1) + b"\n\n"
    member = gzip.compress(raw, mtime=0)
    bz_header = header.replace(b"encoding: raw", b"encoding: bzip2", 1) + b"\n\n"
    bz_member = bz2.compress(raw)
    for name, blob in (("bare-raw", raw), ("empty", b""), ("truncated", bz_member[:-1]),
                       ("crc", bz_member[:10] + bytes([bz_member[10] ^ 1]) + bz_member[11:]),
                       ("short-output", bz2.compress(raw[:-1])), ("long-output", bz2.compress(raw + b"\x00")),
                       ("trailing", bz_member + b"\x00"), ("concatenated", bz_member + bz2.compress(b""))):
        bad("bzip2-" + name, bz_header + blob, 4)
    bad("bzip2-eof-skip", bz_header.replace(b"\n\n", b"\nbyte skip: -1\n\n", 1) + bz_member, 2)
    for name, token in (("empty", ""), ("negative", "-2"), ("suffix", "1x"),
                        ("overflow", "2147483648"), ("double-sign", "+-1"),
                        ("truncated-window", "1")):
        bad("byte-skip-" + name, header + f"\nbyte skip: {token}\n\n".encode("ascii") + raw, 4)
    bad("byte-skip-duplicate", header + b"\nbyte skip: 0\nbyteskip: 0\n\n" + raw, 4)
    bad("byte-skip-eof-short", header + b"\nbyte skip: -1\n\n" + raw[:-1], 4)
    bad("byte-skip-gzip-eof", gzip_header.replace(b"\n\n", b"\nbyte skip: -1\n\n", 1) + member, 2)
    bad("byte-skip-gzip-short", gzip_header.replace(b"\n\n", b"\nbyte skip: 1\n\n", 1) + member, 4)
    bad("byte-skip-gzip-budget", gzip_header.replace(b"\n\n", b"\nbyte skip: 2147483647\n\n", 1) + member, 3)
    bad("byte-skip-ascii-eof", ascii_prefix.replace(b"\n\n", b"\nbyte skip: -1\n\n", 1) + b"1 2 3 4", 2)
    bad("byte-skip-ascii-short", ascii_prefix.replace(b"\n\n", b"\nbyte skip: 8\n\n", 1) + b"1 2 3 4", 4)
    for name, token in (("empty", ""), ("negative", "-1"), ("suffix", "1x"), ("overflow", "2147483648")):
        bad("line-skip-" + name, header + f"\nline skip: {token}\n\n".encode("ascii") + raw, 4)
    bad("line-skip-duplicate", header + b"\nline skip: 0\nlineskip: 0\n\n" + raw, 4)
    bad("line-skip-short", header + b"\nline skip: 2\n\nfirst\npartial\r", 4)
    bad("gzip-zlib-wrapper", gzip_header + zlib.compress(raw), 4)
    bad("gzip-empty-payload", gzip_header, 4)
    bad("gzip-crc", gzip_header + member[:-8] + bytes([member[-8] ^ 1]) + member[-7:], 4)
    bad("gzip-isize", gzip_header + member[:-1] + bytes([member[-1] ^ 1]), 4)
    bad("gzip-truncated", gzip_header + member[:-1], 4)
    bad("gzip-short-output", gzip_header + gzip.compress(raw[:-1], mtime=0), 4)
    bad("gzip-excess-output", gzip_header + gzip.compress(raw + b"\0" * 65536, mtime=0), 4)
    bad("gzip-trailing", gzip_header + member + b"\0", 4)
    bad("gzip-concatenated", gzip_header + member + gzip.compress(b"", mtime=0), 4)
    bad("header-bound", main.replace(b"# Repository-authored scalar codes\n", b"#" + b"a" * 65536 + b"\n", 1), 3)
    bad("size-arithmetic-bound", main.replace(b"sizes: 5 3 2", b"sizes: 9223372036854775807 2 2", 1), 3)
    bad("calibration-overflow", main.replace(b"spacings: 0.25 0.5 1.5", b"spacings: 1e308 0.5 1.5", 1).replace(b'units: "um"', b'units: "m"', 1), 2)
    for name, field in (("space", "space: right-anterior-superior"), ("space-origin", "space origin: (0,0,0)"),
                        ("space-directions", "space directions: (1,0,0) (0,1,0) (0,0,1)"), ("measurement-frame", "measurement frame: (1,0,0) (0,1,0) (0,0,1)"),
                        ("axis-mins", "axis mins: 0 0 0"), ("axis-maxs", "axis maxs: 5 3 2"), ("centers", "centers: cell cell cell"),
                        ("thicknesses", "thicknesses: 1 1 1"), ("sample-units", "sample units: counts"), ("labels", 'labels: "x" "y" "z"'),
                        ("custom-key", "authored:=uninterpreted"), ("unknown-field", "authored meaning: unknown")):
        bad(name, main.replace(b"\n\n", b"\n" + field.encode("ascii") + b"\n\n", 1), 2)

    def bad_detached(name, field, status, suffix=b"\n\n"):
        bad(name, header + b"\ndata file: " + field + suffix, status, extension=".nhdr")

    for name, field in (
        ("empty-component", b"."),
        ("parent-component", b"..\\outside.raw"), ("subdir-component", b"child/payload.raw"),
        ("drive-path", b"C:\\payload.raw"), ("stream-suffix", b"payload.raw:alternate"),
        ("unc-path", b"\\\\server\\share\\payload.raw"), ("trailing-dot", b"payload.raw."),
        ("tab-component", b"payload\tfile.raw"), ("windows-device", b"Con.raw"),
        ("windows-device-spaced", b"./Con .raw"), ("dot-prefix-traversal", b"./../payload.raw"),
        ("primary-alias", b"DETACHED-PRIMARY-ALIAS.NHDR"),
    ):
        bad_detached("detached-" + name, field, 2)
    bad_detached("detached-missing", b"absent-payload.raw", 4)
    bad_detached("list-empty", b"LIST", 4, suffix=b"\n")
    bad_detached("list-subdimension", b"LIST 4\nx.raw", 4, suffix=b"\n")
    bad_detached("list-count", b"LIST\nx.raw", 4, suffix=b"\n")
    bad_detached("list-duplicate", b"LIST\na.raw\nA.raw", 4, suffix=b"\n")
    bad_detached("list-relative-duplicate", b"LIST\n./slice one.raw\nSLICE ONE.raw", 4, suffix=b"\n")
    bad_detached("list-trailing-space", b"LIST\nslice one.raw \nother.raw", 2, suffix=b"\n")
    bad_detached("list-escape", b"LIST\n../x.raw\nother.raw", 2, suffix=b"\n")
    bad_detached("list-missing-member", b"LIST\nlist-project-u16-02.part\nabsent-file.part", 4, suffix=b"\n")
    bad_detached("list-member-extent", b"LIST\nlist-project-u16-02.part\ndetached-u16-header.raw", 4, suffix=b"\n")
    bad_detached("list-unequal-slabs", b"LIST 3\nx.raw\ny.raw\nz.raw", 4, suffix=b"\n")
    bad_detached("list-over-limit", b"LIST\n" + b"\n".join(f"{i}.raw".encode() for i in range(64)), 3, suffix=b"\n")
    for name, field, status in (
        ("zero-step", b"slice%d.raw 0 1 0", 4),
        ("wrong-direction", b"slice%d.raw 0 1 -1", 4),
        ("outside-int32", b"slice%d.raw 2147483648 2147483648 1", 4),
        ("too-many", b"slice%d.raw -2147483648 2147483647 1", 3),
        ("count-mismatch", b"slice%d.raw 0 2 1", 4),
        ("path-component", b"child/slice%d.raw 0 1 1", 2),
        ("width-limit", b"slice%0256d.raw 0 1 1", 3),
        ("missing-member", b"numbered-project-u16-%02d.part 2 3 1", 4),
        ("format-outside-profile", b"slice%x.raw 0 1 1", 2),
        ("two-conversions", b"slice%d-%d.raw 0 1 1", 2),
    ):
        bad_detached("numbered-" + name, field, status)
    bad_detached("detached-line-skip-short", b"detached-u16-header.raw\nline skip: 200", 4)
    bad_detached("detached-duplicate", b"detached-u16-header.raw\ndatafile: detached-u16-header.raw", 4)
    bad_detached("detached-inline-tail", b"detached-u16-header.raw", 4, b"\n\n" + raw)
    hardlink_target = directory / "hardlink-target.raw"
    hardlink_target.write_bytes(raw)
    hardlink_alias = directory / "hardlink-payload.raw"
    if not hardlink_alias.exists():
        os.link(hardlink_target, hardlink_alias)
    assert hardlink_target.samefile(hardlink_alias)
    bad_detached("detached-hardlink", b"hardlink-payload.raw", 2)
    for name, data in (("short", raw[:-1]), ("overlong", raw + b"\0")):
        companion = "detached-raw-" + name + ".payload"
        (directory / companion).write_bytes(data)
        bad_detached("detached-raw-" + name, companion.encode("ascii"), 4)
    corrupted = member[:-8] + bytes([member[-8] ^ 1]) + member[-7:]
    (directory / "bad-gzip.payload").write_bytes(corrupted)
    bad("detached-gzip-crc", gzip_header[:-1] + b"data file: bad-gzip.payload\n\n", 4, extension=".nhdr")

    resource_values = [17] * (256 * 256)
    resource = authored("u8", (256, 256), resource_values)
    resource_path = directory / "budget-input.nrrd"
    resource_path.write_bytes(resource)
    array, _ = nrrd.read(str(resource_path), index_order="C")
    assert array.shape == (256, 256) and array.tobytes() == bytes(resource_values)
    assert len(resource) > 32768 and len(resource) < 256 * 1024 * 1024
    unicode_name = "NRRD-\u03bc-\u56fe.nrrd"
    (directory / unicode_name).write_bytes(main)
    manifest = {"schema": "qpx.nrrd-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"pynrrd": nrrd.__version__, "numpy": np.__version__, "index_order": "C", "native_byte_order": sys.byteorder,
                           "basis": "Authored integer and IEEE bitwords; pynrrd z,y,x array byte checks with byte-swap/view only; Decimal190 display math, with declared binary64 rounding for wide integers",
                           "header_adapter_limit": "pynrrd field/type tokens are case-sensitive and type-word tabs are not normalized; normative variants use a canonical-header seed with identical literal payload."},
                "unicode_filename": {"path": unicode_name, "control": "u16-main", "sha256": hashlib.sha256(main).hexdigest()},
                "source_change_control": {"filename": "u16-source-changed.nrrd", "sha256": hashlib.sha256(source_changed).hexdigest(),
                                          "pynrrd_bit_exact": True, "raw_codes_unchanged": True},
                "resource_fixtures": {"budget-input": {"filename": "budget-input.nrrd", "encoded_bytes": len(resource), "work_budget": 32768,
                                       "sha256": hashlib.sha256(resource).hexdigest(), "pynrrd_bit_exact": True,
                                       "scope": "One owned inline raw input exceeds caller budget; no expanded decoder/RGBA cache claim"}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "source_file", "width", "height", "z_planes", "sample_type", "sample_bytes", "dimension_order", "calibration_flags", "pixel_width_um", "pixel_height_um", "z_spacing_um")
    with (directory / "fixtures.tsv").open("w", encoding="utf8", newline="") as index:
        for c in cases:
            values = [json.dumps(c[key]) if key in ("name", "source_file") else str(c[key]) for key in columns]
            companions = "|".join(c.get("companions", [])) or c["companion"] or "-"
            index.write("\t".join([*values, json.dumps(companions), c["options"] or "-"]) + "\n")
    (directory / "negative.tsv").write_text("".join(f"{row['source_file']}\t{row['status']}\n" for row in negatives.values()), encoding="utf8")
    return manifest

def write_axis_fixtures(directory):
    import nrrd
    import numpy as np

    directory.mkdir()
    rows = []
    volume = [100*t + 20*z + 2*y + x for t in range(3) for z in range(2)
              for y in range(2) for x in range(2)]
    series = [100*t + 2*y + x for t in range(4) for y in range(2) for x in range(2)]

    def header(shape, kinds, encoding, spacings, units, endian="big"):
        return (f"NRRD0005\ntype: uint16\ndimension: {len(shape)}\n"
                f"sizes: {' '.join(map(str, shape))}\nkinds: {kinds}\n"
                f"encoding: {encoding}\nendian: {endian}\nspacings: {spacings}\nunits: {units}\n").encode("ascii")

    def expected(name, file, samples, order, z, t, flags, interval, companions="-", calibration=None):
        (directory / (name + ".raw")).write_bytes(struct.pack(f"<{len(samples)}H", *samples))
        (directory / (name + ".rgba")).write_bytes(
            bytes(v for s in samples[:4] for v in (s, s, s, 255)))
        px, py, pz = calibration if calibration else (0.5, 1.25, 3 if z > 1 else 0)
        rows.append((name, file, 2, 2, z, 2, 2, order, flags, px, py, pz, companions, "-", t, interval))

    xyzt = header((2, 2, 2, 3), "space space space time", "raw", "0.5 1.25 3 250", '"um" "um" "um" "ms"')
    binary = struct.pack(">24H", *volume)
    (directory / "time-xyzt.nrrd").write_bytes(xyzt + b"\n" + binary)
    expected("time-xyzt", "time-xyzt.nrrd", volume, "XYZT", 2, 3, 15, 0.25)

    companions = [f"frame{t}.gz" for t in range(3)]
    (directory / "time-list.nhdr").write_bytes(
        xyzt.replace(b"encoding: raw", b"encoding: gzip") +
        ("data file: LIST\n" + "\n".join(companions) + "\n").encode("ascii"))
    for t, name in enumerate(companions):
        (directory / name).write_bytes(gzip.compress(binary[t*16:(t+1)*16], mtime=0))
    expected("time-list", "time-list.nhdr", volume, "XYZT", 2, 3, 15, 0.25, "|".join(companions))

    xyt = header((2, 2, 4), "domain space time", "ascii", "0.5 1.25 nan", '"um" "um" "s"')
    (directory / "time-text.nrrd").write_bytes(xyt + b"\n" + " ".join(map(str, series)).encode("ascii"))
    expected("time-text", "time-text.nrrd", series, "XYT", 1, 4, 3, 0)

    bzip = header((2, 2, 4), "domain space time", "bzip2", "0.5 1.25 500000", '"um" "um" "us"', "little")
    (directory / "time-slabs.nhdr").write_bytes(bzip + b"data file: slab-%d.bz2 0 1 1 3\n")
    for slab in range(2):
        (directory / f"slab-{slab}.bz2").write_bytes(bz2.compress(struct.pack("<8H", *series[slab*8:(slab+1)*8])))
    expected("time-slabs", "time-slabs.nhdr", series, "XYT", 1, 4, 11, 0.5, "slab-0.bz2|slab-1.bz2")

    aligned = list(range(8))
    nrrd.write(str(directory / "basis-volume.nrrd"), np.array(aligned,dtype=np.uint16).reshape(2,2,2),
        header={"kinds": ["space"]*3, "space dimension": 3, "space directions": np.diag([0.25,0.5,0.002]),
                "space units": ["um","um","mm"], "space origin": [0.0]*3, "encoding": "raw"}, index_order="C")
    expected("basis-volume", "basis-volume.nrrd", aligned, "XYZ", 2, 1, 7, 0, calibration=(0.25,0.5,2))

    basis = (b"space dimension: 3\nspace directions: (0.5, 0, 0) (0, 1.25, 0) (0, 0, 3e0) none\n"
             b'space units: "um" "um" "um"\nspace origin: (0,-0,+0)\n')
    time_basis = xyzt.replace(b"spacings: 0.5 1.25 3 250", b"spacings: nan nan nan 250").replace(
        b'units: "um" "um" "um" "ms"', b'units: "" "" "" "ms"') + basis
    (directory / "basis-list.nhdr").write_bytes(
        time_basis.replace(b"encoding: raw", b"encoding: gzip") +
        ("data file: LIST\n" + "\n".join(companions) + "\n").encode("ascii"))
    expected("basis-list", "basis-list.nhdr", volume, "XYZT", 2, 3, 15, 0.25, "|".join(companions))

    xy = (b"NRRD0005\ntype: uint16\ndimension: 2\nsizes: 2 2\n"
          b"kinds: domain space\nencoding: raw\nendian: little\nspace dimension: 2\n"
          b'space directions: (500, 0) (0, 0.00125)\nspace units: "nm" "mm"\n')
    (directory / "basis-xy.nrrd").write_bytes(xy + b"\n" + struct.pack("<4H",*aligned[:4]))
    expected("basis-xy", "basis-xy.nrrd", aligned[:4], "XY", 1, 1, 3, 0)

    v3 = xyzt.replace(b"NRRD0005", b"NRRD0003", 1)
    (directory / "v3-xyz-time.nrrd").write_bytes(v3 + b"\n" + binary)
    expected("v3-xyz-time", "v3-xyz-time.nrrd", volume, "XYZT", 2, 3, 15, 0.25)
    v3_detached = v3.replace(b"encoding: raw", b"encoding: gzip") + b"data file: ./legacy.raw.gz\n"
    (directory / "v3-detached.nhdr").write_bytes(v3_detached)
    (directory / "legacy.raw.gz").write_bytes(gzip.compress(binary, mtime=0))
    expected("v3-detached", "v3-detached.nhdr", volume, "XYZT", 2, 3, 15, 0.25, "legacy.raw.gz")

    (directory / "v4-volume.nrrd").write_bytes(
        (directory / "basis-volume.nrrd").read_bytes().replace(b"NRRD0005", b"NRRD0004", 1))
    expected("v4-volume", "v4-volume.nrrd", aligned, "XYZ", 2, 1, 7, 0, calibration=(0.25,0.5,2))
    (directory / "v4-list.nhdr").write_bytes(
        (directory / "basis-list.nhdr").read_bytes().replace(b"NRRD0005", b"NRRD0004", 1))
    expected("v4-list", "v4-list.nhdr", volume, "XYZT", 2, 3, 15, 0.25, "|".join(companions))

    (directory / "fixtures.tsv").write_text("".join("\t".join(map(str, row)) + "\n" for row in rows), encoding="ascii")
    bad = (
        ("time-first.nrrd", xyzt.replace(b"space space space time", b"time space space space"), 2),
        ("time-duplicate.nrrd", xyzt.replace(b"space space space time", b"space space time time"), 2),
        ("time-unlabelled.nrrd", xyzt.replace(b"space space space time", b"space space space domain"), 2),
        ("time-unit.nrrd", xyzt.replace(b'"ms"', b'"um"'), 2),
        ("time-negative.nrrd", xyzt.replace(b"3 250", b"3 -250"), 2),
        ("time-excess.nrrd", xyzt.replace(b"sizes: 2 2 2 3", b"sizes: 2 2 2 4294967296"), 3),
        ("basis-shear.nrrd", time_basis.replace(b"(0.5, 0, 0)",b"(0.5, 0.01, 0)"), 2),
        ("basis-flip.nrrd", time_basis.replace(b"(0.5, 0, 0)",b"(-0.5, 0, 0)"), 2),
        ("basis-origin.nrrd", time_basis.replace(b"(0,-0,+0)",b"(0,0,0.001)"), 2),
        ("basis-time-vector.nrrd", time_basis.replace(b" none\n",b" (0,0,1)\n"), 2),
        ("basis-spacing-conflict.nrrd", time_basis.replace(b"nan nan nan 250",b"1 nan nan 250"), 2),
        ("basis-unit-conflict.nrrd", time_basis.replace(b'units: "" "" "" "ms"',b'units: "um" "" "" "ms"'), 2),
        ("basis-missing-unit.nrrd", time_basis.replace(b'space units: "um" "um" "um"',b'space units: "" "um" "um"'), 2),
        ("basis-rank.nrrd", time_basis.replace(b"space dimension: 3",b"space dimension: 2"), 2),
        ("basis-malformed.nrrd", time_basis.replace(b"(0.5, 0, 0)",b"(0.5,,0)"), 4),
        ("v3-basis.nrrd", time_basis.replace(b"NRRD0005", b"NRRD0003", 1), 2),
        ("v4-fields-before-rank.nrrd", b"NRRD0004\nspace dimension: 3\n"
         b"space directions: (1,0,0) (0,1,0) (0,0,1) none\n" + xyzt[9:], 4),
        ("v6.nrrd", xyzt.replace(b"NRRD0005", b"NRRD0006", 1), 2),
    )
    for name, declared, _ in bad:
        (directory / name).write_bytes(declared + b"\n" + binary)
    detached_bad = (
        ("v3-bare.nhdr", v3_detached.replace(b"./legacy.raw.gz",b"legacy.raw.gz")),
        ("v3-escape.nhdr", v3_detached.replace(b"./legacy.raw.gz",b"./../legacy.raw.gz")),
        ("v3-list.nhdr", v3 + b"data file: LIST\nframe0.gz\nframe1.gz\nframe2.gz\n"),
        ("v3-pattern.nhdr", v3 + b"data file: frame%d.gz 0 2 1\n"),
    )
    for name, declared in detached_bad:
        (directory / name).write_bytes(declared)
    (directory / "negative.tsv").write_text(
        "".join(f"{name}\t{status}\n" for name, _, status in bad) +
        "".join(f"{name}\t2\n" for name, _ in detached_bad), encoding="ascii")

    observed, fields = nrrd.read(str(directory / "time-xyzt.nrrd"), index_order="C")
    assert fields["kinds"][-1] == "time" and observed.shape == (3, 2, 2, 2)
    assert observed.astype(np.dtype("<u2"), copy=False).tobytes() == (directory / "time-xyzt.raw").read_bytes()
    observed, fields = nrrd.read(str(directory / "basis-volume.nrrd"), index_order="C")
    assert np.array_equal(fields["space directions"], np.diag([0.25,0.5,0.002]))
    assert observed.astype(np.dtype("<u2"), copy=False).tobytes() == (directory / "basis-volume.raw").read_bytes()
    for name, extension in (("v3-xyz-time", ".nrrd"), ("v3-detached", ".nhdr"), ("v4-volume", ".nrrd")):
        observed, _ = nrrd.read(str(directory / (name + extension)), index_order="C")
        assert observed.astype(np.dtype("<u2"), copy=False).tobytes() == (directory / (name + ".raw")).read_bytes()
    return len(rows), len(bad) + len(detached_bad)


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
    temporal = output / "time-fixtures"
    time_cases, time_negatives = write_axis_fixtures(temporal)
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
    report = {"schema": "qpx.nrrd-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]) + time_cases,
              "negative_cases": len(fixture_manifest["negative"]) + time_negatives, "checks": rows,
              "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        run([str(args.abi_test.resolve()), str(args.module.resolve()), str(fixtures), str(temporal)], output, "direct-abi", rows)
        if args.consumer and args.manifest:
            work = output / "project"
            work.mkdir()
            consumer = [str(args.consumer.resolve())]
            for mode in ("create", "reopen", "missing"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)

            def stage_manifest(directory, *, overlay=False, window=None):
                directory.mkdir()
                changed = json.loads(args.manifest.read_text(encoding="utf8"))
                for descriptor in changed["plugins"]:
                    target = directory / descriptor["module"]["path"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    data = (args.manifest.parent / descriptor["module"]["path"]).read_bytes()
                    if overlay:
                        data += b"QPX NRRD inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    if window:
                        descriptor["options"] = {"window": window}
                    for dependency in descriptor["dependencies"]:
                        destination = directory / dependency["path"]
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(args.manifest.parent / dependency["path"], destination)
                manifest = directory / "reader-plugins.json"
                manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
                return manifest

            changed_manifest = stage_manifest(output / "changed-package", overlay=True)
            run(consumer + ["changed-implementation", str(changed_manifest), str(fixtures), str(work)], output, "project-changed-implementation", rows)
            options_manifest = stage_manifest(output / "options-package", window=[0, 65535])
            run(consumer + ["changed-options", str(options_manifest), str(fixtures), str(work)], output, "project-changed-options", rows)
            run(consumer + ["changed-source", str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-changed-source", rows)
            gzip_work = output / "gzip-project"
            gzip_work.mkdir()
            gzip_fixtures = output / "gzip-project-fixtures"
            gzip_fixtures.mkdir()
            for original, gzip_name in (("u16-main", "gzip-u16"), ("float-volume", "gzip-f32")):
                for source in fixtures.glob(original + ".*"):
                    shutil.copyfile(fixtures / (gzip_name + source.suffix), gzip_fixtures / source.name)
            for mode in ("create", "reopen", "missing"):
                run(consumer + [mode, str(args.manifest.resolve()), str(gzip_fixtures), str(gzip_work)],
                    output, "gzip-project-" + mode, rows)
            detached_work = output / "detached-project"
            for mode in ("detached-create", "detached-reopen", "missing", "changed-detached-source"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(detached_work)],
                    output, "detached-project-" + mode.removeprefix("detached-"), rows)
            by_name = {case["name"]: case for case in fixture_manifest["cases"]}
            for profile in ("ascii", "skip", "line", "hex", "bzip2"):
                project_fixtures = output / (profile + "-project-fixtures")
                project_fixtures.mkdir()
                for kind, payload_alias, oracle_alias in (("u16", "raw", "u16-main"),
                                                           ("f32", "raw.gz", "float-volume")):
                    name = f"detached-{profile}-{kind}"
                    case = by_name[name]
                    companion = case["companion"]
                    header_alias = f"detached-{kind}-header"
                    declared_payload = f"{header_alias}.{payload_alias}"
                    # Reuse the installed SDK consumer's fixed filenames and preserve encoding and offset.
                    original = (fixtures / case["source_file"]).read_bytes()
                    assert companion.encode() in original
                    (project_fixtures / (header_alias + ".nhdr")).write_bytes(
                        original.replace(companion.encode(), declared_payload.encode(), 1))
                    shutil.copyfile(fixtures / companion, project_fixtures / declared_payload)
                    for oracle in fixtures.glob(oracle_alias + ".*"):
                        if oracle.suffix in (".raw", ".rgb", ".rgba"):
                            shutil.copyfile(fixtures / (name + oracle.suffix), project_fixtures / oracle.name)
                for mode in ("detached-create", "detached-reopen", "missing", "changed-detached-source"):
                    run(consumer + [mode, str(args.manifest.resolve()), str(project_fixtures), str(output / (profile + "-project"))],
                        output, profile + "-project-" + mode.removeprefix("detached-"), rows)
            for mode in ("list-create", "list-reopen", "missing", "changed-list-source"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(output / "list-project")],
                    output, "list-project-" + mode, rows)
            for mode in ("numbered-create", "numbered-reopen", "missing", "changed-numbered-source"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(output / "numbered-project")],
                    output, "numbered-project-" + mode, rows)
            for mode in ("wide-create", "wide-reopen"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(output / "wide-project")],
                    output, "wide-project-" + mode, rows)
            for mode in ("time-create", "time-reopen"):
                run(consumer + [mode, str(args.manifest.resolve()), str(temporal), str(output / "time-project")],
                    output, mode, rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            command = [str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.nrrd"]
            for name in ("u16-main", "gzip-u16"):
                image = output / (name + ".pam")
                run(command + [str(fixtures / (name + ".nrrd")), "0", "0", "0", "5", "3", str(image)],
                    output, "host-region-" + name, rows)
                pam = image.read_bytes()
                end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
                assert pam[end:] == (fixtures / (name + ".rgba")).read_bytes(), name
            import numpy as np
            for name, filename, dtype in (("gzip-i16", "gzip-i16.nrrd", "int16"),
                                           ("detached-i16", "detached-i16-header.nhdr", "int16"),
                                           ("detached-ascii-i16", "detached-ascii-i16-header.nhdr", "int16"),
                                           ("detached-eof-i16", "detached-eof-i16-header.nhdr", "int16"),
                                           ("detached-line-eof-i16", "detached-line-eof-i16-header.nhdr", "int16"),
                                           ("list-slices-i16", "list-slices-i16.nhdr", "int16"),
                                           ("numbered-descending-i16", "numbered-descending-i16.nhdr", "int16"),
                                           ("list-u64", "list-u64.nhdr", "uint64"),
                                           ("detached-gzip-f64", "detached-gzip-f64-header.nhdr", "float64"),
                                           ("detached-hex-f64", "detached-hex-f64-header.nhdr", "float64"),
                                           ("numbered-hex-i16", "numbered-hex-i16.nhdr", "int16"),
                                           ("attached-bzip2-i16", "attached-bzip2-i16.nrrd", "int16"),
                                           ("list-bzip2-f64", "list-bzip2-f64.nhdr", "float64")):
                volume = fixtures / filename
                array = output / (name + "-z1.npy")
                run(command + ["--raw", str(volume), str(array), "--width", "5", "--height", "3", "--z", "1"],
                    output, "host-" + name + "-z1", rows)
                samples = np.load(array, allow_pickle=False)
                assert samples.dtype == np.dtype(dtype)
                assert samples.tobytes() == (fixtures / (name + ".raw")).read_bytes()[15 * samples.itemsize:]
            array = output / "nrrd-z1-t2.npy"
            run(command + ["--raw", str(temporal / "v4-list.nhdr"), str(array),
                           "--width", "2", "--height", "2", "--z", "1", "--time", "2"],
                output, "host-time-plane", rows)
            samples = np.load(array, allow_pickle=False)
            assert samples.dtype == np.dtype("uint16")
            assert samples.tobytes() == (temporal / "v4-list.raw").read_bytes()[-8:]
            old_array = output / "nrrd-v3-z1-t2.npy"
            run(command + ["--raw", str(temporal / "v3-detached.nhdr"), str(old_array),
                           "--width", "2", "--height", "2", "--z", "1", "--time", "2"],
                output, "host-legacy-detached", rows)
            assert np.array_equal(np.load(old_array, allow_pickle=False), samples)
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = hashes()
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation input module/executable/manifest bytes changed; see validation.json")
    print(json.dumps({"passed": True, "fixture_cases": report["fixture_cases"], "native_checks": len(rows), "report": str(output / "validation.json")}))


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
