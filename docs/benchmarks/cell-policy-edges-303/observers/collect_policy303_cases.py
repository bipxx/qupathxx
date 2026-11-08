from pathlib import Path
import json
import os
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303'
python=root/'build/ci/cell-policy302-env/Scripts/python.exe';module='test_cell_policy_edges'
args=[str(python),'-I','-X','utf8','-m','pytest','tests/'+module+'.py','--collect-only','-q','--noconftest','--color=no','-o','addopts=']
env=os.environ.copy();env['PYTEST_DISABLE_PLUGIN_AUTOLOAD']='1'
result=subprocess.run(args,cwd=work,env=env,capture_output=True,text=True,encoding='utf-8')
(out/'collection.log').write_text(result.stdout+result.stderr,encoding='utf-8');assert result.returncode==0
names=[line.split('::',1)[1] for line in result.stdout.splitlines() if module+'.py::' in line]
assert len(names)==len(set(names))==32
manifest=work/'tools/ci/scientific-grid-cases.json';cases=json.loads(manifest.read_text(encoding='utf-8'));assert module not in cases
cases[module]=sorted(names);manifest.write_text(json.dumps(cases,indent=2)+'\n',encoding='utf-8')
print('Registered32 held-out characterization and refusal cases')
