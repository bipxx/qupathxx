from pathlib import Path
import base64
import csv
import hashlib
import importlib.util
import io
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile
import qupathxx as q
from qupathxx import _native
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/reader-locale-297'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((out/'compiler-inputs-initial.json').read_text())
observer=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert observer.startswith('e1dc6457') and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
assert q.build_environment().git_revision==manifest['source']
assert q.build_environment().source_tree_sha256==manifest['fingerprint']
assert all(digest(work/p)==h for p,h in manifest['files'].items())
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,observer)
    (out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n')
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
assert 'cpp/ascii_case.hpp' in merged and 'tests/test_reader_locale.py' in merged
for p in manifest['files']: assert p in merged
retained=out/'final-products';retained.mkdir(exist_ok=False)
products=[]
for old in json.loads((out/'prior-products.json').read_text()):
    source=root/'build/ci/gridfrontends167-package'/old['path'];target=retained/old['path'];target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target);assert digest(source)==digest(target)
    products.append(dict(path=old['path'],retained=str(target),bytes=target.stat().st_size,sha256=digest(target)))
(out/'final-products.json').write_text(json.dumps(products,indent=2)+'\n')
wheel,=list((out/'initial-wheel').glob('*.whl'));site=Path(q.__file__).parent.parent
members=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            raw=archive.read(name);assert (site/name).read_bytes()==raw,name
            members.append(dict(path=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
    records,=[n for n in archive.namelist() if n.endswith('.dist-info/RECORD')]
record_rows=list(csv.reader(io.StringIO((site/records).read_text())))
for name,hash_value,size in record_rows:
    if hash_value:
        raw=(site/name).read_bytes();assert len(raw)==int(size)
        assert hash_value=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(raw).digest()).decode().rstrip('='),name
for name in ('_native.pyi','__init__.pyi'):
    assert (site/'qupathxx'/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
(out/'installed-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],fingerprint=manifest['fingerprint'],wheel_sha256=digest(wheel),native_sha256=digest(Path(_native.__file__)),members=members,record_rows=len(record_rows),shared_test_dependencies=True,clean_machine=False),indent=2)+'\n')
counts={}
for filename in ('native.xml','regression.xml','region.xml','scientific.xml','preflight.xml'):
    cases=ET.parse(out/filename).findall('.//testcase')
    assert cases and all(all(c.find(t) is None for t in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases)
    counts[filename]=dict(passed=len(cases)-skipped,skipped=skipped)
module.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
(out/'source-runtime-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],observer_source=observer,fingerprint=manifest['fingerprint'],compiler_inputs=len(manifest['files']),source_paths=len(merged),retained_products=len(products),package_members=len(members),tests=counts,own_source_hosted_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n')
print('Reader repair source/runtime and retained products verified',counts,flush=True)
