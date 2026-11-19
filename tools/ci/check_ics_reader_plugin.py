#!/usr/bin/env python3
"""Development-only literal ICS/IDS fixtures, native libics oracle and controls."""
import argparse
import ast
from decimal import Decimal, localcontext, ROUND_HALF_UP
import gzip
import hashlib
import itertools
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


TYPES = {"u8": (1, 1, "B", "integer", "unsigned", "uint8"), "u16": (2, 2, "H", "integer", "unsigned", "uint16"),
         "i16": (4, 2, "h", "integer", "signed", "int16"), "f32": (9, 4, "I", "real", "signed", "float32")}


def rgba_oracle(values, kind, width, height, channels, selected, window):
    with localcontext() as context:
        context.prec = 190
        low, high = (Decimal.from_float(float(v)) for v in window)
        result = bytearray()
        for pixel in range(width * height):
            word = values[pixel * channels + selected]
            if kind == "f32":
                exponent, fraction = (word >> 23) & 255, word & 0x7fffff
                if exponent == 255:
                    result += bytes(4)
                    continue
                numeric = Decimal(fraction if exponent == 0 else (1 << 23) + fraction) * Decimal(2) ** (-149 if exponent == 0 else exponent - 150)
                if word >> 31:
                    numeric = -numeric
            else:
                numeric = Decimal(word)
            normalized = min(Decimal(1), max(Decimal(0), (numeric - low) / (high - low)))
            code = int((normalized * 255).quantize(Decimal(1), rounding=ROUND_HALF_UP))
            result += bytes((code, code, code, 255))
        return bytes(result)


