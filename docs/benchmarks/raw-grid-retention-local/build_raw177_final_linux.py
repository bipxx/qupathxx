from pathlib import Path
import subprocess,time,json
r=Path(__file__).resolve().parents[2];out=r/'build/raw177-final-linux';out.mkdir(exist_ok=False)
base='/mnt/c/Users/HEQ/Projects/qupathxx';build=base+'/build/ci/raw174-linux-native';source=base+'/build/ci/scientific-grid-159'
env=['env','GIT_DIR='+base+'/.git/worktrees/scientific-grid-159','GIT_WORK_TREE='+source,'GIT_CONFIG_COUNT=1','GIT_CONFIG_KEY_0=core.autocrlf','GIT_CONFIG_VALUE_0=true']
commands=[('configure',['cmake','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release','-DQUPATHXX_BUILD_PYTHON=OFF','-DQUPATHXX_BUILD_GUI=OFF','-DQUPATHXX_BUILD_TESTS=ON','-DQUPATHXX_BUILD_PROJECT=OFF','-DQUPATHXX_BUILD_GEOS=OFF','-DQUPATHXX_BUILD_TIFF=OFF','-DQUPATHXX_BUILD_OPENSLIDE=OFF','-DQUPATHXX_BUILD_CLASSIFIER_TRAINING=OFF']),('build',['cmake','--build',build,'--target','qupathxx_resampling_tests','--parallel','2']),('tests',['ctest','--test-dir',build,'-R','^resampling$','--output-on-failure','--test-output-size-passed','1048576','--no-tests=error','--output-junit',base+'/build/raw177-final-linux/native-tests.xml'])]
results=[]
for name,args in commands:
 cmd=['wsl.exe','-d','Ubuntu','--',*env,*args];print('Linux',name,flush=True);start=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
 results.append(dict(name=name,command=cmd,exit=p.returncode,elapsed_seconds=time.perf_counter()-start));(out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-14000:]);raise SystemExit(p.returncode)
