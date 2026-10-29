from pathlib import Path
import gzip
import json
import numpy as np
root=Path(__file__).resolve().parents[2];folder=root/'build/qupath-input-stages-301/capture3'
rows=[]
for c in json.loads((folder/'cases.json').read_text()):
    rgb=np.frombuffer((folder/(c['image']+'.rgb')).read_bytes(),dtype='u1').reshape(c['height'],c['width'],3)
    values=np.maximum(0.,-np.log10(np.maximum(rgb.astype(float),1)/np.asarray(c['white']))).astype('<f4')
    for channel in range(3):
        expected=np.frombuffer(gzip.decompress((folder/(c['name']+'-od'+str(channel)+'.f32le.gz')).read_bytes()),dtype='<f4').reshape(rgb.shape[:2])
        changed=values[:,:,channel].copy().view('u4')!=expected.view('u4')
        if changed.any():rows.append(dict(name=c['name'],channel=channel,count=int(changed.sum()),all_signed_zero=bool(np.all(values[:,:,channel][changed]==0) and np.all(expected[changed]==0))))
assert rows and all(c['all_signed_zero'] for c in rows)
(folder.parent/'numpy-signed-zero-observer.json').write_text(json.dumps(rows,indent=2)+'\n');print(rows)
