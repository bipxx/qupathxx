from pathlib import Path
import json
import os
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';module='test_qupath_response_reference'
args=[str(root/'build/ci/cell-policy302-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest','tests/'+module+'.py','--collect-only','-q','--noconftest','--color=no','-o','addopts=']
env=os.environ.copy();env['PYTEST_DISABLE_PLUGIN_AUTOLOAD']='1'
r=subprocess.run(args,cwd=work,env=env,capture_output=True,text=True,encoding='utf-8');assert r.returncode==0
(out/'collection.log').write_text(r.stdout+r.stderr,encoding='utf-8')
names=[line.split('::',1)[1] for line in r.stdout.splitlines() if module+'.py::' in line];assert len(names)==len(set(names))==38
path=work/'tools/ci/scientific-grid-cases.json';cases=json.loads(path.read_text(encoding='utf-8'));assert module not in cases;cases[module]=sorted(names)
path.write_text(json.dumps(cases,indent=2)+'\n',encoding='utf-8');print('Registered38 independent response-reference cases')
