from pathlib import Path
import json,os,subprocess,time,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-clipboard-r4'
assert json.loads((o/'build-exit.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel').glob('*.whl'));stage=o/'probe-stage';stage.mkdir()
with zipfile.ZipFile(wheel) as archive:archive.extractall(stage)
source=json.loads((o/'compiler-inputs.json').read_bytes())['source']
modules=['test_object_clipboard','test_interchange','test_object_properties','test_object_edit_state','test_object_planes','test_document_edits','test_geometry_edits','test_classification_edits','test_selection','test_annotation_merge','test_project']
args=['tests/'+m+'.py' for m in modules]+['-q','--junitxml='+str(o/'staged.xml'),'--basetemp',str(o/'staged-temp')]
code="import sys;sys.path.insert(0,"+repr(str(stage))+");import qupathxx as q;assert q.build_environment().git_revision=="+repr(source)+";print('native source',q.build_environment().git_revision,flush=True);import pytest;sys.exit(pytest.main("+repr(args)+"))"
argv=[str(r/'build/ci/object-planes-r5-env/Scripts/python.exe'),'-I','-X','utf8','-c',code];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';start=time.perf_counter()
with (o/'staged.log').open('x',encoding='utf-8') as log:result=subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT)
(o/'staged-command.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,source=source,product_installed=False,elapsed_seconds=time.perf_counter()-start),indent=2),encoding='utf-8')
print('New and older affected staged contracts',result.returncode,flush=True);raise SystemExit(result.returncode)
