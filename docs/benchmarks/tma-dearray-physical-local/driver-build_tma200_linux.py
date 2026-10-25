from pathlib import Path
import subprocess,time,json
r=Path(__file__).resolve().parents[2];out=r/'build/tma200-linux';out.mkdir(exist_ok=False)
base='/mnt/c/Users/HEQ/Projects/qupathxx';build=base+'/build/ci/raw176-tiff-linux-native';source=base+'/build/ci/scientific-grid-159'
env=['env','GIT_DIR='+base+'/.git/worktrees/scientific-grid-159','GIT_WORK_TREE='+source,'GIT_CONFIG_COUNT=1','GIT_CONFIG_KEY_0=core.autocrlf','GIT_CONFIG_VALUE_0=true']
commands=[('configure',['cmake','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release','-DQUPATHXX_BUILD_PYTHON=OFF','-DQUPATHXX_BUILD_GUI=OFF','-DQUPATHXX_BUILD_TESTS=ON','-DQUPATHXX_BUILD_PROJECT=ON','-DQUPATHXX_BUILD_GEOS=OFF','-DQUPATHXX_BUILD_TIFF=ON','-DQUPATHXX_BUILD_OPENSLIDE=OFF','-DQUPATHXX_BUILD_CLASSIFIER_TRAINING=OFF']),('build',['cmake','--build',build,'--target','qupathxx_tma_tests','qupathxx_tma_dearray_tests','qupathxx_viewer_session_tests','qupathxx_qptma_tests','qupathxx_qptma_project_tests','--parallel','2']),('tests',['ctest','--test-dir',build,'-R','^(tma|tma_dearray|viewer_session|qptma|qptma_project)$','--output-on-failure','--test-output-size-passed','1048576','--no-tests=error','--output-junit',base+'/build/tma200-linux/tests.xml'])]
results=[]
failed=False
for name,args in commands:
 cmd=['wsl.exe','-d','Ubuntu','--exec',*env,*args];print('Linux',name,flush=True);start=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
 results.append(dict(name=name,command=cmd,exit=p.returncode,elapsed_seconds=time.perf_counter()-start));(out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:
  failed=True
  print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-14000:])
  if name in ('configure','build'):raise SystemExit(p.returncode)
raise SystemExit(1 if failed else 0)
