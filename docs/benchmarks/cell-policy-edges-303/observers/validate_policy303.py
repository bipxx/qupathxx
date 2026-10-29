from pathlib import Path
import json
import os
import subprocess
import sys
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303';mode=sys.argv[1]
python=root/'build/ci/cell-policy302-env/Scripts/python.exe';env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
if mode=='scientific':tests=['tests/'+name+'.py' for name in json.loads((work/'tools/ci/scientific-grid-cases.json').read_text(encoding='utf-8'))]
elif mode=='preflight':
    prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text(encoding='utf-8'))
    tests=[value.replace('\\','/') for value in prior['argv'] if value.replace('\\','/').startswith('tests/') and value.endswith('.py')];assert len(tests)==20
else:raise ValueError(mode)
args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/(mode+'.xml'))]
with (out/(mode+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=r.returncode),indent=2)+'\n',encoding='utf-8')
print(mode,r.returncode,flush=True);assert r.returncode==0,out/(mode+'.log')
