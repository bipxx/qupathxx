"""Verify independent public response-basin captures and derive native vectors."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-response-basins-0.7.0'
PROBE=ROOT/'tools/upstream/qupath_response_basins_probe.groovy'
HEADER=ROOT/'cpp/tests/qupath_response_reference.hpp'


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'))


def inputs():
    import numpy as np
    patterns={}
    for name,shape in [('single',(1,1)),('row',(1,7)),('column',(7,1)),('flat',(7,9))]:patterns[name]=np.ones(shape,dtype='<f4')
    a=np.zeros((7,9),dtype='<f4');a[2:5,3:6]=1;patterns['interior-plateau']=a.copy();a[0:3,3]=1;patterns['border-plateau']=a.copy()
    a=np.zeros((7,9),dtype='<f4');a[2,2]=a[4,6]=1;a[1:6,1:8]+=np.float32(.2);patterns['fifo-peaks']=a
    a=np.zeros((7,9),dtype='<f4');a[2:5,3:6]=np.float32(.001);patterns['minimum-peak']=a.copy();a*=np.float32(.999);patterns['below-minimum']=a
    for seed in range(24):
        rng=np.random.default_rng(304000+seed);shape=(7+seed%3,9+seed%4)
        patterns['noise-'+str(seed)]=(rng.integers(-2,5,size=shape)/8).astype('<f4')
    return patterns


def cases():
    return [dict(name=name,width=a.shape[1],height=a.shape[0],input_sha256=hashlib.sha256(a.tobytes()).hexdigest()) for name,a in inputs().items()]


def identity():
    return dict(schema='qpx.response-basins-reference/1',
        upstream=read(ROOT/'tests/fixtures/qupath-0.7.0/contract.json')['upstream'],
        probe_sha256=sha(PROBE),cases=cases())


def payloads():
    return {'cases.json','probe.groovy','driver.log','runtime.json',
        *(c['name']+suffix for c in cases() for suffix in ('-input.f32le','-seeds.u32le','-labels.u32le'))}


def verify(folder=FIXTURE):
    import numpy as np
    folder=Path(folder);manifest=read(folder/'manifest.json');expected=payloads()
    if {k:manifest.get(k) for k in identity()}!=identity() or set(manifest.get('files',{}))!=expected:
        raise ValueError('response capture identity differs')
    if {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}!=expected|{'manifest.json'}:
        raise ValueError('response capture members differ')
    for name,item in manifest['files'].items():
        path=folder/name
        if path.stat().st_size!=item['bytes'] or sha(path)!=item['sha256']:raise ValueError('response capture changed: '+name)
    if read(folder/'cases.json')!=cases() or (folder/'probe.groovy').read_bytes()!=PROBE.read_bytes():raise ValueError('response recipe changed')
    runtime=read(folder/'runtime.json')
    wanted=[dict(name=c['name'],outside_is_nan=True,outside_compares_nonzero=True) for c in cases()]
    if runtime!=dict(qupath_version='0.7.0',java_version='25.0.2',cases=wanted):raise ValueError('response runtime/boundary semantics differ')
    for name,array in inputs().items():
        if (folder/(name+'-input.f32le')).read_bytes()!=array.tobytes():raise ValueError('response input differs')
        seeds=np.frombuffer((folder/(name+'-seeds.u32le')).read_bytes(),dtype='<u4')
        labels=np.frombuffer((folder/(name+'-labels.u32le')).read_bytes(),dtype='<u4')
        if seeds.size!=array.size or labels.size!=array.size or np.any(labels>array.size) or np.any(seeds>array.size):raise ValueError('response label dimensions/IDs differ')
        if not np.array_equal(labels[seeds>0],seeds[seeds>0]):raise ValueError('response flood changed a seed')
    return dict(cases=len(cases()),capture_manifest_sha256=sha(folder/'manifest.json'),public_boundary_semantics_verified=True)


def seal(folder):
    folder=Path(folder);manifest=identity()
    manifest['files']={name:dict(bytes=(folder/name).stat().st_size,sha256=sha(folder/name)) for name in sorted(payloads())}
    (folder/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
    return verify(folder)


def capture(folder,runtime_cache):
    if os.name!='nt':raise ValueError('pinned runtime capture requires Windows')
    folder=Path(folder).resolve();folder.mkdir(exist_ok=False,parents=True)
    for name,array in inputs().items():(folder/(name+'-input.f32le')).write_bytes(array.tobytes())
    (folder/'cases.json').write_bytes((json.dumps(cases(),indent=2)+'\n').encode());shutil.copyfile(PROBE,folder/'probe.groovy')
    spec=importlib.util.spec_from_file_location('response_runtime',ROOT/'tools/fetch_qupath.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    runtime=module.fetch(runtime_cache)
    with (folder/'driver.log').open('xb') as stream:
        subprocess.run([str(runtime),'script','--args',str(folder),str(folder/'probe.groovy')],stdout=stream,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW).check_returncode()
    return seal(folder)


def header(folder=FIXTURE):
    import numpy as np
    verify(folder);folder=Path(folder)
    lines=['// Generated by tools/reference_qupath_response.py; no segmentation model used.',
           '#pragma once','#include <cstddef>','#include <cstdint>','namespace qpx_test {',
           'struct ResponseBasinCase {const char* name;std::size_t width,height;const std::uint32_t* input;const std::uint32_t* labels;};']
    for index,case in enumerate(cases()):
        for label,suffix in [('input','-input.f32le'),('labels','-labels.u32le')]:
            values=np.frombuffer((folder/(case['name']+suffix)).read_bytes(),dtype='<u4')
            lines.append('inline constexpr std::uint32_t response_'+str(index)+'_'+label+'[]={'+','.join('0x'+format(int(v),'08x')+'u' for v in values)+'};')
    lines.append('inline constexpr ResponseBasinCase response_basin_cases[]={')
    for index,c in enumerate(cases()):lines.append('{"'+c['name']+'",'+str(c['width'])+','+str(c['height'])+',response_'+str(index)+'_input,response_'+str(index)+'_labels},')
    lines+=['};','}']
    return ('\n'.join(lines)+'\n').encode()


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path);parser.add_argument('--check',action='store_true')
    parser.add_argument('--runtime-cache',type=Path,default=ROOT/'build/upstream/qupath');args=parser.parse_args()
    if args.capture:print(capture(args.capture,args.runtime_cache));return
    text=header()
    if args.check:
        if HEADER.read_bytes()!=text:raise ValueError('response header differs from public capture')
    else:HEADER.write_bytes(text)
    print('33 public response-basin golden vectors verified')


if __name__=='__main__':main()
