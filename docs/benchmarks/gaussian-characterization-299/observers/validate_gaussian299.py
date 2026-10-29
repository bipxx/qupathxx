from pathlib import Path
import json
import os
import subprocess
import sys
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/gaussian-characterization-299'
python=root/'build/ci/reader-locale-297-env/Scripts/python.exe';mode=sys.argv[1]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
if mode=='scientific':
    cases=json.loads((work/'tools/ci/scientific-grid-cases.json').read_text())
    tests=['tests/'+name+'.py' for name in cases]
elif mode=='preflight':
    prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text())
    tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')];assert len(tests)==20
else:raise ValueError(mode)
args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/(mode+'.xml'))]
with (out/(mode+'.log')).open('x',encoding='utf-8') as stream:result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
print(mode,result.returncode,flush=True);assert result.returncode==0
