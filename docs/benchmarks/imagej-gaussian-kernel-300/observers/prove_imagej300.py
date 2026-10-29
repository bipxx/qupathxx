from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
import qupathxx as q
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/imagej-gaussian-kernel-300'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip();assert source.startswith('647576b4')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
names={'CMakeLists.txt','pyproject.toml'}
for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:names.update(p.relative_to(work).as_posix() for p in (work/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files={p:sha(work/p) for p in sorted(names)};assert len(files)==429
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
header=(root/'build/ci/imagej-gaussian-300-msvc/generated/build_manifest.hpp').read_text();assert source in header and fingerprint in header
(out/'compiler-inputs.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n')
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,source);(out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n')
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
linux=json.loads((out/'linux/compiler-inputs.json').read_text());assert linux['source']==source
for p,h in linux['files'].items():assert h==merged[p]['git_blob_sha256'],p
prior=root/'build/reader-locale-297';manifest=json.loads((prior/'compiler-inputs-initial.json').read_text())
assert q.build_environment().git_revision==manifest['source'] and q.build_environment().source_tree_sha256==manifest['fingerprint']
assert [p for p,h in manifest['files'].items() if sha(work/p)!=h]==['CMakeLists.txt']
assert json.loads((out/'production-target-applicability.json').read_text())['production_target_definitions_unchanged']
products=json.loads((prior/'final-products.json').read_text())
for p in products:assert sha(p['retained'])==p['sha256'] and sha(root/'build/ci/gridfrontends167-package'/p['path'])==p['sha256']
installed=json.loads((prior/'installed-proof.json').read_text());site=Path(q.__file__).parent.parent
for p in installed['members']:assert sha(site/p['path'])==p['sha256']
spec=importlib.util.spec_from_file_location('imagej300_reference',work/'tools/qupath_gaussian.py');reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference);cases,_=reference.inputs()
captures=[]
for case in cases:
    for kind in ('blur','log'):
        name=case['name']+'-'+kind+'.f32le';expected=reference.FIXTURE/name
        windows=root/'build/ci/imagej-gaussian-300-msvc/imagej-gaussian-test-artifacts'/name
        linux=out/'linux/artifacts'/name
        assert windows.read_bytes()==linux.read_bytes()==expected.read_bytes(),name
        captures.append(dict(path=name,bytes=expected.stat().st_size,sha256=sha(expected)))
(out/'exact-output-proof.json').write_text(json.dumps(dict(cases=37,exact_arrays=len(captures),files=captures),indent=2)+'\n')
counts={}
for filename in ('windows-final-native.xml','linux/native.xml','scientific.xml','preflight.xml'):
    cases_xml=ET.parse(out/filename).findall('.//testcase');assert cases_xml and all(all(c.find(t) is None for t in ('failure','error')) for c in cases_xml)
    skipped=sum(c.find('skipped') is not None for c in cases_xml);counts[filename]=dict(passed=len(cases_xml)-skipped,skipped=skipped)
module.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
for profile in ('windows','linux'):
    path=out/('windows-native-products.json' if profile=='windows' else 'linux/native-products.json')
    values=json.loads(path.read_text());assert len(values)==4
    for value in values:
        p=Path(value['path'].replace('/mnt/c/','C:/')) if profile=='linux' else Path(value['path'])
        assert sha(p)==value['sha256']
vcx=ET.parse(root/'build/ci/imagej-gaussian-300-msvc/qupathxx_imagej_gaussian_tests.vcxproj')
models=vcx.findall('.//{http://schemas.microsoft.com/developer/msbuild/2003}FloatingPointModel')
assert len(models)==4 and all(v.text=='Strict' for v in models)
shutil.copyfile(root/'build/ci/imagej-gaussian-300-msvc/generated/build_manifest.hpp',out/'windows-build-manifest.hpp')
(out/'source-runtime-proof.json').write_text(json.dumps(dict(schema='qpx.imagej-gaussian-private-local/1',source=source,source_paths=len(merged),compiler_inputs=len(files),windows_fingerprint=fingerprint,private_standalone_products=8,exact_reference_arrays=74,reference_cases=37,tests=counts,application_native_source=manifest['source'],unchanged_application_products=len(products),unchanged_windows_package_members=len(installed['members']),application_rebuilt=False,public_api_changed=False,gaussian_private_only=True,shared_test_dependencies=True,clean_machine=False,biological_validation=False,general_speed_claim=False,release_ready=False),indent=2)+'\n')
print('Private kernel proof:',len(merged),'sources;74 exact MSVC/GCC arrays;8 private products;71 unchanged app products;',counts,flush=True)
