from pathlib import Path
import json,os,subprocess,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-planes-r5'
assert json.loads((o/'build-final-exit.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel-final').glob('*.whl'));stage=o/'final-probe-stage';stage.mkdir()
with zipfile.ZipFile(wheel) as archive:archive.extractall(stage)
source=json.loads((o/'compiler-inputs-final.json').read_bytes())['source']
args=['tests/test_object_planes.py','tests/test_raw_analysis.py','tests/test_raw_batch.py','-q','--junitxml='+str(o/'final-probe.xml'),'--basetemp='+str(o/'final-probe-temp')]
code="import sys,hashlib;from pathlib import Path;sys.path.insert(0,"+repr(str(stage))+");import qupathxx as q;from qupathxx import _native;assert q.build_environment().git_revision=="+repr(source)+";print('staged source',q.build_environment().git_revision,'native sha256',hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),flush=True);import pytest;sys.exit(pytest.main("+repr(args)+"))"
argv=[str(r/'build/ci/object-edit-state-r5-env/Scripts/python.exe'),'-I','-X','utf8','-c',code]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0'
with (o/'final-probe.log').open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/'final-probe-command.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,source=source,product_installed=False),indent=2)+'\n',encoding='utf-8')
print('Final staged plane probe',result.returncode,flush=True)
raise SystemExit(result.returncode)
