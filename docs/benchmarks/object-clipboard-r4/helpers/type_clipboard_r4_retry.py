from pathlib import Path
import json,os,subprocess,time
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-clipboard-r4';python=r/'build/ci/object-clipboard-r4-env/Scripts/python.exe';env=os.environ.copy()
env.pop('MYPYPATH',None)
commands={
 'typing':[str(python),'-I','-m','mypy','--no-incremental','--follow-imports=silent','--strict','tests/typing/object_clipboard_valid.py'],
 'typing-negative':[str(python),'-I','-m','mypy','--no-incremental','--follow-imports=silent','--strict','tests/typing/object_clipboard_invalid.py']}
results={}
for name,argv in commands.items():
 start=time.perf_counter()
 with (o/(name+'-retry.log')).open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
 results[name]=dict(argv=argv,returncode=result.returncode,elapsed_seconds=time.perf_counter()-start)
(o/'typing-retry-commands.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
assert results['typing']['returncode']==0,results
text=(o/'typing-negative-retry.log').read_text(encoding='utf-8')
assert results['typing-negative']['returncode']==1 and text.count(': error:')==4,text
assert all(code in text for code in ('[call-arg]','[list-item]','[arg-type]')),text
print('Installed positive/negative typing and packaged stub contracts passed.',flush=True)
