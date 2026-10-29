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

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-processing-policy-302'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((out/'compiler-inputs-initial.json').read_text())
observer=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert observer.startswith('28e062dd') and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
assert q.build_environment().git_revision==manifest['source']
assert q.build_environment().source_tree_sha256==manifest['fingerprint']
assert all(digest(work/p)==h for p,h in manifest['files'].items())
merged={};science=None
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,observer)
    (out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n')
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
    if name=='scientific_grid_evidence':science=module
assert 'tests/test_imagej_cell_processing.py' in merged
for p in manifest['files']:assert p in merged
retained=out/'final-products';retained.mkdir(exist_ok=False)
paths=[old['path'] for old in json.loads((out/'prior-products.json').read_text())]
paths+=['Release/qupathxx_imagej_gaussian_tests.exe','qupathxx_core.dir/Release/imagej_gaussian.obj']
paths+=['qupathxx_imagej_gaussian_tests.dir/Release/'+p for p in ('imagej_gaussian_tests.obj','imagej_gaussian.obj','sha256.obj')]
assert len(paths)==len(set(paths))==76
products=[]
for name in paths:
    source=root/'build/ci/gridfrontends167-package'/name;target=retained/name;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target);assert digest(source)==digest(target)
    products.append(dict(path=name,retained=str(target),bytes=target.stat().st_size,sha256=digest(target)))
(out/'final-products.json').write_text(json.dumps(products,indent=2)+'\n')
wheel,=list((out/'final-wheel').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            raw=archive.read(name);assert (site/name).read_bytes()==raw,name
            members.append(dict(path=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
    record,=[n for n in archive.namelist() if n.endswith('.dist-info/RECORD')]
rows=list(csv.reader(io.StringIO((site/record).read_text())))
for name,hash_value,size in rows:
    if hash_value:
        raw=(site/name).read_bytes();assert len(raw)==int(size)
        assert hash_value=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(raw).digest()).decode().rstrip('='),name
for name in ('_native.pyi','__init__.pyi'):assert (site/'qupathxx'/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
(out/'installed-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],fingerprint=manifest['fingerprint'],wheel_sha256=digest(wheel),native_sha256=digest(Path(_native.__file__)),members=members,record_rows=len(rows),shared_test_dependencies=True,clean_machine=False),indent=2)+'\n')
counts={}
for filename in ('native.xml','policy.xml','frontends.xml','scientific.xml','preflight.xml'):
    cases=ET.parse(out/filename).findall('.//testcase');assert cases
    assert all(all(c.find(t) is None for t in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases)
    counts[filename]=dict(passed=len(cases)-skipped,skipped=skipped)
science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
science.native(ET.parse(out/'native.xml').findall('.//testcase'))
comparisons={}
for context in (0,32):
    report=json.loads((out/('imagej-context'+str(context)+'.json')).read_text())
    assert report['build_git_revision']==manifest['source'] and len(report['cases'])==9
    comparisons[str(context)]={c['case_name']:{name:metric['foreground_iou'] for name,metric in c['level_zero_segmentation'].items()} for c in report['cases']}
assert all(value==1 for case in comparisons['0'].values() for value in case.values())
assert comparisons['32']['roi_cut_hole']['nucleus']<1
(out/'source-runtime-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],observer_source=observer,fingerprint=manifest['fingerprint'],compiler_inputs=len(manifest['files']),source_paths=len(merged),retained_products=len(products),package_members=len(members),tests=counts,comparison_ious=comparisons,own_source_hosted_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n')
print('Policy302 source/runtime, exact installed members and retained products verified',counts,flush=True)
