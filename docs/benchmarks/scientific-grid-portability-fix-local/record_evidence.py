from pathlib import Path
import collections,datetime,hashlib,importlib.util,json,re,shutil,subprocess,xml.etree.ElementTree as ET
root=Path(r'C:/Users/HEQ/Projects/qupathxx');source=root/'build/ci/scientific-grid-159';build=root/'build/ci/gridfrontends167-package'
out=source/'docs/benchmarks/scientific-grid-portability-fix-local';out.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
revision='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062'
subprocess.run(['git','merge-base','--is-ancestor',revision,'HEAD'],cwd=source,check=True)
inputs=sorted(set([p for pattern in ('cpp/**/*.cpp','cpp/**/*.hpp','cmake/**/*.cmake','cmake/**/*.in','src/**/*.py') for p in source.glob(pattern)]+[source/'CMakeLists.txt',source/'pyproject.toml']),key=lambda p:p.as_posix())
requests=''.join(f'{revision}:{p.relative_to(source).as_posix()}\n' for p in inputs).encode()
raw=subprocess.run(['git','cat-file','--batch'],input=requests,cwd=source,stdout=subprocess.PIPE,check=True).stdout
offset=0;manifest='';proof=[]
for p in inputs:
    end=raw.index(b'\n',offset);header=raw[offset:end].decode().split();assert header[1]=='blob',header
    size=int(header[2]);blob=raw[end+1:end+1+size];assert raw[end+1+size:end+2+size]==b'\n';offset=end+2+size
    actual=p.read_bytes();assert actual==blob or actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),p
    name=p.relative_to(source).as_posix();digest=sha(actual);manifest+=f'{name}={digest}\n'
    proof.append(dict(path=name,checkout_sha256=digest,git_blob_sha256=sha(blob),line_endings_only=actual!=blob))
assert offset==len(raw);fingerprint=sha(manifest.encode());configured=(build/'generated/build_manifest.hpp').read_text(encoding='utf-8')
assert f'source_sha256[]="{fingerprint}"' in configured
(out/'compiler-input-proof.json').write_bytes((json.dumps(dict(source=revision,compiler_inputs=len(inputs),source_tree_sha256=fingerprint,files=proof),indent=2)+'\n').encode())
files=['gridfix169-before-tests.xml','gridfix169-after-tests.xml','gridfix169-auditor-tests.xml','gridfix169-native-tests.xml','gridfix169-native-tests.log','gridfix169-installed-headless-tests.xml','gridfix169-desktop-tests.xml','gridfix169-valid-typing.log','gridfix169-all-invalid-typing.log','gridfix169-first-package.log','gridfix169-final-package.log','gridfix169-first-installed.json','gridfix169-final-installed.json']
for name in files:shutil.copyfile(root/'build'/name,out/name)
shutil.copyfile(build/'generated/build_manifest.hpp',out/'build_manifest.hpp');shutil.copyfile(build/'CMakeCache.txt',out/'CMakeCache.txt');shutil.copyfile(__file__,out/'record_evidence.py')
for label in ('before.cpp','after.cpp','before.log','after.log','result.json'):shutil.copyfile(root/'build/gridfix169-portability'/label,out/('darwin-alias-'+label))
shutil.copyfile(root/'build/gridci168-linux-stubs/stub-difference.patch',out/'linux-stub-difference.patch')
for src,label in [(root/'build/gridci168-hosted/artifacts/Linux-headless-py3.12/stubs.log','linux-stubs-failure.log'),(root/'build/gridci168-hosted/artifacts/macos-native-x86_64/build.log','macos-x86-build-failure.log')]:shutil.copyfile(src,out/label)
shutil.copyfile(root/'build/gridci168-hosted/artifacts/macos-native-arm64/build.log',out/'macos-arm-build-failure.log')
for p in (root/'build/gridci168-hosted/failure-summaries').glob('*.log'):shutil.copyfile(p,out/('failed-'+p.name))
shutil.copyfile(root/'build/ci/gridfrontends167-retained-native/artifact-inventory.json',out/'previous-native-artifact-inventory.json')
def cohort(name,count,skipped=0):
    values=list(ET.parse(out/name).getroot().iter('testcase'));assert len(values)==count+skipped
    assert all(c.find(t) is None for c in values for t in ('failure','error'));assert sum(c.find('skipped') is not None for c in values)==skipped
    return values
