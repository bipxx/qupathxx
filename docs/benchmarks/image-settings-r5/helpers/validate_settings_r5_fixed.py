from pathlib import Path
import json,os,subprocess,sys,time
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/image-settings-r5';python=r/'build/ci/image-settings-r5-probe-env/Scripts/python.exe';mode=sys.argv[1];env=os.environ.copy()
if mode=='desktop':
 env['QUPATHXX_TEST_GUI']='1';argv=[str(python),'-I','-X','utf8','-m','pytest','tests/test_image_analysis_settings.py','-k','desktop','-q','--junitxml='+str(o/'desktop-fixed.xml'),'--basetemp',str(o/'desktop-fixed-temp')]
elif mode=='native':
 argv=[str(r/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(r/'build/ci/gridfrontends167-package'),'-C','Release','-R','^(document_edits|analysis|stains|project|project_save|qupath_project)$','--output-on-failure','--output-junit',str(o/'native.xml')]
else:raise ValueError(mode)
start=time.perf_counter()
with (o/(mode+'-fixed.log')).open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/(mode+'-fixed-command.json')).write_text(json.dumps(dict(argv=argv,returncode=result.returncode,elapsed_seconds=time.perf_counter()-start),indent=2),encoding='utf-8')
print(mode,result.returncode,flush=True);raise SystemExit(result.returncode)
