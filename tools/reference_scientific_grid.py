"""Independent Fraction oracle for proposed scientific grid geometry and RGB."""
import argparse
from fractions import Fraction as F
import hashlib
import json
import math
import struct
from pathlib import Path


def rational(value):
    value=F(value)
    return f'{value.numerator}/{value.denominator}'


def axis_cell(origin,extent,samples,index):
    left=F(origin)+F(index*extent,samples)
    right=F(origin)+F((index+1)*extent,samples)
    center=(left+right)/2
    weights=[(p,min(right,p+1)-max(left,p)) for p in range(math.floor(left),math.ceil(right))]
    assert all(w>0 for _,w in weights) and sum(w for _,w in weights)==F(extent,samples)
    return dict(index=index,left=rational(left),right=rational(right),center=rational(center),
                center_binary64=float(center).hex(),center_ieee754=struct.pack('>d',float(center)).hex(),first=math.floor(left),stop=math.ceil(right),
                weights=[[p,rational(w)] for p,w in weights])


def axis(origin,extent,samples,context):
    return [axis_cell(origin,extent,samples,i) for i in range(-context,samples+context)]


def rgb(x,y):
    return [(17*x+3*y)%256,(5*x+19*y)%256,(11*x+7*y)%256]


def make_case(name,image,extent,samples,context=(0,0),calibration=(None,None),roi=None,alpha=None):
    x,y,w,h=extent;nx,ny=samples;cx,cy=context
    assert all(isinstance(v,int) and not isinstance(v,bool) for v in [*image,*extent,*samples,*context])
    assert w>0 and h>0 and nx>0 and ny>0 and cx>=0 and cy>=0
    xs=axis(x,w,nx,cx);ys=axis(y,h,ny,cy)
    left,right=xs[0]['first'],xs[-1]['stop'];top,bottom=ys[0]['first'],ys[-1]['stop']
    clip_left=min(image[0],max(0,left));clip_top=min(image[1],max(0,top))
    clip_right=min(image[0],max(0,right));clip_bottom=min(image[1],max(0,bottom))
    roi=list(map(F,roi or extent));alpha=alpha or {}
    pixels=[]
    for ay in ys:
        for ax in xs:
            weights=[(px,py,F(wx)*F(wy)) for px,wx in ax['weights'] for py,wy in ay['weights']]
            valid=all(0<=px<image[0] and 0<=py<image[1] and alpha.get(f'{px},{py}',255)==255 for px,py,_ in weights)
            total=F(w,nx)*F(h,ny)
            mean=[sum(weight*rgb(px,py)[c] for px,py,weight in weights)/total for c in range(3)] if valid else None
            center_x,center_y=F(ax['center']),F(ay['center'])
            core=0<=ax['index']<nx and 0<=ay['index']<ny
            inside=roi[0]<=center_x<roi[0]+roi[2] and roi[1]<=center_y<roi[1]+roi[3]
            pixels.append(dict(index=[ax['index'],ay['index']],valid_support=valid,
                               eligible=bool(valid and core and inside),rgb=[rational(v) for v in mean] if mean else None))
    physical=[]
    for scale,cal in zip([F(w,nx),F(h,ny)],calibration):
        if cal is not None and F(cal)<=0:raise ValueError('positive calibration required')
        physical.append(rational(scale*F(cal)) if cal is not None else None)
    return dict(name=name,image=list(image),extent=list(extent),samples=list(samples),context=list(context),
                calibration=list(calibration),roi=[rational(v) for v in roi],alpha=alpha,
                spacing=[rational(F(w,nx)),rational(F(h,ny))],effective_spacing_um=physical,
                physical_parameters_admissible=all(v is not None for v in physical),x=xs,y=ys,
                read_bounds=[left,top,right-left,bottom-top],
                clipped_read_bounds=[clip_left,clip_top,clip_right-clip_left,clip_bottom-clip_top],
                overlap_visits=sum(len(a['weights']) for a in xs)*sum(len(a['weights']) for a in ys),
                pixels=pixels)


