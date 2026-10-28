from pathlib import Path
import importlib.util
import json
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-287'
out.mkdir(exist_ok=False)
spec=importlib.util.spec_from_file_location('rgb',work/'tools/qupath_rgb_resampling.py')
rgb=importlib.util.module_from_spec(spec)
spec.loader.exec_module(rgb)
contract,reference=rgb.verify_capture()
patterns=rgb.patterns()
rows=[]
for item in reference['cases']:
    x,y,w,h=item['bounds'];ow,oh=item['width'],item['height'];step=item['downsample']
    source=patterns[item['case_name']][y:y+h,x:x+w].astype(np.int64)
    actual=np.asarray(Image.open(rgb.FIXTURE/item['case_name']/(item['request_name']+'.png')))
    models={}
    for coordinate in ('float64','fixed32'):
        if coordinate=='float64':
            cx=(np.arange(ow)+.5)*step;cy=(np.arange(oh)+.5)*step
        else:
            dx=int(step*2**32);start=int(.5*step*2**32)
            cx=(start+np.arange(ow,dtype=np.int64)*dx)/2**32
            cy=(start+np.arange(oh,dtype=np.int64)*dx)/2**32
        if step<1:
            ix=np.clip(np.floor(cx).astype(int),0,w-1);iy=np.clip(np.floor(cy).astype(int),0,h-1)
            models[coordinate+'_nearest']=rgb.differences(actual,source[iy[:,None],ix[None,:]])
            continue
        inside=(cy[:,None]<h)&(cx[None,:]<w)
        xs=np.clip(cx-.5,0,w-1);ys=np.clip(cy-.5,0,h-1)
        ix=np.floor(xs).astype(int);iy=np.floor(ys).astype(int)
        for quantization in ('floor','nearest'):
            fx=(xs-ix)*256;fy=(ys-iy)*256
            fx=np.floor(fx if quantization=='floor' else fx+.5).astype(np.int64)[None,:,None]
            fy=np.floor(fy if quantization=='floor' else fy+.5).astype(np.int64)[:,None,None]
            a=source[iy[:,None],ix[None,:]];b=source[iy[:,None],np.minimum(ix+1,w-1)[None,:]]
            c=source[np.minimum(iy+1,h-1)[:,None],ix[None,:]];d=source[np.minimum(iy+1,h-1)[:,None],np.minimum(ix+1,w-1)[None,:]]
            for arithmetic in ('joint','horizontal_round','horizontal_floor'):
                if arithmetic=='joint':
                    values=(a*(256-fx)*(256-fy)+b*fx*(256-fy)+c*(256-fx)*fy+d*fx*fy+32768)//65536
                else:
                    bias=128 if arithmetic=='horizontal_round' else 0
                    top=(a*(256-fx)+b*fx+bias)//256;bottom=(c*(256-fx)+d*fx+bias)//256
                    values=(top*(256-fy)+bottom*fy+128)//256
                values=np.where(inside[:,:,None],values,0).astype(np.uint8)
                models[coordinate+'_'+quantization+'_'+arithmetic]=rgb.differences(actual,values)
    rows.append(dict(case_name=item['case_name'],request_name=item['request_name'],models=models))
(out/'models.json').write_text(json.dumps(dict(cases=rows,scope='Predetermined mathematical candidates; no automatic production edits'),indent=2)+'\n')
keys=sorted({k for row in rows for k in row['models']})
for key in keys:
    selected=[r['models'][key] for r in rows if key in r['models']]
    print(key,'exact',sum(r['changed_components']==0 for r in selected),'/',len(selected),
          'changed',sum(r['changed_components'] for r in selected),'max',max(r['maximum_error'] for r in selected),flush=True)
