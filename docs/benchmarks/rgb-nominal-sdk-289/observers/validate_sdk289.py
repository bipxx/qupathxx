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
mode=sys.argv[1]
spec=importlib.util.spec_from_file_location('science',work/'tools/ci/scientific_grid_evidence.py')
science=importlib.util.module_from_spec(spec)
spec.loader.exec_module(science)
def run(label,args,expected=0):
    log=out/(label+'.log')
    assert not log.exists()
    env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
    (out/(label+'-command.json')).write_text(json.dumps(dict(argv=args,cwd=str(work),returncode=result.returncode),indent=2)+'\n')
    print(label,result.returncode,flush=True)
    assert result.returncode==expected,log
if mode=='typing':
    run('typing',[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental',*science.POSITIVE])
    for log,(filename,lines) in science.NEGATIVE.items():
        run(Path(log).stem,[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental','tests/typing/'+filename],1)
    (out/'scientific-typing-admission.json').write_text(json.dumps(science.typing(out),indent=2)+'\n')
elif mode in ('scientific','preflight','legacy-resampling'):
    if mode=='scientific':tests=['tests/'+n+'.py' for n in science.CASES]
    elif mode=='legacy-resampling':tests=['tests/test_resampling.py','tests/test_qupath_cells_reference.py']
    else:
        old=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text())
        tests=[v.replace('\\','/') for v in old['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')]
        assert len(tests)==20
    run(mode,[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q',
        '--junitxml='+str(out/(mode+'.xml')),'--basetemp',str(out/(mode+'-temp'))])
    if mode=='scientific':
        admitted=science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
        (out/'scientific-admission.json').write_text(json.dumps(admitted,indent=2)+'\n')
        print('Named scientific cohort',admitted,flush=True)
else:raise ValueError(mode)
