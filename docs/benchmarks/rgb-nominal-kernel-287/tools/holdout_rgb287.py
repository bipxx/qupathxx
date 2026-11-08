from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-287'
folder=out/'holdout'
folder.mkdir(exist_ok=False)
spec=importlib.util.spec_from_file_location('fetch',work/'tools/fetch_qupath.py')
fetch=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)
runtime=fetch.fetch(root/'build/upstream/qupath')
arrays={}
for name,w,h in [('gradient',31,29),('checker',31,29),('noise',31,29),('offset',47,33),
                 ('wide',19,1),('thin',1,23),('one',1,1)]:
    y,x=np.mgrid[:h,:w]
    if name=='gradient':pixels=np.stack(((3*x+5*y)%256,(7*x+y)%256,(x+3*y)%256),axis=2)
    elif name=='checker':pixels=np.stack((255*((x+y)%2),255*(x%2),255*(y%2)),axis=2)
    else:pixels=np.random.default_rng(287+w+h).integers(0,256,size=(h,w,3),dtype=np.uint8)
    arrays[name]=pixels.astype(np.uint8)
cases=[]
steps=[.6,.9,1,1.1,1.7,2.1,2.75,4.2,7.125]
for name,pixels in arrays.items():
    h,w=pixels.shape[:2]
    requests=[dict(name='full_'+str(step).replace('.','_'),downsample=step,bounds=[0,0,w,h])
              for step in steps if min(int(np.floor(w/step+.5)),int(np.floor(h/step+.5)))>0]
    if min(w,h)>10:
        requests += [dict(name='crop_'+str(step).replace('.','_'),downsample=step,bounds=[3,5,w-7,h-9])
                     for step in (1.1,2.1,2.75)]
    cases.append(dict(name=name,requests=requests))
    (folder/name).mkdir()
    Image.fromarray(pixels).save(folder/name/'input.png')
(folder/'contract.json').write_text(json.dumps(dict(schema='qpx.rgb-nominal-holdout/1',cases=cases),indent=2)+'\n')
command=[str(runtime),'script','--args',str(folder),str(work/'tools/upstream/qupath_rgb_resampling.groovy')]
with (out/'holdout-runtime.log').open('wb') as log:
    result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
(out/'holdout-exit.json').write_text(json.dumps(dict(argv=command,returncode=result.returncode),indent=2)+'\n')
assert result.returncode==0
reference=json.loads((folder/'reference.json').read_text())
rows=[]
for item in reference['cases']:
    x,y,w,h=item['bounds'];ow,oh=item['width'],item['height'];step=item['downsample']
    source=arrays[item['case_name']][y:y+h,x:x+w].astype(np.int64)
    actual=np.asarray(Image.open(folder/item['case_name']/(item['request_name']+'.png')))
    stride=int(step*2**32);start=int(step*2**31)
    cx=start+np.arange(ow,dtype=np.int64)*stride;cy=start+np.arange(oh,dtype=np.int64)*stride
    if step<1:
        ix=np.clip(cx>>32,0,w-1);iy=np.clip(cy>>32,0,h-1)
        predicted=source[iy[:,None],ix[None,:]].astype(np.uint8)
    else:
        inside=(cy[:,None]<(h<<32))&(cx[None,:]<(w<<32))
        xx=np.clip(cx-(1<<31),0,(w-1)<<32);yy=np.clip(cy-(1<<31),0,(h-1)<<32)
        ix=xx>>32;iy=yy>>32
        fx=((xx>>24)&255)[None,:,None];fy=((yy>>24)&255)[:,None,None]
        a=source[iy[:,None],ix[None,:]];b=source[iy[:,None],np.minimum(ix+1,w-1)[None,:]]
        c=source[np.minimum(iy+1,h-1)[:,None],ix[None,:]];d=source[np.minimum(iy+1,h-1)[:,None],np.minimum(ix+1,w-1)[None,:]]
        predicted=((a*(256-fx)*(256-fy)+b*fx*(256-fy)+c*(256-fx)*fy+d*fx*fy+32768)//65536)
        predicted=np.where(inside[:,:,None],predicted,0).astype(np.uint8)
    delta=np.abs(predicted.astype(int)-actual.astype(int))
    row=dict(case_name=item['case_name'],request_name=item['request_name'],width=ow,height=oh,
        changed_components=int(np.count_nonzero(delta)),maximum_error=int(delta.max()))
    rows.append(row)
    if row['changed_components']:print(row,flush=True)
(out/'holdout-comparison.json').write_text(json.dumps(dict(cases=rows,exact=sum(v['changed_components']==0 for v in rows)),indent=2)+'\n')
print('Held-out exact cases',sum(v['changed_components']==0 for v in rows),'/',len(rows),flush=True)
