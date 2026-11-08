from pathlib import Path
import importlib.util
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-sdk-289'
python=root/'build/ci/rgb-nominal-sdk-289-env/Scripts/python.exe'
spec=importlib.util.spec_from_file_location('science',work/'tools/ci/scientific_grid_evidence.py')
science=importlib.util.module_from_spec(spec)
spec.loader.exec_module(science)
mode=sys.argv[1]
if mode=='typing':
    args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental',*science.POSITIVE]
elif mode in ('scientific','preflight'):
    if mode=='scientific':tests=['tests/'+name+'.py' for name in science.CASES]
    else:
        previous=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text())
        tests=[v.replace('\\','/') for v in previous['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')]
        assert len(tests)==20
    args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q',
        '--junitxml='+str(out/(mode+'-final.xml')),'--basetemp',str(out/(mode+'-final-temp'))]
else:raise ValueError(mode)
log=out/(mode+'-final.log');assert not log.exists()
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
with log.open('w',encoding='utf-8') as stream:
    result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-final-command.json')).write_text(json.dumps(dict(argv=args,cwd=str(work),returncode=result.returncode),indent=2)+'\n')
print(mode,'final exit',result.returncode,flush=True)
assert result.returncode==0
if mode=='scientific':
    admitted=science.python(ET.parse(out/'scientific-final.xml').findall('.//testcase'),gui=False)
    (out/'scientific-final-admission.json').write_text(json.dumps(admitted,indent=2)+'\n')
elif mode=='typing':
    admitted=dict(positive=science.positive_typing(log),negative={name:science.negative_typing(out/name,*v) for name,v in science.NEGATIVE.items()})
    (out/'scientific-typing-final-admission.json').write_text(json.dumps(admitted,indent=2)+'\n')
