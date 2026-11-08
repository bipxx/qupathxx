"""Independent struct/hashlib QRA1 and QRA2 synthetic byte references."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

def load(name):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(name+'.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
sampling=load('reference_raw_sampling')
statistics=load('reference_raw_grid_statistics')
def integer(value):return struct.pack('<Q',value & ((1<<64)-1))
def real(value):return struct.pack('<d',value)
def text(value):
    data=value.encode('utf-8');return integer(len(data))+data
def optional(value):return integer(value is not None)+(real(value) if value is not None else b'')
def reference():
    sample=sampling.reference();case=sample['cases'][0];stats=statistics.reference()['cases'][0]
    raw=bytes(int(v,16) for v in sample['banks']['u8'][:60])
    source_text=['raw-analysis-synthetic','tiff:synthetic','/raw-analysis-synthetic.tif','single_file','/raw-analysis-synthetic.tif','posix.device-inode/1','1:2','posix.seconds-nanoseconds/1','1:0','2:0']
    natural=text('qpx.raw-roi-input/1')+b''.join(integer(v) for v in [5,4,3,2,2,1,0,0,0,5,4,0,0,3,0,0,16])
    natural+=b''.join(optional(v) for v in [1.,.5,.25,1.5,2.])+b''.join(text(v) for v in ['c0','c1','c2'])+integer(0)
    natural+=b''.join(integer(v) for v in [0,0,5,4])+bytes([1]*20)+raw
    natural_hash=hashlib.sha256(natural).hexdigest()
    grid_hash=hashlib.sha256(text('qpx.raw-grid-statistics-input/1')+text(case['input_sha256'])+optional(.5)+optional(.25)+integer(0)).hexdigest()
    result=[]
    for grid in (False,True):
        data=b'QRA'+bytes([2 if grid else 1])
        if grid:
            data+=b''.join(integer(v) for v in [5,4,0,0,5,4,5,4,0,0])+integer(256*1024*1024)
            data+=text('raw-reference')+text(case['decoded_sha256'])+text(case['input_sha256'])
            data+=b''.join(integer(v) for v in [5,4,3,2,2,1,2,0])+text('XYZCT')+integer(1)+integer(5)+integer(4)+real(1.)
            data+=integer(3)+b''.join(text(v) for v in ['c0','c1','c2'])+b''.join(optional(v) for v in [.5,.25,1.5,2.])
        data+=b''.join(integer(v) for v in [1,0 if grid else 16,1000000000,0,0,0,0,0 if grid else 16,1])
        data+=text('')+text('')+integer(0)+integer(0)+integer(0)+real(0.)
        data+=text('qpx.raw-grid-statistics/1' if grid else 'qpx.raw-roi-statistics/1')+text(grid_hash if grid else natural_hash)+optional(.5)+optional(.25)
        data+=b''.join(integer(v) for v in [0,0,1 if grid else 5,1 if grid else 4,0,0,3,0,0,5,4,5,4,3,2,2,1,0])
        data+=(integer(0x7ff8000000000000) if grid else real(1.))+optional(1.5)+optional(2.)
        pixels_offset=len(data)
        data+=integer(stats['pixels'])+integer(stats['valid_pixels'])
        for i,c in enumerate(stats['channels']):
            data+=text('c'+str(i))+integer(c['finite'])+integer(c['nonfinite'])
            for key in ['mean','minimum','maximum']:data+=integer(c[key] is not None)+(bytes.fromhex(c[key])[::-1] if c[key] is not None else b'')
        data+=integer(0)+b''.join(text(v) for v in source_text)+integer(100)+integer(0)
        data+=hashlib.sha256(data).hexdigest().encode('ascii')
        result.append(dict(name='grid' if grid else 'legacy',pixels_offset=pixels_offset,bytes_hex=data.hex(),sha256=hashlib.sha256(data).hexdigest(),input_sha256=grid_hash if grid else natural_hash))
    return dict(schema='qpx.raw-grid-archive-reference/1',scope='Synthetic byte contracts; not observed files or scientific acceptance',cases=result)
def native_header(data):
    lines=['#pragma once','namespace raw_grid_archive_reference {']
    for case in data['cases']:
        raw=bytes.fromhex(case['bytes_hex']);lines.append('inline constexpr unsigned char '+case['name']+'[]={')
        for at in range(0,len(raw),24):lines.append(','.join(str(v) for v in raw[at:at+24])+',')
        lines.append('};')
        lines.append('inline constexpr unsigned long long '+case['name']+'_pixels_offset='+str(case['pixels_offset'])+';')
    return '\n'.join(lines+['}',''])
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--native-header',type=Path,required=True);a=p.parse_args()
    if a.output.exists() or a.native_header.exists():p.error('reference outputs must be new paths')
    data=reference()
    for path in [a.output,a.native_header]:path.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes((json.dumps(data,indent=2)+'\n').encode('utf-8'));a.native_header.write_bytes(native_header(data).encode('utf-8'))
if __name__=='__main__':main()
