"""Independent Fraction oracle for all ten raw dtypes; no native imports."""
import argparse
from fractions import Fraction
import json
import math
from pathlib import Path
import random
import struct

TYPES = ['u8', 'u16', 'i8', 'i16', 'u32', 'i32', 'u64', 'i64', 'f32', 'f64']
WIDTHS = [8, 16, 8, 16, 32, 32, 64, 64, 32, 64]


def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]


def value(kind, raw):
    width = WIDTHS[TYPES.index(kind)]
    if kind.startswith('f'):
        return struct.unpack('>f' if width == 32 else '>d', raw.to_bytes(width//8, 'big'))[0]
    return raw-(1 << width) if kind.startswith('i') and raw & (1 << (width-1)) else raw


def reference():
    rng = random.Random(173)
    cases = []

    def add(name, kind, raw, weights):
        numbers = [value(kind, r) for r in raw]
        assert all(math.isfinite(v) for v in numbers)
        exact = sum((Fraction(v)*w for v, w in zip(numbers, weights)), Fraction())/sum(weights)
        result = float(exact)
        cases.append(dict(name=name, type=kind, raw=[f'{r:016x}' for r in raw], weights=weights,
                          expected_binary64=f'{bits(result):016x}'))

    for kind, width in zip(TYPES[:8], WIDTHS[:8]):
        mask = (1 << width)-1
        extremes = [0, 1, mask, mask-1, 1 << (width-1), (1 << (width-1))-1]
        for i, raw in enumerate(extremes):
            add(kind+'-identity-'+str(i), kind, [raw], [1])
        for i in range(96):
            count = 1+i % 9
            raw = [rng.randrange(1 << width) for _ in range(count)]
            weights = [rng.randint(1, 1 << 18) for _ in raw]
            add(kind+'-weighted-'+str(i), kind, raw, weights)
    add('u64-no-input-double-rounding', 'u64', [2**53+1, 2**53+2], [1, 1])
    add('i64-cancel-extremes', 'i64', [1 << 63, (1 << 63)-1], [1, 1])
    add('u64-maximum-weight', 'u64', [(1 << 64)-1, 0], [(1 << 28)-1, 1])

    for kind, width in [('f32', 32), ('f64', 64)]:
        sign = 1 << (width-1)
        exponent_bits, fraction_bits = (8, 23) if width == 32 else (11, 52)
        maximum = (((1 << exponent_bits)-2) << fraction_bits) | ((1 << fraction_bits)-1)
        minimum_normal = 1 << fraction_bits
        one = (127 if width == 32 else 1023) << fraction_bits
        named = [
            ('signed-zero', [sign, 0], [1, 1]),
            ('maximum-no-float-overflow', [maximum, maximum], [1, 1]),
            ('cancel-maximum', [maximum, sign | maximum], [1, 1]),
            ('minimum-normal-boundary', [minimum_normal-1, minimum_normal], [1, 1]),
            ('positive-underflow-tie', [1, 0], [1, 1]),
            ('negative-underflow-tie', [sign | 1, 0], [1, 1]),
            ('subnormal-tie-even-up', [1, 2], [1, 1]),
            ('subnormal-tie-even-down', [2, 3], [1, 1]),
            ('normal-tie-even-down', [one, one+1], [1, 1]),
            ('normal-tie-even-up', [one+1, one+2], [1, 1]),
            ('cancel-retain-subnormal', [maximum, 3, sign | maximum], [1, 1, 1]),
            ('maximum-weight', [maximum, 0], [(1 << 28)-1, 1]),
        ]
        for name, raw, weights in named:
            add(kind+'-'+name, kind, raw, weights)
        for i in range(1024):
            count = 1+i % 11
            raw = []
            while len(raw) < count:
                candidate = rng.getrandbits(width)
                if math.isfinite(value(kind, candidate)):
                    raw.append(candidate)
            add(kind+'-weighted-'+str(i), kind, raw, [rng.randint(1, 1 << 17) for _ in raw])
    return dict(schema='qpx.exact-raw-weighted-mean-reference/1',
                scope='Finite weighted means rounded once to binary64; not raw sampling/workflow acceptance',
                types=TYPES, maximum_total_weight=1 << 28, cases=cases)


def native_header(data):
    out = ['#pragma once', '#include <cstdint>', 'namespace raw_mean_reference {',
           'struct Term {std::uint64_t raw;std::uint32_t weight;};',
           'struct Case {const char* name;unsigned type,offset,count;std::uint64_t expected;};',
           'inline constexpr Term terms[]={']
    for case in data['cases']:
        for raw, weight in zip(case['raw'], case['weights']):
            out.append('{0x'+raw+'ULL,'+str(weight)+'u},')
    out+=['};', 'inline constexpr Case cases[]={']
    offset = 0
    for case in data['cases']:
        count = len(case['raw'])
        out.append('{"'+case['name']+'",'+str(TYPES.index(case['type']))+','+str(offset)+','+str(count)+',0x'+case['expected_binary64']+'ULL},')
        offset += count
    return '\n'.join(out+['};', '}',''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native-header', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.native_header.exists():
        parser.error('reference outputs must be new paths')
    data = reference()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.native_header.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes((json.dumps(data, indent=2)+'\n').encode('utf-8'))
    args.native_header.write_bytes(native_header(data).encode('utf-8'))
    print(json.dumps(dict(cases=len(data['cases']), types=len(TYPES))))


if __name__ == '__main__':
    main()
