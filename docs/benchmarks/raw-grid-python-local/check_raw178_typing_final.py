from pathlib import Path
import json,os,subprocess,sys
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159';out=r/'build/raw178-typing-final';out.mkdir(exist_ok=False)
env=os.environ.copy();env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8';env['MYPYPATH']=str(r/'build/raw178-python-isolated')
results=[]
for name,expected in [('raw_grid_valid',0),('raw_grid_invalid',1)]:
 cmd=[sys.executable,'-I','-m','mypy','--strict','--no-incremental',str(root/'tests/typing'/(name+'.py'))]
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 results.append({'name':name,'command':cmd,'exit':p.returncode,'expected':expected});assert p.returncode==expected,(out/(name+'.log')).read_text(encoding='utf-8')
(out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'));print('Positive and negative raw-grid typing checks passed')
