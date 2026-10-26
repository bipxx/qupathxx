"""Independent Decimal 2-D quantitative feature oracle (stdlib only).

Run from any directory; --check verifies the committed C++ fixture byte-for-byte.
No native module, NumPy, separable filter, or production kernel is imported.
"""
from __future__ import annotations

import argparse
from decimal import Decimal, localcontext
import math
from pathlib import Path
import struct

WIDTH, HEIGHT, CHANNELS = 7, 5, 2
SIGMAS = ((0, 0), (0, 1), (.5, 1), (1, 1))
TYPES = (("UInt8", "B", 8), ("UInt16", "H", 16), ("Int8", "b", 8),
         ("Int16", "h", 16), ("UInt32", "I", 32), ("Int32", "i", 32),
         ("UInt64", "Q", 64), ("Int64", "q", 64),
         ("Float32", "f", 32), ("Float64", "d", 64))


def bits32(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def rounded32(value: Decimal) -> int:
    # Compare exact Decimal distances to neighboring binary32 values, avoiding
    # a binary64 -> binary32 double-rounding ambiguity at halfway cases.
    magnitude = abs(value)
    seed = bits32(float(magnitude))
    candidates = (max(0, seed - 1), seed, seed + 1)
    def distance(bits: int) -> tuple[Decimal, int]:
        candidate = struct.unpack("<f", struct.pack("<I", bits))[0]
        return abs(Decimal.from_float(candidate) - magnitude), bits & 1
    result = min(candidates, key=distance)
    return result | (0x80000000 if value.is_signed() else 0)


def source(case: int, x: int, y: int, c: int) -> tuple[int, float | int]:
    dtype = min(case, 9) if case < 10 else (9 if case == 10 else 8)
    name, fmt, size = TYPES[dtype]
    index = y * WIDTH + x
    if case == 10:
        value = (-1 if (index + c) % 2 else 1) * 1e308
    elif case == 11:
        value = math.ldexp(index + c + 1, -149)
    elif dtype == 6:
        value = (1 << 64) - 1 - (index * 2 + c)
    elif dtype == 7:
        value = -(1 << 63) + index * 2 + c
    elif name.startswith("UInt"):
        value = index + 20 * c
    elif name.startswith("Int"):
        value = index + 20 * c - 60
    else:
        value = (index - 12 + 7 * c) / 8
        if (x, y, c) == (0, 0, 0):
            value = math.nan
        if (x, y, c) == (1, 0, 1):
            value = math.inf
    data = struct.pack("<" + fmt, value)
    return int.from_bytes(data, "little"), struct.unpack("<" + fmt, data)[0]


def reference(case: int) -> tuple[list[int], list[int], float, float]:
    dtype = case if case < 10 else (9 if case == 10 else 8)
    scale = (math.ldexp(1., -1024) if case == 10 else
             math.ldexp(1., 120) if case == 11 else
             math.ldexp(1., -TYPES[dtype][2]) if dtype < 8 else 1 / 32)
    raw, base, domain = [], [], []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            for c in range(CHANNELS):
                bits, value = source(case, x, y, c)
                raw.append(bits)
                valid = (x, y) != (3, 2) and math.isfinite(value)
                domain.append(valid)
                # Typed scientific mean -> binary64, explicit binary64 affine.
                normalized = (float(value) - 0.) * scale if valid else 0.
                base.append(Decimal.from_float(normalized))
    planes: list[list[Decimal]] = []
    for c in range(CHANNELS):
        for sx, sy in SIGMAS:
            rx, ry = math.ceil(3 * sx), math.ceil(3 * sy)
            kernel = []
            for dy in range(-ry, ry + 1):
                for dx in range(-rx, rx + 1):
                    exponent = (Decimal(dx * dx) / (2 * Decimal.from_float(sx) ** 2) if sx else Decimal(0))
                    exponent += (Decimal(dy * dy) / (2 * Decimal.from_float(sy) ** 2) if sy else Decimal(0))
                    kernel.append((dx, dy, (-exponent).exp()))
            smoothed = []
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    numerator = support = Decimal(0)
                    for dx, dy, weight in kernel:
                        xx, yy = x + dx, y + dy
                        if 0 <= xx < WIDTH and 0 <= yy < HEIGHT:
                            index = (yy * WIDTH + xx) * CHANNELS + c
                            if domain[index]:
                                numerator += base[index] * weight
                                support += weight
                    smoothed.append(numerator / support if support else Decimal(0))
            gaussian, gradient, laplacian = [], [], []
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    center = smoothed[y * WIDTH + x]
                    def neighbor(xx: int, yy: int) -> Decimal:
                        return smoothed[yy * WIDTH + xx] if 0 <= xx < WIDTH and 0 <= yy < HEIGHT and domain[(yy * WIDTH + xx) * CHANNELS + c] else center
                    left, right = neighbor(x - 1, y), neighbor(x + 1, y)
                    top, bottom = neighbor(x, y - 1), neighbor(x, y + 1)
                    gaussian.append(center)
                    gradient.append((((right - left) / 2) ** 2 + ((bottom - top) / 2) ** 2).sqrt())
                    laplacian.append(left + right + top + bottom - 4 * center)
            planes.extend((gaussian, gradient, laplacian))
    expected = []
    for i in range(WIDTH * HEIGHT):
        for col, plane in enumerate(planes):
            valid = domain[i * CHANNELS + col // 12]
            expected.append(rounded32(plane[i]) if valid else 0x7fc00000)
    return raw, expected, scale, max(float(abs(v)) for v in base)


def render() -> str:
    lines = ["// Generated by tools/reference_raw_pixel_features.py; do not edit.",
             "#pragma once", "#include <cstdint>", "namespace raw_feature_reference {",
             "struct Case {int type;double scale,max_normalized;std::uint64_t raw[70];std::uint32_t expected[840];};",
             "inline const Case cases[]={"]
    with localcontext() as context:
        context.prec = 80
        for case in range(12):
            raw, expected, scale, maximum = reference(case)
            dtype = case if case < 10 else (9 if case == 10 else 8)
            lines.append("{" + f"{dtype},{scale.hex()},{maximum.hex()}," + "{")
            for i in range(0, len(raw), 10):
                lines.append(",".join(f"0x{v:016x}ULL" for v in raw[i:i + 10]) + ",")
            lines.append("},{")
            for i in range(0, len(expected), 9):
                lines.append(",".join(f"0x{v:08x}u" for v in expected[i:i + 9]) + ",")
            lines.append("}},")
    return "\n".join(lines + ["};", "}", ""])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    with localcontext() as context:
        context.prec = 160
        half_ulp = Decimal(2) ** -24
        assert rounded32(Decimal(1) + half_ulp) == 0x3f800000
        assert rounded32(Decimal(1) + 3 * half_ulp) == 0x3f800002
        half_subnormal = Decimal(2) ** -150
        assert rounded32(half_subnormal) == 0
        assert rounded32(3 * half_subnormal) == 2
        assert rounded32(-half_subnormal) == 0x80000000
    target = Path(__file__).resolve().parents[1] / "cpp/tests/raw_pixel_feature_reference.hpp"
    data = render().encode("utf-8")
    if args.check:
        if target.read_bytes() != data:
            raise SystemExit("raw feature reference differs; regenerate and review")
        print("12 independently generated raw feature cases match committed bytes")
    else:
        target.write_bytes(data)
        print(f"Wrote {target.name}: {len(data)} bytes, 12 cases / 10,080 features")


if __name__ == "__main__":
    main()
