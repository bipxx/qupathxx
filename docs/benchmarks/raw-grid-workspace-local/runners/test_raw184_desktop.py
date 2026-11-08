from pathlib import Path
import os,json,subprocess,sys
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159';out=r/'build/raw184-desktop';out.mkdir(exist_ok=False)
python=r/'build/ci/raw183-python-env/Scripts/python.exe';env=os.environ.copy();env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8';env['QUPATHXX_TEST_GUI']='1'
cmd=[str(python),'-I','-m','pytest',str(root/'tests/test_raw_grid_frontends.py'),str(root/'tests/test_gui_raw.py'),str(root/'tests/test_gui_raw_records.py'),str(root/'tests/test_gui_batch.py'),'-q','--junitxml='+str(out/'desktop-tests.xml'),'--basetemp='+str(out/'tmp')]
with (out/'tests.log').open('wb') as log:p=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
(out/'commands.json').write_bytes((json.dumps(dict(command=cmd,exit=p.returncode),indent=2)+'\n').encode('utf-8'));print((out/'tests.log').read_text(encoding='utf-8',errors='replace'));sys.exit(p.returncode)
