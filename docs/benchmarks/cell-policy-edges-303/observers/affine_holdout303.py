from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303'
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
baseline=load('affine_baseline',work/'tools/qupath_rgb_resampling.py');baseline.verify_capture()
holdout=load('affine_holdout',work/'tools/reference_rgb_nominal.py');holdout.verify_holdout()
requests=[]
for prefix,folder in [('baseline',baseline.FIXTURE),('holdout',holdout.HOLDOUT)]:
    contract=json.loads((folder/'contract.json').read_text(encoding='utf-8'))
    for case in contract['cases']:
        for request in case['requests']:
            requests.append(dict(name=prefix+'/'+case['name']+'/'+request['name'],step=request['downsample'],
                origin=request['bounds'][:2],source=str(folder/case['name']/'input.png'),expected=str(folder/case['name']/(request['name']+'.png'))))
assert len(requests)==99
(out/'affine-heldout-requests.json').write_text(json.dumps(requests,indent=2)+'\n',encoding='utf-8')
probe=out/'affine-heldout-probe.groovy'
probe.write_text('''import java.awt.geom.AffineTransform
import java.awt.geom.Point2D
import java.nio.file.Path
import java.nio.file.Files
import qupath.lib.io.GsonTools
def root=Path.of(args[0])
def cases=GsonTools.getInstance().fromJson(Files.readString(root.resolve('affine-heldout-requests.json')),List.class)
def results=cases.collect { item ->
    def transform=new AffineTransform()
    transform.scale(1d/item.step,1d/item.step);transform.translate(-item.origin[0],-item.origin[1])
    def inverse=transform.createInverse();def center=inverse.transform(new Point2D.Double(.5,.5),null)
    [name:item.name,first_center:[center.x,center.y]]
}
Files.writeString(root.resolve('affine-heldout-reference.json'),GsonTools.getInstance(true).toJson(results)+'\\n')
''',encoding='utf-8')
runtime=load('affine_heldout_runtime',work/'tools/fetch_qupath.py').fetch(root/'build/upstream/qupath')
args=[str(runtime),'script','--args',str(out),str(probe)]
with (out/'affine-heldout-driver.log').open('xb') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
(out/'affine-heldout-command.json').write_text(json.dumps(dict(argv=args,returncode=r.returncode),indent=2)+'\n',encoding='utf-8');assert r.returncode==0
references=json.loads((out/'affine-heldout-reference.json').read_text(encoding='utf-8'));assert [r['name'] for r in references]==[r['name'] for r in requests]
checks=[]
for request,reference in zip(requests,references):
    with Image.open(request['source']) as image:source=np.asarray(image.convert('RGB')).astype(np.uint32)
    with Image.open(request['expected']) as image:expected=np.asarray(image.convert('RGB'))
    h,w=expected.shape[:2];step=request['step'];stride=int(np.floor(step*2**32));axes=[]
    for axis,count,limit in [(0,w,source.shape[1]),(1,h,source.shape[0])]:
        center=int(np.floor(reference['first_center'][axis]*2**32))+np.arange(count,dtype=np.int64)*stride
        integer=center//2**32;valid=(integer>=0)&(integer<limit)
        first=(center-2**31)//2**32;second=first+1;fraction=((center-2**31)&0xffffffff)>>24
        if step<1:first=second=integer;fraction=np.zeros(count,dtype=np.int64)
        axes.append((np.clip(first,0,limit-1),np.clip(second,0,limit-1),fraction.astype(np.uint32),valid))
    (x,x1,fx,vx),(y,y1,fy,vy)=axes;fx=fx[None,:,None];fy=fy[:,None,None]
    actual=(source[y[:,None],x[None,:]]*(256-fx)*(256-fy)+source[y[:,None],x1[None,:]]*fx*(256-fy)+
        source[y1[:,None],x[None,:]]*(256-fx)*fy+source[y1[:,None],x1[None,:]]*fx*fy+32768)>>16
    actual[~(vy[:,None]&vx[None,:])]=0
    checks.append(dict(name=request['name'],differing_components=int(np.count_nonzero(actual!=expected))))
(out/'affine-heldout-comparison.json').write_text(json.dumps(dict(schema='qpx.public-affine-heldout-diagnostic/1',cases=checks,native_changed=False,generalized_sampler_accepted=False),indent=2)+'\n',encoding='utf-8')
print('Earlier99 captures: exact',sum(c['differing_components']==0 for c in checks),'differences',[(c['name'],c['differing_components']) for c in checks if c['differing_components']],flush=True)
