from pathlib import Path
import sys,gzip,json
import numpy as np
from PIL import Image
import qupathxx as q
root=Path(__file__).resolve().parent/'scientific-grid-159';sys.path.insert(0,str(root/'tests'))
from test_log_cells import scene,CONTRACT
from test_qupath_cells_reference import CAPTURE,comparison
case=next(c for c in CONTRACT['cases'] if c['name']=='roi_cut_hole');doc,obj,t=scene(case);w,h=CONTRACT['width'],CONTRACT['height']
p=q.GridCellParameters();p.threshold=.4;p.sigma=1;p.cell_expansion=5;p.min_nucleus_area=16;p.max_nucleus_area=512
reports=[]
for cx in [0,2,8,32]:
 x,y,sw,sh=case['roi'];g=q.ScientificGrid(w,h,q.ScientificGridAxis(x,sw,sw,cx),q.ScientificGridAxis(y,sh,sh,cx));r=q.detect_cells(q.CellRequest(doc,obj,t,g,p))
 row=dict(context=cx,core=case['roi'],compartments={})
 for name,attr in [('nucleus','nuclei'),('body','cells')]:
  a=np.zeros((h,w),np.uint32);a[y:y+sh,x:x+sw]=getattr(r,attr).labels_numpy()[cx:cx+sh,cx:cx+sw]
  b=np.frombuffer(gzip.decompress((CAPTURE/case['name']/(name+'.u32le.gz')).read_bytes()),dtype='<u4').reshape(h,w)
  m=comparison.metrics(b,a,CONTRACT['comparison']);row['compartments'][name]=dict(iou=m['foreground_iou'],native_count=m['native_count'],reference_count=m['reference_count'],native_only=int(np.count_nonzero((a>0)&(b==0))),reference_only=int(np.count_nonzero((b>0)&(a==0))))
 reports.append(row)
print(json.dumps(reports,indent=2))
