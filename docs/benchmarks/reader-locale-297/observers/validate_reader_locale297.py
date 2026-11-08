from pathlib import Path
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/reader-locale-297'
python=root/'build/ci/reader-locale-297-env/Scripts/python.exe'
mode=sys.argv[1];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0'
if mode=='install':
    assert not python.exists()
    subprocess.run([str(root/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
    (python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(root/'.venv/Lib/site-packages')+'\n')
    wheel,=list((out/'initial-wheel').glob('*.whl'))
    args=[str(python),'-I','-m','pip','install','--no-index','--no-deps',str(wheel)]
elif mode=='native':
    args=[str(root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(root/'build/ci/gridfrontends167-package'),'-C','Release','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native.xml')]
elif mode=='regression':
    args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_reader_locale.py','tests/test_nominal_cell_batches.py','-q','--junitxml='+str(out/'regression.xml')]
elif mode=='scientific':
    cases=json.loads((work/'tools/ci/scientific-grid-cases.json').read_text())
    args=[str(python),'-I','-X','utf8','-m','pytest',*['tests/'+n+'.py' for n in cases],'-q','--junitxml='+str(out/'scientific.xml')]
elif mode=='preflight':
    prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text())
    tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')]
    assert len(tests)==20
    args=[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/'preflight.xml')]
else: raise ValueError(mode)
with (out/(mode+'.log')).open('x',encoding='utf-8') as stream:
    result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,cwd=str(work),returncode=result.returncode),indent=2)+'\n')
if mode=='native':
    shutil.copyfile(root/'build/ci/gridfrontends167-package/Testing/Temporary/LastTest.log',out/'native-complete.log')
print(mode,result.returncode,flush=True)
assert result.returncode==0
