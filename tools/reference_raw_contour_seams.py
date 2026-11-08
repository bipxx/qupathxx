"""Tiny exact cell-edge oracle; no qupathxx, NumPy or geometry backend imports."""
from collections import Counter, deque
from fractions import Fraction
import argparse
import hashlib
import json
from pathlib import Path


def cases():
    return [
        dict(name='nested_holes', rows=['0000000', '0111110', '0122210',
             '0120210', '0122210', '0111110', '0000000'], holes=[1,1]),
        dict(name='diagonal_contacts', rows=['000000', '010000', '001000',
             '000100', '000010', '000000'], holes=[0]),
        dict(name='three_class_junction', rows=['1112', '1132', '3332', '2222'], holes=[0,0,0]),
        dict(name='cropped_anisotropic', rows=['1111', '1221', '1221', '1111'],
             origin=[1, 2], extent=[30, 63], samples=[12, 18], first=[3, 3],
             image_size=[32, 66], holes=[1,0]),
    ]


def normalize(case):
    case=dict(case)
    height, width=len(case['rows']), len(case['rows'][0])
    assert all(len(row)==width and set(row)<={'0', '1', '2', '3'} for row in case['rows'])
    case.setdefault('origin', [0, 0]);case.setdefault('extent', [width, height])
    case.setdefault('samples', [width, height]);case.setdefault('first', [0, 0])
    case.setdefault('image_size', [width, height])
    return case


def edges(rows, first=(0, 0)):
    """Directed unit edges around each labelled closed cell, including holes."""
    height, width=len(rows), len(rows[0])
    result={int(label):[] for label in sorted(set(''.join(rows))-{'0'})}
    for y, row in enumerate(rows):
        for x, value in enumerate(row):
            if value=='0':continue
            xx, yy=first[0]+x, first[1]+y
            sides=[(0, -1, (xx, yy, xx+1, yy)), (1, 0, (xx+1, yy, xx+1, yy+1)),
                   (0, 1, (xx+1, yy+1, xx, yy+1)), (-1, 0, (xx, yy+1, xx, yy))]
            for dx, dy, segment in sides:
                nx, ny=x+dx, y+dy
                if not (0<=nx<width and 0<=ny<height) or rows[ny][nx]!=value:
                    result[int(value)].append(segment)
    return {label:sorted(segments) for label, segments in result.items()}


def components(rows, label):
    remaining={(x, y) for y, row in enumerate(rows) for x, value in enumerate(row)
               if value==str(label)}
    count=0
    while remaining:
        count+=1;pending=deque([remaining.pop()])
        while pending:
            x, y=pending.popleft()
            for point in [(x-1,y), (x+1,y), (x,y-1), (x,y+1)]:
                if point in remaining:remaining.remove(point);pending.append(point)
    return count


def tile_edges(case, width, height):
    rows=case['rows'];first=case['first'];result=[]
    for y in range(0, len(rows), height):
        for x in range(0, len(rows[0]), width):
            tile=[row[x:x+width] for row in rows[y:y+height]]
            result.append(edges(tile, (first[0]+x, first[1]+y)))
    return result


def cancel_seams(blocks):
    """Reference cancellation only; this does not polygonize or authorize apply."""
    pending={}
    for block in blocks:
        for label, segments in block.items():
            counts=pending.setdefault(label, Counter())
            for segment in segments:
                x0, y0, x1, y1=segment;reverse=(x1, y1, x0, y0)
                if counts[reverse]:
                    counts[reverse]-=1
                    if not counts[reverse]:del counts[reverse]
                else:counts[segment]+=1
    return {label:sorted(counts.elements()) for label, counts in pending.items()}


def reference():
    output=[]
    for case in map(normalize, cases()):
        sx, sy=[Fraction(span, count) for span, count in zip(case['extent'],case['samples'])]
        boundary=edges(case['rows'], case['first']);classes=[]
        for label, segments in boundary.items():
            pixels=sum(row.count(str(label)) for row in case['rows'])
            horizontal=sum(y0==y1 for x0,y0,x1,y1 in segments)
            perimeter=horizontal*sx+(len(segments)-horizontal)*sy
            classes.append(dict(label=label, pixels=pixels, parts=components(case['rows'],label),
                                holes=case['holes'][label-1],
                                area=str(pixels*sx*sy), perimeter=str(perimeter),
                                edges=[list(edge) for edge in segments]))
        tilings=[]
        for width,height in [(1,1), (2,3), (3,2), (4,4)]:
            blocks=tile_edges(case,width,height)
            assert cancel_seams(blocks)==boundary==cancel_seams(reversed(blocks))
            tilings.append(dict(width=width,height=height,blocks=len(blocks),
                                naive_edges=sum(len(v) for b in blocks for v in b.values()),
                                joined_edges=sum(map(len,boundary.values()))))
        output.append(dict(**case,classes=classes,tilings=tilings))
    return dict(schema='qpx.raw-contour-seams-reference/1',
                scope='Synthetic exact unit-cell boundaries and rational axis mapping; no biological or streamed-native acceptance.',
                label_zero='Unclassified; no geometry',
                connectivity='Four-neighbour positive cell components; point contacts remain separate polygon parts.',
                cases=output)


def encoded():
    return (json.dumps(reference(),indent=2,sort_keys=True)+'\n').encode('utf-8')


def cpp_header():
    lines=['#pragma once','#include <array>','#include <cstdint>','#include <vector>',
           '// Independent cell-edge vectors; reference JSON SHA256 '+hashlib.sha256(encoded()).hexdigest(),
           'namespace raw_contour_seam_reference {',
           'struct Edge {std::uint32_t label;std::int64_t x0,y0,x1,y1;};',
           'struct Case {const char* name;std::array<std::int64_t,2> origin,extent,samples,first,image_size;std::int64_t width,height;std::vector<std::uint32_t> labels;std::vector<Edge> edges;std::array<std::uint64_t,3> counts;};',
           'inline std::vector<Case> cases() {return {']
    pair=lambda values:'{{'+','.join(map(str,values))+'}}'
    for case in reference()['cases']:
        fields=[json.dumps(case['name'])]
        fields.extend(pair(case[name]) for name in ['origin','extent','samples','first','image_size'])
        fields.extend([str(len(case['rows'][0])),str(len(case['rows']))])
        fields.append('{'+','.join(''.join(case['rows']))+'}')
        edges=['{'+','.join(map(str,[c['label'],*edge]))+'}' for c in case['classes'] for edge in c['edges']]
        fields.append('{\n'+',\n'.join(edges)+'\n}')
        counts={c['label']:c['pixels'] for c in case['classes']}
        fields.append(pair([counts.get(label,0) for label in [1,2,3]]))
        lines.append('{'+','.join(fields)+'},')
    lines.extend(['};}','}'])
    return ('\n'.join(lines)+'\n').encode('utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--header',type=Path)
    args=parser.parse_args()
    # An existing capture is evidence, not a destination to overwrite.
    outputs=[(args.output,encoded())]
    if args.header is not None:outputs.append((args.header,cpp_header()))
    if len({p.resolve() for p,_ in outputs})!=len(outputs):raise ValueError('capture destinations must differ')
    for path,_ in outputs:
        if path.exists():raise FileExistsError(path)
    for path,data in outputs:
        with path.open('xb') as stream:stream.write(data)
