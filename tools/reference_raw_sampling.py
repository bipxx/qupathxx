"""Independent all-source Fraction overlaps and hashlib raw-grid identities."""
import argparse
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path
import struct

TYPES = ['u8', 'u16', 'i8', 'i16', 'u32', 'i32', 'u64', 'i64', 'f32', 'f64']
WIDTHS = [8, 16, 8, 16, 32, 32, 64, 64, 32, 64]
IW, IH, CHANNELS, Z, TIME = 5, 4, 3, 2, 2


def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]


def number(kind, raw):
    width = WIDTHS[TYPES.index(kind)]
    if kind.startswith('f'):
        return struct.unpack('>f' if width == 32 else '>d', raw.to_bytes(width//8, 'big'))[0]
    return raw-(1 << width) if kind.startswith('i') and raw & (1 << (width-1)) else raw


def bank(kind):
    width = WIDTHS[TYPES.index(kind)]
    if kind.startswith('f'):
        mantissa, exponent = (23, 127) if width == 32 else (52, 1023)
        sign = 1 << (width-1)
        maximum = (((1 << (8 if width == 32 else 11))-2) << mantissa) | ((1 << mantissa)-1)
        values = [0, sign, 1, (1 << mantissa), maximum, sign | maximum,
                  exponent << mantissa, (exponent+1) << mantissa, sign | (exponent << mantissa)]
    else:
        values = [0, 1, (1 << width)-1, 1 << (width-1), (1 << (width-1))-1,
                  (1 << width)-2, 3, 7, 11]
        if width == 64:
            values += [2**53+1, 2**53+2]
    return [values[(x+3*y+5*c+7*z+11*t) % len(values)]
            for t in range(TIME) for z in range(Z) for y in range(IH) for x in range(IW) for c in range(CHANNELS)]


def bounds(case):
    out = []
    for origin, extent, count, context, limit in zip(case['origin'], case['extent'], case['samples'], case['context'], [IW, IH]):
        first = math.floor(F(origin)-F(context*extent, count))
        stop = math.ceil(F(origin)+F((count+context)*extent, count))
        left, right = max(0, min(limit, first)), max(0, min(limit, stop))
        out.append((left, right-left))
    return out[0][0], out[1][0], out[0][1], out[1][1]


def identities(case, raw, invalid):
    kind, first, count, z, t = case['type'], *case['selection']
    dtype = TYPES.index(kind)
    bx, by, bw, bh = bounds(case)
    decoded = hashlib.sha256(b'qpx.raw-level-zero-window/1')
    def integer(h, v): h.update((v & ((1 << 64)-1)).to_bytes(8, 'little'))
    def text(h, v):
        data = v.encode('utf-8');integer(h, len(data));h.update(data)
    for v in [IW, IH, CHANNELS, Z, TIME, 1, 2, dtype, bx, by, bw, bh, first, count, z, t]:
        integer(decoded, v)
    valid = []
    for y in range(by, by+bh):
        for x in range(bx, bx+bw):
            pixel = ((t*Z+z)*IH+y)*IW+x
            valid.append(int(pixel not in invalid))
            for c in range(first, first+count):
                decoded.update(raw[pixel*CHANNELS+c].to_bytes(WIDTHS[dtype]//8, 'little'))
    decoded.update(bytes(valid))
    input_hash = hashlib.sha256(b'qpx.grid-raw-area-exact/1')
    text(input_hash, decoded.hexdigest());text(input_hash, 'raw-reference')
    for v in [IW, IH, *case['origin'], *case['extent'], *case['samples'], *case['context']]:
        integer(input_hash, v)
    text(input_hash, 'XYZCT');integer(input_hash, 1)
    integer(input_hash, IW);integer(input_hash, IH);input_hash.update(struct.pack('<d', 1.))
    integer(input_hash, 3)
    for name in ['c0', 'c1', 'c2']: text(input_hash, name)
    for v in [.5, .25, 1.5, 2.]:
        integer(input_hash, 1);input_hash.update(struct.pack('<d', v))
    return decoded.hexdigest(), input_hash.hexdigest()


def expected(case, raw, invalid):
    kind = case['type'];first, channels, z, t = case['selection']
    nx, ny = case['samples'];cx, cy = case['context']
    ox, oy = case['origin'];ex, ey = case['extent']
    values, reasons = [], []
    for j in range(-cy, ny+cy):
        y0, y1 = F(oy)+F(j*ey, ny), F(oy)+F((j+1)*ey, ny)
        for i in range(-cx, nx+cx):
            x0, x1 = F(ox)+F(i*ex, nx), F(ox)+F((i+1)*ex, nx)
            if x0 < 0 or y0 < 0 or x1 > IW or y1 > IH:
                values.extend(['0000000000000000']*channels);reasons.extend([1]*channels);continue
            # All-source intersections, not the production intersecting-range walk.
            pixels = []
            for y in range(IH):
                for x in range(IW):
                    overlap = max(F(0), min(x1, x+1)-max(x0, x))*max(F(0), min(y1, y+1)-max(y0, y))
                    if overlap:
                        pixel = ((t*Z+z)*IH+y)*IW+x
                        pixels.append((pixel, overlap))
            for c in range(first, first+channels):
                if any(pixel in invalid for pixel, _ in pixels):
                    values.append('0000000000000000');reasons.append(2);continue
                terms = [(number(kind, raw[pixel*CHANNELS+c]), weight) for pixel, weight in pixels]
                if any(not math.isfinite(v) for v, _ in terms):
                    values.append('0000000000000000');reasons.append(3);continue
                mean = sum((F(v)*weight for v, weight in terms), F())/((x1-x0)*(y1-y0))
                values.append(f'{bits(float(mean)):016x}');reasons.append(0)
    return values, reasons


def reference():
    banks = {kind: bank(kind) for kind in TYPES}
    plans = [
        ('identity', [0, 0], [5, 4], [5, 4], [0, 0], [0, 3, 0, 0]),
        ('fractional', [0, 0], [5, 4], [3, 3], [1, 1], [1, 2, 1, 0]),
        ('upsampling', [1, 1], [3, 2], [5, 3], [1, 1], [0, 3, 0, 1]),
        ('outside-context', [-2, -1], [4, 3], [3, 2], [2, 2], [1, 2, 1, 1]),
        ('outside', [9, 9], [2, 2], [2, 2], [0, 0], [0, 1, 0, 0]),
        ('partial-support', [0, 0], [6, 5], [4, 3], [0, 0], [0, 3, 1, 1]),
        ('one-channel', [0, 0], [5, 4], [2, 2], [0, 0], [2, 1, 1, 0]),
        ('invalid-source', [0, 0], [5, 4], [3, 3], [0, 0], [0, 3, 0, 0]),
    ]
    cases = []
    def add(kind, name, origin, extent, samples, context, selection, overrides=None, invalid=None):
        case = dict(name=kind+'-'+name, type=kind, origin=origin, extent=extent, samples=samples, context=context,
                    selection=selection, overrides=overrides or [], invalid=invalid or [])
        raw = banks[kind].copy()
        for index, value in case['overrides']: raw[index] = value
        wanted, reasons = expected(case, raw, set(case['invalid']))
        decoded, digest = identities(case, raw, set(case['invalid']))
        case.update(overrides=[[i, f'{v:016x}'] for i, v in case['overrides']],
                    expected_binary64=wanted, reasons=reasons, decoded_sha256=decoded, input_sha256=digest)
        cases.append(case)
    for kind in TYPES:
        for name, origin, extent, samples, context, selection in plans:
            add(kind, name, origin, extent, samples, context, selection, invalid=[7] if name == 'invalid-source' else [])
        if kind in ('u64', 'i64'):
            pair = [2**53+1, 2**53+2] if kind == 'u64' else [1 << 63, (1 << 63)-1]
            add(kind, 'integer-pair', [0, 0], [2, 1], [1, 1], [0, 0], [0, 1, 0, 0], [[0, pair[0]], [3, pair[1]]])
        if kind.startswith('f'):
            width = WIDTHS[TYPES.index(kind)];mantissa = 23 if width == 32 else 52
            nan = ((1 << (8 if width == 32 else 11))-1) << mantissa
            for tag, raw in [('nan-payload', nan+0x123), ('infinity', nan), ('negative-infinity', (1 << (width-1)) | nan)]:
                add(kind, tag, [0, 0], [2, 1], [1, 1], [0, 0], [0, 3, 0, 0], [[1, raw]])
            add(kind, 'invalid-before-nan', [0, 0], [2, 1], [1, 1], [0, 0], [0, 3, 0, 0], [[1, nan+1]], [0])
            add(kind, 'unselected-nan', [0, 0], [2, 1], [1, 1], [0, 0], [1, 2, 0, 0], [[0, nan+1]])
            add(kind, 'signed-underflow', [0, 0], [2, 1], [1, 1], [0, 0], [0, 1, 0, 0], [[0, (1 << (width-1)) | 1], [3, 0]])
    return dict(schema='qpx.scientific-raw-sampling-reference/1', image=[IW, IH], channels=CHANNELS, z=Z, t=TIME,
                types=TYPES, banks={k: [f'{v:016x}' for v in raw] for k, raw in banks.items()}, cases=cases)


def native_header(data):
    out = ['#pragma once', '#include <cstdint>', 'namespace raw_sampling_reference {',
        'struct Case {const char* name;unsigned type;std::int64_t x,y,w,h,nx,ny,cx,cy;unsigned first,channels,z,t,offset;const char* decoded;const char* input;};',
        'struct Override {unsigned which,index;std::uint64_t raw;};', 'struct Invalid {unsigned which,index;};',
        'inline constexpr std::uint64_t banks[10][240]={']
    for kind in TYPES: out.append('{'+','.join('0x'+v+'ULL' for v in data['banks'][kind])+'},')
    out+=['};', 'inline constexpr Case cases[]={']
    offset = 0
    for c in data['cases']:
        fields = [TYPES.index(c['type']), *c['origin'], *c['extent'], *c['samples'], *c['context'], *c['selection'], offset]
        out.append('{"'+c['name']+'",'+','.join(map(str, fields))+',"'+c['decoded_sha256']+'","'+c['input_sha256']+'"},')
        offset += len(c['reasons'])
    out+=['};', 'inline constexpr Override overrides[]={']
    for i, c in enumerate(data['cases']):
        for index, raw in c['overrides']: out.append('{'+str(i)+','+str(index)+',0x'+raw+'ULL},')
    out+=['};', 'inline constexpr Invalid invalid[]={']
    for i, c in enumerate(data['cases']):
        for index in c['invalid']: out.append('{'+str(i)+','+str(index)+'},')
    out+=['};', 'inline constexpr std::uint64_t expected[]={']
    for c in data['cases']: out.append(','.join('0x'+v+'ULL' for v in c['expected_binary64'])+',')
    out+=['};', 'inline constexpr unsigned char reasons[]={']
    for c in data['cases']: out.append(','.join(map(str, c['reasons']))+',')
    return '\n'.join(out+['};', '}', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native-header', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.native_header.exists(): parser.error('reference outputs must be new paths')
    data = reference()
    for path in [args.output, args.native_header]: path.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes((json.dumps(data, indent=2)+'\n').encode('utf-8'))
    args.native_header.write_bytes(native_header(data).encode('utf-8'))
    print(json.dumps(dict(cases=len(data['cases']), elements=sum(len(c['reasons']) for c in data['cases']))))


if __name__ == '__main__': main()
