"""Capture and compare public RGB OD/stain input stages for nominal cells."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-cell-inputs-0.7.0'
GENERATOR=ROOT/'tools/upstream/qupath_cell_inputs_probe.groovy'


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


def read(path):return json.loads(Path(path).read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inputs():
    import numpy as np
    from PIL import Image
    debug=load('cell_input_roi',ROOT/'tools/qupath_roi_cell_debug.py');debug.verify()
    with Image.open(ROOT/'tests/fixtures/qupath-cells-0.7.0/capture/roi_cut_hole/input.png') as image:
        roi=np.asarray(image.convert('RGB'))[32:160,32:224].copy()
    sampled=load('cell_input_sampled',ROOT/'tools/qupath_resampled_cell_debug.py')
    _,_,rgb,_,_,_=sampled.verified_data(sampled.FIXTURE)
    y,x=np.indices((256,256));images={'pairwise':np.stack([x,y,x^y],axis=2).astype(np.uint8),'roi':roi,'resampled':rgb}
    basis=[[.65,.70,.29],[.07,.99,.11]]
    whites=[[255,255,255],[240,250,230],[1,1,1],[1.25,127.5,254.5],[31.5,128,200],[255,1,255]]
    cases=[dict(name='white-'+str(i),image='pairwise',white=white,vectors=basis) for i,white in enumerate(whites)]
    cases += [dict(name='identity',image='pairwise',white=[255,255,255],vectors=[[1,0,0],[0,1,0],[0,0,1]]),
        dict(name='three-explicit',image='pairwise',white=[240.5,251.25,232.75],vectors=[[.7,.3,.2],[.1,.8,.3],[.2,.15,.9]]),
        dict(name='signed-residual',image='pairwise',white=[255,255,255],vectors=[[.8,.4,.2],[.2,.8,.4]]),
        dict(name='roi-default',image='roi',white=[255,255,255],vectors=basis),
        dict(name='resampled-default',image='resampled',white=[255,255,255],vectors=basis)]
    for case in cases:case.update(width=images[case['image']].shape[1],height=images[case['image']].shape[0])
    return cases,images


KINDS=('od0','od1','od2','sum','stain0','stain1','stain2')


def payloads(cases):
    return {'cases.json','probe.groovy','driver.log','runtime.json','pairwise.rgb','roi.rgb','resampled.rgb',
        *(c['name']+'-model.json' for c in cases),
        *(c['name']+'-'+kind+'.f32le.gz' for c in cases for kind in KINDS)}


def identity():
    return dict(schema='qpx.qupath-cell-input-capture/1',upstream=read(ROOT/'tests/fixtures/qupath-0.7.0/contract.json')['upstream'],
        generator_sha256=sha(GENERATOR),roi_debug_manifest_sha256=sha(ROOT/'tests/fixtures/qupath-cells-roi-debug-0.7.0/manifest.json'),
        resampled_debug_manifest_sha256=sha(ROOT/'tests/fixtures/qupath-cells-resampled-debug-0.7.0/manifest.json'))


def array(folder,case,kind):
    import numpy as np
    raw=gzip.decompress((folder/(case['name']+'-'+kind+'.f32le.gz')).read_bytes())
    if len(raw)!=case['height']*case['width']*4:raise ValueError('cell input array size differs')
    values=np.frombuffer(raw,dtype='<f4').reshape(case['height'],case['width'])
    if not np.all(np.isfinite(values)):raise ValueError('cell input array must be finite')
    return values


def verify(folder=FIXTURE):
    import numpy as np
    from PIL import Image
    folder=Path(folder).resolve();cases,images=inputs();expected=payloads(cases);manifest=read(folder/'manifest.json')
    if ({k:manifest.get(k) for k in identity()}!=identity() or set(manifest.get('files',{}))!=expected
            or {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}!=expected|{'manifest.json'}):
        raise ValueError('cell input capture identity differs')
    for name,entry in manifest['files'].items():
        if (folder/name).stat().st_size!=entry['bytes'] or sha(folder/name)!=entry['sha256']:
            raise ValueError('cell input capture bytes changed: '+name)
    if (read(folder/'cases.json')!=cases or (folder/'probe.groovy').read_bytes()!=GENERATOR.read_bytes()
            or read(folder/'runtime.json')!={'imagej_version':'1.54p','java_version':'25.0.2'}
            or any((folder/(name+'.rgb')).read_bytes()!=values.tobytes() for name,values in images.items())):
        raise ValueError('cell input capture recipe differs')
    checked=[]
    for case in cases:
        values={kind:array(folder,case,kind) for kind in KINDS};rgb=images[case['image']]
        model=read(folder/(case['name']+'-model.json'));normalized=np.asarray(model['normalized']);inverse=np.asarray(model['inverse'])
        if (normalized.shape!=(3,3) or inverse.shape!=(3,3) or not np.all(np.isfinite(normalized)) or not np.all(np.isfinite(inverse))
                or not np.allclose(np.linalg.norm(normalized,axis=1),1,rtol=0,atol=1e-12)
                or not np.allclose(normalized@inverse,np.eye(3),rtol=0,atol=1e-12)):
            raise ValueError('cell input stain model differs')
        od=np.maximum(0.,-np.log10(np.maximum(rgb.astype(float),1)/np.asarray(case['white'])))
        # Java Math.max(+0, -0) yields +0; NumPy maximum keeps the second zero.
        od[od==0]=0.
        for channel in range(3):
            if od[:,:,channel].astype('<f4').tobytes()!=values['od'+str(channel)].tobytes():raise ValueError('cell input OD arithmetic differs')
        total=(values['od0']+values['od1'])+values['od2']
        if total.tobytes()!=values['sum'].tobytes():raise ValueError('cell input float32 sum order differs')
        checked.append(dict(name=case['name'],pixels=case['width']*case['height'],od_arrays=3,float32_sum_exact=True,
            sum_once_in_float64_changed_pixels=int(np.count_nonzero(od.sum(axis=2).astype('<f4').view('u4')!=values['sum'].view('u4'))),
            signed_stain_samples=int(sum(np.count_nonzero(values['stain'+str(c)]<0) for c in range(3)))))
    for name,fixture in [('roi-default','qupath-cells-roi-debug-0.7.0'),('resampled-default','qupath-cells-resampled-debug-0.7.0')]:
        with Image.open(ROOT/'tests/fixtures'/fixture/'debug-stack.tif') as image:
            image.seek(0);expected=np.asarray(image).astype('<f4')
        case=next(c for c in cases if c['name']==name)
        if array(folder,case,'sum').tobytes()!=expected.tobytes():raise ValueError('cell input differs from original plugin stage')
    return dict(schema='qpx.qupath-cell-input-verification/1',cases=checked,original_plugin_inputs_exact=True,
        manifest_sha256=sha(folder/'manifest.json'),biological_validation=False,segmentation_parity_accepted=False)


def compare_native(folder=FIXTURE):
    import numpy as np
    import qupathxx as q
    folder=Path(folder).resolve();report=verify(folder);cases,images=inputs();comparisons=[]
    for case in cases:
        server=q.dense_image_server(q.Image.from_numpy(images[case['image']]))
        try:
            pixels=server.read_region(q.RegionRequest(0,0,0,case['width'],case['height']))
            transform=q.StainTransform(case['vectors'],case['white'])
            od=transform.optical_densities(pixels).numpy().copy();stains=transform.deconvolve(pixels).numpy().copy()
            captured=read(folder/(case['name']+'-model.json'))
            normalized=q.StainTransform.from_normalized(captured['normalized'],case['white']).deconvolve(pixels).numpy().copy()
            actual={**{'od'+str(c):od[:,:,c] for c in range(3)},'sum':(od[:,:,0]+od[:,:,1])+od[:,:,2],
                    **{'stain'+str(c):stains[:,:,c] for c in range(3)}}
            for kind,value in actual.items():
                if value.astype('<f4').tobytes()!=array(folder,case,kind).tobytes():raise ValueError('native cell input bits differ: '+case['name']+'/'+kind)
            for c in range(3):
                if normalized[:,:,c].astype('<f4').tobytes()!=array(folder,case,'stain'+str(c)).tobytes():raise ValueError('native normalized stain bits differ')
            comparisons.append(dict(name=case['name'],exact_arrays=7,normalized_stain_arrays=3))
        finally:server.close()
    report.update(native_comparisons=comparisons,native_source=q.build_environment().git_revision,
        native_fingerprint=q.build_environment().source_tree_sha256,native_math_changed=False)
    return report


def capture(folder,runtime_cache):
    if os.name!='nt':raise ValueError('pinned capture requires Windows')
    folder=Path(folder).resolve();folder.mkdir(parents=True,exist_ok=False);cases,images=inputs()
    (folder/'cases.json').write_bytes((json.dumps(cases,indent=2)+'\n').encode())
    for name,values in images.items():(folder/(name+'.rgb')).write_bytes(values.tobytes())
    shutil.copyfile(GENERATOR,folder/'probe.groovy');runtime=load('cell_input_runtime',ROOT/'tools/fetch_qupath.py').fetch(runtime_cache)
    with (folder/'driver.log').open('xb') as stream:
        subprocess.run([str(runtime),'script','--args',str(folder),str(folder/'probe.groovy')],stdout=stream,stderr=subprocess.STDOUT,
            timeout=180,creationflags=subprocess.CREATE_NO_WINDOW).check_returncode()
    manifest=identity();manifest['files']={name:dict(bytes=(folder/name).stat().st_size,sha256=sha(folder/name)) for name in sorted(payloads(cases))}
    (folder/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode());return verify(folder)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path)
    parser.add_argument('--reference',type=Path,default=FIXTURE);parser.add_argument('--runtime-cache',type=Path,default=ROOT/'build/upstream/qupath')
    parser.add_argument('--native',action='store_true');parser.add_argument('--report',type=Path,required=True);args=parser.parse_args()
    report=capture(args.capture,args.runtime_cache) if args.capture else verify(args.reference)
    if args.native:report=compare_native(args.capture or args.reference)
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_bytes((json.dumps(report,indent=2)+'\n').encode())
    print('Verified',len(report['cases']),'OD/stain input cases; original plugin inputs exact')


if __name__=='__main__':main()
