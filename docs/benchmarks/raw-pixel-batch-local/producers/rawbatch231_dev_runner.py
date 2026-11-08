from pathlib import Path
import json,runpy,sys
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawbatch231-development'
sys.path.insert(0,str(out/'package'))
import qupathxx as q
m=json.loads((out/'compiler-inputs.json').read_text(encoding='utf-8'));assert q.build_environment().source_tree_sha256==m['fingerprint'];assert q.build_environment().git_revision==m['source'];assert q.has_classifier_training
if sys.argv[1]=='stubs':
 sys.argv=[str(s/'tools/generate_stubs.py')];runpy.run_path(sys.argv[0],run_name='__main__')
else:
 import pytest
 raise SystemExit(pytest.main([str(s/'tests'/n) for n in ['test_raw_pixel_batch.py','test_project_pixel_batch.py','test_raw_pixel_cli.py','test_raw_pixel_python.py','test_raw_batch.py']]+['-q','--junitxml='+str(out/'python-tests.xml')]))
