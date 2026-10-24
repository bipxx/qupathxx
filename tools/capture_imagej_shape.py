"""Generate a fresh, byte-addressed ImageJ shape-stage oracle corpus."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    import numpy as np
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-directory',type=Path,required=True)
    parser.add_argument('--runtime-cache',type=Path,required=True)
    parser.add_argument('--ndpi-zero-mask',type=Path)
    parser.add_argument('--ndpi-context-mask',type=Path)
    args=parser.parse_args();folder=args.output_directory.resolve();folder.mkdir(parents=True,exist_ok=False)
    masks=[]
    def add(name,mask,**origin):masks.append((name,np.asarray(mask,dtype=np.uint8),origin))
    add('blank',np.zeros((7,9)));add('full',np.ones((7,9)))
    a=np.zeros((7,9));a[3,4]=1;add('single',a)
    add('row',[[0,1,1,0,1,0,1,1,1,0]]);add('column',np.array([[0,1,1,0,1,0,1,1,1,0]]).T)
    y,x=np.mgrid[:64,:72]
    add('plateau',(x>=18)&(x<42)&(y>=20)&(y<44))
    add('touching_disks',((x-27)**2+(y-32)**2<=14**2)|((x-45)**2+(y-32)**2<=14**2))
    add('thin_neck',(((x-20)**2+(y-32)**2<=10**2)|((x-50)**2+(y-32)**2<=10**2)|((abs(y-32)<=1)&(x>=20)&(x<=50))))
    add('border',((x-0)**2+(y-20)**2<=16**2)|((x-23)**2+(y-20)**2<=15**2))
    add('diagonal',(x+y)%2==0)
    add('ring',(((x-36)**2+(y-32)**2<=20**2)&((x-36)**2+(y-32)**2>=10**2)))
    for seed in range(12):
        a=np.random.default_rng(seed).integers(0,2,(48,64),dtype=np.uint8)
        for _ in range(2):a=(a+np.roll(a,1,0)+np.roll(a,-1,0)+np.roll(a,1,1)+np.roll(a,-1,1)>=3).astype(np.uint8)
        add('random_'+str(seed),a)
    y,x=np.mgrid[:144,:192]
    add('large_disks',((x-55)**2+(y-72)**2<=45**2)|((x-129)**2+(y-72)**2<=45**2))
    for name,path in [('ndpi_context0',args.ndpi_zero_mask),('ndpi_context32',args.ndpi_context_mask)]:
        if path:
            raw=path.read_bytes();width,height=struct.unpack_from('<II',raw,4)
            if raw[:4]!=b'QLR1' or width*height>1024*1024 or len(raw)!=16+width*height*4:raise ValueError('invalid native region protocol')
            add(name,np.frombuffer(raw,'<u4',offset=16).reshape(height,width)!=0,
                native_region_sha256=sha(path),native_source='655c8541d0632adb1ae9662ffc80fbff16aa7e8f',
                slide_sha256='edf4a1ccf395c7000ae93ad3b44c07d97043810e00be0c1d167dd09bbe436e46')
    cases=[]
    for name,mask,origin in masks:
        target=folder/name;target.mkdir();(target/'input.u8.gz').write_bytes(gzip.compress(mask.tobytes(),mtime=0))
        cases.append(dict(name=name,width=mask.shape[1],height=mask.shape[0],origin=origin))
    contract=dict(schema='qpx.imagej-shape-contract/1',imagej_version='1.54p',prominence=.5,
        edges_are_background=False,upstream_contract='../qupath-0.7.0/contract.json',cases=cases,
        scope='Shape-stage conformance only. Synthetic topology cases and native real-slide masks. No complete detector, biology, platform or performance acceptance.')
    (folder/'contract.json').write_bytes((json.dumps(contract,indent=2)+'\n').encode())
    spec=importlib.util.spec_from_file_location('qpx_shape_runtime',ROOT/'tools/fetch_qupath.py')
    runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime);executable=runtime.fetch(args.runtime_cache)
    script=ROOT/'tools/upstream/imagej_shape_reference.groovy'
    result=subprocess.run([str(executable),'script','--args',str(folder),str(script)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
    (folder/'runtime.log').write_bytes(result.stdout);result.check_returncode()
    files={p.relative_to(folder).as_posix():dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.rglob('*')) if p.is_file()}
    upstream=json.loads((ROOT/'tests/fixtures/qupath-0.7.0/contract.json').read_text())['upstream']
    manifest=dict(schema='qpx.imagej-shape-capture/1',upstream=upstream,generator_sha256=sha(script),
                  input_generator_sha256=sha(Path(__file__)),files=files)
    (folder/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
    print(json.dumps(dict(cases=len(cases),capture=str(folder))))


if __name__=='__main__':main()
