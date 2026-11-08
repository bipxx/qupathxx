from pathlib import Path
import importlib.util,json,os,subprocess,sys
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159';out=r/'build/raw185-scientific-final';out.mkdir(exist_ok=False)
spec=importlib.util.spec_from_file_location('evidence',root/'tools/ci/scientific_grid_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
python=r/'build/ci/raw183-python-env/Scripts/python.exe';env=os.environ.copy();env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8';env['QUPATHXX_TEST_GUI']='1'
cmd=[str(python),'-I','-m','pytest',*[str(root/'tests'/(name+'.py')) for name in m.CASES],'-q','--junitxml='+str(out/'scientific-tests.xml'),'--basetemp='+str(out/'tmp')]
with (out/'tests.log').open('wb') as log:p=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
(out/'commands.json').write_bytes((json.dumps(dict(command=cmd,exit=p.returncode),indent=2)+'\n').encode('utf-8'));print((out/'tests.log').read_text(encoding='utf-8',errors='replace'));sys.exit(p.returncode)
