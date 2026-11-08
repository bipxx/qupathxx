from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/qupath-rgb-resampling-286'
capture=out/'capture'
capture.mkdir(exist_ok=False)
spec=importlib.util.spec_from_file_location('fetch',work/'tools/fetch_qupath.py')
fetch=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)
runtime=fetch.fetch(root/'build/upstream/qupath')
y,x=np.mgrid[:13,:17]
patterns={
    'gradient':np.stack((7*x+3*y,3*x+11*y,13*x+2*y),axis=2).astype(np.uint8),
    'checker':np.stack((255*((x+y)%2),255*(x%2),255*(y%2)),axis=2).astype(np.uint8),
    'noise':np.random.default_rng(286).integers(0,256,size=(13,17,3),dtype=np.uint8),
    'cells':np.asarray(Image.open(work/'tests/fixtures/qupath-cells-0.7.0/capture/resampled_reference/input.png').convert('RGB'))}
cases=[]
for name,rgb in patterns.items():
    folder=capture/name
    folder.mkdir()
    Image.fromarray(rgb).save(folder/'input.png')
    h,w=rgb.shape[:2]
    requests=[dict(name='full_'+str(step).replace('.','_'),downsample=step,bounds=[0,0,w,h])
              for step in (.75,1,1.25,1.5,2,2.3,3.75)]
    requests += [dict(name='crop_1_5',downsample=1.5,bounds=[1,2,w-3,h-4]),
                 dict(name='crop_2_3',downsample=2.3,bounds=[3,1,w-5,h-2])]
    cases.append(dict(name=name,requests=requests))
(capture/'contract.json').write_text(json.dumps(dict(schema='qpx.qupath-rgb-resampling-probe/1',cases=cases),indent=2)+'\n')
args=[str(runtime),'script','--args',str(capture),str(out/'reference.groovy')]
with (out/'capture.log').open('wb') as log:
    result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
(out/'capture-exit.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
print('Pinned QuPath capture exit',result.returncode,flush=True)
assert result.returncode==0
reference=json.loads((capture/'reference.json').read_text())
assert len(reference['cases'])==36
reports=[]
for case in reference['cases']:
    name=case['case_name'];rgb=patterns[name]
    x0,y0,w,h=case['bounds'];source=rgb[y0:y0+h,x0:x0+w].astype(np.float64)
    actual=np.asarray(Image.open(capture/name/(case['request_name']+'.png')).convert('RGB'))
    oh,ow=actual.shape[:2]
    models={}
    for spacing in ('nominal','realized'):
        sx,sy=(case['downsample'],)*2 if spacing=='nominal' else (w/ow,h/oh)
        xs=np.clip((np.arange(ow)+.5)*sx-.5,0,w-1);ys=np.clip((np.arange(oh)+.5)*sy-.5,0,h-1)
        ix=np.floor(xs).astype(int);iy=np.floor(ys).astype(int)
        wx=(xs-ix)[None,:,None];wy=(ys-iy)[:,None,None]
        values=(source[iy[:,None],ix[None,:]]*(1-wx)*(1-wy)
            +source[iy[:,None],np.minimum(ix+1,w-1)[None,:]]*wx*(1-wy)
            +source[np.minimum(iy+1,h-1)[:,None],ix[None,:]]*(1-wx)*wy
            +source[np.minimum(iy+1,h-1)[:,None],np.minimum(ix+1,w-1)[None,:]]*wx*wy)
        for rounding in ('half_up','ties_even'):
            predicted=(np.floor(values+.5) if rounding=='half_up' else np.rint(values)).astype(np.uint8)
            delta=np.abs(predicted.astype(int)-actual.astype(int))
            models[spacing+'_'+rounding]=dict(changed_components=int(np.count_nonzero(delta)),maximum_error=int(delta.max()))
    reports.append(dict(**case,shape_round_half_up=[int(np.floor(w/case['downsample']+.5)),int(np.floor(h/case['downsample']+.5))],
        shape_ceil=[int(np.ceil(w/case['downsample'])),int(np.ceil(h/case['downsample']))],models=models))
    print(name,case['request_name'],(ow,oh),models['nominal_half_up'],flush=True)
(out/'model-report.json').write_text(json.dumps(dict(schema='qpx.qupath-rgb-resampling-models/1',cases=reports,
    general_sampler_equivalence_accepted=False),indent=2)+'\n')
