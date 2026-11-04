from pathlib import Path
import json,os,subprocess,time,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/root-facts-r5'
assert json.loads((o/'build-exit.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel').glob('*.whl'));stage=o/'probe-stage';stage.mkdir()
with zipfile.ZipFile(wheel) as archive:archive.extractall(stage)
source=json.loads((o/'compiler-inputs.json').read_bytes())['source']
args=['tests/test_image_properties.py::test_async_image_facts_final_release_needs_no_python_on_save_worker','-q','--junitxml='+str(o/'ownership-before.xml'),'--basetemp',str(o/'ownership-before-temp')]
code="import sys;sys.path.insert(0,"+repr(str(stage))+");import qupathxx as q;assert q.build_environment().git_revision=="+repr(source)+";assert q.has_project and q.has_interchange and q.has_tiff;print('native source',q.build_environment().git_revision,flush=True);import pytest;sys.exit(pytest.main("+repr(args)+"))"
argv=[str(r/'build/ci/object-planes-r5-env/Scripts/python.exe'),'-I','-X','utf8','-c',code];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';start=time.perf_counter()
with (o/'ownership-before.log').open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/'ownership-before-command.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,source=source,product_installed=False,elapsed_seconds=time.perf_counter()-start),indent=2),encoding='utf-8')
print('New and older affected staged contracts',result.returncode,flush=True);raise SystemExit(result.returncode)