def reference():
    cases=[make_case('identity',(16,16),(2,3,3,2),(3,2)),
           make_case('fractional-reduction',(16,16),(1,2,7,5),(3,2)),
           make_case('enlargement',(16,16),(2,1,2,3),(5,4)),
           make_case('anisotropic-ratios',(16,16),(1,1,9,2),(3,5)),
           make_case('context-phase',(20,20),(3,4,7,5),(3,2),(1,2)),
           make_case('image-edge-context',(5,5),(0,0,3,3),(2,2),(2,1)),
           make_case('half-open-roi',(8,8),(0,0,6,4),(4,4),roi=('3/4','1/2',3,3)),
           make_case('partial-alpha',(8,8),(1,1,3,2),(2,2),alpha={'2,1':128}),
           make_case('transparent-alpha',(8,8),(1,1,3,2),(2,2),alpha={'2,1':0}),
           make_case('anisotropic-calibration',(16,16),(1,2,7,5),(3,2),calibration=('1/2','1/4')),
           make_case('isotropic-physical-spacing',(16,16),(1,2,6,4),(3,1),calibration=('1/2','1/4')),
           make_case('one-unknown-axis',(16,16),(1,2,7,5),(3,2),calibration=(None,'1/4')),
           make_case('large-origin',(10**12,16),(10**12-11,2,7,5),(3,2),(1,0)),
           make_case('no-source-support',(4,4),(10,10,2,2),(1,1))]
    probes=[]
    # Counterexamples to converting a large product to binary64 before division.
    for origin in [10**12-2000017,10**12-2000001,10**12-2000000]:
        for extent in [1,7,1000007]:
            for samples in [999983,1000000]:
                for index in [0,1,samples//2,samples-1]:
                    exact=F(origin)+F((2*index+1)*extent,2*samples)
                    naive=float(origin*2*samples+(2*index+1)*extent)/(2*samples)
                    if float(exact)!=naive:
                        probes.append(dict(origin=origin,extent=extent,samples=samples,index=index,
                                           center=rational(exact),center_binary64=float(exact).hex(),
                                           center_ieee754=struct.pack('>d',float(exact)).hex(),
                                           incorrect_product_first_binary64=naive.hex()))
    assert probes
    conversions=[F(n,d) for n,d in [(0,1),(-1,3),(1,3),(-1,1999966),(1,1999966),
                 (2**53+1,1),(2**53+3,1),(-(2**53+1),1),(4100000000000000000,3)]]
    seed=159
    for index in range(512):
        seed=(6364136223846793005*seed+1442695040888963407)%(2**64)
        numerator=seed%4100000000000000001
        if index%2:numerator=-numerator
        conversions.append(F(numerator,1+seed%2000000))
    return dict(schema='qpx.scientific-grid-oracle/1',scope='Independent exact arithmetic; no native detector or biological acceptance',
                generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=cases,rounding_probes=probes[:8],
                fraction_probes=[dict(n=v.numerator,d=v.denominator,bits=struct.pack('>d',float(v)).hex()) for v in conversions])


def native_header(data):
    lines=['// Generated only by tools/reference_scientific_grid.py; exact independent Fraction expectations.',
           '#pragma once','#include <cstdint>','#include <array>','namespace grid_reference {',
           'struct Axis {std::int64_t origin,extent,samples,context,index,ln,ld,rn,rd,cn,cd;std::uint64_t bits;};',
           'inline constexpr Axis axes[]={']
    for case in data['cases']:
        for axis_name,origin,extent,samples,context in zip(['x','y'],case['extent'][:2],case['extent'][2:],case['samples'],case['context']):
            for cell in case[axis_name]:
                fractions=[F(cell[key]) for key in ['left','right','center']]
                values=[origin,extent,samples,context,cell['index'],*[v for f in fractions for v in [f.numerator,f.denominator]]]
                lines.append('{'+','.join(map(str,values))+',0x'+cell['center_ieee754']+'ULL},')
    lines+=['};','inline constexpr Axis probes[]={']
    for cell in data['rounding_probes']:
        value=axis_cell(cell['origin'],cell['extent'],cell['samples'],cell['index'])
        fractions=[F(value[key]) for key in ['left','right','center']]
        values=[cell['origin'],cell['extent'],cell['samples'],0,cell['index'],*[v for f in fractions for v in [f.numerator,f.denominator]]]
        lines.append('{'+','.join(map(str,values))+',0x'+cell['center_ieee754']+'ULL},')
    lines+=['};','struct Weight {std::int64_t origin,extent,samples,context,index,pixel,n,d;};','inline constexpr Weight weights[]={']
    for case in data['cases']:
        for axis_name,origin,extent,samples,context in zip(['x','y'],case['extent'][:2],case['extent'][2:],case['samples'],case['context']):
            for cell in case[axis_name]:
                for pixel,weight in cell['weights']:
                    f=F(weight);lines.append('{'+','.join(map(str,[origin,extent,samples,context,cell['index'],pixel,f.numerator,f.denominator]))+'},')
    lines+=['};','struct Plan {std::int64_t iw,ih,x,y,w,h,nx,ny,cx,cy;std::array<std::int64_t,4> read,clipped;std::uint64_t visits,bytes;};','inline constexpr Plan plans[]={']
    for case in data['cases']:
        rect=case['clipped_read_bounds'];count=(case['samples'][0]+2*case['context'][0])*(case['samples'][1]+2*case['context'][1])
        values=[*case['image'],*case['extent'],*case['samples'],*case['context']]
        lines.append('{'+','.join(map(str,values))+',{'+','.join(map(str,case['read_bounds']))+'},{'+','.join(map(str,rect))+'},'+str(case['overlap_visits'])+','+str(4*rect[2]*rect[3]+25*count)+'},')
    lines+=['};','struct Fraction {std::int64_t n,d;std::uint64_t bits;};','inline constexpr Fraction fractions[]={']
    for item in data['fraction_probes']:
        lines.append('{'+str(item['n'])+','+str(item['d'])+',0x'+item['bits']+'ULL},')
    lines+=['};','struct Alpha {std::size_t case_index;std::int64_t x,y;std::uint8_t alpha;};','inline constexpr Alpha alpha[]={']
    for ci,case in enumerate(data['cases']):
        for key,value in case['alpha'].items():
            lines.append('{'+','.join(map(str,[ci,*map(int,key.split(',')),value]))+'},')
    lines+=['};','struct Pixel {std::size_t case_index,index;bool valid;std::array<std::uint64_t,3> bits;};','inline constexpr Pixel pixels[]={']
    for ci,case in enumerate(data['cases']):
        for index,pixel in enumerate(case['pixels']):
            channels=pixel['rgb'] or ['0/1']*3
            values=['0x'+struct.pack('>d',float(F(v))).hex()+'ULL' for v in channels]
            lines.append('{'+str(ci)+','+str(index)+','+str(pixel['valid_support']).lower()+',{'+','.join(values)+'}},')
    return '\n'.join(lines+['};','}'])+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--native-header',type=Path);args=parser.parse_args()
    reference_data=reference();data=(json.dumps(reference_data,indent=2)+'\n').encode('utf-8')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('xb') as stream:stream.write(data)
    if args.native_header:
        with args.native_header.open('xb') as stream:stream.write(native_header(reference_data).encode('utf-8'))
    print('Wrote independent scientific grid oracle:',args.output)


if __name__=='__main__':main()
