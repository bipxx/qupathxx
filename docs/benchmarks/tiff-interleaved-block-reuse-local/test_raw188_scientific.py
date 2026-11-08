from pathlib import Path
import json,os,subprocess,sys
root=Path.cwd();source=root/'build/ci/scientific-grid-159';out=root/'build/raw188-scientific';out.mkdir(exist_ok=False)
cases=json.loads((source/'tools/ci/scientific-grid-cases.json').read_text(encoding='utf-8'));env=os.environ.copy();env['QUPATHXX_TEST_GUI']='1';env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8'
command=[str(root/'build/ci/raw188-python-env/Scripts/python.exe'),'-I','-m','pytest',*[str(source/'tests'/(m+'.py')) for m in cases],'-q','--tb=short','--junitxml='+str(out/'tests.xml'),'--basetemp='+str(out/'tmp')]
with (out/'tests.log').open('wb') as log:p=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
(out/'commands.json').write_bytes((json.dumps(dict(command=command,exit=p.returncode),indent=2)+'\n').encode());print((out/'tests.log').read_text(encoding='utf-8',errors='replace'));sys.exit(p.returncode)
