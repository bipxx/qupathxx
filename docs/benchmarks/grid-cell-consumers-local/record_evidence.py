from pathlib import Path
import datetime, hashlib, json, re, shutil, subprocess, xml.etree.ElementTree as ET

root=Path('C:/Users/HEQ/Projects/qupathxx');source=root/'build/ci/scientific-grid-159';build=root/'build/ci/gridconsumers166-native'
out=source/'docs/benchmarks/grid-cell-consumers-local';out.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()
inputs=sorted(set([p for pattern in ('cpp/**/*.cpp','cpp/**/*.hpp','cmake/**/*.cmake','cmake/**/*.in','src/**/*.py') for p in source.glob(pattern)]+[source/'CMakeLists.txt',source/'pyproject.toml']),key=lambda p:p.as_posix())
requests=''.join(f'{revision}:{p.relative_to(source).as_posix()}\n' for p in inputs).encode()
proc=subprocess.run(['git','cat-file','--batch'],input=requests,cwd=source,stdout=subprocess.PIPE,check=True)
raw=proc.stdout;offset=0;manifest='';proof=[]
for path in inputs:
    stop=raw.index(b'\n',offset);header=raw[offset:stop].decode().split();assert header[1]=='blob',header
    size=int(header[2]);blob=raw[stop+1:stop+1+size];assert raw[stop+1+size:stop+2+size]==b'\n';offset=stop+2+size
    actual=path.read_bytes();assert actual==blob or actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),path
    name=path.relative_to(source).as_posix();digest=sha(actual);manifest+=f'{name}={digest}\n'
    proof.append(dict(path=name,checkout_sha256=digest,git_blob_sha256=sha(blob),line_endings_only=actual!=blob))
assert offset==len(raw);fingerprint=sha(manifest.encode());configured=(build/'generated/build_manifest.hpp').read_text(encoding='utf-8')
assert f'source_sha256[]="{fingerprint}"' in configured,fingerprint
(out/'compiler-input-proof.json').write_bytes((json.dumps(dict(source=revision,compiler_inputs=len(inputs),source_tree_sha256=fingerprint,files=proof),indent=2)+'\n').encode())
for name in ('gridconsumers166-final-build.log','gridconsumers166-final-tests.log','gridconsumers166-final-tests.xml','gridconsumers166-contract-tests.xml'):
    shutil.copyfile(root/'build'/name,out/name)
shutil.copyfile(build/'generated/build_manifest.hpp',out/'build_manifest.hpp');shutil.copyfile(build/'CMakeCache.txt',out/'CMakeCache.txt');shutil.copyfile(__file__,out/'record_evidence.py')
native=list(ET.parse(out/'gridconsumers166-final-tests.xml').getroot().iter('testcase'));pure=list(ET.parse(out/'gridconsumers166-contract-tests.xml').getroot().iter('testcase'))
for cases,count in ((native,49),(pure,33)):
    assert len(cases)==count
    assert all(c.find('failure') is None and c.find('error') is None and c.find('skipped') is None for c in cases)
markers=['Independent Python QCR1/QCA5 typed grid source bytes decode and canonical re-encode passed',
         'QCA5 mixed grid/natural classifier sources and CSV project save/reopen passed',
         'Grid cell CSV schema 5/6, mixed/manual rows, captured units, QCA5 provenance and upsampled source limits passed',
         'Independent Python struct/hashlib QCL5 bytes decode and canonical re-encode passed',
         'QCL5 SQLite save, reopen, provenance attachment, replay and second save passed']
text=(out/'gridconsumers166-final-tests.xml').read_text(encoding='utf-8');assert all(m in text for m in markers)
cache=(out/'CMakeCache.txt').read_text(encoding='utf-8');configuration={}
for key in ('PYTHON','GUI','PROJECT','INTERCHANGE','GEOS','OPENSLIDE','TIFF','CLASSIFIER_TRAINING','TESTS'):
    found=re.search(r'^QUPATHXX_BUILD_'+key+r':BOOL=(ON|OFF)$',cache,re.M)
    if found:configuration[key.lower()]=found[1]=='ON'
assert configuration['geos'] and configuration['project'] and not configuration['python'] and not configuration['gui']
receipt=dict(schema='qpx.grid-cell-consumers-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=revision,
    compiled_revision=re.search(r'git_revision\[\]="([^"]+)"',configured)[1],source_tree_sha256=fingerprint,compiler_inputs=len(inputs),
    compiler='MSVC 19.44.35225.0',platform='Windows AMD64',configuration=configuration,native_suites_passed=len(native),independent_contract_tests_passed=len(pure),required_semantic_markers=markers,
    qca5_native_sources_verified=True,csv_schema_5_6_verified=True,mixed_natural_grid_sources_verified=True,upsampled_sample_limit_verified=True,
    sqlite_classifier_csv_reopen_verified=True,qcl5_independent_byte_regression_verified=True,installed_python_verified=False,
    binding_scalar_guards_edited_but_not_built=True,scientific_detector_accepted=False,platform_campaign_accepted=False,release_ready=False,
    scope='Native QCA5 typed source capture/canonical persistence, mixed natural and grid manifests, core sample/area limits, backend-free prediction snapshots and project reopening; CSV schema 5/6 preserves explicit grid/units/calibration across mixed/manual/project rows. No-grid formats remain unchanged.',
    independent_vector='tests/fixtures/grid-cell-consumers-v1/synthetic-classifier.qcr',
    independent_vector_scope='Python struct/hashlib protocol snapshot, independent QCL5 descriptor and hand-authored forest; not model training or scientific acceptance.',
    remaining=['typed nanobind grid request/result/source metadata and installed package validation','CLI/ImGui/managed viewer/batch workflow integration','exact-source complete platform cohorts','all remaining M0-M7 scientific/hardware/rights/licensing/release criteria'])
receipt['source_proof_note']='Compiled before source commit; configured dirty revision retained verbatim. All compiler inputs match committed source modulo recorded Git line endings and exactly match the configured raw-byte fingerprint. Final full build relinked every native target after the last compiler-input change; no binary rebuilt solely to change a Git label.'
receipt['native_artifacts']={p.name:dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted((build/'Release').iterdir()) if p.suffix=='.dll' or (p.name.startswith('qupathxx_') and p.name.endswith('_tests.exe'))}
receipt['sqlite_dependency']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in (root/'build/deps/sqlite-amalgamation-3540100').glob('sqlite3.[ch]')}
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/grid-cell-consumers-local-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps({k:receipt[k] for k in ('source','source_tree_sha256','compiler_inputs','native_suites_passed','independent_contract_tests_passed')},indent=2))