def write_fixtures(directory, oracle=None):
    oracle_before = hashlib.sha256(oracle.read_bytes()).hexdigest() if oracle else None
    directory.mkdir(parents=True, exist_ok=True)
    reference_dir = directory / ".reference"
    reference_dir.mkdir(exist_ok=True)
    cases, negatives = [], {}
    literals = {"u8": [0, 9, 10, 13, 32, 35, 127, 128, 129, 254, 255, 1, 2, 3, 4],
                "u16": [0, 1, 127, 128, 129, 254, 255, 256, 257, 1000, 4095, 4096, 32767, 32768, 65535],
                "i16": [-32768, -257, -256, -255, -129, -128, -1, 0, 1, 127, 128, 255, 256, 32766, 32767],
                "f32": [0, 0x80000000, 0x3f000000, 0x3f800000, 0xbf800000, 0x437f0000, 0x43800000,
                        0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345, 0xffa54321, 1, 0x80000001]}

    def values_for(kind, dimensions, channels, frames=1):
        width, height = dimensions[:2]
        return [literals[kind][(x + y * width + ch * 3 + z * 7 + t * 11) % 15]
                for t in range(frames) for z in range(dimensions[2] if len(dimensions) == 3 else 1)
                for y in range(height) for x in range(width) for ch in range(channels)]

    def pair(name, kind, dimensions, channels, values, *, version=1, channel_axis=None, ranks=None, scales=None, units=None,
             endings="lf", labels=True, history="Literal repository-authored values", end_tab=True, filename=None,
             blank=False, compression="uncompressed", external=None, external_end=False,
             frames=None, time_step=1, time_unit="undefined", reference_labels=False, stored_axes=None):
        _, sample, pack, form, sign, _ = TYPES[kind]
        channel_axis = channels > 1 if channel_axis is None else channel_axis
        ranks = tuple(range(1, sample + 1)) if ranks is None else tuple(ranks)
        assert sorted(ranks) == list(range(1, sample + 1))
        axes = (["ch"] if channel_axis else []) + ["x", "y"] + (["z"] if len(dimensions) == 3 else [])
        sizes = ([channels] if channel_axis else []) + list(dimensions)
        scales = tuple(scales or [1] * len(dimensions))
        units = tuple(["undefined"] * len(dimensions) if units is None else units)
        if frames is not None:
            axes.append("t")
            sizes.append(frames)
            scales += (time_step,)
            units += (time_unit,)
        origins = [0] * (len(axes) + 1)
        all_scales = [1] + ([1] if channel_axis else []) + list(scales)
        all_units = ["relative"] + (["undefined"] if channel_axis else []) + list(units)
        stored_values = values
        if stored_axes is not None:
            assert sorted(stored_axes) == sorted(axes)
            perm = [axes.index(axis) for axis in stored_axes]
            logical_strides = [math.prod(sizes[:i]) for i in range(len(sizes))]
            sizes = [sizes[i] for i in perm]
            all_scales = [all_scales[0]] + [all_scales[i + 1] for i in perm]
            all_units = [all_units[0]] + [all_units[i + 1] for i in perm]
            axes = list(stored_axes)
            strides = [logical_strides[i] for i in perm]
            # Enumerate files from the slowest physical axis to the fastest.
            stored_values = [values[sum(i * stride for i, stride in zip(coords, reversed(strides)))]
                             for coords in itertools.product(*(range(size) for size in reversed(sizes)))]
        rows = [["ics_version", str(version) + ".0"], ["filename", filename or name], ["layout", "parameters", str(len(axes) + 1)],
                ["layout", "order", "bits"] + axes, ["layout", "sizes", str(sample * 8)] + list(map(str, sizes)),
                ["layout", "coordinates", "video"], ["layout", "significant_bits", str(sample * 8)],
                ["representation", "format", form], ["representation", "sign", sign],
                ["representation", "compression", compression], ["representation", "byte_order"] + list(map(str, ranks)),
                ["parameter", "origin"] + list(map(str, origins)), ["parameter", "scale"] + list(map(str, all_scales)),
                ["parameter", "units"] + all_units]
        if labels:
            defaults = {"x": "x-position", "y": "y-position", "z": "z-position", "t": "time"}
            rows.append(["parameter", "labels", "intensity"] +
                        [defaults.get(axis, axis) if reference_labels else axis for axis in axes])
        rows.append(["history", "qpx", history])
        if blank:
            rows.insert(6, [""])
        if external is not None:
            assert version == 2
            source_file, source_offset = external
            rows.extend((["source", "file", source_file], ["source", "offset", str(source_offset)]))
        if version == 2 and (external is None or external_end):
            rows.append(["end", ""] if end_tab else ["end"])
        def newline(index):
            return b"\r\n" if endings == "crlf" or endings == "mixed" and index % 2 else b"\n"
        header = b"\t" + newline(0) + b"".join("\t".join(row).encode("ascii") + newline(i + 1) for i, row in enumerate(rows))
        little = struct.pack("<" + pack * len(stored_values), *stored_values)
        payload = bytes(little[at + rank - 1] for at in range(0, len(little), sample) for rank in ranks)
        if compression == "gzip":
            payload = gzip.compress(payload, mtime=0)
        reference = struct.pack("=" + pack * len(stored_values), *stored_values)
        return header, payload, axes, sizes, all_scales, all_units, reference

    def gate(name, kind, expected, axes, sizes, scales, units):
        if oracle is None:
            return {"pending": True}
        prefix = ".reference/" + name
        command = [str(oracle.resolve()), name + ".ics", prefix]
        result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8", errors="strict")
        (reference_dir / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
        raw_path, meta_path = directory / (prefix + ".raw"), directory / (prefix + ".json")
        assert raw_path.read_bytes() == expected, (name, "libics decoded bytes differ from literal original codes")
        metadata = json.loads(meta_path.read_text(encoding="utf8"))
        assert metadata["type"] == TYPES[kind][5] and metadata["sample_bytes"] == TYPES[kind][1]
        assert metadata["data_bytes"] == len(expected) and metadata["dimensions"] == sizes
        assert [a["order"] for a in metadata["axes"]] == axes
        assert metadata["significant_bits"] == TYPES[kind][1] * 8 and metadata["coordinates"] == "video"
        assert metadata["scalar_origin"] == 0 and metadata["scalar_scale"] == 1 and metadata["scalar_units"] == "relative"
        for actual, scale, unit in zip(metadata["axes"], scales[1:], units[1:]):
            assert actual["origin"] == 0 and actual["scale"] == float(scale)
            assert actual["unit"] == unit or not unit and actual["unit"] == "undefined"
        return {"passed": True, "command": command, "raw_sha256": hashlib.sha256(raw_path.read_bytes()).hexdigest(),
                "metadata_sha256": hashlib.sha256(meta_path.read_bytes()).hexdigest()}

    def add(name, kind, dimensions=(5, 3, 2), channels=3, *, values=None, options="", selected=0, window=(0, 255),
            external_prefix=None, external_suffix=b"", gzip_companion=False, **kwargs):
        frames = kwargs.get("frames") or 1
        values = values_for(kind, dimensions, channels, frames) if values is None else list(values)
        assert len(values) == math.prod(dimensions) * channels * frames
        companion_name = None
        if external_prefix is not None:
            companion_name = name + ".bin"
            kwargs["external"] = (companion_name, len(external_prefix))
        header, payload, axes, sizes, scales, units, stored = pair(name, kind, dimensions, channels, values, **kwargs)
        version = kwargs.get("version", 1)
        primary = header + (payload if version == 2 and companion_name is None else b"")
        (directory / (name + ".ics")).write_bytes(primary)
        companion = None
        if version == 1:
            companion = payload
            assert not gzip_companion or kwargs.get("compression") == "gzip"
            companion_name = name + (".ids.gz" if gzip_companion else ".ids")
            (directory / companion_name).write_bytes(companion)
        elif companion_name is not None:
            companion = external_prefix + payload + external_suffix
            (directory / companion_name).write_bytes(companion)
        raw = struct.pack("=" + TYPES[kind][2] * len(values), *values)
        rgba = rgba_oracle(values, kind, *dimensions[:2], channels, selected, window)
        (directory / (name + ".raw")).write_bytes(raw)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if rgba[3::4] == bytes([255]) * math.prod(dimensions[:2]):
            (directory / (name + ".rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        physical, flags = [0., 0., 0.], 0
        spatial_scale = kwargs.get("scales", [1] * len(dimensions))
        spatial_units = kwargs.get("units", ["undefined"] * len(dimensions))
        for axis, (scale, unit) in enumerate(zip(spatial_scale, spatial_units)):
            if unit not in ("", "undefined"):
                physical[axis] = float(scale) * {"nm": .001, "um": 1., "mm": 1000., "cm": 10000., "m": 1000000.}[unit]
                flags |= 1 << axis
        time_unit = kwargs.get("time_unit", "undefined")
        interval = 0
        if time_unit not in ("", "undefined"):
            interval = float(kwargs["time_step"]) / {"s": 1, "ms": 1000, "us": 1000000}[time_unit]
            flags |= 8
        reference = gate(name, kind, stored, axes, sizes, scales, units)
        cases.append({"name": name, "kind": kind, "version": version, "width": dimensions[0], "height": dimensions[1],
                      "compression": kwargs.get("compression", "uncompressed"),
                      "z_planes": dimensions[2] if len(dimensions) == 3 else 1, "channels": channels,
                      "timepoints": frames, "time_interval_seconds": interval,
                      "sample_type": TYPES[kind][0], "sample_bytes": TYPES[kind][1], "dimension_order": "".join(axes).upper().replace("CH", "C"),
                      "calibration_flags": flags, "pixel_width_um": physical[0], "pixel_height_um": physical[1], "z_spacing_um": physical[2],
                      "options": options, "selected_channel": selected, "window": list(window), "byte_order": list(kwargs.get("ranks", range(1, TYPES[kind][1] + 1))),
                      "primary_sha256": hashlib.sha256(primary).hexdigest(), "companion_sha256": hashlib.sha256(companion).hexdigest() if companion is not None else None,
                      "companion_file": companion_name or "-", "external_offset": len(external_prefix) if external_prefix is not None else None,
                      "raw_sha256": hashlib.sha256(raw).hexdigest(), "rgba_sha256": hashlib.sha256(rgba).hexdigest(), "reference": reference})
        return header, payload

    for kind in TYPES:
        for version in (1, 2):
            add(f"{kind}-v{version}-channels", kind, version=version)
        add(kind + "-scalar-xy", kind, (5, 3), 1)
        add(kind + "-scalar-xyz", kind, (5, 3, 2), 1, version=2)
    main_args = {"scales": (.25, .5, 1.5), "units": ("um", "um", "um")}
    main_header, main_payload = add("channels-main", "u16", **main_args)
    inline_header, inline_payload = add("float-inline", "f32", channels=2, version=2)
    time_header, time_payload = add("time-ids", "u16", frames=3, time_step=0.5, time_unit="s",
                                    reference_labels=True)
    add("time-inline-gzip", "f32", (5, 3), 2, version=2, frames=2, time_step=250,
        time_unit="ms", compression="gzip", ranks=(3, 1, 4, 2))
    add("time-external", "u16", frames=2, time_step=125000, time_unit="us", version=2,
        external_prefix=b"opaque prefix", compression="gzip")
    add("time-unknown", "u8", (5, 3), 1, version=2, frames=2, time_step=2)
    add("axis-planar-ids", "u16", stored_axes=("x", "y", "z", "ch"),
        scales=(0.25, 0.5, 1.5), units=("um", "um", "um"), ranks=(2, 1))
    add("axis-time", "u16", frames=3, time_step=0.5, time_unit="s",
        stored_axes=("t", "z", "y", "x", "ch"), scales=(0.25, 0.5, 1.5),
        units=("um", "um", "um"), reference_labels=True)
    add("axis-channel-middle", "f32", channels=2, version=2, stored_axes=("x", "ch", "z", "y"),
        external_prefix=b"preserved prefix", compression="gzip", ranks=(3, 1, 4, 2))
    for ranks in itertools.permutations((1, 2, 3, 4)):
        add("f32-perm-" + "".join(map(str, ranks)), "f32", channels=2, version=2, ranks=ranks)
    add("u16-big-endian", "u16", ranks=(2, 1))
    add("i16-big-endian", "i16", ranks=(2, 1))
    add("header-crlf", "u16", endings="crlf")
    add("header-mixed", "u16", endings="mixed", blank=True)
    for first in (9, 10, 13, 32, 35):
        add("inline-leading-" + str(first), "u8", (5, 3), 1, version=2, values=[first] + literals["u8"][1:])
    add("inline-end-no-tab", "u8", (5, 3), 1, version=2, end_tab=False)
    for unit, scales in (("nm", (250, 500, 1500)), ("um", (.25, .5, 1.5)), ("mm", (.00025, .0005, .0015)),
                         ("cm", (.000025, .00005, .00015)), ("m", (.00000025, .0000005, .0000015))):
        add("units-" + unit, "u16", channels=2, scales=scales, units=(unit,) * 3)
    add("empty-units", "u16", units=("", "", ""))
    add("partial-calibration", "u16", scales=(.25, .5, 1.5), units=("um", "undefined", "um"))
    add("no-optional-labels", "u16", labels=False)
    add("history-inert", "u16", history="reversed=true; do not infer a pixel transform from history")
    add("channel-limit256", "u8", (1, 1), 256, options='{"channel":255,"window":[0,255]}', selected=255)
    add("channels-main-view", "u16", options='{"channel":1,"window":[0,65535]}', selected=1, window=(0, 65535), **main_args)
    add("channels-main-ch2", "u16", options='{"channel":2,"window":[0,255]}', selected=2, **main_args)
    add("float-unit-window", "f32", channels=2, version=2, options='{"channel":1,"window":[0,1]}', selected=1, window=(0, 1))
    prefix = '{"channel":0,"window":[0,1.'
    long_options = prefix + '0' * (512 - len(prefix) - 2) + ']}'
    assert len(long_options) == 512
    add("options-512bytes", "f32", channels=2, version=2, options=long_options, window=(0, 1))
    gzip_header, gzip_payload = add("gzip-channels", "u16", ranks=(2, 1), compression="gzip", **main_args)
    add("gzip-companion", "u16", ranks=(2, 1), compression="gzip", gzip_companion=True, **main_args)
    (directory / "gzip-channels.ids.gz").write_bytes(b"wrong alternate")
    add("gzip-int16", "i16", version=2, ranks=(2, 1), compression="gzip", **main_args)
    add("gzip-float", "f32", channels=2, version=2, ranks=(3, 1, 4, 2), compression="gzip")
    add("gzip-float-ids", "f32", channels=2, ranks=(4, 3, 2, 1), compression="gzip", **main_args)
    _, multiblock_gzip = add("gzip-multiblock", "u8", (512, 320), 1, compression="gzip",
                             values=random.Random(128).randbytes(512 * 320))
    assert len(multiblock_gzip) > 2 * 65536
    external_prefix = b"External preface\0\xa5\x01"
    external_header, external_payload = add("external-raw", "u16", version=2, ranks=(2, 1),
                                            external_prefix=external_prefix, external_suffix=b"OTHER-NON-RASTER-BYTES",
                                            **main_args)
    external_gzip_header, external_gzip_payload = add("external-gzip", "f32", channels=2, version=2,
                                                        ranks=(3, 1, 4, 2), compression="gzip",
                                                        external_prefix=external_prefix, external_end=True)
    add("external-offset-zero", "u8", (5, 3), 1, version=2, external_prefix=b"")
    large_prefix = b"NON-RASTER-PREFIX" * 12288
    add("external-large-prefix", "i16", version=2, ranks=(2, 1), external_prefix=large_prefix, **main_args)

    def bad(name, header, status, payload=main_payload, version=1, companion=True):
        (directory / (name + ".ics")).write_bytes(header + (payload if version == 2 else b""))
        if version == 1 and companion:
            (directory / (name + ".ids")).write_bytes(payload)
        negatives[name] = {"status": status, "primary_sha256": hashlib.sha256(header + (payload if version == 2 else b"")).hexdigest()}

    for name, old, new, status in (("version", b"ics_version\t1.0", b"ics_version\t3.0", 2),
                                   ("missing-filename", b"filename\tchannels-main\n", b"", 4),
                                   ("empty-filename", b"filename\tchannels-main", b"filename\t", 4),
                                   ("parameters-count", b"parameters\t5", b"parameters\t4", 4),
                                   ("sizes-count", b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t3\t5\t3", 4),
                                   ("channel-zero", b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t0\t5\t3\t2", 4),
                                   ("channel-limit257", b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t257\t5\t3\t2", 3),
                                   ("order-unknown", b"order\tbits\tch\tx\ty\tz", b"order\tbits\tprobe\tx\ty\tz", 2),
                                   ("order-missing-x", b"order\tbits\tch\tx\ty\tz", b"order\tbits\tch\tt\ty\tz", 2),
                                   ("coordinates", b"coordinates\tvideo", b"coordinates\tcartesian", 2),
                                   ("partial-significant", b"significant_bits\t16", b"significant_bits\t12", 2),
                                   ("gzip-mislabeled-raw", b"compression\tuncompressed", b"compression\tgzip", 4),
                                   ("compression-legacy", b"compression\tuncompressed", b"compression\tcompress", 2),
                                   ("byte-order-zero", b"byte_order\t1\t2", b"byte_order\t0\t2", 4),
                                   ("byte-order-duplicate", b"byte_order\t1\t2", b"byte_order\t1\t1", 4),
                                   ("byte-order-range", b"byte_order\t1\t2", b"byte_order\t1\t3", 4),
                                   ("byte-order-count", b"byte_order\t1\t2", b"byte_order\t1", 4),
                                   ("nonzero-origin", b"origin\t0\t0\t0\t0\t0", b"origin\t0\t0\t1\t0\t0", 2),
                                   ("scaled-scalar", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t2\t1\t0.25\t0.5\t1.5", 2),
                                   ("scaled-channel", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t2\t0.25\t0.5\t1.5", 2),
                                   ("negative-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\t-0.25\t0.5\t1.5", 2),
                                   ("zero-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\t0\t0.5\t1.5", 4),
                                   ("nan-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\tnan\t0.5\t1.5", 4),
                                   ("inf-spatial-scale", b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\tinf\t0.5\t1.5", 4),
                                   ("unknown-units", b"units\trelative\tundefined\tum\tum\tum", b"units\trelative\tundefined\tmicrons\tum\tum", 2),
                                   ("units-case", b"units\trelative\tundefined\tum\tum\tum", b"units\trelative\tundefined\tUM\tum\tum", 2),
                                   ("scalar-units", b"units\trelative\tundefined", b"units\tcounts\tundefined", 2),
                                   ("channel-units", b"units\trelative\tundefined", b"units\trelative\tum", 2),
                                   ("labels-noncanonical", b"labels\tintensity\tch\tx\ty\tz", b"labels\tintensity\twavelength\tx\ty\tz", 2)):
        assert old in main_header, name
        bad(name, main_header.replace(old, new, 1), status)
    for token in (b"layout\tparameters\t5\n", b"layout\torder\tbits\tch\tx\ty\tz\n", b"layout\tsizes\t16\t3\t5\t3\t2\n",
                  b"layout\tcoordinates\tvideo\n", b"layout\tsignificant_bits\t16\n", b"representation\tformat\tinteger\n",
                  b"representation\tsign\tunsigned\n", b"representation\tcompression\tuncompressed\n", b"representation\tbyte_order\t1\t2\n",
                  b"parameter\torigin\t0\t0\t0\t0\t0\n", b"parameter\tscale\t1\t1\t0.25\t0.5\t1.5\n", b"parameter\tunits\trelative\tundefined\tum\tum\tum\n"):
        bad("missing-" + token.split(b"\t")[1].decode(), main_header.replace(token, b"", 1), 4)
    unsupported_u32 = main_header.replace(b"sizes\t16\t", b"sizes\t32\t", 1).replace(b"significant_bits\t16", b"significant_bits\t32", 1).replace(b"byte_order\t1\t2", b"byte_order\t1\t2\t3\t4", 1)
    bad("unsupported-u32", unsupported_u32, 2)
    unsupported_f64 = unsupported_u32.replace(b"sizes\t32\t", b"sizes\t64\t", 1).replace(b"significant_bits\t32", b"significant_bits\t64", 1).replace(b"format\tinteger", b"format\treal", 1).replace(b"sign\tunsigned", b"sign\tsigned", 1).replace(b"byte_order\t1\t2\t3\t4", b"byte_order\t1\t2\t3\t4\t5\t6\t7\t8", 1)
    bad("unsupported-f64", unsupported_f64, 2)
    bad("duplicate-sizes", main_header.replace(b"layout\tsizes", b"layout\tsizes\t16\t3\t5\t3\t2\nlayout\tsizes", 1), 4)
    bad("header-no-final-newline", main_header[:-1], 4)
    bad("header-nonascii", main_header.replace(b"Literal", b"Literal\xc2\xb5", 1), 4)
    bad("header-nul", main_header.replace(b"Literal", b"Literal\0", 1), 4)
    bad("header-bound", main_header + b"history\tqpx\t" + b"a" * 65536 + b"\n", 3)
    bad("size-arithmetic-bound", main_header.replace(b"sizes\t16\t3\t5\t3\t2", b"sizes\t16\t3\t9223372036854775807\t3\t2", 1), 3)
    overflow = main_header.replace(b"scale\t1\t1\t0.25\t0.5\t1.5", b"scale\t1\t1\t1e308\t0.5\t1.5", 1).replace(b"units\trelative\tundefined\tum", b"units\trelative\tundefined\tm", 1)
    bad("calibration-overflow", overflow, 2)
    bad("ids-truncated", main_header, 4, payload=main_payload[:-1])
    bad("ids-trailing", main_header, 4, payload=main_payload + b"\0")
    bad("ids-concatenated", main_header, 4, payload=main_payload + main_payload)
    bad("inline-truncated", inline_header, 4, payload=inline_payload[:-1], version=2)
    bad("inline-trailing", inline_header, 4, payload=inline_payload + b"\0", version=2)
    bad("inline-end-value", inline_header.replace(b"end\t\n", b"end\tunexpected\n", 1), 4, payload=inline_payload, version=2)

    def corrupt_byte(data, index):
        at = len(data) + index
        return data[:at] + bytes((data[at] ^ 1,)) + data[at + 1:]

    inline_gzip_header = inline_header.replace(b"compression\tuncompressed", b"compression\tgzip", 1)
    inline_gzip = gzip.compress(inline_payload, mtime=0)
    bad("gzip-ids-crc", gzip_header, 4, payload=corrupt_byte(gzip_payload, -8))
    bad("gzip-ids-missing", gzip_header, 4, companion=False)
    bad("gzip-alternate-crc", gzip_header, 4, companion=False)
    (directory / "gzip-alternate-crc.ids.gz").write_bytes(corrupt_byte(gzip_payload, -8))
    bad("gzip-preferred-corrupt", gzip_header, 4, payload=corrupt_byte(gzip_payload, -8))
    (directory / "gzip-preferred-corrupt.ids.gz").write_bytes(gzip_payload)
    for name, broken in (("empty", b""), ("zlib-wrapper", zlib.compress(inline_payload)),
                         ("crc", corrupt_byte(inline_gzip, -8)), ("isize", corrupt_byte(inline_gzip, -1)),
                         ("truncated", inline_gzip[:-1]), ("short-output", gzip.compress(inline_payload[:-1], mtime=0)),
                         ("excess-output", gzip.compress(inline_payload + bytes(65536), mtime=0)),
                         ("trailer", inline_gzip + b"\0"), ("concatenated", inline_gzip + gzip.compress(b"", mtime=0))):
        bad("gzip-inline-" + name, inline_gzip_header, 4, payload=broken, version=2)
    bad("missing-derived-companion", main_header, 4, companion=False)
    (directory / "missing-derived-companion.ids.gz").write_bytes(main_payload)
    assert not (directory / "missing-derived-companion.ids").exists()
    for name, row in (("sensor", b"sensor\ttype\tIncohConfocal\n"), ("scil", b"layout\tscil_type\tg3d\n"),
                      ("source-file", b"source\tfile\tchannels-main.ids\n"), ("source-offset", b"source\toffset\t0\n"),
                      ("unknown", b"authored\tmeaning\tunknown\n")):
        bad(name, main_header + row, 2)
    file_field = b"source\tfile\texternal-raw.bin\n"
    offset_field = f"source\toffset\t{len(external_prefix)}\n".encode("ascii")
    assert file_field in external_header and offset_field in external_header
    for name, header, expected in (
        ("missing-file-field", external_header.replace(file_field, b""), 4),
        ("missing-offset-field", external_header.replace(offset_field, b""), 4),
        ("duplicate-file", external_header + file_field, 4),
        ("duplicate-offset", external_header + offset_field, 4),
        ("negative-offset", external_header.replace(offset_field, b"source\toffset\t-1\n"), 4),
        ("empty-offset", external_header.replace(offset_field, b"source\toffset\t\n"), 4),
        ("fractional-offset", external_header.replace(offset_field, b"source\toffset\t1.5\n"), 4),
        ("overflow-offset", external_header.replace(offset_field, b"source\toffset\t18446744073709551616\n"), 3),
        ("beyond-eof", external_header.replace(offset_field, b"source\toffset\t12345\n"), 4),
        ("uint64-max-offset", external_header.replace(offset_field, b"source\toffset\t18446744073709551615\n"), 4),
        ("trailing-primary", external_header + b"end\n\x00", 4),
        ("empty-file", external_header.replace(file_field, b"source\tfile\t\n"), 4),
        ("extra-file-field", external_header.replace(file_field, b"source\tfile\texternal-raw.bin\tother\n"), 4),
        ("unknown-source-field", external_header + b"source\tencoding\texternal\n", 2),
    ):
        bad("external-" + name, header, expected, version=2, payload=b"")
    (directory / "external-directory.bin").mkdir(exist_ok=True)
    (directory / "external-hardlink-target.bin").write_bytes(external_prefix + external_payload)
    alias = directory / "external-hardlink.bin"
    if not alias.exists():
        os.link(directory / "external-hardlink-target.bin", alias)
    for name, file_name, expected in (
        ("parent", b"../external-raw.bin", 2),
        ("subdirectory", b"nested/external-raw.bin", 2),
        ("absolute-drive", b"C:\\external-raw.bin", 2),
        ("alternate-stream", b"external-raw.bin:secret", 2),
        ("device", b"NuL.bin", 2),
        ("leading-dot", b".external-raw.bin", 2),
        ("trailing-dot", b"external-raw.bin.", 2),
        ("self", b"EXTERNAL-SELF.ICS", 2),
        ("directory", b"external-directory.bin", 2),
        ("hardlink", alias.name.encode("ascii"), 2),
        ("not-found", b"absent-external.bin", 4),
        ("truncated", b"external-truncated.bin", 4),
    ):
        bad("external-" + name, external_header.replace(file_field, b"source\tfile\t" + file_name + b"\n"), expected,
            version=2, payload=b"")
    (directory / "external-truncated.bin").write_bytes(external_prefix + external_payload[:-1])
    for name, damaged in (("trailing", external_gzip_payload + b"tail"),
                          ("crc", corrupt_byte(external_gzip_payload, -8))):
        target = "external-gzip-" + name + ".bin"
        (directory / target).write_bytes(external_prefix + damaged)
        header = external_gzip_header.replace(b"external-gzip.bin", target.encode("ascii"))
        bad("external-gzip-" + name, header, 4, version=2, payload=b"")

    for label, field, replacement, status in (
        ("zero-count", b"sizes\t16\t3\t5\t3\t2\t3", b"sizes\t16\t3\t5\t3\t2\t0", 4),
        ("duplicate-axis", b"order\tbits\tch\tx\ty\tz\tt", b"order\tbits\tch\tx\ty\tz\tz", 4),
        ("nonzero-origin", b"origin\t0\t0\t0\t0\t0\t0", b"origin\t0\t0\t0\t0\t0\t1", 2),
        ("zero-scale", b"\t0.5\n", b"\t0\n", 4),
        ("frequency-unit", b"\ts\n", b"\tHz\n", 2),
    ):
        assert field in time_header
        bad(f"time-{label}", time_header.replace(field, replacement), status, time_payload)
    bad("time-truncated", time_header, 4, time_payload[:-1])
    changed_header = main_header + b"history\tqpx\tprimary-only exact identity mutation\n"
    (directory / "primary-changed.ics").write_bytes(changed_header)
    changed_payload = bytes((main_payload[0] ^ 1,)) + main_payload[1:]
    (directory / "companion-changed.ids").write_bytes(changed_payload)
    file_set_header, file_set_payload, *_ = pair("file-set-changed", "u16", (5, 3, 2), 3, values_for("u16", (5, 3, 2), 3), version=2, **main_args)
    (directory / "file-set-changed.ics").write_bytes(file_set_header + file_set_payload)
    resource_values = [17] * (256 * 256)
    resource_header, resource_payload, *_ = pair("budget-input", "u8", (256, 256), 1, resource_values)
    (directory / "budget-input.ics").write_bytes(resource_header)
    (directory / "budget-input.ids").write_bytes(resource_payload)
    assert len(resource_header) < 4096 and len(resource_header) + len(resource_payload) > 32768
    unicode_name = "ICS-\u03bc-\u56fe"
    (directory / (unicode_name + ".ics")).write_bytes(main_header)
    (directory / (unicode_name + ".ids")).write_bytes(main_payload)
    oracle_after = hashlib.sha256(oracle.read_bytes()).hexdigest() if oracle else None
    assert oracle_before == oracle_after, "Reference executable changed while qualifying fixtures"
    manifest = {"schema": "qpx.ics-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"kind": "Native libics1.7.2 public API helper; test-only",
                           "primary_authority": "Literal original integer/F32 bitwords plus Decimal190 channel/window expectations",
                           "source_pin": "c1a1ae6f46ed1167001c0a14d9ef767c37ff4833", "pending": oracle is None,
                           "executable_sha256": oracle_before, "executable_unchanged": oracle_before == oracle_after,
                           "ascii_relative_paths": True, "scope": "Zlib-enabled reference only; libics is not linked into the native provider"},
                "unicode_filename": {"stem": unicode_name, "control": "channels-main"},
                "resource_fixtures": {"budget-input": {"primary_bytes": len(resource_header), "companion_bytes": len(resource_payload),
                                       "work_budget": 32768, "discovery_budget": 16384, "scope": "Owned primary+IDS input bound; discovery reads only primary"},
                                      "gzip-multiblock": {"encoded_bytes": len(multiblock_gzip), "decoded_bytes": 512 * 320,
                                                          "scope": "Exact budget and cancellation through multiple inflate chunks"},
                                      "external-large-prefix": {"skipped_bytes": len(large_prefix), "work_budget": 32768,
                                                                "scope": "Direct open seeks past excluded bytes without charging prefix to budget"}}}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    columns = ("name", "version", "width", "height", "z_planes", "timepoints", "channels", "sample_type", "sample_bytes", "dimension_order", "calibration_flags", "pixel_width_um", "pixel_height_um", "z_spacing_um", "time_interval_seconds", "companion_file")
    (directory / "fixtures.tsv").write_text("".join("\t".join(str(c[key]) for key in columns) + "\t" + (c["options"] or "-") + "\n" for c in cases), encoding="utf8")
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
    rows = []
    artifacts = {name: str(getattr(args, name).resolve()) if getattr(args, name) else None
                 for name in ("module", "abi_test", "consumer", "region", "manifest", "oracle")}

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
    report = {"schema": "qpx.ics-plugin-validation/1", "checks": rows,
              "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        fixture_manifest = write_fixtures(fixtures, args.oracle)
        report.update(fixture_cases=len(fixture_manifest["cases"]), negative_cases=len(fixture_manifest["negative"]),
                      independent_reference=fixture_manifest["oracle"])
        run([str(args.abi_test.resolve()), str(args.module.resolve()), str(fixtures)], output, "direct-abi", rows)
        if args.consumer and args.manifest:
            work = output / "project"
            work.mkdir()
            consumer = [str(args.consumer.resolve())]
            for mode in ("create", "reopen", "missing"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)
            gzip_work = output / "gzip-project"
            gzip_work.mkdir()
            for mode in ("gzip-create", "gzip-reopen", "missing", "gzip-changed-companion"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(gzip_work)],
                    output, "gzip-project-" + mode.removeprefix("gzip-"), rows)
            external_work = output / "external-project"
            external_work.mkdir()
            for mode in ("external-create", "external-reopen", "missing", "external-changed-companion"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(external_work)],
                    output, "external-project-" + mode.removeprefix("external-"), rows)

            def stage_manifest(directory, *, overlay=False, window=None):
                directory.mkdir()
                changed = json.loads(args.manifest.read_text(encoding="utf8"))
                for descriptor in changed["plugins"]:
                    target = directory / descriptor["module"]["path"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    data = (args.manifest.parent / descriptor["module"]["path"]).read_bytes()
                    if overlay:
                        data += b"QPX ICS inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    if window:
                        descriptor["options"] = {"channel": 1, "window": window}
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
            for mode in ("changed-source", "changed-companion", "changed-file-set"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.ics", str(fixtures / "channels-main.ics"), "0", "0", "0", "5", "3", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "channels-main.rgba").read_bytes(), "Host PAM differs from independent scalar-window oracle"
            command = [str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.ics"]
            for stem, descriptor, channels, sample in (
                ("gzip-channels", "<u2", 3, 2), ("gzip-companion", "<u2", 3, 2),
                ("gzip-int16", "<i2", 3, 2), ("gzip-float", "<f4", 2, 4),
                ("external-raw", "<u2", 3, 2), ("external-gzip", "<f4", 2, 4)
            ):
                target = output / (stem + "-z1.npy")
                run(command + ["--raw", str(fixtures / (stem + ".ics")), str(target), "--width", "5", "--height", "3",
                               "--z", "1", "--channels", str(channels)], output, "host-" + stem + "-z1", rows)
                exported = target.read_bytes()
                assert exported[:8] == b"\x93NUMPY\x01\0", stem
                end = 10 + int.from_bytes(exported[8:10], "little")
                header = ast.literal_eval(exported[10:end].decode("ascii").strip())
                assert header == {"descr": descriptor, "fortran_order": False, "shape": (3, 5, channels)}, (stem, header)
                assert exported[end:] == (fixtures / (stem + ".raw")).read_bytes()[15 * channels * sample:], stem
            for stem, channels, sample, z, t, depth in (
                ("time-ids", 3, 2, 1, 2, 2),
                ("time-inline-gzip", 2, 4, 0, 1, 1),
                ("time-external", 3, 2, 1, 1, 2),
                ("axis-time", 3, 2, 1, 2, 2),
                ("axis-channel-middle", 2, 4, 1, 0, 2),
            ):
                target = output / (stem + "-later.npy")
                run(command + ["--raw", str(fixtures / (stem + ".ics")), str(target), "--width", "5", "--height", "3",
                               "--z", str(z), "--time", str(t), "--channels", str(channels)],
                    output, "host-" + stem + "-later", rows)
                exported = target.read_bytes()
                assert exported[:8] == b"\x93NUMPY\x01\0", stem
                end = 10 + int.from_bytes(exported[8:10], "little")
                plane_bytes = 15 * channels * sample
                plane = t * depth + z
                assert exported[end:] == (fixtures / (stem + ".raw")).read_bytes()[plane*plane_bytes:(plane+1)*plane_bytes]
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = hashes()
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation input module/executable/manifest bytes changed; see validation.json")
    print(json.dumps({"passed": True, "fixture_cases": len(fixture_manifest["cases"]), "native_checks": len(rows), "oracle_exe_sha256": before["oracle"], "report": str(output / "validation.json")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="mode", required=True)
    fixtures = commands.add_parser("fixtures")
    fixtures.add_argument("output", type=Path)
    fixtures.add_argument("--oracle", type=Path)
    validation = commands.add_parser("validate")
    for argument in ("module", "abi-test", "output", "oracle"):
        validation.add_argument("--" + argument, required=True, type=Path)
    for argument in ("consumer", "manifest", "region"):
        validation.add_argument("--" + argument, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        manifest = write_fixtures(args.output.resolve(), args.oracle)
        print(json.dumps({"cases": len(manifest["cases"]), "negatives": len(manifest["negative"]), "output": str(args.output.resolve())}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
