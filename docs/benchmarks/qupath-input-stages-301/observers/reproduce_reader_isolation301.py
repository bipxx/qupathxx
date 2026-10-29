from pathlib import Path
import importlib.util
import json
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2];source=root/'build/ci/reader-locale-qualification-298-audit/tests/test_reader_locale.py';out=root/'build/qupath-input-stages-301'
spec=importlib.util.spec_from_file_location('frozen298_reader_test',source);test=importlib.util.module_from_spec(spec);spec.loader.exec_module(test)
original=subprocess.run;observations=[]
def isolated(argv,**kwargs):
    actual=[argv[0],'-I',*argv[1:]]
    result=original(actual,**kwargs)
    observations.append(dict(argv=actual,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
    return result
test.subprocess.run=isolated
try:
    with tempfile.TemporaryDirectory(prefix='reader-isolation301-') as temp:
        try:test.test_unicode_reader_dispatch_and_nominal_resume(Path(temp),'C')
        except AssertionError:pass
        else:raise AssertionError('Expected original isolated helper import failure')
finally:subprocess.run=original
assert len(observations)==1 and "No module named 'test_nominal_cell_batches'" in observations[0]['stderr']
(out/'original-isolated-reader-red.json').write_text(json.dumps(dict(frozen_source='9fbc9315e8c2417c35492cf505a8f676ecdfcf75',reproduced=True,observations=observations),indent=2)+'\n')
print('Original frozen test reproduces isolated helper import failure without a native change')
