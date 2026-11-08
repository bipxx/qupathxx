from pathlib import Path
import json
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2];out=root/'build/cell-policy-edges-303'
reference=json.loads((out/'affine-reference.json').read_text(encoding='utf-8'))
checks=[]
for case in reference:
    folder=out/'capture1'/case['name']
    with Image.open(folder/'input.png') as image:source=np.asarray(image).astype(np.uint32)
    with Image.open(folder/'processed-input.png') as image:expected=np.asarray(image)
    h,w=expected.shape[:2];step=case['step'];stride=int(np.floor(step*2**32))
    start=[int(np.floor(value*2**32)) for value in case['first_center']]
    axes=[]
    for axis,count,limit in [(0,w,source.shape[1]),(1,h,source.shape[0])]:
        center=start[axis]+np.arange(count,dtype=np.int64)*stride
        lower=(center-2**31)//2**32
        fraction=((center-2**31)&0xffffffff)>>24
        lower=np.clip(lower,0,limit-1);upper=np.clip(lower+1,0,limit-1)
        axes.append((lower,upper,fraction.astype(np.uint32)))
    (x,x1,fx),(y,y1,fy)=axes;fx=fx[None,:,None];fy=fy[:,None,None]
    values=(source[y[:,None],x[None,:]]*(256-fx)*(256-fy)+source[y[:,None],x1[None,:]]*fx*(256-fy)+
            source[y1[:,None],x[None,:]]*(256-fx)*fy+source[y1[:,None],x1[None,:]]*fx*fy+32768)>>16
    count=int(np.count_nonzero(values!=expected))
    checks.append(dict(name=case['name'],inverse=case['inverse'],first_center=case['first_center'],
                       global_fixed_start=start,differing_components=count))
(out/'affine-model-comparison.json').write_text(json.dumps(dict(schema='qpx.public-affine-phase-diagnostic/1',
    model='captured public Java inverse first center, 32.32 start/stride, eight-bit fractions, one final rounding',
    cases=checks,native_changed=False,generalized_sampler_accepted=False),indent=2)+'\n',encoding='utf-8')
print([(case['name'],case['differing_components']) for case in checks])
