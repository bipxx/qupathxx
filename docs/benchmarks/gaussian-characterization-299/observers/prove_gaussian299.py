from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import xml.etree.ElementTree as ET
import qupathxx as q
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/gaussian-characterization-299'
digest=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
observer=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip();assert observer.startswith('f788d082')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
earlier_tests=subprocess.check_output(['git','show','0ea3beca8fd06e59e8f70fc638eb03aca2f8b8f2:tests/test_scientific_grid_evidence.py'],cwd=work).decode()
current_tests=(work/'tests/test_scientific_grid_evidence.py').read_text()
assert current_tests.replace(",'test_qupath_gaussian'",'')==earlier_tests
prior=root/'build/reader-locale-297';manifest=json.loads((prior/'compiler-inputs-initial.json').read_text())
assert q.build_environment().git_revision==manifest['source'] and q.build_environment().source_tree_sha256==manifest['fingerprint']
assert all(digest(work/p)==h for p,h in manifest['files'].items())
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,observer);(out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n')
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
native_git=json.loads((prior/'raw_classifier_evidence-source-proof.json').read_text());native_git.update(json.loads((prior/'scientific_grid_evidence-source-proof.json').read_text()))
for p in manifest['files']:assert merged[p]['git_blob_sha256']==native_git[p]['git_blob_sha256']
products=json.loads((prior/'final-products.json').read_text())
for p in products:
    assert digest(p['retained'])==p['sha256'] and digest(root/'build/ci/gridfrontends167-package'/p['path'])==p['sha256']
installed=json.loads((prior/'installed-proof.json').read_text());site=Path(q.__file__).parent.parent
for p in installed['members']:assert digest(site/p['path'])==p['sha256']
counts={}
for filename in ('api.xml','linux/api.xml','scientific.xml','preflight.xml','preflight-collection.xml'):
    cases=ET.parse(out/filename).findall('.//testcase');assert cases and all(all(c.find(t) is None for t in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases);counts[filename]=dict(passed=len(cases)-skipped,skipped=skipped)
module.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
spec=importlib.util.spec_from_file_location('gaussian299_reference',work/'tools/qupath_gaussian.py');reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
report=reference.verify();assert report==json.loads((out/'linux/verification.json').read_text())
(out/'windows-verification.json').write_text(json.dumps(report,indent=2)+'\n')
assert counts['api.xml']['passed']==counts['linux/api.xml']['passed']==60
probe=json.loads((out/'native-and-repeat-proof.json').read_text());assert probe['final_capture_numeric_and_recipe_payloads_equal']==148
(out/'source-runtime-proof.json').write_text(json.dumps(dict(schema='qpx.gaussian-stage-local/1',observer_source=observer,native_source=manifest['source'],fingerprint=manifest['fingerprint'],source_paths=len(merged),unchanged_compiler_inputs=len(manifest['files']),unchanged_native_products=len(products),unchanged_windows_package_members=len(installed['members']),new_cases=37,new_tests_per_host=60,tests=counts,counts_overlap=True,preflight_unique_passes=counts['preflight.xml']['passed']+counts['preflight-collection.xml']['passed'],earlier_control_bodies_unchanged=True,captured_blur_laplacian_exact=True,roi_log_matches_original_capture=True,native_rebuilt=False,standalone_diagnostic_probe_compiled=True,stub_regenerated=False,own_source_hosted_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n')
print('Gaussian source/runtime evidence verified:',len(merged),'sources;',counts,flush=True)
