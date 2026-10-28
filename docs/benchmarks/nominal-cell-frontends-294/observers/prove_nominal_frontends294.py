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

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-frontends-294'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((out/'compiler-inputs-initial.json').read_text(encoding='utf-8'));native_source=manifest['source']
observer=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert native_source.startswith('bb190eb7') and observer!=native_source
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
assert q.build_environment().git_revision==native_source and q.build_environment().source_tree_sha256==manifest['fingerprint']
assert q.has_gui and q.has_geos and q.has_tiff and q.has_project and not q.has_openslide
assert hasattr(q,'NominalCellRoiSampling') and 'NominalCellRoiSampling' in q.__all__ and 'nominal-cell-frontends-294-env' in q.__file__
initial,=list((out/'initial-wheel').glob('*.whl'));final,=list((out/'final-wheel').glob('*.whl'));package=Path(q.__file__).parent
installed=[]
with zipfile.ZipFile(initial) as before,zipfile.ZipFile(final) as after:
    assert before.testzip() is None and after.testzip() is None and before.namelist()==after.namelist()
    record,=[n for n in after.namelist() if n.endswith('.dist-info/RECORD')]
    changed=[name for name in before.namelist() if before.read(name)!=after.read(name)]
    assert set(changed)=={'qupathxx/_native.pyi',record}
    rows=list(csv.reader(io.StringIO(after.read(record).decode())));assert len(rows)==43
    for name,checksum,size in rows:
        data=after.read(name)
        if checksum:assert checksum=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=') and int(size)==len(data)
    for name in after.namelist():
        if not name.startswith('qupathxx/') or name.endswith('/'):continue
        data=after.read(name);assert (package.parent/name).read_bytes()==data
        installed.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    assert len(installed)==39
    for name in ('__init__.py','__init__.pyi','_native.pyi'):assert (package/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
(out/'installed-proof.json').write_text(json.dumps(dict(native_source=native_source,observer_source=observer,package=str(package),fingerprint=manifest['fingerprint'],
    initial_wheel_sha256=digest(initial),final_wheel_sha256=digest(final),changed_members=changed,records=43,members=installed,
    native_and_font_bytes_unchanged_by_stub_patch=True,shared_test_dependencies=True,clean_machine=False),indent=2)+'\n',encoding='utf-8')
proofs={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proofs[name]=module.source_contract(work,observer)
    (out/(name+'-source-proof.json')).write_text(json.dumps(proofs[name],indent=2)+'\n',encoding='utf-8')
merged={}
for proof in proofs.values():
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
linux=json.loads((out/'linux/linux-compiler-inputs.json').read_text(encoding='utf-8'));assert linux['source']==native_source
assert len(linux['files'])==len(manifest['files'])==424
for p,h in manifest['files'].items():assert digest(work/p)==h and merged[p]['git_blob_sha256']==linux['files'][p]['git_blob_sha256']
for p in ('tests/test_nominal_cell_frontends.py','tests/typing/nominal_frontends_valid.py','tests/typing/nominal_frontends_invalid.py','tests/test_nominal_cell_python.py','tests/typing/nominal_cells_valid.py','tests/typing/nominal_cells_invalid.py','tests/test_object_classifier_archives.py'):assert p in merged
retained=out/'final-products';retained.mkdir(exist_ok=False);products=[]
for old in json.loads((out/'prior-products.json').read_text(encoding='utf-8')):
    assert digest(Path(old['retained']))==old['sha256']
    src=root/'build/ci/gridfrontends167-package'/old['path'];dst=retained/old['path'];dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
    assert digest(src)==digest(dst);products.append(dict(path=old['path'],bytes=dst.stat().st_size,sha256=digest(dst),retained=str(dst)))
assert len(products)==71
(out/'final-products.json').write_text(json.dumps(products,indent=2)+'\n',encoding='utf-8')
shutil.copy2(root/'build/ci/gridfrontends167-package/generated/build_manifest.hpp',out/'windows-build-manifest.hpp')
for p in json.loads((out/'linux/linux-native-products.json').read_text(encoding='utf-8')):
    target=out/'linux/linux-products'/Path(p['path']).name;assert target.stat().st_size==p['bytes'] and digest(target)==p['sha256']
counts={}
for name,expected in [('native.xml',(55,0)),('linux/linux-native.xml',(8,0)),('api.xml',(9,5)),('scientific.xml',(665,13)),('preflight.xml',None),('consumers.xml',(390,36))]:
    cases=ET.parse(out/name).findall('.//testcase');assert all(all(c.find(t) is None for t in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases);pair=(len(cases)-skipped,skipped);assert expected is None or pair==expected,(name,pair)
    if expected is None:assert pair[0]>=1076 and pair[1]==0
    counts[name]=dict(passed=pair[0],skipped=pair[1])
for name in ('native.xml','linux/linux-native.xml'):
    cell,=[c for c in ET.parse(out/name).findall('.//testcase') if c.get('name')=='cells']
    assert cell.findtext('system-out','').splitlines().count('Native nominal ROI bounds factory and versioned nominal cell CSV descriptors passed')==1
science=module
native_admission=science.native(ET.parse(out/'native.xml').findall('.//testcase'),project=True,geos=True,tiff=True)
(out/'native-admission.json').write_text(json.dumps(native_admission,indent=2)+'\n',encoding='utf-8')
for path,marker in [('native.xml','Nominal cell batch options capture per-ROI bounds, QCL6 checkpoints, hash-only resume and typed CSV passed'),('linux/linux-native.xml','Nominal cell batch option admission passed; project storage unavailable')]:
    case,=[c for c in ET.parse(out/path).findall('.//testcase') if c.get('name')=='project_batch']
    assert case.findtext('system-out','').splitlines().count(marker)==1
standalone=json.loads((out/'standalone-proof.json').read_text(encoding='utf-8'))
assert standalone['native_source']==native_source and standalone['images']==2 and standalone['source_unchanged']
assert standalone['clean_path'] and standalone['python_files_absent'] and all(c['returncode']==0 for c in standalone['commands'])
initial_desktop=ET.parse(out/'desktop.xml').findall('.//testcase')
failed=[c for c in initial_desktop if c.find('failure') is not None]
assert len(failed)==1 and failed[0].get('name')=='test_nominal_desktop_batch_uses_existing_checkpoints_and_resumes'
repair=ET.parse(out/'desktop-repair.xml').findall('.//testcase');assert len(repair)==1 and repair[0].get('name')==failed[0].get('name')
desktop_cases=[c for c in initial_desktop if c not in failed]+repair
assert desktop_cases and all(all(c.find(t) is None for t in ('failure','error','skipped')) for c in desktop_cases)
assert len({(c.get('classname'),c.get('name')) for c in desktop_cases})==len(desktop_cases)
selected=[c for c in desktop_cases if c.get('classname','').rsplit('.',1)[-1]=='test_nominal_cell_frontends']
assert sorted(c.get('name') for c in selected)==science.CASES['test_nominal_cell_frontends']
applicability=json.loads((out/'desktop-observer-applicability.json').read_text(encoding='utf-8'))
assert applicability['repair_observer']==observer and applicability['unchanged_compiler_inputs']==424 and not applicability['native_rebuilt']
counts['desktop-combined']=dict(passed=len(desktop_cases),skipped=0,new_nominal_cases=len(selected),initial_passed=len(initial_desktop)-1,initial_failed=1,repaired=1)

viewer,=[c for c in ET.parse(out/'linux/linux-native.xml').findall('.//testcase') if c.get('name')=='viewer_session']
assert viewer.findtext('system-out','').splitlines().count('Nominal viewer copied options, captured ROI/calibration, pinned masks, retention, cancellation and stale delivery passed')==1
assert standalone['gui_smokes']==2
controls=json.loads((out/'controls-toggle3-proof.json').read_text(encoding='utf-8'))
assert controls['native_source']==native_source and controls['pinned_detection_after_mode_switch'] and controls['lossless']
visual=json.loads((out/'desktop-visual-inspection.json').read_text(encoding='utf-8'));assert visual['native_source']==native_source and len(visual['inspected'])==4
summary=dict(native_source=native_source,stub_observer_source=applicability['initial_test_observer'],evidence_observer_source=observer,windows_fingerprint=manifest['fingerprint'],
    linux_fingerprint=json.loads((out/'linux/linux-source-fingerprint.json').read_text())['fingerprint'],source_paths=len(merged),compiler_inputs=424,
    retained_windows_products=71,retained_gcc_products=9,package_members=39,wheel_records=43,tests=counts,
    positive_typing_files=11,negative_typing_files=8,new_nominal_typing_errors=3,
    csv_nominal_versions=[7,8],nominal_python_api_delivered=True,original_document_desktop_capture_inspected=True,
    nominal_cli_batch_delivered=True,nominal_gui_controls_delivered=True,nominal_managed_command_delivered=True,own_source_platform_qualified=False,scientific_acceptance=False,general_speed_claim=False,release_ready=False)
(out/'source-runtime-proof.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print('Nominal frontend294 source, installed package, retained products and test results authenticated',flush=True)
