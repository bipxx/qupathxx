from pathlib import Path
import json,os,subprocess,time,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/root-facts-r5'
assert json.loads((o/'build-store-exit.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel-store').glob('*.whl'));stage=o/'probe-stage';assert stage.is_dir()
# The failed candidate wheel is retained; only its redundant extraction is reused.
assert stage.is_dir()
source=json.loads((o/'compiler-inputs-store.json').read_bytes())['source']
args=['tests/test_pixel_stream_record_benchmark.py::test_pixel_stream_record_benchmark_accepts_current_project_schema','-q','--junitxml='+str(o/'staged-report-fixed.xml'),'--basetemp',str(o/'staged-report-fixed-temp')]
code="import sys;sys.path.insert(0,"+repr(str(stage))+");import qupathxx as q;assert q.build_environment().git_revision=="+repr(source)+";assert q.has_project and q.has_interchange and q.has_tiff;print('native source',q.build_environment().git_revision,flush=True);import pytest;sys.exit(pytest.main("+repr(args)+"))"
argv=[str(r/'build/ci/root-facts-r5-probe-env/Scripts/python.exe'),'-I','-X','utf8','-c',code];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';start=time.perf_counter()
with (o/'staged-report-fixed.log').open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/'staged-report-fixed-command.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,source=source,product_installed=False,elapsed_seconds=time.perf_counter()-start),indent=2),encoding='utf-8')
print('New and older affected staged contracts',result.returncode,flush=True);raise SystemExit(result.returncode)
