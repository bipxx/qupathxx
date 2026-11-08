"""Exact independent source-capture partitions and stored scalar preservation."""
from pathlib import Path
import hashlib,importlib.util,json,math,struct,sys
import numpy as np
import qupathxx as q
ROOT=Path(__file__).resolve().parents[3]
w=Path(sys.argv[1]).resolve()
project_root=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else w/'qpdata'
preview=q.preview_qupath_project(str(project_root/'project/project.qpproj'))
image=preview.images[0]
doc=q.Project.open(str(project_root/'imported.qpxproj')).document(image.native_id)
mapping=dict(zip(image.source_object_ids,image.imported_object_ids,strict=True))
source=json.loads((w/'upstream-objects.json').read_text())
assert len(source)==len(mapping)==len(doc.annotations)==559
scalar_count=0
for row in source:
 obj=doc.annotation(mapping[row['id']])
 assert obj.parent_id==(mapping[row['parent']] if row.get('parent') else 0)
 assert obj.name==(row.get('name') or '') and obj.class_name==(row.get('classification') or '')
 measured=obj.properties.measurements
 assert set(measured)==set(row['measurements'])
 for name,value in row['measurements'].items():
  actual=measured[name]
  if isinstance(value,str):assert ('NaN' if math.isnan(actual) else 'Infinity' if actual>0 else '-Infinity')==value
  else:assert struct.pack('>d',actual)==struct.pack('>d',value),(row['id'],name,actual,value)
  scalar_count+=1
masks={key:np.zeros((256,256),dtype='<u4') for key in ('nucleus','body')}
count=0
for obj in doc.annotations:
 if obj.kind!=q.ObjectKind.CELL:continue
 count+=1
 for key,roi in (('nucleus',obj.nucleus),('body',obj.geometry)):
  b=roi.bounds;left=max(0,math.floor(b.x)-11392);right=min(256,math.ceil(b.x+b.width)-11392)
  top=max(0,math.floor(b.y)-17792);bottom=min(256,math.ceil(b.y+b.height)-17792)
  for y in range(top,bottom):
   for x in range(left,right):
    if roi.contains(q.Point(11392+x+.5,17792+y+.5)):
     assert masks[key][y,x]==0,(key,x,y)
     masks[key][y,x]=count
assert count==558
spec=importlib.util.spec_from_file_location('oracle',ROOT/'docs/benchmarks/candidate5-performance/summarize.py')
o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
reference=ROOT.parents[1]/'gap-closure/ndpi-qupath/0'
r=[np.fromfile(reference/(k+'.u32'),dtype='<u4').reshape(256,256) for k in ('nucleus','body')]
assert all(np.array_equal(a,b) for a,b in zip(o.canonical(masks['nucleus'],masks['body']),o.canonical(*r)))
issues=[dict(code=i.code,count=i.count,message=i.message) for i in preview.issues]
assert any(i['code']=='polygon_vertex_touches_normalized' for i in issues)
result=dict(source_tree=q.build_environment().source_tree_sha256,cells=count,objects=len(source),measurement_values_bit_exact=scalar_count,canonical_nucleus_body_partitions_exact=True,hierarchy_names_classes_preserved=True,calibration_um=[doc.pixel_width_um,doc.pixel_height_um],explicit_issues=issues,source_qpdata_sha256=hashlib.sha256((reference/'study.qpdata').read_bytes()).hexdigest())
(w/'import-preservation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
