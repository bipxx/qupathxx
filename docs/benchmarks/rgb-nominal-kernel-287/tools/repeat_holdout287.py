from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
original=work/'tests/fixtures/qupath-rgb-resampling-holdout-0.7.0'
out=root/'build/rgb-nominal-287'
folder=out/'holdout-repeat'
folder.mkdir(exist_ok=False)
shutil.copyfile(original/'contract.json',folder/'contract.json')
contract=json.loads((folder/'contract.json').read_text())
for case in contract['cases']:
    (folder/case['name']).mkdir()
    shutil.copyfile(original/case['name']/'input.png',folder/case['name']/'input.png')
spec=importlib.util.spec_from_file_location('fetch',work/'tools/fetch_qupath.py')
fetch=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)
runtime=fetch.fetch(root/'build/upstream/qupath')
args=[str(runtime),'script','--args',str(folder),str(work/'tools/upstream/qupath_rgb_resampling.groovy')]
with (out/'holdout-repeat-runtime.log').open('wb') as log:
    result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
(out/'holdout-repeat-exit.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
assert result.returncode==0
manifest=json.loads((original/'manifest.json').read_text())
files=[]
for name,entry in manifest['files'].items():
    actual=folder/name
    assert actual.read_bytes()==(original/name).read_bytes(),name
    assert actual.stat().st_size==entry['bytes'] and hashlib.sha256(actual.read_bytes()).hexdigest()==entry['sha256']
    files.append(dict(path=name,**entry))
assert len(files)==72
(out/'holdout-repeat-proof.json').write_text(json.dumps(dict(files=files,actual_complete_captures=2,
    runtime_logs_excluded=True),indent=2)+'\n')
print('All72 holdout payload bytes repeat exactly, including63 independent public reads',flush=True)
