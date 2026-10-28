from pathlib import Path
import json
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2]
out=root/'build/rgb-nominal-287'
folders=[root/'build/ci/scientific-grid-159/tests/fixtures/qupath-rgb-resampling-0.7.0',out/'holdout']
rows=[]
for folder in folders:
    for item in json.loads((folder/'reference.json').read_text())['cases']:
        source=np.asarray(Image.open(folder/item['case_name']/'input.png')).astype(np.int64)
        actual=np.asarray(Image.open(folder/item['case_name']/(item['request_name']+'.png')))
        h,w=source.shape[:2];ow,oh=item['width'],item['height'];step=item['downsample'];x,y,_,_=item['bounds']
        stride=int(step*2**32);start=int(step*2**31)
        cx=(x<<32)+start+np.arange(ow,dtype=np.int64)*stride
        cy=(y<<32)+start+np.arange(oh,dtype=np.int64)*stride
        inside=(cy[:,None]>=0)&(cy[:,None]<(h<<32))&(cx[None,:]>=0)&(cx[None,:]<(w<<32))
        if step<1:
            ix=np.clip(cx>>32,0,w-1);iy=np.clip(cy>>32,0,h-1)
            predicted=source[iy[:,None],ix[None,:]]
        else:
            xx=np.clip(cx-(1<<31),0,(w-1)<<32);yy=np.clip(cy-(1<<31),0,(h-1)<<32)
            ix=xx>>32;iy=yy>>32
            fx=((xx>>24)&255)[None,:,None];fy=((yy>>24)&255)[:,None,None]
            a=source[iy[:,None],ix[None,:]];b=source[iy[:,None],np.minimum(ix+1,w-1)[None,:]]
            c=source[np.minimum(iy+1,h-1)[:,None],ix[None,:]];d=source[np.minimum(iy+1,h-1)[:,None],np.minimum(ix+1,w-1)[None,:]]
            predicted=(a*(256-fx)*(256-fy)+b*fx*(256-fy)+c*(256-fx)*fy+d*fx*fy+32768)//65536
        predicted=np.where(inside[:,:,None],predicted,0).astype(np.uint8)
        delta=np.abs(predicted.astype(int)-actual.astype(int))
        row=dict(corpus=folder.name,case_name=item['case_name'],request_name=item['request_name'],
            changed_components=int(np.count_nonzero(delta)),maximum_error=int(delta.max()))
        rows.append(row)
        if row['changed_components']:print(row,flush=True)
(out/'global-domain-model.json').write_text(json.dumps(dict(cases=rows,
    exact=sum(v['changed_components']==0 for v in rows),total=len(rows)),indent=2)+'\n')
print('Whole-source-domain fixed-point exact',sum(v['changed_components']==0 for v in rows),'/',len(rows),flush=True)
