"""Copy to an owned scratch directory and run with NumPy 2.5.4/tifffile 2024.9.20."""
import json,hashlib
from pathlib import Path
import numpy as np,tifffile
root=Path(__file__).parent
cases=[]
for name,dims in [('narrow',[(128,64),(32,16),(8,4)]),('partial',[(129,67),(33,17),(9,5)]),('full',[(128,64),(64,32),(32,16)])]:
    path=root/(name+'.tif');levels=[]
    with tifffile.TiffWriter(path) as writer:
        for i,(w,h) in enumerate(dims):
            rgb=np.random.default_rng(i).integers(0,256,(h,w,3),dtype=np.uint8)
            writer.write(rgb,photometric='rgb',tile=(16,16),compression='deflate',metadata=None,subfiletype=0 if i==0 else 1)
            expected=b''.join(bytes(p)+b'\xff' for p in rgb.reshape(-1,3))
            expected_path=root/f'{name}-{i}.rgba';expected_path.write_bytes(expected)
            levels.append(dict(width=w,height=h,expected=expected_path.name))
    cases.append(dict(path=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),levels=levels))
(root/'fixtures.json').write_text(json.dumps(dict(tifffile=tifffile.__version__,numpy=np.__version__,cases=cases),indent=2)+'\n',encoding='utf8')
