from pathlib import Path
import json,subprocess,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-edit-state-r5'
assert json.loads((o/'build-exit.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel').glob('*.whl'));stage=o/'budget-probe-stage';stage.mkdir(exist_ok=False)
with zipfile.ZipFile(wheel) as archive:archive.extractall(stage)
source=json.loads((o/'compiler-inputs.json').read_bytes())['source']
code="import sys,hashlib;from pathlib import Path;sys.path.insert(0,"+repr(str(stage))+");import qupathxx as q;from qupathxx import _native;assert q.build_environment().git_revision=="+repr(source)+";print('staged source',q.build_environment().git_revision,'native sha256',hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),flush=True);import pytest;sys.exit(pytest.main("+repr(['tests/test_object_edit_state.py::test_plain_classification_keeps_the_existing_compact_history_budget','-q','--junitxml='+str(o/'history-budget-first.xml'),'--basetemp='+str(o/'history-budget-first-temp')])+"))"
argv=[str(r/'.venv/Scripts/python.exe'),'-I','-X','utf8','-c',code]
with (o/'history-budget-first.log').open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,stdout=log,stderr=subprocess.STDOUT)
(o/'history-budget-first-command.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,source=source,product_installed=False),indent=2)+'\n',encoding='utf-8')
assert result.returncode==1 and 'history payload budget' in (o/'history-budget-first.log').read_text(encoding='utf-8')
print('Confirmed compact history regression on staged product; no install.',flush=True)
