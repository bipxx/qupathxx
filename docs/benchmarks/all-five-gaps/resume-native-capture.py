"""Recover only independent oracle capture from the successful retained archive."""
from pathlib import Path
import json,sys
import numpy as np
import qupathxx as q
w=Path(sys.argv[1]);roi=list(map(int,sys.argv[2:6]))
project=q.Project.open(str(w/'0/study.qpxproj'));doc=project.document(1)
assert doc.cell_record_count==1
result=doc.cell_records[0].result
b=doc.annotation(1).geometry.bounds;assert [b.x,b.y,b.width,b.height]==roi
rgb=doc.server.read_region(q.RegionRequest(roi[0],roi[1],0,roi[2],roi[3])).numpy()[...,:3].copy()
od=np.maximum(0,-np.log10(np.maximum(rgb.astype(float),1)/255)).astype(np.float32).astype(float)
for feature in result.features:
 for raster,prefix in ((result.nuclei,'nucleus'),(result.cells,'cell')):
  mask=raster.labels_numpy()==feature.label
  assert mask.sum()==getattr(feature,prefix+'_pixels')
  np.testing.assert_allclose(rgb[mask].mean(axis=0),getattr(feature,prefix+'_rgb'),rtol=0,atol=1e-10)
  np.testing.assert_allclose(od[mask].mean(axis=0),getattr(feature,prefix+'_channels'),rtol=0,atol=1e-10)
for name,raster in (('nucleus',result.nuclei),('body',result.cells)):
 assert (w/'0'/(name+'.u32')).read_bytes()==raster.labels_numpy().astype('<u4').tobytes()
rgb.tofile(w/'0/rgb.u8')
row=dict(cells=result.cell_count,pixel_width_um=doc.pixel_width_um,pixel_height_um=doc.pixel_height_um,area_mm2=roi[2]*roi[3]*doc.pixel_width_um*doc.pixel_height_um/1e6,rgb_sum=rgb.astype(np.uint64).sum(axis=(0,1)).tolist())
(w/'report.json').write_text(json.dumps(dict(source_tree_sha256=q.build_environment().source_tree_sha256,roi=roi,runs=[row],recovered_oracle_from_successful_archive=True,no_redetection=True,od_domain='float32 per pixel then double aggregation',feature_absolute_tolerance=1e-10),indent=2),encoding='utf-8')
print('Recovered exact source/mask capture and compartment arithmetic without redetection:',result.cell_count)
