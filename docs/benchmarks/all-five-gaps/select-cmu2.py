"""Select three separated tissue-rich crops from source pixels before segmentation."""
from pathlib import Path
import hashlib,json,sys
import numpy as np
import qupathxx as q
slide=Path(sys.argv[1]).resolve();output=Path(sys.argv[2])
server=q.open_slide(str(slide));m=server.metadata;level=len(m.levels)-1;l=m.levels[level]
rgba=server.read_region(q.RegionRequest(0,0,level,l.width,l.height)).numpy()
od=np.maximum(0,np.log10(255/np.maximum(rgba[...,:3].astype(float),1))).sum(axis=2)
candidates=[]
for y in range(1024,m.height-1536,2048):
 for x in range(1024,m.width-1536,2048):
  a=int(y*m.levels[level].height/m.height);b=int(x*m.levels[level].width/m.width)
  h=max(1,int(512*m.levels[level].height/m.height));w=max(1,int(512*m.levels[level].width/m.width))
  candidates.append((float(od[a:a+h,b:b+w].mean()),x,y))
candidates.sort(key=lambda row:(-row[0],row[2],row[1]));selected=[]
for score,x,y in candidates:
 if all((x-r['roi'][0])**2+(y-r['roi'][1])**2>=(min(m.width,m.height)/4)**2 for r in selected):
  selected.append(dict(roi=[x,y,512,512],thumbnail_od_sum_score=score))
  if len(selected)==3:break
assert len(selected)==3
result=dict(slide=str(slide),source_sha256=hashlib.sha256(slide.read_bytes()).hexdigest(),dimensions=[m.width,m.height],selection='Descending thumbnail OD-sum, deterministic tie/order, minimum separation min(width,height)/4; no segmentation outcomes used',recipe=dict(context=0,step=1,threshold=.4,sigma=1,min_area=16,max_area=512,expansion=5,processing='QUPATH_070_V2',sampling='AFFINE_INVERSE'),criteria=dict(rgb='exact',canonical_partitions='exact',counts='exact',compartment_pixels='exact',rgb_and_od_mean_absolute_tolerance=1e-10),crops=selected)
output.write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2));server.close()
