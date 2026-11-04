from pathlib import Path
import json,os,subprocess,time
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/root-facts-r5';python=r/'build/ci/root-facts-r5-env/Scripts/python.exe';env=os.environ.copy()
env.pop('MYPYPATH',None)
commands={
 'typing':[str(python),'-I','-m','mypy','--no-incremental','--follow-imports=silent','--strict','tests/typing/image_properties_valid.py'],
 'typing-negative':[str(python),'-I','-m','mypy','--no-incremental','--follow-imports=silent','--strict','tests/typing/image_properties_invalid.py'],
 'public-stubs':[str(python),'-I','-X','utf8','-m','pytest','tests/test_public_stubs.py','-q','--junitxml='+str(o/'public-stubs.xml'),'--basetemp',str(o/'public-stubs-temp')]}
results={}
for name,argv in commands.items():
 start=time.perf_counter()
 with (o/(name+'.log')).open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
 results[name]=dict(argv=argv,returncode=result.returncode,elapsed_seconds=time.perf_counter()-start)
(o/'typing-commands.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
assert results['typing']['returncode']==results['public-stubs']['returncode']==0,results
text=(o/'typing-negative.log').read_text(encoding='utf-8')
assert results['typing-negative']['returncode']==1 and text.count(': error:')==3,text
assert all(code in text for code in ('[arg-type]','[assignment]','[misc]')),text
print('Installed positive/negative typing and packaged stub contracts passed.',flush=True)
