from pathlib import Path
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-processing-policy-302'
python=root/'build/ci/cell-policy302-env/Scripts/python.exe'
mode=sys.argv[1];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
spec=importlib.util.spec_from_file_location('science',work/'tools/ci/scientific_grid_evidence.py');science=importlib.util.module_from_spec(spec);spec.loader.exec_module(science)
if mode=='install':
    assert not python.exists();subprocess.run([str(root/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
    (python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(root/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
    wheel,=list((out/'initial-wheel').glob('*.whl'));args=[str(python),'-I','-m','pip','install','--no-index','--no-deps',str(wheel)]
elif mode=='install-final':
    wheel,=list((out/'final-wheel').glob('*.whl'));args=[str(python),'-I','-m','pip','install','--no-index','--no-deps','--force-reinstall',str(wheel)]
elif mode=='native':
    args=[str(root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(root/'build/ci/gridfrontends167-package'),'-C','Release','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native.xml')]
elif mode in ('generate-stubs','stubs-check'):
    args=[str(python),'-I','-X','utf8','tools/generate_stubs.py']+(['--check'] if mode=='stubs-check' else [])
elif mode in ('policy','frontends','scientific','preflight'):
    if mode=='policy':tests=['tests/test_imagej_cell_processing.py','tests/test_nominal_cell_python.py','tests/test_nominal_cell_batches.py','tests/test_qupath_nominal_cells_reference.py']
    elif mode=='frontends':
        env['QUPATHXX_TEST_GUI']='1';tests=['tests/test_nominal_cell_frontends.py']
    elif mode=='scientific':tests=['tests/'+n+'.py' for n in science.CASES]
    else:
        prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text())
        tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')];assert len(tests)==20
    args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/(mode+'.xml')),'--basetemp',str(out/(mode+'-temp'))]
elif mode=='typing':args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental',*science.POSITIVE]
else:raise ValueError(mode)
with (out/(mode+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=r.returncode),indent=2)+'\n')
if mode=='native':shutil.copyfile(root/'build/ci/gridfrontends167-package/Testing/Temporary/LastTest.log',out/'native-complete.log')
print(mode,r.returncode,flush=True);assert r.returncode==0,out/(mode+'.log')
if mode=='scientific':
    (out/'scientific-admission.json').write_text(json.dumps(science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False),indent=2)+'\n')
if mode=='typing':
    results={}
    for name,(filename,lines) in science.NEGATIVE.items():
        target=out/name
        args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental','tests/typing/'+filename]
        with target.open('x',encoding='utf-8') as stream:result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
        assert result.returncode==1,target;results[name]=science.negative_typing(target,filename,lines)
    (out/'typing-admission.json').write_text(json.dumps(dict(positive=science.positive_typing(out/'typing.log'),negative=results),indent=2)+'\n')
