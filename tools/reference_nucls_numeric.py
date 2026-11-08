"""Independent legacy float/fsum centroid oracle; never imported by product code."""
import argparse
import json
import math
from pathlib import Path
import random
import struct


def bits(value):
    return f"{struct.unpack('<Q', struct.pack('<d', value))[0]:016x}"


def generate():
    numbers = ["0", "-0", "+1.25e2", "1_000.50", ".5", "-1e-999", "1e-999",
               "4.9406564584124654e-324", "1.7976931348623157e308",
               "\u0661\u0662.\u0665", "\U0001d7d9\U0001d7da", "\u3000+1_2.5\u00a0"]
    invalid = ["", "nan", "inf", "1e999", "0x1p2", "_1", "1_", "1__0", "1_e2", "1e_2", "\x1c1"]
    sums = [[1e16, 1., -1e16], [1., 2.**-53, 2.**-1074], [-1., -2.**-53, -2.**-1074], [-0.], []]
    rng = random.Random(0x4e55434c53)
    for _ in range(24):
        values = [math.ldexp(rng.uniform(-1, 1), rng.randrange(-500, 500)) for _ in range(40)]
        sums.append(values)
    polygons = []
    for offset in [0., 1e6, 1e12, 1e15]:
        for count in [3, 5, 17, 64]:
            for reverse in [False, True]:
                points = []
                for i in range(count):
                    angle = 2 * math.pi * (i + rng.uniform(-.1, .1)) / count
                    radius = rng.uniform(32, 256)
                    points.append((offset + radius * math.cos(angle), offset + radius * math.sin(angle)))
                if reverse:
                    points.reverse()
                if count % 2:
                    points.append(points[0])
                xs, ys = list(zip(*points))
                x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
                local = [(x-x0, y-y0) for x, y in points]
                pairs = list(zip(local, local[1:] + local[:1]))
                cross = [a[0]*b[1]-b[0]*a[1] for a, b in pairs]
                area = math.fsum(cross)
                center = [origin + math.fsum((a[k]+b[k])*c for (a, b), c in zip(pairs, cross))/(3*area)
                          for k, origin in enumerate((x0, y0))]
                polygons.append(dict(row=dict(type='polyline', xmin=repr(x0), ymin=repr(y0), xmax=repr(x1), ymax=repr(y1),
                    coords_x=','.join(map(repr, xs)), coords_y=','.join(map(repr, ys))), centroid_bits=list(map(bits, center)), vertices=len(points)))
    return dict(numbers=[dict(text=s, bits=bits(float(s))) for s in numbers], invalid_numbers=invalid,
                sums=[dict(values=v, bits=bits(math.fsum(v))) for v in sums], polygons=polygons)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(generate(), indent=2, allow_nan=False)+'\n', encoding='utf-8')
