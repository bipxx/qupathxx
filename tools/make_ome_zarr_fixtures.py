"""Development-only independent byte author for the native NGFF0.4/Zarr2 tests.

Uses Python standard library, never a runtime reader or fallback. Chunk byte
layout is independently authored in C/F order, including padded edge chunks.
"""
from pathlib import Path
import gzip
import hashlib
import itertools
import json
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1] / 'tests/fixtures/ome-zarr'


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def array(path, shape, chunks, dtype, fmt, order, codec, fill, value, missing=()):
    write_json(path / '.zarray', dict(zarr_format=2, shape=shape, chunks=chunks,
               dtype=dtype, order=order, compressor=None if not codec else dict(id=codec, level=4),
               filters=None, fill_value=fill, dimension_separator='.'))
    for at in itertools.product(*(range((n+c-1)//c) for n, c in zip(shape, chunks))):
        if at in missing:
            continue
        coordinates = list(itertools.product(*(range(c) for c in chunks)))
        if order == 'F':
            coordinates.sort(key=lambda row: tuple(reversed(row)))
        values = []
        for coordinate in coordinates:
            global_index = tuple(a*c+i for a, c, i in zip(at, chunks, coordinate))
            values.append(value(global_index) if all(i < n for i, n in zip(global_index, shape)) else fill)
        blob = struct.pack(('>' if dtype[0] == '>' else '<') + fmt * len(values), *values)
        if codec == 'zlib':
            blob = zlib.compress(blob, 4)
        elif codec == 'gzip':
            blob = gzip.compress(blob, compresslevel=4, mtime=0)
        (path / '.'.join(map(str, at))).write_bytes(blob)


def group(path, axes, datasets, extra=None):
    write_json(path / '.zgroup', dict(zarr_format=2))
    multiscale = dict(version='0.4', axes=axes, datasets=datasets)
    if extra:
        multiscale.update(extra)
    write_json(path / '.zattrs', dict(multiscales=[multiscale]))


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    axes = [dict(name=n, type=t, **({'unit': u} if u else {})) for n, t, u in
            [('t', 'time', 'second'), ('c', 'channel', ''), ('z', 'space', 'micrometer'),
             ('y', 'space', 'micrometer'), ('x', 'space', 'micrometer')]]
    for name, dtype, order, codec in [('big-f-gzip', '>u2', 'F', 'gzip'),
                                     ('little-c-zlib', '<u2', 'C', 'zlib'),
                                     ('plain', '<u2', 'C', '')]:
        path = ROOT / name
        datasets = []
        for level, shape in enumerate(([2, 3, 2, 5, 7], [2, 3, 2, 3, 4])):
            factor = 2 ** level
            datasets.append(dict(path=str(level), coordinateTransformations=[
                dict(type='scale', scale=[.5, 1, 2, .5*factor, .75*factor]),
                dict(type='translation', translation=[0, 0, 1, 10, 20])]))
            array(path / str(level), shape, [1, 2, 1, 3, 4], dtype, 'H', order, codec, 7,
                  lambda p, d=factor: p[0]*10000+p[1]*1000+p[2]*100+p[3]*d*10+p[4]*d,
                  missing=[(1, 1, 0, 1, 1)] if level == 0 else [])
        group(path, axes, datasets, dict(coordinateTransformations=[
            dict(type='scale', scale=[1, 1, 2, 2, 2]),
            dict(type='translation', translation=[0, 0, 2, 3, 4])]))
    scalars = [('u1', 'B', [0, 1, 2, 253, 254, 255]),
               ('i1', 'b', [-128, -1, 0, 1, 126, 127]),
               ('u2', 'H', [0, 1, 2, 65533, 65534, 65535]),
               ('i2', 'h', [-32768, -1, 0, 1, 32766, 32767]),
               ('u4', 'I', [0, 1, 2, 4294967293, 4294967294, 4294967295]),
               ('i4', 'i', [-2147483648, -1, 0, 1, 2147483646, 2147483647]),
               ('u8', 'Q', [0, 1, 9007199254740993, 18446744073709551613, 18446744073709551614, 18446744073709551615]),
               ('i8', 'q', [-9223372036854775808, -9007199254740993, 0, 1, 9223372036854775806, 9223372036854775807]),
               ('f4', 'f', [-1.25, 0, 3.5, float('nan'), float('inf'), -float('inf')]),
               ('f8', 'd', [-1.25, 0, 3.5, float('nan'), float('inf'), -float('inf')])]
    for dtype, fmt, values in scalars:
        path = ROOT / dtype
        group(path, [dict(name=n, type='space') for n in ['y', 'x']],
              [dict(path='0', coordinateTransformations=[dict(type='scale', scale=[1, 1])])])
        array(path/'0', [2, 3], [2, 3], ('|' if dtype[-1] == '1' else '>')+dtype,
              fmt, 'C', '', 0, lambda p: values[p[0]*3+p[1]])
    entries = []
    for path in sorted(ROOT.rglob('*')):
        if path.is_file() and path.name != 'manifest.json':
            data = path.read_bytes()
            entries.append(dict(path=path.relative_to(ROOT).as_posix(), bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    write_json(ROOT/'manifest.json', dict(schema='qpx.authored-ome-zarr/1', license='AGPL-3.0-only',
               attribution='qupathxx contributors; independent synthetic values, no biological claims',
               generator='tools/make_ome_zarr_fixtures.py', files=entries))
    print(json.dumps(dict(files=len(entries), bytes=sum(e['bytes'] for e in entries))))


if __name__ == '__main__':
    main()
