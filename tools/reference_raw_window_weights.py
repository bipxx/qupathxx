"""Independent exact Fraction oracle for wide raw-window means (stdlib only)."""
from fractions import Fraction
from pathlib import Path
import json
import math
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
MASK = (1 << 64) - 1

def bits(value):
    return struct.unpack('<Q', struct.pack('<d', value))[0]

maximum = sys.float_info.max
tiny = float.fromhex('0x0.0000000000001p-1022')
large = (1 << 64) - 1
cases = [
    (1, large, large-2, (1 << 63)+17, large-30),
    (0, (1 << 63)-1, -(1 << 63), large, large),
    (0, -(1 << 63), -(1 << 63), 1 << 63, 1 << 63),
    (2, maximum, -maximum, 1 << 63, 1 << 63),
    (2, maximum, -maximum, large, large-2),
    (2, tiny, 0., 1 << 63, 1 << 63),
    (2, 3*tiny, 0., 1 << 63, 1 << 63),
    (2, -tiny, 0., 1 << 63, 1 << 63),
    (2, 1., math.nextafter(1., math.inf), large, large),
    (2, 1., math.nextafter(1., math.inf), large-1, large),
    (2, tiny, -3*tiny, large, large-8),
    (1, large, 0, large-2, large),
    (2, float.fromhex('0x1p+1023'), tiny, large, large-2),
]
rows = []
for kind, a, b, wa, wb in cases:
    answer = float((Fraction(a)*wa + Fraction(b)*wb)/(wa+wb))
    rows.append(dict(kind=kind, a=bits(a) if kind==2 else a & MASK,
                     b=bits(b) if kind==2 else b & MASK,
                     weight_a=wa, weight_b=wb, expected=bits(answer)))

def pixel(x, y):
    return MASK - (x*65537 + y*16777259)

def mean(x, y):
    # Original axes (-1,999983,1000000), (2,999979,1000000).
    left = Fraction(-1) + Fraction(x*999983, 1000000)
    right = left + Fraction(999983, 1000000)
    top = Fraction(2) + Fraction(y*999979, 1000000)
    bottom = top + Fraction(999979, 1000000)
    total = Fraction()
    for sy in range(math.floor(top), math.ceil(bottom)):
        for sx in range(math.floor(left), math.ceil(right)):
            wx = min(right, sx+1)-max(left, sx)
            wy = min(bottom, sy+1)-max(top, sy)
            total += pixel(sx, sy)*wx*wy
    return bits(float(total/((right-left)*(bottom-top))))

expected = [mean(x,y) for y in range(123455,123459) for x in range(543209,543214)]
header = ['#pragma once', '#include <cstdint>', 'namespace raw_window_reference {',
          'struct Mean {unsigned kind;std::uint64_t a,b,weight_a,weight_b,expected;};',
          'inline constexpr Mean means[]={']
for row in rows:
    header.append('    {'+str(row['kind'])+','+','.join(str(row[k])+'ULL' for k in ('a','b','weight_a','weight_b','expected'))+'},')
header += ['};', 'inline constexpr std::uint64_t large_window[]={',
           '    '+','.join(str(v)+'ULL' for v in expected), '};', '}']
(ROOT/'cpp/tests/raw_window_weight_reference.hpp').write_text('\n'.join(header)+'\n', encoding='utf-8', newline='\n')
(ROOT/'docs/benchmarks/raw-window-fraction-reference.json').write_text(json.dumps(dict(
    schema='qpx.raw-window-fraction-reference/1', authority='Python Fraction exact arithmetic; one float conversion to binary64 nearest/even',
    weighted_cases=[{k:(str(v) if k!='kind' else v) for k,v in row.items()} for row in rows],
    lattice=dict(image_width=1000000,image_height=1000000,x=[-1,999983,1000000,1],y=[2,999979,1000000,1],first=[543210,123456],core=[3,2]),
    source='uint64 max - (x*65537 + y*16777259)', expected_binary64_bits=[str(v) for v in expected]), indent=2)+'\n', encoding='utf-8', newline='\n')
print(f'{len(rows)} wide means and {len(expected)} irreducible-lattice sampled slots')
