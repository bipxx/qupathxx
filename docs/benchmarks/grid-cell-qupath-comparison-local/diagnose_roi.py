from pathlib import Path
import sys,gzip,json
import numpy as np
import qupathxx as q
root=Path(__file__).resolve().parent/'scientific-grid-159';sys.path.insert(0,str(root/'tests'))
from test_log_cells import scene,CONTRACT
from test_qupath_cells_reference import CAPTURE,comparison
case=next(c for c in CONTRACT['cases'] if c['name']=='roi_cut_hole');doc,obj,t=scene(case)
w,h=CONTRACT['width'],CONTRACT['height'];g=q.ScientificGrid(w,h,q.ScientificGridAxis(0,w,w,32),q.ScientificGridAxis(0,h,h,32))
p=q.GridCellParameters();p.threshold=.4;p.sigma=1;p.cell_expansion=5;p.min_nucleus_area=16;p.max_nucleus_area=512
r=q.detect_cells(q.CellRequest(doc,obj,t,g,p));a=r.nuclei.labels_numpy()[32:32+h,32:32+w]
b=np.frombuffer(gzip.decompress((CAPTURE/case['name']/'nucleus.u32le.gz').read_bytes()),dtype='<u4').reshape(h,w)
print(json.dumps(dict(native_only_yx=np.argwhere((a>0)&(b==0)).tolist(),reference_only_yx=np.argwhere((b>0)&(a==0)).tolist(),metrics=comparison.metrics(b,a,CONTRACT['comparison'])),indent=2))
