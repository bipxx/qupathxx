from pathlib import Path
import json
import os
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-305'
prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text(encoding='utf-8'))
tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')];assert len(tests)==20
args=[str(root/'build/ci/original305-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/'preflight-final.xml'),'--basetemp',str(out/'preflight-final-temp')]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
with (out/'preflight-final.log').open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
with (out/'preflight-final-command.json').open('x',encoding='utf-8') as stream:json.dump(dict(argv=args,returncode=r.returncode),stream,indent=2);stream.write('\n')
print('Final complete preflight',r.returncode,flush=True);raise SystemExit(r.returncode)
