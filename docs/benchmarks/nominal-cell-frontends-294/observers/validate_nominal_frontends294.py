from pathlib import Path
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-frontends-294'
python=root/'build/ci/nominal-cell-frontends-294-env/Scripts/python.exe'
mode=sys.argv[1];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
spec=importlib.util.spec_from_file_location('science',work/'tools/ci/scientific_grid_evidence.py');science=importlib.util.module_from_spec(spec);spec.loader.exec_module(science)
if mode=='install':
    assert not python.exists();subprocess.run([str(root/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
    (python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(root/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
    wheel,=list((out/'initial-wheel').glob('*.whl'));args=[str(python),'-I','-m','pip','install','--no-deps','--force-reinstall',str(wheel)]
elif mode=='install-final':
    wheel,=list((out/'final-wheel').glob('*.whl'));args=[str(python),'-I','-m','pip','install','--no-deps','--force-reinstall',str(wheel)]
elif mode=='native':
    args=[str(root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(root/'build/ci/gridfrontends167-package'),'-C','Release',
      '--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native.xml')]
elif mode in ('generate-stubs','stubs-check'):
    args=[str(python),'-I','-X','utf8','tools/generate_stubs.py']+(['--check'] if mode=='stubs-check' else [])
elif mode=='api':
    args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_nominal_cell_frontends.py','-q','--junitxml='+str(out/'api.xml'),'--basetemp',str(out/'api-temp')]
elif mode=='desktop-repair':
    env['QUPATHXX_TEST_GUI']='1'
    args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_nominal_cell_frontends.py::test_nominal_desktop_batch_uses_existing_checkpoints_and_resumes','-q','--junitxml='+str(out/'desktop-repair.xml'),'--basetemp',str(out/'desktop-repair-temp')]
elif mode=='desktop':
    env['QUPATHXX_TEST_GUI']='1'
    args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_nominal_cell_frontends.py','tests/test_grid_cell_frontends.py','tests/test_gui_batch.py','tests/test_viewer_session.py','-q','--junitxml='+str(out/'desktop.xml'),'--basetemp',str(out/'desktop-temp')]
elif mode=='scientific':
    args=[str(python),'-I','-X','utf8','-m','pytest',*['tests/'+n+'.py' for n in science.CASES],'-q','--junitxml='+str(out/'scientific.xml'),'--basetemp',str(out/'scientific-temp')]
elif mode=='preflight':
    prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text(encoding='utf-8'))
    tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')];assert len(tests)==20
    args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/'preflight.xml'),'--basetemp',str(out/'preflight-temp')]
elif mode=='typing':args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental',*science.POSITIVE]
else:raise ValueError(mode)
log=out/(mode+'.log');assert not log.exists()
with log.open('w',encoding='utf-8') as stream:result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
if mode=='native':shutil.copyfile(root/'build/ci/gridfrontends167-package/Testing/Temporary/LastTest.log',out/'native-complete.log')
print(mode,result.returncode,flush=True);assert result.returncode==0,log
if mode=='scientific':
    (out/'scientific-admission.json').write_text(json.dumps(science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False),indent=2)+'\n',encoding='utf-8')
if mode=='typing':
    results={}
    for name,(filename,lines) in science.NEGATIVE.items():
        target=out/name;assert not target.exists()
        args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental','tests/typing/'+filename]
        with target.open('w',encoding='utf-8') as stream:r=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
        assert r.returncode==1,target;results[name]=science.negative_typing(target,filename,lines)
    (out/'typing-admission.json').write_text(json.dumps(dict(positive=science.positive_typing(log),negative=results),indent=2)+'\n',encoding='utf-8')
