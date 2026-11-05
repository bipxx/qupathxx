from pathlib import Path
import json,os,subprocess,time,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/image-data-properties-r5'
assert json.loads((o/'build-exit.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel').glob('*.whl'))
with zipfile.ZipFile(wheel) as z:z.extractall(o/'stage')
source=json.loads((o/'compiler-inputs.json').read_bytes())['source']
args=json.loads((o/'affected-selection.json').read_bytes())
python=r/'build/ci/image-data-properties-r5-probe-env/Scripts/python.exe'
code="import qupathxx as q;assert q.build_environment().git_revision=="+repr(source)+";assert q.has_project and q.has_interchange and q.has_tiff;print('native source',q.build_environment().git_revision,flush=True);import pytest;raise SystemExit(pytest.main("+repr(args+['-q','--junitxml='+str(o/'staged.xml'),'--basetemp',str(o/'staged-temp')])+"))"
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';start=time.perf_counter();argv=[str(python),'-I','-X','utf8','-c',code]
with (o/'staged.log').open('x',encoding='utf-8') as log:
 try:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240);status=result.returncode
 except subprocess.TimeoutExpired:status=124
(o/'staged-command.json').write_text(json.dumps(dict(argv=argv,returncode=status,source=source,product_installed=False,elapsed_seconds=time.perf_counter()-start),indent=2),encoding='utf-8')
print('Staged affected contracts',status,flush=True);raise SystemExit(status)
