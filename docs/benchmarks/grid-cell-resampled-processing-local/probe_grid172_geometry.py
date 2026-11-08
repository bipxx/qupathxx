from pathlib import Path
import json,sys
import numpy as np
from PIL import Image
import qupathxx as q
root=Path(__file__).resolve().parent/'scientific-grid-159';sys.path.insert(0,str(root/'tools'));import compare_qupath_cells as c
folder=root.parents[1].parent/'build/grid172-resampled-rgb-debug/resampled_reference'
features=json.loads((folder/'cells.geojson').read_text(encoding='utf-8'))['features'];h,w=128,171
y,x=np.mgrid[:h,:w];x=x+.5;y=y+.5
masks={}
for name,key in [('nucleus','nucleusGeometry'),('body','geometry')]:
 labels=np.zeros((h,w),np.uint32);errors=[]
 for f in features:
  inside=np.zeros((h,w),bool)
  for ring in f[key]['coordinates']:
   points=np.asarray(ring)/1.5;errors.extend((points-np.rint(points)).ravel().tolist())
   for a,b in zip(points[:-1],points[1:]):
    if a[1]==b[1]:continue
    inside^=((a[1]>y)!=(b[1]>y)) & (x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])
  assert not np.any(inside & (labels>0));labels[inside]=int(f['properties']['name'].split('_')[-1])
 masks[name]=labels
 print(name,'integer vertices',max(map(abs,errors)), 'pixels',int(np.count_nonzero(labels)))
with Image.open(folder/'processed-input.png') as image:rgb=np.asarray(image.convert('RGB'))
doc=q.Document(q.Image.from_numpy(rgb));obj=doc.add_rectangle(q.Rect(0,0,w,h));p=q.GridCellParameters();p.threshold=.4;p.sigma=1/1.5;p.cell_expansion=5/1.5;p.min_nucleus_area=16/(1.5*1.5);p.max_nucleus_area=512/(1.5*1.5)
grid=q.ScientificGrid(w,h,q.ScientificGridAxis(0,w,w),q.ScientificGridAxis(0,h,h));r=q.detect_cells(q.CellRequest(doc,obj,q.StainTransform([[.65,.7,.29],[.07,.99,.11]]),grid,p))
for name,attr in [('nucleus','nuclei'),('body','cells')]:print(name,c.metrics(masks[name],getattr(r,attr).labels_numpy(),c.read(c.FIXTURES/'contract.json')['comparison']))
