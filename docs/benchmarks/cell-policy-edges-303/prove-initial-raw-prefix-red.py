from pathlib import Path
import base64
import csv
import hashlib
import importlib.util
import io
import json
import subprocess
import xml.etree.ElementTree as ET
import zipfile
import qupathxx as q
from qupathxx import _native

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303';prior=root/'build/cell-processing-policy-302'
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source.startswith('dc76f21e') and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
manifest=json.loads((prior/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
assert len(manifest['files'])==429 and all(digest(work/name)==value for name,value in manifest['files'].items())
assert q.build_environment().git_revision==manifest['source'] and q.build_environment().source_tree_sha256==manifest['fingerprint']
assert 'cell-policy302-env' in str(Path(q.__file__).resolve())
products=json.loads((prior/'final-products.json').read_text(encoding='utf-8'));assert len(products)==76
for product in products:
    assert digest(Path(product['retained']))==product['sha256']
    assert digest(root/'build/ci/gridfrontends167-package'/product['path'])==product['sha256']
private=[]
for path in [root/'build/imagej-gaussian-kernel-300/windows-native-products.json',root/'build/imagej-gaussian-kernel-300/linux/native-products.json']:
    for product in json.loads(path.read_text(encoding='utf-8')):
        p=Path(product['path'].replace('/mnt/c/','C:/',1));assert p.stat().st_size==product['bytes'] and digest(p)==product['sha256'];private.append(product)
assert len(private)==8
merged={};science=None
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/'tools/ci'/(name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,source);(out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    for key,value in proof.items():assert key not in merged or merged[key]==value;merged[key]=value
    if name=='scientific_grid_evidence':science=module
assert len(merged)==1310
for path in manifest['files']:assert path in merged
counts={}
for mode in ('api-final','scientific','preflight'):
    cases=ET.parse(out/(mode+'.xml')).findall('.//testcase');assert cases
    assert all(all(case.find(tag) is None for tag in ('failure','error')) for case in cases)
    skipped=sum(case.find('skipped') is not None for case in cases)
    counts[mode]=dict(passed=len(cases)-skipped,skipped=skipped)
science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
wheel,=list((prior/'final-wheel').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            raw=archive.read(name);assert (site/name).read_bytes()==raw
            members.append(dict(path=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
    record,=[name for name in archive.namelist() if name.endswith('.dist-info/RECORD')]
rows=list(csv.reader(io.StringIO((site/record).read_text(encoding='utf-8'))))
for name,value,size in rows:
    if value:
        raw=(site/name).read_bytes();assert len(raw)==int(size)
        assert value=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(raw).digest()).decode().rstrip('=')
assert len(members)==39 and len(rows)==43
(out/'installed-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],fingerprint=manifest['fingerprint'],wheel_sha256=digest(wheel),native_sha256=digest(Path(_native.__file__)),members=members,record_rows=len(rows),shared_test_dependencies=True,clean_machine=False),indent=2)+'\n',encoding='utf-8')
kernel=json.loads((out/'kernel-probe-proof.json').read_text(encoding='utf-8'))
assert len(kernel['cases'])==13 and all(case['differing_float32_values']==0 for case in kernel['cases'])
for product in kernel['products']:assert digest(Path(product['path']))==product['sha256']
tree=ET.parse(out/'kernel-probe/build/gaussian303.vcxproj');namespace={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
release=[node for node in tree.findall('.//m:ItemDefinitionGroup',namespace) if 'Release|' in node.get('Condition','')]
assert len(release)==1 and release[0].findtext('m:ClCompile/m:FloatingPointModel',namespaces=namespace)=='Strict'
affine=json.loads((out/'affine-model-comparison.json').read_text(encoding='utf-8'))['cases']+json.loads((out/'affine-heldout-comparison.json').read_text(encoding='utf-8'))['cases']
assert len(affine)==112 and all(case['differing_components']==0 for case in affine)
report=json.loads((out/'native-characterization.json').read_text(encoding='utf-8'))
exact=[case['name'] for case in report['cases'] if all(metric['foreground_iou']==1 for metric in case['metrics'].values())]
gaps=[case['name'] for case in report['cases'] if case['name'] not in exact]
assert len(exact)==5 and len(gaps)==8
(out/'source-runtime-proof.json').write_text(json.dumps(dict(source=source,native_source=manifest['source'],fingerprint=manifest['fingerprint'],compiler_inputs_unchanged=429,source_paths=len(merged),unchanged_application_products=76,unchanged_private_products=8,installed_members=39,tests=counts,new_cases=13,exact_new_mask_cases=exact,unresolved_new_mask_cases=gaps,exact_affine_diagnostic_reads=112,exact_private_log_arrays=13,new_probe_products=3,application_rebuilt=False,stubs_regenerated=False,own_source_hosted_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n',encoding='utf-8')
print('Policy303 source/runtime and unchanged products authenticate',len(merged),'source paths',counts)
