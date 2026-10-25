"""Independent Fraction reduction of the typed sampling reference, never native output."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import struct
import importlib.util
_spec=importlib.util.spec_from_file_location('raw_sampling_stats_dependency',Path(__file__).with_name('reference_raw_sampling.py'))
reference_raw_sampling=importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(reference_raw_sampling)

def bits(value):return struct.pack('>d',value).hex()
def value(raw):return struct.unpack('>d',bytes.fromhex(raw))[0]
def reference():
    result=[]
    for index,c in enumerate(reference_raw_sampling.reference()['cases']):
        nx,ny=c['samples'];cx,cy=c['context'];channels=c['selection'][1];width=nx+2*cx
        for roi in (False,True):
            selected=[]
            for y in range(ny):
                for x in range(nx):
                    wx=Fraction(c['origin'][0])+Fraction((2*x+1)*c['extent'][0],2*nx)
                    wy=Fraction(c['origin'][1])+Fraction((2*y+1)*c['extent'][1],2*ny)
                    if roi and not (1<=wx<4 and 1<=wy<3):continue
                    selected.append(((y+cy)*width+x+cx)*channels)
            valid=sum(c['reasons'][at] in (0,3) for at in selected)
            stats=[]
            for channel in range(channels):
                finite=[value(c['expected_binary64'][at+channel]) for at in selected if c['reasons'][at+channel]==0]
                nonfinite=sum(c['reasons'][at+channel]==3 for at in selected)
                mean=float(sum((Fraction.from_float(v) for v in finite),Fraction())/len(finite)) if finite else None
                stats.append(dict(finite=len(finite),nonfinite=nonfinite,mean=bits(mean) if finite else None,
                                  minimum=bits(min(finite)) if finite else None,maximum=bits(max(finite)) if finite else None))
            result.append(dict(source_case=index,roi=roi,pixels=len(selected),valid_pixels=valid,channels=stats))
    return dict(schema='qpx.raw-grid-statistics-reference/1',semantics='Exact final mean of rounded binary64 area samples; core-center ROI coverage; missing support is neither finite nor nonfinite',cases=result)
def native_header(data):
    lines=['#pragma once','#include <cstdint>','namespace raw_grid_statistics_reference {',
           'struct Channel {unsigned finite,nonfinite;bool present;std::uint64_t mean,minimum,maximum;};',
           'struct Case {unsigned source;bool roi;unsigned pixels,valid,offset;};','inline constexpr Channel channels[]={']
    for c in data['cases']:
        for s in c['channels']:
            lines.append('{'+str(s['finite'])+','+str(s['nonfinite'])+','+str(s['mean'] is not None).lower()+','+','.join('0x'+(s[k] or '0000000000000000')+'ULL' for k in ('mean','minimum','maximum'))+'},')
    lines+=['};','inline constexpr Case cases[]={'];offset=0
    for c in data['cases']:
        lines.append('{'+str(c['source_case'])+','+str(c['roi']).lower()+','+str(c['pixels'])+','+str(c['valid_pixels'])+','+str(offset)+'},');offset+=len(c['channels'])
    return '\n'.join(lines+['};','}',''])
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--native-header',type=Path,required=True);a=p.parse_args()
    if a.output.exists() or a.native_header.exists():p.error('reference outputs must be new paths')
    data=reference()
    for path in [a.output,a.native_header]:path.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes((json.dumps(data,indent=2)+'\n').encode('utf-8'));a.native_header.write_bytes(native_header(data).encode('utf-8'))
    print(len(data['cases']))
if __name__=='__main__':main()
