from pathlib import Path
import json,os,subprocess,xml.etree.ElementTree as E,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-planes-r5'
assert json.loads((o/'build-compat-exit.json').read_bytes())['returncode']==0
first=json.loads((o/'compiler-inputs-final.json').read_bytes());now=json.loads((o/'compiler-inputs-compat.json').read_bytes())
assert first['files'].keys()==now['files'].keys()
assert [p for p,h in now['files'].items() if first['files'][p]!=h]==['cpp/interchange.cpp']
wheel,=list((o/'wheel-compat').glob('*.whl'));stage=o/'compat-probe-stage';stage.mkdir()
with zipfile.ZipFile(wheel) as archive:archive.extractall(stage)
failed=[c for c in E.parse(o/'model.xml').iter('testcase') if c.find('failure') is not None];assert len(failed)==1
case=failed[0];assert case.get('classname')=='tests.test_interchange'
args=['tests/test_interchange.py::'+case.get('name'),'-q','--junitxml='+str(o/'compat-probe.xml'),'--basetemp='+str(o/'compat-probe-temp')]
code="import sys;sys.path.insert(0,"+repr(str(stage))+");import qupathxx as q;assert q.build_environment().git_revision=="+repr(now['source'])+";import pytest;sys.exit(pytest.main("+repr(args)+"))"
argv=[str(r/'build/ci/object-edit-state-r5-env/Scripts/python.exe'),'-I','-X','utf8','-c',code]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0'
with (o/'compat-probe.log').open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/'compat-probe-command.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,native_source=now['source'],changed_native_inputs=['cpp/interchange.cpp'],product_installed=False),indent=2),encoding='utf-8')
print('Localized staged compatibility check',result.returncode,flush=True);raise SystemExit(result.returncode)
