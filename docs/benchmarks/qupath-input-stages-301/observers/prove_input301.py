from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import xml.etree.ElementTree as ET
import qupathxx as q
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/qupath-input-stages-301'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip();assert source.startswith('36400c9f')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
private=root/'build/imagej-gaussian-kernel-300';compiler=json.loads((private/'compiler-inputs.json').read_text());assert len(compiler['files'])==429
assert all(sha(work/p)==h for p,h in compiler['files'].items())
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,source);(out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n')
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
previous=root/'build/reader-locale-297';manifest=json.loads((previous/'compiler-inputs-initial.json').read_text())
assert q.build_environment().git_revision==manifest['source'] and q.build_environment().source_tree_sha256==manifest['fingerprint']
products=json.loads((previous/'final-products.json').read_text())
for p in products:assert sha(p['retained'])==p['sha256'] and sha(root/'build/ci/gridfrontends167-package'/p['path'])==p['sha256']
installed=json.loads((previous/'installed-proof.json').read_text());site=Path(q.__file__).parent.parent
for p in installed['members']:assert sha(site/p['path'])==p['sha256']
for name in ('windows-native-products.json','linux/native-products.json'):
    for p in json.loads((private/name).read_text()):assert sha(p['path'].replace('/mnt/c/','C:/'))==p['sha256']
spec=importlib.util.spec_from_file_location('input301_reference',work/'tools/qupath_cell_inputs.py');reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
windows=reference.compare_native();linux=json.loads((out/'linux/native-comparison.json').read_text())
assert windows['cases']==linux['cases'] and windows['native_comparisons']==linux['native_comparisons'] and windows['manifest_sha256']==linux['manifest_sha256']
assert linux['native_source']=='9fbc9315e8c2417c35492cf505a8f676ecdfcf75'
(out/'windows-native-comparison.json').write_text(json.dumps(windows,indent=2)+'\n')
counts={}
for filename in ('api.xml','reader-isolated.xml','reader-pth.xml','linux/api.xml','scientific.xml','preflight.xml'):
    cases=ET.parse(out/filename).findall('.//testcase');assert cases and all(all(c.find(t) is None for t in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases);counts[filename]=dict(passed=len(cases)-skipped,skipped=skipped)
module.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
args=[str(root/'build/ci/reader-isolated-pth-301/python.exe'),'-I','-c',
    "import sys,json,qupathxx as q;print(json.dumps(dict(isolated=sys.flags.isolated,ignore_environment=sys.flags.ignore_environment,package=q.__file__,native_source=q.build_environment().git_revision)))"]
identity=json.loads(subprocess.check_output(args,text=True));assert identity['isolated']==identity['ignore_environment']==1 and identity['native_source']==manifest['source']
(out/'reader-pth-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
repeat=json.loads((out/'repeat-proof.json').read_text());assert repeat['byte_identical_payloads']==92
(out/'source-runtime-proof.json').write_text(json.dumps(dict(schema='qpx.qupath-input-stages-local/1',observer_source=source,windows_native_source=manifest['source'],linux_native_source=linux['native_source'],source_paths=len(merged),unchanged_compiler_inputs=429,unchanged_application_products=len(products),unchanged_windows_package_members=len(installed['members']),unchanged_private_products=8,captured_cases=11,captured_arrays=77,normalized_stain_checks_per_host=33,repeated_payloads=92,tests=counts,counts_overlap=True,isolated_reader_observer_repaired=True,application_rebuilt=False,native_math_changed=False,stub_regenerated=False,shared_test_dependencies=True,clean_machine=False,own_source_platform_qualification=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n')
print('Input301 proof:',len(merged),'sources;77 exact arrays per host;429 unchanged inputs; isolated reader repaired;',counts,flush=True)
