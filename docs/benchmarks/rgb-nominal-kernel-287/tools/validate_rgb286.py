from pathlib import Path
import importlib.util
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build'/(sys.argv[2] if len(sys.argv)>2 else 'qupath-rgb-resampling-286')
python=root/'build/ci/gui-font-delivery-283-env/Scripts/python.exe'
mode=sys.argv[1]
assert mode in ('scientific','preflight')
if mode=='scientific':
    spec=importlib.util.spec_from_file_location('scientific',work/'tools/ci/scientific_grid_evidence.py')
    science=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(science)
    tests=['tests/'+name+'.py' for name in science.CASES]
else:
    old=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text())
    tests=[v.replace('\\','/') for v in old['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')]
    assert len(tests)==20
args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q',
    '--junitxml='+str(out/(mode+'.xml')),'--basetemp',str(out/(mode+'-temp'))]
env=os.environ.copy()
env['QUPATHXX_TEST_GUI']='0'
assert not (out/(mode+'.log')).exists()
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,cwd=str(work),
    runtime_source='7231d95c25fbc9515d937036986e8c92b19da261',gui_capability=True,gui_opt_in=False),indent=2)+'\n')
with (out/(mode+'.log')).open('w',encoding='utf-8') as log:
    result=subprocess.run(args,cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT)
(out/(mode+'-exit.json')).write_text(json.dumps(dict(returncode=result.returncode),indent=2)+'\n')
print(mode,'exit',result.returncode,flush=True)
assert result.returncode==0
if mode=='scientific':
    admitted=science.python(ET.parse(out/(mode+'.xml')).findall('.//testcase'),gui=False)
    (out/'scientific-admission.json').write_text(json.dumps(admitted,indent=2)+'\n')
    print('Exact scientific admission',admitted,flush=True)
