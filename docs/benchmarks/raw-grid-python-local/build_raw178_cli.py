from pathlib import Path
import subprocess,time,json
r=Path(__file__).resolve().parents[2];out=r/'build/raw178-cli';out.mkdir(exist_ok=False)
cmd=[str(r/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe'),'--build',str(r/'build/ci/gridfrontends167-package'),'--config','Release','--target','qupathxx_batch','--parallel','2']
start=time.perf_counter()
with (out/'build.log').open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=600)
(out/'commands.json').write_bytes((json.dumps({'command':cmd,'exit':p.returncode,'elapsed_seconds':time.perf_counter()-start},indent=2)+'\n').encode('utf-8'))
print('CLI build exit',p.returncode);raise SystemExit(p.returncode)
