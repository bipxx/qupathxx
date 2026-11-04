from pathlib import Path
import json,os,subprocess,sys,time
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-clipboard-r4';python=r/'build/ci/object-clipboard-r4-env/Scripts/python.exe';mode=sys.argv[1];env=os.environ.copy()
if mode=='desktop':
 env['QUPATHXX_TEST_GUI']='1';argv=[str(python),'-I','-X','utf8','-m','pytest','tests/test_object_clipboard.py','-k','desktop','-q','--junitxml='+str(o/'desktop-retry.xml'),'--basetemp',str(o/'desktop-retry-temp')]
elif mode=='native':
 argv=[str(r/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(r/'build/ci/gridfrontends167-package'),'-C','Release','-R','^(interchange|document_edits|selection|geometry_edits|project)$','--output-on-failure','--output-junit',str(o/'native.xml')]
else:raise ValueError(mode)
start=time.perf_counter()
with (o/(mode+'-retry.log')).open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/(mode+'-retry-command.json')).write_text(json.dumps(dict(argv=argv,returncode=result.returncode,elapsed_seconds=time.perf_counter()-start),indent=2),encoding='utf-8')
print(mode,result.returncode,flush=True);raise SystemExit(result.returncode)