native=cohort('gridfix169-native-tests.xml',50);headless=cohort('gridfix169-installed-headless-tests.xml',108,2);desktop=cohort('gridfix169-desktop-tests.xml',50);cohort('gridfix169-auditor-tests.xml',193);cohort('gridfix169-after-tests.xml',2)
prior=list(ET.parse(out/'gridfix169-before-tests.xml').getroot().iter('testcase'));assert len(prior)==2 and all(c.find('failure') is not None for c in prior)
spec=importlib.util.spec_from_file_location('grid',source/'tools/ci/scientific_grid_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source_proof=m.source_contract(source,revision);(out/'source-proof.json').write_bytes((json.dumps(source_proof,indent=2)+'\n').encode())
checks=dict(native=m.native(native),headless=m.python(headless,False),installed_gui=m.python([c for c in headless if not c.get('classname','').endswith('test_grid_cell_frontends')]+desktop,True),positive_typing=m.positive_typing(out/'gridfix169-valid-typing.log'))
errors=collections.Counter((m[0],int(m[1])) for m in re.findall(r'(\w+\.py):(\d+): error:',(out/'gridfix169-all-invalid-typing.log').read_text(encoding='utf-8-sig')))
expected=collections.Counter({(name,line):1 for name,lines in {'invalid_api.py':[6,7,8,9,11,13,14],'qptma_invalid.py':[4,5,6],'hierarchy_invalid.py':[4,5,7],'scientific_grid_invalid.py':[4,6,7,8,9,10],'scientific_channels_invalid.py':[4,6,7,8,9],'grid_cells_invalid.py':[3,4,5,6,7,8]}.items() for line in lines});expected[('grid_cells_invalid.py',3)]=2;assert errors==expected
installed=json.loads((out/'gridfix169-final-installed.json').read_text(encoding='utf-8'));assert installed['source_tree_sha256']==fingerprint and installed['packaged_sources_and_stubs_match'] and installed['native_bytes_unchanged_after_stub_packaging']
receipt=dict(schema='qpx.scientific-grid-portability-fix-local/1',source=subprocess.check_output(['git','rev-parse',revision],cwd=source,text=True).strip(),failed_source='5ebdb961e7c9c2bad8c82367ad090eb23ba4ec57',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_tree_sha256=fingerprint,compiler_inputs=len(inputs),compiled_revision=re.search(r'git_revision\[\]="([^"]+)"',configured)[1],native_passed=50,installed_headless_passed=108,declared_desktop_skips=2,desktop_cohort_passed=50,auditor_and_stub_guard_passed=193,regressions_failed_before=2,regressions_passed_after=2,checks=checks,installed=installed,
 fixes=['Register the nanobind string caster for ScientificRgbGrid.decoded_source_sha256','Register the optional caster for TissueResult.scientific_grid','Separate size_t and uint64_t auto declarations in QCL5 decoding for Darwin'],
 scope='Local Windows installed binding/metadata and archive regression checks. A WSL probe using the retained Linux wheel reproduced the stub difference; a distinct-unsigned-alias compile probe isolates the Darwin declaration failure. Neither is a corrected Linux/macOS platform qualification.',
 previous_artifacts='All 56 previous runnable artifacts were hash-verified and copied to build/ci/gridfrontends167-retained-native before reusing the active incremental CMake cache. Old installed environments, wheels, raw logs and frozen failed source remain intact.',
 source_proof_note='Native compilation preceded the source commit. The dirty compiled revision is preserved verbatim; all compiler inputs match the source commit modulo recorded Git line endings and exactly match the configured raw-byte fingerprint.',
 platform_campaign_accepted=False,scientific_detector_accepted=False,release_ready=False,remaining=['Freeze corrected candidate and rerun complete engine/macOS-native/portable cohorts','Retain all failed source168 artifacts; do not transfer its passing job observations as source169 qualification','All remaining M0-M7 scientific, typed/vendor/pyramid/tiled, hardware, rights/licensing and supported-release requirements'])
receipt['native_artifacts']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted((build/'Release').iterdir()) if p.suffix in ('.dll','.pyd') or (p.name.startswith('qupathxx_') and p.suffix=='.exe')}
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/scientific-grid-portability-fix-local-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps(dict(source=receipt['source'],fingerprint=fingerprint,native_passed=50,headless=108,desktop=50,platform_campaign_accepted=False),indent=2))
