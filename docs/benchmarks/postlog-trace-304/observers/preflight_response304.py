from pathlib import Path
import subprocess
import json
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';build=root/'build/ci/qupath-response304-msvc'
assert not build.exists();cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe'
commands=[('unit-configure',[str(cmake),'-S',str(work),'-B',str(build),'-G','Visual Studio 17 2022',*['-DQUPATHXX_BUILD_'+name+'=OFF' for name in ('PYTHON','GUI','OPENSLIDE','TIFF','GEOS','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING')],'-DQUPATHXX_BUILD_TESTS=ON']),
 ('unit-build',[str(cmake),'--build',str(build),'--config','Release','--target','qupathxx_qupath_response_tests','--parallel','2']),
 ('unit-native',[str(cmake.with_name('ctest.exe')),'--test-dir',str(build),'-C','Release','-R','^qupath_response$','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'unit-native.xml')])]
results=[]
for name,args in commands:
    with (out/(name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=name,argv=args,returncode=r.returncode));(out/'unit-commands.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
    print(name,r.returncode,flush=True);assert r.returncode==0
