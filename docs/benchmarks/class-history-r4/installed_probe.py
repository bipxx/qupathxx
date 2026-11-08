import math,json
from pathlib import Path
import qupathxx as q
out=Path(__file__).resolve().parent;scope=json.loads((out/'compiler-inputs.json').read_text())
assert q.build_environment().git_revision==scope['source']
assert q.build_environment().source_tree_sha256==scope['source_tree_sha256']
assert Path(q.__file__).resolve().parent==out.parent/'ci/project-metadata-r5-env/Lib/site-packages/qupathxx'
doc=q.Document(q.Image.demo())
roi=q.Roi.polygon([q.Point(200+100*math.cos(i*math.tau/2000),180+100*math.sin(i*math.tau/2000)) for i in range(2000)])
obj=doc.add_detection(roi,class_name='Root');doc.set_history_limits(128,16384)
before=(doc.revision,doc.history_bytes,doc.annotation(obj).class_name,doc.annotation(obj).geometry.area)
doc.rename_class('Root','Renamed')
after=(doc.revision,doc.history_bytes,doc.annotation(obj).class_name,doc.annotation(obj).geometry.area)
assert after[0]==before[0]+1 and 0<after[1]<16384 and after[2]=='Renamed' and after[3]==before[3]
assert doc.annotation(obj).geometry.equals(roi) and doc.undo() and doc.annotation(obj).class_name=='Root' and doc.redo()
result=dict(source=q.build_environment().git_revision,vertices=2000,history_budget=16384,before=before,after=after,rejected=False,undo_redo=True)
(out/'installed-admission.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result),flush=True)
import pytest
raise SystemExit(pytest.main(['tests/test_classes.py::test_dense_class_only_rename_fits_field_history_budget','tests/test_class_registry.py::test_document_class_snapshot_and_recovery_are_owning','-q','--junitxml='+str(out/'installed.xml'),'--basetemp',str(out/'installed-temp')]))
