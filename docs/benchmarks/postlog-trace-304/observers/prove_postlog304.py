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

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source.startswith('e9d71f28') and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
manifest=json.loads((out/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
assert source==manifest['source']==q.build_environment().git_revision
assert manifest['fingerprint']==q.build_environment().source_tree_sha256
assert len(manifest['files'])==433 and all(digest(work/path)==value for path,value in manifest['files'].items())
merged={};science=None
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/'tools/ci'/(name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    values=module.source_contract(work,source);(out/(name+'-source-proof.json')).write_text(json.dumps(values,indent=2)+'\n',encoding='utf-8')
    for path,value in values.items():assert path not in merged or merged[path]==value;merged[path]=value
    if name=='scientific_grid_evidence':science=module
assert len(merged)==1421
for path in manifest['files']:assert path in merged
paths=[item['path'] for item in json.loads((out/'prior-products.json').read_text(encoding='utf-8'))]
paths+=['Release/qupathxx_qupath_response_tests.exe','qupathxx_qupath_response_tests.dir/Release/qupath_response_tests.obj','qupathxx_qupath_response_tests.dir/Release/imagej_gaussian.obj']
assert len(paths)==len(set(paths))==79
retained=out/'final-products';retained.mkdir(exist_ok=False);products=[]
for name in paths:
    path=root/'build/ci/gridfrontends167-package'/name;target=retained/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target);assert digest(path)==digest(target)
    products.append(dict(path=name,retained=str(target),bytes=target.stat().st_size,sha256=digest(target)))
(out/'final-products.json').write_text(json.dumps(products,indent=2)+'\n',encoding='utf-8')
wheel,=list((out/'initial-wheel').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
assert 'postlog304-env' in str(site)
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            value=archive.read(name);assert (site/name).read_bytes()==value
            members.append(dict(path=name,bytes=len(value),sha256=hashlib.sha256(value).hexdigest()))
    record,=[name for name in archive.namelist() if name.endswith('.dist-info/RECORD')]
    wheel_rows=list(csv.reader(io.StringIO(archive.read(record).decode())))
rows=list(csv.reader(io.StringIO((site/record).read_text(encoding='utf-8'))))
for name,value,size in rows:
    if value:
        raw=(site/name).read_bytes();assert len(raw)==int(size)
        assert value=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(raw).digest()).decode().rstrip('=')
assert len(members)==39 and len(wheel_rows)==43 and len(rows)==49
for name in ('_native.pyi','__init__.pyi'):assert (site/'qupathxx'/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
(out/'installed-proof.json').write_text(json.dumps(dict(native_source=source,fingerprint=manifest['fingerprint'],wheel_sha256=digest(wheel),native_sha256=digest(Path(_native.__file__)),members=members,wheel_record_rows=43,installed_record_rows=49,shared_test_dependencies=True,clean_machine=False,stubs_regenerated=False),indent=2)+'\n',encoding='utf-8')
counts={}
for name in ('native','scientific','preflight','frontends'):
    cases=ET.parse(out/(name+'.xml')).findall('.//testcase');assert cases
    assert all(all(case.find(tag) is None for tag in ('failure','error')) for case in cases)
    skipped=sum(case.find('skipped') is not None for case in cases);counts[name]=dict(passed=len(cases)-skipped,skipped=skipped)
science.native(ET.parse(out/'native.xml').findall('.//testcase'))
science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
linux=json.loads((out/'linux/products.json').read_text(encoding='utf-8'))
for item in linux:
    p=Path(item['path'].replace('/mnt/c/','C:/',1));assert p.stat().st_size==item['bytes'] and digest(p)==item['sha256']
linux_cases=ET.parse(out/'linux/native.xml').findall('.//testcase');assert len(linux_cases)==3
assert all(all(case.find(tag) is None for tag in ('failure','error','skipped')) for case in linux_cases)
stages=json.loads((out/'linux/cross-host-probe.json').read_text(encoding='utf-8'));assert len(stages['cases'])==13
for case in stages['cases']:
    for item in case['payloads']:assert digest(out/'arrays'/item['name'])==item['sha256']
candidate=json.loads((out/'candidate-nuclei-final.json').read_text(encoding='utf-8'));assert len(candidate['cases'])==13
for case in candidate['cases']:
    assert case['binary_differences']==0 and case['refinement_differences'] in (None,0)
    for kind in ('nuclei','body'):
        metric=case[kind];assert metric['foreground_iou']==metric['foreground_dice']==1
        assert metric['unmatched_reference']==metric['unmatched_native']==0
        assert not metric['split_reference_labels'] and not metric['merged_native_labels']
        assert all(match['iou']==1 and match['centroid_error_px']==0 for match in metric['matches'])
probe_products=[]
for path in [out/'nuclei-probe/build/Release/nuclei304.exe',out/'nuclei-probe/build/nuclei304.dir/Release/main.obj']:
    target=out/'private-products'/path.name;target.parent.mkdir(exist_ok=True);shutil.copyfile(path,target)
    probe_products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=digest(target)))
(out/'private-products.json').write_text(json.dumps(probe_products,indent=2)+'\n',encoding='utf-8')
(out/'source-runtime-proof.json').write_text(json.dumps(dict(native_source=source,fingerprint=manifest['fingerprint'],compiler_inputs=433,source_paths=len(merged),windows_products=len(products),linux_products=len(linux),installed_members=39,tests=counts,linux_sanitizer_suites=3,private_exact_reference_basin_cases=33,private_exact_heldout_workflows=13,cross_host_exact_stage_payloads=78,public_recipe_delivered=False,public_api_changed=False,stubs_regenerated=False,own_source_hosted_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n',encoding='utf-8')
print('Post-LoG304 source/runtime/products authenticate',len(merged),'paths',counts,'private13 masks and78 stage payloads exact',flush=True)
