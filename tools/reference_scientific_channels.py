"""Independent Decimal logarithms and pivoted matrix solve on sampled RGB means."""
import argparse
from decimal import Decimal,localcontext
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]


def bits(value):
    return struct.pack('>d',float(value)).hex()


def inverse(basis):
    # Gaussian elimination on a row-major matrix of stain columns; no native
    # cofactor formula or lookup table is used by this independent reference.
    matrix=[[Decimal.from_float(float(basis[c][r])) for c in range(3)]+
            [Decimal(int(r==c)) for c in range(3)] for r in range(3)]
    for col in range(3):
        pivot=max(range(col,3),key=lambda r:abs(matrix[r][col]))
        matrix[col],matrix[pivot]=matrix[pivot],matrix[col]
        scale=matrix[col][col];assert scale!=0
        matrix[col]=[v/scale for v in matrix[col]]
        for row in range(3):
            if row==col:continue
            scale=matrix[row][col];matrix[row]=[v-scale*p for v,p in zip(matrix[row],matrix[col])]
    return [row[3:] for row in matrix]


def profiles():
    return [dict(name='identity',raw=[[1,0,0],[0,1,0],[0,0,1]],basis=[[1,0,0],[0,1,0],[0,0,1]],white=[255]*3),
            dict(name='two-stain',raw=[[1,0,0],[3,4,0]],basis=[[1,0,0],[.6,.8,0],[0,0,1]],white=[255]*3),
            dict(name='signed-residual',raw=[[1,0,0],[3,0,4]],basis=[[1,0,0],[.6,0,.8],[0,-1,0]],white=[200,240,180]),
            dict(name='three-stain',raw=[[3,4,0],[0,3,4],[4,0,3]],basis=[[.6,.8,0],[0,.6,.8],[.8,0,.6]],white=[255,230,200])]


def reference():
    path=ROOT/'tests/fixtures/scientific-grid-v1/reference.json';sampler=json.loads(path.read_text(encoding='utf-8'))
    names=['identity','fractional-reduction','enlargement','context-phase','image-edge-context','partial-alpha','transparent-alpha','anisotropic-calibration']
    cases=[v for v in sampler['cases'] if v['name'] in names]
    cases+=[dict(name='mean-before-od',image=[2,1],extent=[0,0,2,1],samples=[1,1],context=[0,0],alpha={},
                 overrides=[[0,0,0,32,255,255],[1,0,255,64,0,255]],pixels=[dict(valid_support=True,rgb=['255/2','48/1','255/2'])]),
            dict(name='subunit-white-clamp',image=[2,1],extent=[0,0,2,1],samples=[1,1],context=[0,0],alpha={},
                 overrides=[[0,0,0,0,255,255],[1,0,1,2,255,255]],pixels=[dict(valid_support=True,rgb=['1/2','1/1','255/1'])]),
            dict(name='invalid-override',image=[2,1],extent=[0,0,2,1],samples=[1,1],context=[0,0],alpha={},
                 overrides=[[0,0,0,32,255,255],[1,0,255,64,0,128]],pixels=[dict(valid_support=False,rgb=None)])]
    results=[]
    with localcontext() as ctx:
        ctx.prec=60;ln10=Decimal(10).ln()
        for case in cases:
            for profile in profiles():
                rows=inverse(profile['basis']);od=[];stains=[]
                for pixel in case['pixels']:
                    if not pixel['valid_support']:
                        od.extend([bits(0)]*3);stains.extend([bits(0)]*3);continue
                    rgb=[Decimal.from_float(float(F(v))) for v in pixel['rgb']]
                    channels=[max(Decimal(0),-(max(Decimal(1),v)/Decimal(w)).ln()/ln10) for v,w in zip(rgb,profile['white'])]
                    od.extend(bits(v) for v in channels)
                    stains.extend(bits(sum(rows[s][c]*channels[c] for c in range(3))) for s in range(3))
                overrides=list(case.get('overrides',[]))
                for xy,alpha in case['alpha'].items():
                    x,y=map(int,xy.split(','));overrides.append([x,y,(17*x+3*y)%256,(5*x+19*y)%256,(11*x+7*y)%256,alpha])
                results.append(dict(name=case['name']+'-'+profile['name'],sampler_case=case['name'],profile=profile['name'],
                                    image=case['image'],extent=case['extent'],samples=case['samples'],context=case['context'],
                                    overrides=overrides,raw_stains=profile['raw'],normalized_stains=profile['basis'],white=profile['white'],
                                    valid=[int(p['valid_support']) for p in case['pixels']],od_bits=od,stain_bits=stains))
    return dict(schema='qpx.scientific-channel-oracle/1',scope='Float64 transform preparation, not detector or biological acceptance',
                generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),sampler_reference_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                absolute_tolerance=2e-12,relative_tolerance=2e-12,cases=results)


def native_header(data):
    lines=['// Independent Decimal channel expectations; generated, do not edit.','#pragma once','#include <array>','#include <cstdint>','#include <vector>',
           'namespace channel_reference {','struct Override {std::int64_t x,y;std::array<std::uint8_t,4> rgba;};',
           'struct Case {std::int64_t iw,ih,x,y,w,h,nx,ny,cx,cy;std::vector<Override> overrides;std::vector<std::array<double,3>> raw;std::array<std::array<double,3>,3> basis;std::array<double,3> white;std::vector<std::uint8_t> valid;std::vector<std::uint64_t> od,stains;};',
           'inline const Case cases[]={']
    vec=lambda v:'{'+','.join(map(str,v))+'}'
    nested=lambda v:'{'+','.join(vec(t) for t in v)+'}'
    hexes=lambda v:'{'+','.join('0x'+x+'ULL' for x in v)+'}'
    for v in data['cases']:
        geometry=[*v['image'],*v['extent'],*v['samples'],*v['context']]
        overrides='{'+','.join('{'+str(a[0])+','+str(a[1])+','+vec(a[2:])+'}' for a in v['overrides'])+'}'
        # std::array of std::array requires an extra aggregate brace layer.
        lines.append('{'+','.join(map(str,geometry))+','+','.join([overrides,nested(v['raw_stains']),'{'+nested(v['normalized_stains'])+'}',vec(v['white']),vec(v['valid']),hexes(v['od_bits']),hexes(v['stain_bits'])])+'},')
    return '\n'.join(lines+['};','}'])+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--native-header',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists() or args.native_header.exists():parser.error('capture outputs must not already exist')
    data=reference();json_data=(json.dumps(data,indent=2)+'\n').encode('utf-8');header_data=native_header(data).encode('utf-8')
    with args.output.open('xb') as out:out.write(json_data)
    with args.native_header.open('xb') as out:out.write(header_data)


if __name__=='__main__':main()
