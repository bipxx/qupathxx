from pathlib import Path
import datetime, hashlib, json, re, shutil, subprocess, xml.etree.ElementTree as ET

root=Path('C:/Users/HEQ/Projects/qupathxx')
source=root/'build/ci/scientific-grid-159'
build=root/'build/ci/gridarchive165-native'
out=source/'docs/benchmarks/grid-cell-archive-local'
out.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()
inputs=sorted(set([p for pattern in ('cpp/**/*.cpp','cpp/**/*.hpp','cmake/**/*.cmake','cmake/**/*.in','src/**/*.py') for p in source.glob(pattern)]+[source/'CMakeLists.txt',source/'pyproject.toml']),key=lambda p:p.as_posix())
requests=''.join(f'{revision}:{p.relative_to(source).as_posix()}\n' for p in inputs).encode()
proc=subprocess.run(['git','cat-file','--batch'],input=requests,cwd=source,stdout=subprocess.PIPE,check=True)
raw=proc.stdout;offset=0;manifest='';proof=[]
for path in inputs:
    stop=raw.index(b'\n',offset);header=raw[offset:stop].decode().split();assert header[1]=='blob',header
    size=int(header[2]);blob=raw[stop+1:stop+1+size];assert raw[stop+1+size:stop+2+size]==b'\n'
    offset=stop+2+size
    actual=path.read_bytes();assert actual==blob or actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),path
    name=path.relative_to(source).as_posix();digest=sha(actual);manifest+=f'{name}={digest}\n'
    proof.append(dict(path=name,checkout_sha256=digest,git_blob_sha256=sha(blob),line_endings_only=actual!=blob))
assert offset==len(raw)
fingerprint=sha(manifest.encode())
configured=(build/'generated/build_manifest.hpp').read_text(encoding='utf-8')
assert f'source_sha256[]="{fingerprint}"' in configured,fingerprint
(out/'compiler-input-proof.json').write_bytes((json.dumps(dict(source=revision,compiler_inputs=len(inputs),source_tree_sha256=fingerprint,files=proof),indent=2)+'\n').encode())
for name in ('gridarchive165-exact-build.log','gridarchive165-exact-tests.log','gridarchive165-exact-tests.xml','gridarchive165-contract-tests.xml'):
    shutil.copyfile(root/'build'/name,out/name)
shutil.copyfile(build/'generated/build_manifest.hpp',out/'build_manifest.hpp');shutil.copyfile(build/'CMakeCache.txt',out/'CMakeCache.txt');shutil.copyfile(__file__,out/'record_evidence.py')
native=list(ET.parse(out/'gridarchive165-exact-tests.xml').getroot().iter('testcase'));pure=list(ET.parse(out/'gridarchive165-contract-tests.xml').getroot().iter('testcase'))
for cases,count in ((native,49),(pure,30)):
    assert len(cases)==count
    assert all(c.find('failure') is None and c.find('error') is None and c.find('skipped') is None for c in cases)
markers=['Independent Python struct/hashlib QCL5 bytes decode and canonical re-encode passed',
         'QCL5 SQLite save, reopen, provenance attachment, replay and second save passed',
         'QCL5 analytic ellipse, polygon hole and winding-policy geometry persistence passed',
         'QCL5 canonical encoding, semantic rejection, captured calibration, record authority and replay passed',
         'Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed']
text=(out/'gridarchive165-exact-tests.xml').read_text(encoding='utf-8');assert all(m in text for m in markers)
cache=(out/'CMakeCache.txt').read_text(encoding='utf-8');configuration={}
for key in ('PYTHON','GUI','PROJECT','INTERCHANGE','GEOS','OPENSLIDE','TIFF','CLASSIFIER_TRAINING','TESTS'):
    found=re.search(r'^QUPATHXX_BUILD_'+key+r':BOOL=(ON|OFF)$',cache,re.M)
    if found:configuration[key.lower()]=found[1]=='ON'
assert configuration['geos'] and configuration['project'] and not configuration['python'] and not configuration['gui']
receipt=dict(schema='qpx.grid-cell-archive-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=revision,
    compiled_revision=re.search(r'git_revision\[\]="([^"]+)"',configured)[1],source_tree_sha256=fingerprint,compiler_inputs=len(inputs),
    compiler='MSVC 19.44.35225.0',platform='Windows AMD64',configuration=configuration,native_suites_passed=len(native),independent_contract_tests_passed=len(pure),required_semantic_markers=markers,
    native_qcl5_persistence_verified=True,native_sqlite_reopen_replay_verified=True,embedded_external_provenance_agreement_verified=True,
    geometry_policy_round_trips_verified=['rectangle','analytic ellipse','polygon with hole','polygon winding'],
    independent_byte_vector='tests/fixtures/grid-cell-archive-v1/synthetic-single-cell.qcl',
    independent_vector_scope='Synthetic historical numeric snapshot, packed with Python struct/hashlib; placeholder input hash is not sampled pixels or segmentation acceptance.',
    integrity_scope='Checksums and semantic validation are not cryptographic authorship; fresh replay compares actual input pixels.',
    installed_python_verified=False,qca5_classifier_implemented=False,grid_csv_schema_implemented=False,scientific_detector_accepted=False,platform_campaign_accepted=False,release_ready=False,
    scope='Native QCL5 full-grid/typed-request/resolved/calibration/domain persistence; canonical decoding, semantic rejection, record authority/publication, changed-pixel verification, changed-calibration rejection and real SQLite save/reopen/replay.',
    remaining=['QCA5 classifier feature-source provenance for grid cells','CSV schemas preserving per-axis grid and units','Python/CLI/ImGui/batch exposure and installed rebuild','exact-source platform cohorts','all remaining M0-M7 scientific/hardware/rights/licensing/release criteria'])
receipt['source_proof_note']='Compiled before source commit; configured dirty revision retained verbatim. All compiler inputs match the committed source modulo recorded Git line endings and exactly match the configured raw-byte fingerprint. Final full build relinked all native targets after the last test change; no binary rebuilt solely to change a Git label.'
receipt['native_artifacts']={p.name:dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted((build/'Release').iterdir()) if p.suffix=='.dll' or (p.name.startswith('qupathxx_') and p.name.endswith('_tests.exe'))}
receipt['sqlite_dependency']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in (root/'build/deps/sqlite-amalgamation-3540100').glob('sqlite3.[ch]')}
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/grid-cell-archive-local-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps({k:receipt[k] for k in ('source','source_tree_sha256','compiler_inputs','native_suites_passed','independent_contract_tests_passed')},indent=2))
