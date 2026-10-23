"""Paired request/detection observations; no hardware or speedup acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

parser=argparse.ArgumentParser()
parser.add_argument('--child-size',type=int,choices=[128,512,1024])
parser.add_argument('--child-out',type=Path)
parser.add_argument('--physical-first',action='store_true')
parser.add_argument('--output-directory',type=Path)
args=parser.parse_args()

if args.child_size:
    import numpy as np
    import qupathxx as q
    from qupathxx import _native
    size=args.child_size;yy,xx=np.mgrid[:size,:size]
    image=np.full((size,size,3),245,dtype=np.uint8)
    mask=(xx%16>=4)&(xx%16<12)&(yy%16>=4)&(yy%16<12)
    image[mask]=[35,45,75]
    doc=q.Document(q.Image.from_numpy(image));doc.set_calibration(.5,.25)
    obj=doc.add_rectangle(q.Rect(0,0,size,size));transform=q.StainTransform([[1,0,0],[0,1,0]])
    raw=q.CellParameters();raw.sigma=0;raw.threshold=.5;raw.min_nucleus_pixels=1;raw.cell_expansion=3;raw.level=0
    sampled=q.CellParameters();sampled.sigma=0;sampled.threshold=.5;sampled.min_nucleus_pixels=64;sampled.max_nucleus_pixels=96;sampled.cell_expansion=3;sampled.level=0
    creators=dict(sampled=lambda:q.CellRequest(doc,obj,transform,sampled),
        physical=lambda:q.CellRequest.with_nucleus_area_limits(doc,obj,transform,8.,12.,raw))
    order=['physical','sampled'] if args.physical_first else ['sampled','physical']
    for name in order:
        for _ in range(20):creators[name]()
        q.detect_cells(creators[name]())
    constructor={};iterations=500
    for name in order:
        start=time.perf_counter_ns()
        for _ in range(iterations):request=creators[name]()
        constructor[name]=(time.perf_counter_ns()-start)/iterations
    results={};timings={};requests={}
    for name in order:
        start=time.perf_counter_ns();request=creators[name]();value=q.detect_cells(request)
        timings[name]=(time.perf_counter_ns()-start)/1e6
        requests[name]=request;results[name]=value
    hashes={}
    for name,value in results.items():
        assert value.cell_count==(size//16)**2
        assert (value.parameters.min_nucleus_pixels,value.parameters.max_nucleus_pixels)==(64,96)
        assert all(f.nucleus_pixels==64 for f in value.features)
        nucleus=value.nuclei.labels_numpy().astype('<u4',copy=False).tobytes()
        bodies=value.cells.labels_numpy().astype('<u4',copy=False).tobytes()
        features=[(f.label,f.nucleus_pixels,f.cell_pixels,list(f.nucleus_rgb),list(f.cell_rgb),
            list(f.nucleus_channels),list(f.cell_channels),list(f.cytoplasm_rgb),list(f.cytoplasm_channels)) for f in value.features]
        hashes[name]=dict(nuclei_sha256=hashlib.sha256(nucleus).hexdigest(),bodies_sha256=hashlib.sha256(bodies).hexdigest(),
            features_sha256=hashlib.sha256(json.dumps(features,separators=(',',':'),allow_nan=False).encode()).hexdigest())
    assert hashes['sampled']==hashes['physical']
    assert requests['sampled'].estimated_work_bytes==requests['physical'].estimated_work_bytes
    output=dict(schema='qpx.cell-area-paired-observation/1',size=size,cells=(size//16)**2,order=order,
        constructor_iterations=iterations,constructor_ns_per_call=constructor,request_and_detection_ms=timings,
        label_and_feature_hashes=hashes['sampled'],estimated_work_bytes=requests['physical'].estimated_work_bytes,
        source_tree_sha256=q.build_environment().source_tree_sha256,build_git_revision=q.build_environment().git_revision,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        python=sys.version,script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),performance_accepted=False)
    assert args.child_out is not None
    args.child_out.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
    print(size,output['cells'],constructor,timings,flush=True)
else:
    assert args.output_directory is not None
    destination=args.output_directory;destination.mkdir(parents=True,exist_ok=False)
    cases=[]
    for size in [128,512,1024]:
        samples=[]
        for index in range(9):
            path=destination/f'{size}-{index}.json'
            command=[sys.executable,'-I',str(Path(__file__).absolute()),'--child-size',str(size),'--child-out',str(path)]
            if index%2:command.append('--physical-first')
            subprocess.run(command,check=True)
            result=json.loads(path.read_text(encoding='utf-8'))
            if index:samples.append(result)
        assert len(samples)==8 and len({s['source_tree_sha256'] for s in samples})==1
        assert len({json.dumps(s['label_and_feature_hashes'],sort_keys=True) for s in samples})==1
        summary={}
        for key in ['constructor_ns_per_call','request_and_detection_ms']:
            summary[key]={}
            for arm in ['sampled','physical']:
                values=[s[key][arm] for s in samples]
                summary[key][arm]=dict(median=statistics.median(values),minimum=min(values),maximum=max(values))
            differences=[s[key]['physical']-s[key]['sampled'] for s in samples]
            summary[key]['paired_physical_minus_sampled']=dict(median=statistics.median(differences),minimum=min(differences),maximum=max(differences))
        cases.append(dict(size=size,cells=samples[0]['cells'],summary=summary,samples=samples))
    report=dict(schema='qpx.cell-area-paired-profile/1',performance_accepted=False,cases=cases,
        protocol='One excluded fresh-process warmup and eight paired fresh-process observations per size; alternating first arm. Within each child, input setup and per-arm warmup precede timers. Exact nucleus/body labels and numeric compartment features agree.',
        limitations=['Only synthetic dense natural-grid RGB squares, sigma zero, anisotropic calibration and nonbinding inclusive physical limits.',
            'Work-byte estimates are not process RSS; imported libraries, allocators and other processes affect measurements.',
            'No universal speedup, biological throughput, hardware or UI/frame-budget acceptance.'])
    (destination/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Wrote',destination/'summary.json',flush=True)
