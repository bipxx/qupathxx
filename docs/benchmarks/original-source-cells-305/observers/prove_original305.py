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

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-305'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
observer=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
manifest=json.loads((out/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
assert manifest['source']==q.build_environment().git_revision=='26cae2af9f8d1ee54b1b5871079e67646a9faf07'
assert manifest['fingerprint']==q.build_environment().source_tree_sha256
assert len(manifest['files'])==434 and all(digest(work/name)==value for name,value in manifest['files'].items())
merged={};science=None
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/'tools/ci'/(name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,observer)
    with (out/(name+'-source-proof.json')).open('x',encoding='utf-8') as stream:json.dump(proof,stream,indent=2);stream.write('\n')
    for path,value in proof.items():assert path not in merged or merged[path]==value;merged[path]=value
    if name=='scientific_grid_evidence':science=module
assert all(name in merged for name in manifest['files'])
retained=out/'final-products';retained.mkdir();products=[]
for item in json.loads((out/'prior-products.json').read_text(encoding='utf-8')):
    path=root/'build/ci/gridfrontends167-package'/item['path'];target=retained/item['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    assert digest(path)==digest(target)
    products.append(dict(path=item['path'],retained=str(target),bytes=target.stat().st_size,sha256=digest(target)))
assert len(products)==79
with (out/'final-products.json').open('x',encoding='utf-8') as stream:json.dump(products,stream,indent=2);stream.write('\n')
wheel,=list((out/'final-wheel').glob('*.whl'));site=Path(q.__file__).parent.parent;assert 'original305-env' in str(site)
members=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            value=archive.read(name);assert (site/name).read_bytes()==value
            members.append(dict(path=name,bytes=len(value),sha256=hashlib.sha256(value).hexdigest()))
    record,=[n for n in archive.namelist() if n.endswith('.dist-info/RECORD')]
    wheel_rows=list(csv.reader(io.StringIO(archive.read(record).decode())))
rows=list(csv.reader(io.StringIO((site/record).read_text(encoding='utf-8'))))
for name,value,size in rows:
    if value:
        raw=(site/name).read_bytes();assert len(raw)==int(size)
        assert value=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(raw).digest()).decode().rstrip('=')
assert len(members)==39 and len(wheel_rows)==43 and len(rows)==49
for name in ('_native.pyi','__init__.pyi'):assert (site/'qupathxx'/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
with (out/'installed-proof.json').open('x',encoding='utf-8') as stream:json.dump(dict(native_source=manifest['source'],observer_source=observer,fingerprint=manifest['fingerprint'],wheel_sha256=digest(wheel),native_sha256=digest(Path(_native.__file__)),members=members,wheel_record_rows=43,installed_record_rows=49,shared_test_dependencies=True,clean_machine=False,stubs_regenerated=True),stream,indent=2);stream.write('\n')
counts={}
for name in ('native','scientific','preflight-final','frontends','linux/native'):
    cases=ET.parse(out/(name+'.xml')).findall('.//testcase');assert cases
    assert all(all(c.find(tag) is None for tag in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases);counts[name]=dict(passed=len(cases)-skipped,skipped=skipped)
assert counts=={'native':dict(passed=57,skipped=0),'scientific':dict(passed=943,skipped=23),'preflight-final':dict(passed=1099,skipped=0),'frontends':dict(passed=24,skipped=0),'linux/native':dict(passed=6,skipped=0)}
science.native(ET.parse(out/'native.xml').findall('.//testcase'))
science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
linux=json.loads((out/'linux/products.json').read_text(encoding='utf-8'))
for item in linux:
    path=out/'linux'/item['path'];assert path.stat().st_size==item['bytes'] and digest(path)==item['sha256']
for kind,total in [('edges',13),('original',9)]:
    comparison=json.loads((out/(kind+'-original-source-comparison.json')).read_text(encoding='utf-8'));assert len(comparison['cases'])==total
    assert comparison['native_sha256']==digest(Path(_native.__file__))
    for case in comparison['cases']:
        if kind=='edges':assert case['input_rgb_exact'] and case['input_component_differences']==0
        else:assert case['independent_processing_rgb_exact']
        for metric in case['metrics' if kind=='edges' else 'level_zero_segmentation'].values():
            assert metric['foreground_iou']==metric['foreground_dice']==1
            assert metric['unmatched_reference']==metric['unmatched_native']==0
            assert not metric['split_reference_labels'] and not metric['merged_native_labels']
            assert all(m['iou']==1 and m['centroid_error_px']==0 for m in metric['matches'])
        assert all(f['processing_arithmetic_accepted'] for f in case['features' if kind=='edges' else 'feature_checks'])
summary=dict(native_source=manifest['source'],observer_source=observer,fingerprint=manifest['fingerprint'],compiler_inputs=434,source_paths=len(merged),windows_products=79,linux_products=len(linux),installed_members=39,tests=counts,original_slide_exact_heldout_workflows=13,original_slide_exact_prior_workflows=9,affine_captured_rgb_reads=112,public_recipe_delivered=True,public_api_changed=True,stubs_regenerated=True,own_source_hosted_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False)
with (out/'source-runtime-proof.json').open('x',encoding='utf-8') as stream:json.dump(summary,stream,indent=2);stream.write('\n')
print(json.dumps(summary,indent=2),flush=True)
