from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/imagej-gaussian-kernel-300'
python=root/'build/ci/reader-locale-297-env/Scripts/python.exe';mode=sys.argv[1]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
if mode=='native':
    cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';ctest=cmake.with_name('ctest.exe');build=root/'build/ci/imagej-gaussian-300-msvc'
    commands=[('windows-final-configure',[str(cmake),'-S',str(work),'-B',str(build),'-G','Visual Studio 17 2022',
        *['-DQUPATHXX_BUILD_'+name+'=OFF' for name in ('PYTHON','GUI','OPENSLIDE','TIFF','GEOS','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING')],'-DQUPATHXX_BUILD_TESTS=ON']),
        ('windows-final-build',[str(cmake),'--build',str(build),'--config','Release','--target','qupathxx_imagej_gaussian_tests','--parallel','2']),
        ('windows-final-native',[str(ctest),'--test-dir',str(build),'-C','Release','-R','^imagej_gaussian$','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'windows-final-native.xml')])]
else:
    if mode=='scientific':tests=['tests/'+name+'.py' for name in json.loads((work/'tools/ci/scientific-grid-cases.json').read_text())]
    elif mode=='preflight':
        prior=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text());tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')];assert len(tests)==20
    else:raise ValueError(mode)
    commands=[(mode,[str(python),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/(mode+'.xml'))])]
results=[]
for name,args in commands:
    with (out/(name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=name,argv=args,returncode=r.returncode));(out/(mode+'-command-results.json')).write_text(json.dumps(results,indent=2)+'\n')
    print(name,r.returncode,flush=True);assert r.returncode==0
if mode=='native':
    shutil.copyfile(build/'Testing/Temporary/LastTest.log',out/'windows-final-native-complete.log')
    folder=out/'windows-products';folder.mkdir(exist_ok=False);products=[]
    for p in [build/'Release/qupathxx_imagej_gaussian_tests.exe',*sorted((build/'qupathxx_imagej_gaussian_tests.dir/Release').glob('*.obj'))]:
        target=folder/p.name;shutil.copyfile(p,target);assert p.read_bytes()==target.read_bytes()
        products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    (out/'windows-native-products.json').write_text(json.dumps(products,indent=2)+'\n')
