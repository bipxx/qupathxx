from pathlib import Path
import sys,json,gzip
import numpy as np
from PIL import Image
import qupathxx as q
root=Path(__file__).resolve().parent/'scientific-grid-159';sys.path.insert(0,str(root/'tools'))
import compare_qupath_cells as comparison
raw=root.parents[1].parent/'build/grid172-resampled-rgb-debug';case=raw/'resampled_reference'
contract=comparison.verify_capture(comparison.FIXTURES/'capture')
with Image.open(raw/'debug-stack.tif') as image:image.seek(4);labels=np.asarray(image).astype(np.uint32)
ref=np.frombuffer(gzip.decompress((comparison.FIXTURES/'capture/resampled_reference/nucleus.u32le.gz').read_bytes()),dtype='<u4').reshape(192,256)
rows=[]
for name,sx,sy in [('nominal',1.5,1.5),('realized',256/171,1.5)]:
 x=np.floor((np.arange(256)+.5)/sx).astype(int);y=np.floor((np.arange(192)+.5)/sy).astype(int)
 projected=labels[y[:,None],x[None,:]]
 m=comparison.metrics(ref,projected,contract['comparison'])
 rows.append(dict(mapping=name,metric=m,disagreement=int(np.count_nonzero((ref>0)!=(projected>0)))))
with Image.open(case/'processed-input.png') as image:rgb=np.asarray(image.convert('RGB'))
with Image.open(case/'input.png') as image:source=np.asarray(image.convert('RGB'))
server=q.dense_image_server(q.Image.from_numpy(source));grid=q.ScientificGrid(256,192,q.ScientificGridAxis(0,256,171),q.ScientificGridAxis(0,192,128))
native=q.read_scientific_rgb(server,grid).numpy()
print(json.dumps(dict(projections=rows,native_area_input=dict(max_rgb_absolute_difference=float(np.max(np.abs(native-rgb))),different_rgb_components=int(np.count_nonzero(native!=rgb)),rounded_different_components=int(np.count_nonzero(np.rint(native)!=rgb)))),indent=2))
doc=q.Document(q.Image.from_numpy(rgb));obj=doc.add_rectangle(q.Rect(0,0,171,128));p=q.GridCellParameters();p.threshold=.4;p.sigma=1/1.5;p.cell_expansion=5/1.5;p.min_nucleus_area=16/(1.5*1.5);p.max_nucleus_area=512/(1.5*1.5)
grid=q.ScientificGrid(171,128,q.ScientificGridAxis(0,171,171),q.ScientificGridAxis(0,128,128));r=q.detect_cells(q.CellRequest(doc,obj,q.StainTransform(contract['vectors'],contract['white']),grid,p))
print('PROCESSED INPUT DETECTOR',json.dumps(comparison.metrics(labels,r.nuclei.labels_numpy(),contract['comparison'])))
