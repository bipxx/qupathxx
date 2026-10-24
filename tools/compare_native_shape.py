"""Verify frozen ImageJ shape oracles and compare native binary masks/maxima."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/imagej-shape-1.54p'


def read(path):return json.loads(path.read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,value):path.write_bytes((json.dumps(value,indent=2,allow_nan=False)+'\n').encode())


def decode(path,size):
    with gzip.open(path,'rb') as stream:raw=stream.read(size+1)
    if len(raw)!=size or any(v>1 for v in raw):raise ValueError('invalid binary shape canvas')
    return raw


def verify_capture(folder=FIXTURE/'capture'):
    manifest=read(folder/'manifest.json');contract=read(folder/'contract.json')
    upstream=read(ROOT/'tests/fixtures/qupath-0.7.0/contract.json')['upstream']
    if (manifest['schema']!='qpx.imagej-shape-capture/1' or manifest['upstream']!=upstream or
            manifest['generator_sha256']!=sha(ROOT/'tools/upstream/imagej_shape_reference.groovy') or
            manifest['input_generator_sha256']!=sha(ROOT/'tools/capture_imagej_shape.py') or
            contract['schema']!='qpx.imagej-shape-contract/1' or contract['imagej_version']!='1.54p' or
            contract['prominence']!=.5 or contract['edges_are_background'] is not False):
        raise ValueError('shape capture identity differs')
    names=[c['name'] for c in contract['cases']]
    if len(names)!=26 or len(set(names))!=26 or any(not re.fullmatch('[a-z0-9_]+',n) for n in names):
        raise ValueError('shape capture cohort differs')
    expected={'contract.json','reference.json','runtime.log'}|{n+'/'+part+'.u8.gz' for n in names for part in ['input','mask','maxima']}
    if set(manifest['files'])!=expected:raise ValueError('shape capture inventory differs')
    for name,identity in manifest['files'].items():
        path=folder/name
        if path.stat().st_size!=identity['bytes'] or sha(path)!=identity['sha256']:raise ValueError('shape capture bytes changed: '+name)
    reference=read(folder/'reference.json')
    if reference['imagej_version']!='1.54p' or {c['name'] for c in reference['cases']}!=set(names):
        raise ValueError('shape reference metadata differs')
    records={c['name']:c for c in reference['cases']}
    for case in contract['cases']:
        size=case['width']*case['height']
        if case['width']<=0 or case['height']<=0 or size>1024*1024:raise ValueError('shape dimensions exceed protocol')
        raw={part:decode(folder/case['name']/(part+'.u8.gz'),size) for part in ['input','mask','maxima']}
        if any(v and not u for u,v in zip(raw['input'],raw['mask'])):raise ValueError('shape reference grew foreground')
        if (sum(raw['mask'])!=records[case['name']]['foreground'] or sum(raw['maxima'])!=records[case['name']]['maxima']):
            raise ValueError('shape count ledger differs')
    return contract


def materialize_native(folder,target):
    contract=verify_capture(folder);target.mkdir(parents=True,exist_ok=False)
    lines=[]
    for case in contract['cases']:
        name=case['name'];size=case['width']*case['height'];lines.append(f"{name} {case['width']} {case['height']}\n")
        for part in ['input','mask','maxima']:(target/(name+'.'+part+'.u8')).write_bytes(decode(folder/name/(part+'.u8.gz'),size))
    (target/'index.txt').write_bytes(''.join(lines).encode())
    write(target/'manifest.json',dict(schema='qpx.shape-native-fixtures/1',capture_manifest_sha256=sha(folder/'manifest.json'),
        converter_sha256=sha(Path(__file__)),files={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(target.iterdir())}))


def verify_native(folder=FIXTURE/'capture',target=FIXTURE/'native'):
    contract=verify_capture(folder);manifest=read(target/'manifest.json')
    expected={'index.txt'}|{c['name']+'.'+part+'.u8' for c in contract['cases'] for part in ['input','mask','maxima']}
    if (manifest['schema']!='qpx.shape-native-fixtures/1' or manifest['capture_manifest_sha256']!=sha(folder/'manifest.json') or set(manifest['files'])!=expected):
        raise ValueError('native shape fixture identity differs')
    for name,identity in manifest['files'].items():
        path=target/name
        if path.stat().st_size!=identity['bytes'] or sha(path)!=identity['sha256']:raise ValueError('native shape fixture bytes changed')
    for case in contract['cases']:
        for part in ['input','mask','maxima']:
            if (target/(case['name']+'.'+part+'.u8')).read_bytes()!=decode(folder/case['name']/(part+'.u8.gz'),case['width']*case['height']):
                raise ValueError('native shape fixture differs from captured data')
    expected_index=''.join(f"{c['name']} {c['width']} {c['height']}\n" for c in contract['cases']).encode()
    if (target/'index.txt').read_bytes()!=expected_index:raise ValueError('native shape index differs')
    return contract


def compare(executable,folder,directory):
    import numpy as np
    contract=verify_capture(folder);directory.mkdir(parents=True,exist_ok=False)
    environment=json.loads(subprocess.check_output([str(executable),'--environment']))
    if '+dirty' in environment['git_revision']:raise ValueError('native source was not committed')
    records=[]
    for case in contract['cases']:
        name=case['name'];size=case['width']*case['height'];input=directory/(name+'.qlr')
        pixels=np.frombuffer(decode(folder/name/'input.u8.gz',size),dtype=np.uint8)
        input.write_bytes(b'QLR1'+struct.pack('<III',case['width'],case['height'],1)+pixels.astype('<u4').tobytes())
        record=dict(name=name,input_sha256=sha(input),outputs={})
        for part,option in [('mask','--shape-split'),('maxima','--shape-maxima')]:
            output=directory/(name+'.'+part+'.qlr')
            subprocess.run([str(executable),option,str(input),str(output)],check=True,timeout=120)
            raw=output.read_bytes()
            if len(raw)!=16+size*4 or raw[:4]!=b'QLR1' or struct.unpack_from('<II',raw,4)!=(case['width'],case['height']):
                raise ValueError('native shape output protocol differs')
            native=np.frombuffer(raw,'<u4',offset=16)
            expected=np.frombuffer(decode(folder/name/(part+'.u8.gz'),size),dtype=np.uint8)
            disagreements=int(np.count_nonzero(native!=expected))
            record['outputs'][part]=dict(disagreements=disagreements,exact=disagreements==0,output_sha256=sha(output))
        records.append(record)
    return dict(schema='qpx.native-shape-conformance/1',native_environment=environment,native_sha256=sha(executable),
        tool_sha256=sha(Path(__file__)),capture_manifest_sha256=sha(folder/'manifest.json'),cases=records,
        shape_stage_conformance_accepted=all(all(v['exact'] for v in c['outputs'].values()) for c in records),
        production_integrated=False,segmentation_parity_accepted=False,biological_validation=False,performance_accepted=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,default=FIXTURE/'capture')
    parser.add_argument('--materialize-native',type=Path);parser.add_argument('--executable',type=Path)
    parser.add_argument('--output-directory',type=Path);parser.add_argument('--report',type=Path);args=parser.parse_args()
    if args.materialize_native:materialize_native(args.capture,args.materialize_native);return 0
    if not args.executable or not args.output_directory or not args.report:parser.error('supply executable, output-directory and report')
    result=compare(args.executable,args.capture,args.output_directory);write(args.report,result)
    print(json.dumps(dict(cases=len(result['cases']),accepted=result['shape_stage_conformance_accepted'])))
    return 0 if result['shape_stage_conformance_accepted'] else 1


if __name__=='__main__':raise SystemExit(main())
