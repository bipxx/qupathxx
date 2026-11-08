"""Actual pinned upstream NDPI saved-data migration; preserve explicit losses."""
import hashlib,json,shutil,sys
from pathlib import Path
import qupathxx as q
work=Path(sys.argv[1]).resolve();work.mkdir(exist_ok=True)
source=Path(sys.argv[2]).resolve();data=Path(sys.argv[3]).resolve()
folder=work/'project/data/1';folder.mkdir(parents=True,exist_ok=True)
if not (folder/'data.qpdata').exists():shutil.copyfile(data,folder/'data.qpdata')
project_path=work/'project/project.qpproj'
project_path.write_text(json.dumps(dict(version='0.7.0',lastID=1,images=[dict(entryID=1,imageName='NDPI cells',serverBuilder=dict(builderType='uri',providerClassName='qupath.lib.images.servers.openslide.OpenSlideServerBuilder',uri=source.as_uri(),args=[]))])),encoding='utf-8')
try:
 preview=q.preview_qupath_project(str(project_path))
except ValueError as error:
 result=dict(source_tree=q.build_environment().source_tree_sha256,supported=False,error=str(error),publication_absent=not (work/'imported.qpxproj').exists(),source_qpdata_sha256=hashlib.sha256(data.read_bytes()).hexdigest(),remaining_gap='Real upstream ROI contains invalid/self-intersecting geometry; no repair or silent loss authorized by current format contract')
 (work/'receipt.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
 print(json.dumps(result,indent=2))
 assert 'Ring Self-intersection' in str(error) and result['publication_absent']
 sys.exit(0)
assert len(preview.images)==1 and preview.images[0].available
issues=[dict(code=i.code,message=i.message) for i in preview.issues]
output=work/'imported.qpxproj'
if issues:
 try:preview.import_new(str(output))
 except ValueError:pass
 else:raise AssertionError('loss admission must be explicit')
 assert not output.exists() and not preview.consumed
project=preview.import_new(str(output),allow_losses=bool(issues))
doc=project.document(preview.images[0].native_id)
cells=[a for a in doc.annotations if a.kind==q.ObjectKind.CELL]
assert len(cells)==558
assert (doc.pixel_width_um,doc.pixel_height_um)==(.45641259698767683,.4550625711035267)
restored=q.Project.open(str(output)).document(preview.images[0].native_id)
assert len(restored.annotations)==len(doc.annotations)
for a,b in zip(doc.annotations,restored.annotations,strict=True):
 assert a.id==b.id and a.geometry.wkt==b.geometry.wkt and a.properties.measurements==b.properties.measurements
 if a.kind==q.ObjectKind.CELL:assert a.nucleus.wkt==b.nucleus.wkt
result=dict(source_tree=q.build_environment().source_tree_sha256,cells=len(cells),objects=len(doc.annotations),issues=issues,loss_admission_explicit=True,exact_native_reopen=True,source_qpdata_sha256=hashlib.sha256(data.read_bytes()).hexdigest(),format='QuPath0.7.0 data-file version3, OpenSlide NDPI, explicit supplied project manifest; not arbitrary project builders')
(work/'receipt.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
