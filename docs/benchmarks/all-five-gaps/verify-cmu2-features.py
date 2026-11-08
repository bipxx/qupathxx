"""Additional quantitative domains from retained records; never redetect."""
from pathlib import Path
import json,sys
import numpy as np
import qupathxx as q
w=Path(sys.argv[1]);plan=json.loads((w/'cmu2-plan.json').read_text());rows=[]
for i,case in enumerate(plan['crops']):
 project=q.Project.open(str(w/('cmu2-'+str(i))/'native/0/study.qpxproj'));doc=project.document(1);result=doc.cell_records[0].result
 x,y,width,height=case['roi'];rgba=doc.server.read_region(q.RegionRequest(x,y,0,width,height)).numpy();rgb=rgba[...,:3]
 od=np.maximum(0,-np.log10(np.maximum(rgb.astype(float),1)/255)).astype(np.float32).astype(float)
 nuclei=result.nuclei.labels_numpy();body=result.cells.labels_numpy();checked=0
 for f in result.features:
  calibrated=result.calibrated_feature(f.label)
  masks={'nucleus':nuclei==f.label,'cell':body==f.label,'cytoplasm':(body==f.label)&(nuclei==0)}
  for name,mask in masks.items():
   count=int(mask.sum());area=count*doc.pixel_width_um*doc.pixel_height_um
   actual=getattr(calibrated,name+'_sample_area_um2')
   if name=='cytoplasm' and count==0:assert actual in (None,0);continue
   np.testing.assert_allclose(actual,area,atol=1e-10,rtol=0)
   np.testing.assert_allclose(getattr(f,name+'_rgb'),rgb[mask].mean(axis=0),atol=1e-10,rtol=0)
   np.testing.assert_allclose(getattr(f,name+'_channels'),od[mask].mean(axis=0),atol=1e-10,rtol=0)
   checked+=1
 rows.append(dict(case=i,cells=result.cell_count,compartments_checked=checked,calibrated_sample_areas=True,rgb_od_and_cytoplasm_means=True))
result=dict(source_tree=q.build_environment().source_tree_sha256,cases=rows,tolerance=1e-10,od_domain='float32 per pixel; double mean, matching declared FloatRgbImage domain',no_redetection=True)
(w/'cmu2-feature-domains.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
