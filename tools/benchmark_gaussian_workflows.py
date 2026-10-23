"""Pair complete native workflow calls in two installed-package interpreters.

Inputs/requests are deterministic; result hashing and transport are outside the
timed calls. Outputs must match byte-for-byte before a speed ratio is reported.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import time


def digest(data):
    return hashlib.sha256(data).hexdigest()


def worker(size):
    import numpy as np
    import qupathxx as q
    from qupathxx import _native
    y,x=np.indices((size,size))
    mask=(x%64-32)**2+(y%64-32)**2<=144
    rgb=np.where(mask[...,None],[45,35,90],[245,240,235]).astype(np.uint8)
    rgba=np.concatenate([rgb,np.full((size,size,1),255,dtype=np.uint8)],axis=2)
    rgba[::17,::19,3]=0
    pixels=q.PixelBuffer.from_numpy(rgba)
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(.5,.5)
    obj=doc.add_rectangle(q.Rect(0,0,size,size))
    transform=q.StainTransform([[.65,.70,.29],[.07,.99,.11]])
    print(json.dumps(dict(ready=True,python=sys.version,platform=sys.platform,
        native_sha256=digest(Path(_native.__file__).read_bytes()),
        source_tree_sha256=q.build_environment().source_tree_sha256,
        pixel_input_sha256=digest(rgba.tobytes()),cell_input_sha256=digest(rgb.tobytes()))),flush=True)
    for line in sys.stdin:
        request=json.loads(line)
        if request.get('stop'):
            break
        sigma=request['sigma'];kind=request['kind']
        if kind=='pixel-features':
            options=q.PixelFeatureOptions();options.sigmas=[sigma]
            options.rgb=True;options.optical_density=True;options.stains=False
            options.gradient_magnitude=True;options.laplacian=True
            start=time.perf_counter()
            result=q.extract_pixel_features(pixels,options,transform)
            elapsed=(time.perf_counter()-start)*1000
            signature=dict(algorithm=result.algorithm,sha256=result.sha256,input_sha256=result.input_sha256,
                features_sha256=digest(result.numpy().tobytes()),valid_sha256=digest(result.valid_numpy().tobytes()),
                valid_pixels=result.valid_pixels,columns=options.feature_count)
            work=result.estimated_work_bytes;retained=result.retained_bytes
        elif kind=='cells':
            parameters=q.CellParameters();parameters.sigma=sigma;parameters.threshold=.4
            parameters.min_nucleus_pixels=16;parameters.max_nucleus_pixels=0
            parameters.cell_expansion=5;parameters.level=0;parameters.tile_size=64
            start=time.perf_counter()
            captured=q.CellRequest(doc,obj,transform,parameters)
            result=q.detect_cells(captured)
            elapsed=(time.perf_counter()-start)*1000
            features=[dict(label=f.label,nucleus_pixels=f.nucleus_pixels,cell_pixels=f.cell_pixels,
                nucleus_rgb=f.nucleus_rgb,cell_rgb=f.cell_rgb,nucleus_channels=f.nucleus_channels,
                cell_channels=f.cell_channels,cytoplasm_rgb=f.cytoplasm_rgb,cytoplasm_channels=f.cytoplasm_channels)
                for f in result.features]
            signature=dict(algorithm=result.algorithm,count=result.cell_count,input_sha256=result.nuclei.input_sha256,
                nucleus_sha256=digest(result.nuclei.labels),body_sha256=digest(result.cells.labels),
                compartment_features_sha256=digest(json.dumps(features,sort_keys=True,allow_nan=False).encode()),
                nucleus_geometry_sha256=digest('\n'.join(c.geometry.wkt for c in result.nucleus_contours.components).encode()),
                body_geometry_sha256=digest('\n'.join(c.geometry.wkt for c in result.cell_contours.components).encode()))
            work=captured.estimated_work_bytes;retained=result.retained_bytes
        else:
            raise ValueError('unknown workload')
        print(json.dumps(dict(wall_ms=elapsed,native_ms=result.elapsed_ms,signature=signature,
            estimated_work_bytes=work,retained_bytes=retained)),flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker',action='store_true',help=argparse.SUPPRESS)
    parser.add_argument('--baseline-python',type=Path)
    parser.add_argument('--candidate-python',type=Path)
    parser.add_argument('--size',type=int,default=512)
    parser.add_argument('--repeats',type=int,default=7)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.size<64 or args.size>1024 or args.size%64 or not 3<=args.repeats<=15:
        parser.error('size must be 64..1024 divisible by 64; repeats 3..15')
    if args.worker:
        worker(args.size);return
    if not args.baseline_python or not args.candidate_python or not args.output:
        parser.error('baseline-python, candidate-python and output are required')
    processes={}
    try:
        env=os.environ.copy()
        for key in ['PYTHONPATH','PYTHONHOME','LD_LIBRARY_PATH','LD_PRELOAD','DYLD_LIBRARY_PATH','DYLD_INSERT_LIBRARIES']:
            env.pop(key,None)
        env.update(OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='1')
        identities={}
        def receive(process):
            line=process.stdout.readline()
            if not line:
                raise RuntimeError('benchmark worker ended: '+process.stderr.read())
            return json.loads(line)
        for name,python in [('baseline',args.baseline_python),('candidate',args.candidate_python)]:
            # Resolving a venv interpreter symlink would silently select the
            # base interpreter and lose the installed package/dependencies.
            process=subprocess.Popen([str(python.absolute()),'-I',str(Path(__file__).resolve()),'--worker','--size',str(args.size)],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',env=env)
            processes[name]=process;identities[name]=receive(process)
        for key in ['python','platform','pixel_input_sha256','cell_input_sha256']:
            if identities['baseline'][key]!=identities['candidate'][key]:
                raise ValueError('paired execution context differs: '+key)
        cases=[]
        for kind in ['pixel-features','cells']:
            for sigma in [1,4,16]:
                samples={name:[] for name in processes};signature=None;resources={}
                for repeat in range(args.repeats+1):
                    order=['baseline','candidate'] if repeat%2==0 else ['candidate','baseline']
                    for name in order:
                        process=processes[name]
                        process.stdin.write(json.dumps(dict(kind=kind,sigma=sigma))+'\n');process.stdin.flush()
                        result=receive(process)
                        if signature is None:signature=result['signature']
                        if result['signature']!=signature:
                            raise ValueError('paired outputs differ: '+kind+' sigma '+str(sigma))
                        if repeat:samples[name].append(dict(wall_ms=result['wall_ms'],native_ms=result['native_ms']))
                        resources[name]={k:result[k] for k in ['estimated_work_bytes','retained_bytes']}
                medians={name:statistics.median(s['wall_ms'] for s in rows) for name,rows in samples.items()}
                cases.append(dict(kind=kind,sigma=sigma,samples=samples,median_wall_ms=medians,
                    speed_ratio=medians['baseline']/medians['candidate'],signature=signature,
                    output_bits_equal=True,resources=resources))
                print(json.dumps(dict(kind=kind,sigma=sigma,speed_ratio=cases[-1]['speed_ratio'])),flush=True)
        report=dict(schema='qpx.gaussian-workflow-benchmark/1',size=args.size,repeats=args.repeats,warmups=1,
            order='alternate baseline-first/candidate-first in persistent isolated interpreters',identities=identities,
            benchmark_source_sha256=digest(Path(__file__).read_bytes()),cases=cases,
            scope='Paired single-host synthetic native workflow calls. Pixel timing includes all feature/derivative extraction; cell timing includes request capture, detection, contours and features. Input construction, worker startup, result hashing, transport and file I/O excluded. No biological, whole-slide, GPU/frame, RSS or universal hardware speed claim.')
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    finally:
        for process in processes.values():
            if process.poll() is None:
                try:
                    process.stdin.write('{"stop": true}\n');process.stdin.flush();process.stdin.close()
                except (BrokenPipeError,OSError):
                    pass
                try:process.wait(timeout=30)
                except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
            for stream in [process.stdout,process.stderr]:
                stream.close()


if __name__=='__main__':
    main()
