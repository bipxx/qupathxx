"""Independent exhaustive Fraction distances and Decimal 2D Gaussian oracle."""
import argparse
from decimal import Decimal,localcontext
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path
import struct


def bits(value):
    return struct.pack('>d',float(value)).hex()


def distance_case(name,width,height,sx,sy,sites,outside=False):
    sx,sy=F(sx),F(sy);values=[];nearest=[]
    for y in range(height):
        for x in range(width):
            candidates=[((x-i%width)**2*sx*sx+(y-i//width)**2*sy*sy,i) for i in sites]
            if outside:
                candidates.extend([(F((x+1)**2)*sx*sx,2**32-1),(F((width-x)**2)*sx*sx,2**32-1),
                                   (F((y+1)**2)*sy*sy,2**32-1),(F((height-y)**2)*sy*sy,2**32-1)])
            cost,index=min(candidates) if candidates else (math.inf,2**32-1)
            values.append(bits(cost));nearest.append(index)
    return dict(name=name,width=width,height=height,sx=str(sx),sy=str(sy),sites=sites,outside=outside,
                squared_bits=values,nearest=nearest,
                work_bytes=12*width*height+12*height+4096)


def gaussian_case(name,width,height,sx,sy,sigma,kind='mixed'):
    sx,sy,sigma=F(sx),F(sy),F(sigma);sigmas=[sigma/sx,sigma/sy]
    radii=[math.ceil(3*v) for v in sigmas]
    values=[F(7,4) if kind=='constant' else F((i*37)%29-14,4) for i in range(width*height)]
    valid=[int(kind=='all' or (kind!='empty' and i%7!=3)) for i in range(width*height)]
    output=[]
    with localcontext() as ctx:
        ctx.prec=60
        def dec(value):
            value=F(value);return Decimal(value.numerator)/Decimal(value.denominator)
        for y in range(height):
            for x in range(width):
                if not valid[y*width+x]:output.append(bits(0));continue
                total=Decimal(0);weight=Decimal(0)
                # Direct exhaustive 2D convolution: no separable native algorithm.
                for py in range(height):
                    for px in range(width):
                        dx,dy=px-x,py-y
                        if not valid[py*width+px] or abs(dx)>radii[0] or abs(dy)>radii[1]:continue
                        wx=(-dec(dx*dx)/(2*dec(sigmas[0])**2)).exp() if sigmas[0] else Decimal(int(dx==0))
                        wy=(-dec(dy*dy)/(2*dec(sigmas[1])**2)).exp() if sigmas[1] else Decimal(int(dy==0))
                        w=wx*wy;total+=dec(values[py*width+px])*w;weight+=w
                output.append(bits(total/weight))
    visits=0
    for length,other,radius in [(width,height,radii[0]),(height,width,radii[1])]:
        if radius:visits+=other*sum(min(length-1,i+radius)-max(0,i-radius)+1 for i in range(length))
    return dict(name=name,width=width,height=height,sx=str(sx),sy=str(sy),sigma=str(sigma),values=[str(v) for v in values],
                valid=valid,expected_bits=output,radii=radii,visits=visits,
                work_bytes=(33 if any(radii) else 9)*width*height+8*(2*sum(radii)+2)+4096)


def reference():
    distances=[]
    for width,height in [(1,1),(1,7),(7,1),(4,3),(7,6)]:
        for sx,sy in [('1','1'),('1/2','2'),('2','1/4')]:
            sites=[i for i in range(width*height) if i%7==1 or i%13==4]
            for outside in [False,True]:
                distances.append(distance_case(f'{width}x{height}-{sx}-{sy}-{outside}',width,height,sx,sy,sites,outside))
    distances.extend([distance_case('none',5,4,1,2,[]),distance_case('all',4,3,'1/2',2,list(range(12))),
                      distance_case('row-before-column-tie',3,3,1,1,[1,3,5,7]),
                      distance_case('anisotropic-owner',5,5,2,'1/2',[2,10]),
                      distance_case('extreme-x-metric-owner',3,8,F(2)**300,F(2)**-300,[i*3 for i in range(8)]),
                      distance_case('extreme-y-metric-owner',8,3,F(2)**-300,F(2)**300,list(range(8)))])
    gaussians=[gaussian_case('anisotropic',7,6,'1/2',2,'3/4'),gaussian_case('swapped-axes',6,7,2,'1/2','3/4'),
               gaussian_case('fractional-sigma',5,4,1,2,'5/4'),gaussian_case('zero',5,4,'1/2',2,0),
               gaussian_case('one-column',1,7,2,'1/2','3/4'),gaussian_case('one-row',7,1,'1/2',2,'3/4'),
               gaussian_case('constant-with-holes',5,4,'1/2',2,'3/4','constant'),
               gaussian_case('all-invalid',4,3,'1/2',2,'3/4','empty'),gaussian_case('all-valid',4,3,1,1,'3/4','all')]
    comparisons=[];seed=161
    for i in range(512):
        words=[]
        for _ in range(4):
            seed=(seed*6364136223846793005+1442695040888963407)%(2**64);words.append(seed)
        ae=(i*37)%2201-1100;be=(i*71)%2201-1100
        if i%2==0:be=ae+(words[0]*words[1]).bit_length()-(words[2]*words[3]).bit_length()
        a=F(words[0]*words[1])*F(2)**ae;b=F(words[2]*words[3])*F(2)**be
        comparisons.append(dict(words=words,ae=ae,be=be,order=(a>b)-(a<b)))
    for words,ae,be in [([0,1,1,1],0,0),([2**64-1]*4,0,0),([2**64-1,2**64-1,1,1],-128,0),([1,1,2,1],1,0),
                        ([2**32+1,2**32+1,2**32,2**32+2],0,0)]:
        a=F(words[0]*words[1])*F(2)**ae;b=F(words[2]*words[3])*F(2)**be
        comparisons.append(dict(words=words,ae=ae,be=be,order=(a>b)-(a<b)))
    return dict(schema='qpx.physical-grid-oracle/1',scope='Numerical kernel preparation; no detector or biological acceptance',
                generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),gaussian_absolute_tolerance=2e-12,
                gaussian_relative_tolerance=2e-12,distance=distances,gaussian=gaussians,wide_comparisons=comparisons)


def native_header(data):
    lines=['// Generated independent physical-grid expectations; do not edit.','#pragma once','#include <cstdint>','#include <vector>',
           'namespace physical_reference {','struct Distance {std::size_t width,height;double sx,sy;bool outside;std::vector<std::uint32_t> sites;std::vector<std::uint64_t> squared;std::vector<std::uint32_t> nearest;std::uint64_t bytes;};',
           'inline const Distance distances[]={']
    integers=lambda values:'{'+','.join(map(str,values))+'}'
    hexes=lambda values:'{'+','.join('0x'+v+'ULL' for v in values)+'}'
    number=lambda value:repr(float(F(value)))
    for v in data['distance']:
        lines.append('{'+','.join([str(v['width']),str(v['height']),number(v['sx']),number(v['sy']),str(v['outside']).lower(),
                                  integers(v['sites']),hexes(v['squared_bits']),integers(v['nearest']),str(v['work_bytes'])])+'},')
    lines+=['};','struct Gaussian {std::size_t width,height;double sx,sy,sigma;std::vector<double> values;std::vector<std::uint8_t> valid;std::vector<std::uint64_t> expected;std::uint64_t visits,bytes;};','inline const Gaussian gaussians[]={']
    for v in data['gaussian']:
        lines.append('{'+','.join([str(v['width']),str(v['height']),number(v['sx']),number(v['sy']),number(v['sigma']),
                                  '{'+','.join(number(t) for t in v['values'])+'}',integers(v['valid']),hexes(v['expected_bits']),str(v['visits']),str(v['work_bytes'])])+'},')
    lines+=['};','struct Comparison {std::uint64_t a,b,c,d;int ae,be,order;};','inline constexpr Comparison comparisons[]={']
    for v in data['wide_comparisons']:
        lines.append('{'+','.join([*['0x'+format(w,'016x')+'ULL' for w in v['words']],str(v['ae']),str(v['be']),str(v['order'])])+'},')
    return '\n'.join(lines+['};','}'])+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--native-header',type=Path,required=True);args=parser.parse_args()
    if args.output.exists() or args.native_header.exists():parser.error('capture outputs must not already exist')
    data=reference()
    with args.output.open('xb') as out:out.write((json.dumps(data,indent=2)+'\n').encode('utf-8'))
    with args.native_header.open('xb') as out:out.write(native_header(data).encode('utf-8'))


if __name__=='__main__':main()
