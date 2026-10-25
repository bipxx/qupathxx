from pathlib import Path
import subprocess,time,json
r=Path(__file__).resolve().parents[2];out=r/'build/raw173-mean-windows';out.mkdir(exist_ok=False)
cmake=r/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';ctest=cmake.with_name('ctest.exe');build=r/'build/ci/raw173-mean-windows'
commands=[('configure',[str(cmake),'-S',str(r/'build/ci/raw173-mean-source'),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']),('build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2']),('tests',[str(ctest),'--test-dir',str(build),'-C','Release','--output-on-failure','--no-tests=error','--output-junit',str(out/'tests.xml')])]
results=[]
for name,cmd in commands:
 print(name,flush=True);started=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 results.append(dict(name=name,command=cmd,exit=p.returncode,elapsed_seconds=time.perf_counter()-started))
 (out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:
  print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-8000:]);raise SystemExit(p.returncode)
