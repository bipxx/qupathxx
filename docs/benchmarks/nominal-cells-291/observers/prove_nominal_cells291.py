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

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cells-291'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((out/'compiler-inputs-repaired.json').read_text(encoding='utf-8'));source=manifest['source']
assert source=='37a7df5195af47e0f5c1965e09f8dbec4e469a60'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()==source
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
assert q.build_environment().git_revision==source and q.build_environment().source_tree_sha256==manifest['fingerprint']
assert q.has_gui and q.has_geos and q.has_tiff and q.has_project and q.has_classifier_training and not q.has_openslide
assert 'nominal-cells-291-env' in q.__file__ and not hasattr(q,'NominalCellSampling')
wheel,=list((out/'repaired-wheel').glob('*.whl'));package=Path(q.__file__).parent
installed=[]
with zipfile.ZipFile(wheel) as archive:
    assert archive.testzip() is None
    record,=[n for n in archive.namelist() if n.endswith('.dist-info/RECORD')]
    rows=list(csv.reader(io.StringIO(archive.read(record).decode())));assert len(rows)==43
    for name,checksum,size in rows:
        data=archive.read(name)
        if checksum:
            assert checksum=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=') and len(data)==int(size)
    for name in archive.namelist():
        if not name.startswith('qupathxx/') or name.endswith('/'):continue
        data=archive.read(name);assert (package.parent/name).read_bytes()==data
        installed.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    assert len(installed)==39
    for name in ('__init__.py','__init__.pyi','_native.pyi'):
        assert (package/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
(out/'installed-proof.json').write_text(json.dumps(dict(native_source=source,fingerprint=manifest['fingerprint'],package=str(package),wheel_sha256=digest(wheel),records=43,members=installed,stubs_unchanged=True,new_nominal_python_api=False,shared_test_dependencies=True,clean_machine=False),indent=2)+'\n',encoding='utf-8')
proofs={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    proofs[name]=mod.source_contract(work,source)
    (out/(name+'-source-proof.json')).write_text(json.dumps(proofs[name],indent=2)+'\n',encoding='utf-8')
merged={}
for proof in proofs.values():
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
linux=json.loads((out/'linux-repaired/linux-compiler-inputs.json').read_text(encoding='utf-8'))
assert linux['source']==source and len(linux['files'])==len(manifest['files'])==423
for p,h in manifest['files'].items():
    assert digest(work/p)==h and merged[p]['git_blob_sha256']==linux['files'][p]['git_blob_sha256']
assert 'tests/fixtures/nominal-cells-v1/source.rgb' in merged and 'tools/reference_nominal_cells.py' in merged
products=[];retained=out/'final-products';retained.mkdir(exist_ok=False)
old=json.loads((out/'prior-products.json').read_text(encoding='utf-8'))
for p in old:
    assert digest(Path(p['retained']))==p['sha256']
    src=root/'build/ci/gridfrontends167-package'/p['path'];dst=retained/p['path'];dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
    assert digest(src)==digest(dst);products.append(dict(path=p['path'],bytes=dst.stat().st_size,sha256=digest(dst),retained=str(dst)))
assert len(products)==71
(out/'final-products.json').write_text(json.dumps(products,indent=2)+'\n',encoding='utf-8')
shutil.copy2(root/'build/ci/gridfrontends167-package/generated/build_manifest.hpp',out/'windows-build-manifest.hpp')
for entry in json.loads((out/'linux-repaired/linux-native-products.json').read_text(encoding='utf-8')):
    path=out/'linux-repaired/linux-products'/Path(entry['path']).name
    assert path.stat().st_size==entry['bytes'] and digest(path)==entry['sha256']
marker='Original-source nominal cell captured RGB/masks/features, QCL6 canonical history, replay, calibration, budgets and cancellation passed'
for name,count in [('native-repaired.xml',55),('linux-repaired/linux-native.xml',6)]:
    cases=ET.parse(out/name).findall('.//testcase');assert len(cases)==count and all(all(c.find(t) is None for t in ('failure','error','skipped')) for c in cases)
    cell,=[c for c in cases if c.get('name')=='cells'];assert cell.findtext('system-out','').splitlines().count(marker)==1
    if count==55:
        for text in ('Nominal cell original-coordinate contours, publication, undo/redo and QCA6 classifier origins passed','QCL6 project save/reopen, source-bound replay, verification and second save passed'):
            assert cell.findtext('system-out','').splitlines().count(text)==1
summary=dict(native_source=source,windows_fingerprint=manifest['fingerprint'],linux_fingerprint=json.loads((out/'linux-repaired/linux-source-fingerprint.json').read_text())['fingerprint'],
    compiler_inputs=423,source_paths=len(merged),windows_native_suites=55,gcc_sanitized_native_suites=6,retained_windows_products=71,retained_repaired_gcc_products=7,
    captured_native_nucleus_pixels=379,captured_native_body_pixels=825,captured_cells=3,
    independent_original_document_capture=True,QCL6_project_replay_verified=True,QCA6_classifier_origins_verified_windows=True,
    python_nominal_cell_api_delivered=False,nominal_cli_batch_gui_delivered=False,own_source_platform_qualified=False,
    scientific_acceptance=False,general_speed_claim=False,release_ready=False)
(out/'source-runtime-proof.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print('Native55/GCC6,423 compiler inputs,'+str(len(merged))+' source paths,71/7 retained products and39/43 package records authenticate',flush=True)
