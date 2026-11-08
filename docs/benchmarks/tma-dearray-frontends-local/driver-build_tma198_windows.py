from pathlib import Path
import json,os,subprocess,time
r=Path(__file__).resolve().parents[2];out=r/'build/tma198-windows';out.mkdir(exist_ok=False)
cmake=r/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=r/'build/ci/gridfrontends167-package'
env=os.environ.copy();env['PATH']=str(r/'.venv/Scripts')+os.pathsep+env['PATH']
commands=[('configure',[str(cmake),'-S',str(r/'build/ci/scientific-grid-159'),'-B',str(build),'-DQUPATHXX_BUILD_TIFF=ON']),('build',[str(cmake),'--build',str(build),'--config','Release','--target','_native','qupathxx_region','qupathxx_batch','qupathxx_tma_tests','qupathxx_tma_dearray_tests','qupathxx_viewer_session_tests','qupathxx_qptma_tests','qupathxx_qptma_project_tests','--parallel','2'])]
for suite in ['tma','tma_dearray','viewer_session','qptma','qptma_project']:
 commands.append(('test-'+suite,[str(cmake.with_name('ctest.exe')),'--test-dir',str(build),'-C','Release','-R','^'+suite+'$','--output-on-failure','--test-output-size-passed','1048576','--no-tests=error','--output-junit',str(out/(suite+'.xml'))]))
results=[]
failed=False
for name,cmd in commands:
 print(name,flush=True);start=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=2400)
 results.append({'name':name,'command':cmd,'exit':p.returncode,'elapsed_seconds':time.perf_counter()-start});(out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:
  failed=True;print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-14000:])
  if name in ('configure','build'):raise SystemExit(p.returncode)
raise SystemExit(1 if failed else 0)
