"""Independent canonical large-window hashes; no native or NumPy imports."""
import argparse
import hashlib
from pathlib import Path
import struct

SHAPES = [(400,200), (10000,3)]


def expected(width, height):
    digest = hashlib.sha256(b'qpx.raw-level-zero-window/1')
    for n in [width,height,3,1,1,0,1,6,0,0,width,height,1,2,0,0]:
        digest.update(struct.pack('<Q', n))
    for y in range(height):
        for x in range(width):
            for c in [1,2]: digest.update(struct.pack('<Q', 2**63+((y*width+x)*3+c)*7+1))
    digest.update(bytes(int((x+y)%19 != 0) for y in range(height) for x in range(width)))
    decoded = digest.hexdigest()
    source = hashlib.sha256(b'qpx.grid-raw-area-exact/1')
    def integer(n): source.update(struct.pack('<Q', n))
    def text(s):
        value=s.encode();integer(len(value));source.update(value)
    text(decoded);text('stream-reference')
    for n in [width,height,0,0,width,height,7,5,0,0]: integer(n)
    text('XYC');integer(1);integer(width);integer(height);source.update(struct.pack('<d',1))
    integer(3)
    for name in ['c0','c1','c2']:text(name)
    for value in [.5,.25,None,None]:
        integer(value is not None)
        if value is not None:source.update(struct.pack('<d',value))
    return decoded,source.hexdigest()


def native_header():
    rows=[]
    for w,h in SHAPES:
        decoded,source=expected(w,h)
        rows.append(f'    {{{w},{h},"{decoded}","{source}"}},')
    return ('// Independent struct/hashlib hashes; regenerate with tools/reference_raw_streaming.py.\n'
            '#pragma once\nnamespace raw_streaming_reference {\n'
            'struct Case { long long width,height; const char* decoded; const char* input; };\n'
            'inline constexpr Case cases[]={\n'+'\n'.join(rows)+'\n};\n}\n')


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--native-header',type=Path,required=True)
    args=parser.parse_args()
    if args.native_header.exists():parser.error('output must be a new path')
    args.native_header.write_bytes(native_header().encode())


if __name__ == '__main__': main()
