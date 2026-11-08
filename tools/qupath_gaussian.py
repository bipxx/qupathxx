"""Capture/verify ImageJ Gaussian stages without changing native cell recipes."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-gaussian-1.54p'
GENERATOR=ROOT/'tools/upstream/qupath_gaussian_probe.groovy'


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'))


def inputs():
    import numpy as np
    from PIL import Image
    debug=load('gaussian_roi_debug',ROOT/'tools/qupath_roi_cell_debug.py')
    debug.verify()
    with Image.open(debug.FIXTURE/'debug-stack.tif') as image:
        image.seek(0);score=np.asarray(image).copy()
    values={'roi-sigma1':(score,1.)}
    patterns={'constant':np.full((9,11),.75),
        'edge-impulse':np.pad(np.array([[1.]]),((0,8),(0,10))),
        'center-impulse':np.pad(np.array([[1.]]),((4,4),(5,5))),
        'ramp':np.fromfunction(lambda y,x:((37*x+71*y)%53)/53,(17,19)),
        'vertical':np.arange(9).reshape(9,1)/8,
        'horizontal':np.arange(9).reshape(1,9)/8}
    for name,array in patterns.items():
        for sigma in (.25,1.,2.,8.5,8.500001,16.):values[name+'-'+str(sigma)]=(array,sigma)
    cases=[];payload={}
    for name,(array,sigma) in values.items():
        array=np.asarray(array,dtype='<f4');raw=array.tobytes()
        cases.append(dict(name=name,width=array.shape[1],height=array.shape[0],sigma=sigma,
                          input_sha256=hashlib.sha256(raw).hexdigest()))
        payload[name+'-input.f32le']=raw
    return cases,payload


def identity():
    return dict(schema='qpx.qupath-gaussian-capture/1',
        upstream=read(ROOT/'tests/fixtures/qupath-0.7.0/contract.json')['upstream'],
        generator_sha256=sha(GENERATOR),
        roi_debug_manifest_sha256=sha(ROOT/'tests/fixtures/qupath-cells-roi-debug-0.7.0/manifest.json'),
        requested_kernel_is_diagnostic=True)


def payloads(cases):
    return {'cases.json','probe.groovy','runtime.json','driver.log',*(c['name']+suffix for c in cases
        for suffix in ('-input.f32le','-blur.f32le','-log.f32le','-kernel.json'))}


def laplacian(blur):
    """Independent neighbor differences; nearest edge neighbors give zero flux."""
    import numpy as np
    a=blur.astype(np.float64);pad=np.pad(a,1,mode='edge')
    return (((a-pad[1:-1,:-2])+(a-pad[1:-1,2:]))+
            ((a-pad[:-2,1:-1])+(a-pad[2:,1:-1]))).astype('<f4')


def verify(folder=FIXTURE):
    import numpy as np
    from PIL import Image
    folder=Path(folder).resolve();cases,expected_inputs=inputs();expected=payloads(cases)
    manifest=read(folder/'manifest.json')
    if ({k:manifest.get(k) for k in identity()}!=identity()
            or set(manifest.get('files',{}))!=expected
            or {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}!=expected|{'manifest.json'}):
        raise ValueError('Gaussian capture identity differs')
    for name,entry in manifest['files'].items():
        path=folder/name
        if path.stat().st_size!=entry['bytes'] or sha(path)!=entry['sha256']:
            raise ValueError('Gaussian capture bytes changed: '+name)
    if (read(folder/'cases.json')!=cases or (folder/'probe.groovy').read_bytes()!=GENERATOR.read_bytes()
            or read(folder/'runtime.json')!={'imagej_version':'1.54p','java_version':'25.0.2'}
            or any((folder/name).read_bytes()!=raw for name,raw in expected_inputs.items())):
        raise ValueError('Gaussian capture recipe differs')
    checked=[]
    for case in cases:
        name=case['name'];shape=(case['height'],case['width']);arrays={}
        for kind in ('input','blur','log'):
            raw=(folder/(name+'-'+kind+'.f32le')).read_bytes()
            if len(raw)!=4*shape[0]*shape[1]:raise ValueError('Gaussian array size differs: '+name)
            array=np.frombuffer(raw,dtype='<f4').reshape(shape)
            if not np.all(np.isfinite(array)):raise ValueError('Gaussian array is nonfinite: '+name)
            arrays[kind]=array
        kernel=np.asarray(read(folder/(name+'-kernel.json')),dtype=float)
        if kernel.ndim!=2 or kernel.shape[0]!=2 or kernel.shape[1]<1 or not np.all(np.isfinite(kernel)):
            raise ValueError('Gaussian requested kernel differs: '+name)
        if laplacian(arrays['blur']).tobytes()!=arrays['log'].tobytes():
            raise ValueError('Gaussian captured-blur Laplacian differs: '+name)
        checked.append(dict(name=name,shape=list(shape),sigma=case['sigma'],laplacian_from_blur_exact=True))
    with Image.open(ROOT/'tests/fixtures/qupath-cells-roi-debug-0.7.0/debug-stack.tif') as image:
        image.seek(1);reference=np.asarray(image).astype('<f4')
    if (folder/'roi-sigma1-log.f32le').read_bytes()!=reference.tobytes():
        raise ValueError('Gaussian ROI LoG differs from original plugin capture')
    return dict(schema='qpx.qupath-gaussian-verification/1',cases=checked,
        original_roi_log_exact=True,manifest_sha256=sha(folder/'manifest.json'),
        native_recipe_changed=False,segmentation_parity_accepted=False,biological_validation=False)


def capture(folder,runtime_cache):
    if os.name!='nt':raise ValueError('pinned reference capture requires Windows')
    folder=Path(folder).resolve();folder.mkdir(parents=True,exist_ok=False)
    cases,values=inputs()
    (folder/'cases.json').write_bytes((json.dumps(cases,indent=2)+'\n').encode())
    for name,raw in values.items():(folder/name).write_bytes(raw)
    shutil.copyfile(GENERATOR,folder/'probe.groovy')
    runtime=load('gaussian_runtime',ROOT/'tools/fetch_qupath.py').fetch(runtime_cache)
    with (folder/'driver.log').open('xb') as log:
        subprocess.run([str(runtime),'script','--args',str(folder),str(folder/'probe.groovy')],
            stdout=log,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW).check_returncode()
    manifest=identity();manifest['files']={name:dict(bytes=(folder/name).stat().st_size,sha256=sha(folder/name)) for name in sorted(payloads(cases))}
    (folder/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
    return verify(folder)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path)
    parser.add_argument('--reference',type=Path,default=FIXTURE)
    parser.add_argument('--runtime-cache',type=Path,default=ROOT/'build/upstream/qupath')
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    report=capture(args.capture,args.runtime_cache) if args.capture else verify(args.reference)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_bytes((json.dumps(report,indent=2)+'\n').encode())
    print('Verified',len(report['cases']),'Gaussian/LoG cases; original ROI stage exact')


if __name__=='__main__':main()
