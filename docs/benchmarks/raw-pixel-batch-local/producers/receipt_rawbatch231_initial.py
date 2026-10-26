from pathlib import Path
import ast,collections,datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawbatch231-final';off=r/'build/rawbatch231-backend-off';cache=r/'build/ci/rawtraining218-windows';off_cache=r/'build/ci/gridfrontends167-package'
m=json.loads((out/'compiler-inputs.json').read_text(encoding='utf-8'));source=m['source'];assert source==subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip();assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in m['files'].items())
cases=list(E.parse(out/'python-tests.xml').iter('testcase'));native=list(E.parse(out/'native-tests.xml').iter('testcase'));off_native=list(E.parse(off/'native-tests.xml').iter('testcase'))
cohorts=dict(collections.Counter(c.attrib['classname'] for c in cases));assert cohorts=={'tests.test_raw_pixel_batch':14,'tests.test_project_batch':22,'tests.test_raw_pixel_cli':15,'tests.test_raw_pixel_python':10,'tests.test_pixel_stream_cli':46,'tests.test_project_pixel_batch':25,'tests.test_raw_batch':4,'tests.test_public_stubs':5}
assert len(cases)==141 and all(c.find(x) is None for c in cases for x in ('failure','error'))
skips=[dict(classname=c.attrib['classname'],name=c.attrib['name'],reason=c.find('skipped').attrib['message']) for c in cases if c.find('skipped') is not None]
assert {v['name'] for v in skips}=={'test_digest_coarse_grid_independent[1]','test_digest_coarse_grid_independent[2]','test_optional_cells_published_to_checkpoint_only','test_checkpoint_opens_in_existing_viewer','test_cli_training_backend_off_refuses_before_output'}
raw=[c for c in cases if c.attrib['classname']=='tests.test_raw_pixel_batch'];assert len(raw)==14 and all(c.find('skipped') is None for c in raw)
assert {c.attrib['name'] for c in native}=={'pixel_workflows','pixel_features','classifier','project_batch'} and len(off_native)==1
for cohort in (native,off_native):
 assert all(c.find(x) is None for c in cohort for x in ('failure','error','skipped'))
 batch=next(c for c in cohort if c.attrib['name']=='project_batch');assert 'Raw pixel batch: portable model, exact input reuse, QWL1 verification, interruption and owned cleanup passed' in batch.findtext('system-out')
for kind,count,errors in [('valid',3,0),('invalid',2,7)]:
 report=json.loads((out/('typing-'+kind+'.log')).read_text(encoding='utf-8'));assert report['summary']['filesAnalyzed']==count and report['summary']['errorCount']==errors
assert all(v['exit']==v['expected'] for v in json.loads((out/'verification-commands.json').read_text(encoding='utf-8')))
proof=json.loads((out/'installed-proof.json').read_text(encoding='utf-8'));off_proof=json.loads((off/'portable-backend-off-proof.json').read_text(encoding='utf-8'));assert proof['source']==off_proof['source']==source and proof['fingerprint']==off_proof['fingerprint']==m['fingerprint'];assert off_proof['saved']==off_proof['reused']==2 and off_proof['verified_numeric_blocks']==27
assert json.loads((off/'compiler-inputs.json').read_text(encoding='utf-8'))['fingerprint']==m['fingerprint']
dest=s/'docs/benchmarks/raw-pixel-batch-local';dest.mkdir(exist_ok=False);payloads=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);data=target.read_bytes();assert data==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
def save(name,data):
 p=r/'build'/name;p.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n');return p
for directory,label in [(out,'final'),(off,'backend-off'),(r/'build/rawbatch231-development','development'),(r/'build/rawbatch231-development-b','development-b')]:
 for p in sorted(directory.iterdir()):
  if p.is_file():copy(p,dest/label/p.name)
for label,c in [('training-on',cache),('training-off',off_cache)]:
 header=c/'generated/build_manifest.hpp';text=header.read_text(encoding='utf-8');assert source in text and m['fingerprint'] in text;copy(header,dest/label/'build_manifest.hpp')
 data=(c/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in data.decode().splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line};assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if label=='training-on' else 'OFF');copy(save('rawbatch231-'+label+'-configuration.json',dict(cache_sha256=hashlib.sha256(data).hexdigest(),flags=flags)),dest/label/'configuration.json')
sdk_path=r/'build/deps/opencv-sdk/qupathxx-opencv-sdk.json';sdk=json.loads(sdk_path.read_text(encoding='utf-8'));assert len(sdk['files'])==195 and all(hashlib.sha256((sdk_path.parent/n).read_bytes()).hexdigest()==d for n,d in sdk['files'].items());copy(sdk_path,dest/'opencv-sdk-manifest.json')
# Follow actual test-module imports rather than recording only top-level files.
consumers={c.attrib['classname'].replace('.','/')+'.py' for c in cases};pending=list(consumers);modules={p.stem:p.relative_to(s).as_posix() for p in (s/'tests').glob('*.py')}
while pending:
 name=pending.pop();tree=ast.parse((s/name).read_text(encoding='utf-8'))
 for node in ast.walk(tree):
  names=[node.module.split('.')[0]] if isinstance(node,ast.ImportFrom) and node.module else [v.name.split('.')[0] for v in node.names] if isinstance(node,ast.Import) else []
  for module in names:
   path=modules.get(module)
   if path and path not in consumers:consumers.add(path);pending.append(path)
consumers.update(['tools/generate_stubs.py','examples/raw_classifier_workflow.py','tests/data/pixel-streams/rectangle-v1-windows.qps','src/qupathxx/_native.pyi','src/qupathxx/__init__.pyi']+['tests/typing/'+n for n in ('raw_pixel_valid.py','raw_pixel_invalid.py','raw_batch_valid.py','raw_batch_invalid.py')])
paths=list(dict.fromkeys(list(m['files'])+sorted(consumers)));raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in paths).encode(),capture_output=True,check=True).stdout;at=0;entries=[]
for name in paths:
 end=raw.index(b'\n',at);header=raw[at:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);at=end+1;blob=raw[at:at+size];at+=size;assert raw[at:at+1]==b'\n';at+=1;data=(s/name).read_bytes();assert blob.replace(b'\r\n',b'\n')==data.replace(b'\r\n',b'\n');entries.append(dict(path=name,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=hashlib.sha256(data).hexdigest(),crlf_normalized=blob!=data))
assert at==len(raw);copy(save('rawbatch231-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],files=entries,test_consumer_closure=sorted(consumers))),dest/'source-proof.json')
retained=r/'build/rawbatch231-retained-binaries';retained.mkdir(exist_ok=False);products=[]
def retain(path,label,identity):
 target=retained/label/path.name;target.parent.mkdir(exist_ok=True);shutil.copy2(path,target);assert target.read_bytes()==path.read_bytes();products.append(dict(path=target.relative_to(r).as_posix(),source=identity['source'],fingerprint=identity['fingerprint'],bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for p in list((cache/'Release').glob('_native*.pyd'))+[cache/'Release'/n for n in ('qupathxx_region.exe','qupathxx_batch.exe','qupathxx_project_batch_tests.exe','qupathxx_pixel_workflow_tests.exe','qupathxx_pixel_feature_tests.exe','qupathxx_classifier_tests.exe')]+list((out/'wheel').glob('*.whl')):retain(p,'training-on',m)
for n in ('qupathxx_batch.exe','qupathxx_project_batch_tests.exe'):retain(off_cache/'Release'/n,'training-off',m)
dev=r/'build/rawbatch231-development';old=json.loads((dev/'compiler-inputs.json').read_text(encoding='utf-8'))
for p in list((dev/'package/qupathxx').glob('_native*.pyd'))+[dev/'package/qupathxx/qupathxx_batch.exe']:retain(p,'original-development',old)
assert len(products)==12;copy(save('rawbatch231-retained-products.json',dict(products=products)),dest/'retained-products.json')
for name in ('build_rawbatch231.py','build_rawbatch231_b.py','build_rawbatch231_off.py','rawbatch231_backend.py','build_rawbatch231_wheel.py','verify_rawbatch231_final.py','rawbatch231_installed_proof.py','rawbatch231_off_proof.py','rawbatch231_dev_runner.py','rawbatch231_dev_runner_b.py','receipt_rawbatch231.py'):copy(r/'build/ci'/name,dest/'producers'/name)
receipt=dict(schema='qpx.raw-pixel-batch-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='windows_cp312_installed_raw_project_batches_locally_verified',installed_python_passes=136,installed_skips=skips,cohorts=cohorts,new_raw_batch_cases=[c.attrib['name'] for c in raw],native_training_on_suites=4,native_training_off_suites=1,portable_backend_off=off_proof,positive_typing_files=3,negative_typing_files=2,negative_typing_errors=7,wheel=dict(bytes=proof['wheel_bytes'],sha256=proof['wheel_sha256'],members=len(proof['wheel_members'])),scope=[
 'Additive RawPixels/RAW_PIXELS and --kind raw-pixels reuse ProjectBatchRequest/kernel, snapshots, owned output bundles, SQLite checkpoint seals, reports and Stop/Continue; old Raw/RGB enum meanings stay unchanged',
 'Typed model, exact optional axes, bounded native stream policy and independent file metadata budget are shared by C++/nanobind/Python/native CLI',
 'Actual TIFF numeric blocks/votes/support match direct prediction across different source dimensions with unit spacing and explicit fractional axes',
 'Completed-image reuse hashes current raw inputs without inference and verifies every saved QWS1 file and QWL1 descriptor; mixed raw/RGB catalogs select tags correctly',
 'Changed source data, incompatible reader/model/lattice, missing/tampered output or checkpoint, quotas and no-resume preservation have distinguishing controls',
 'Native interruption retains completed images, resumes them and processes the suffix; unknown files survive owned cleanup; CLI/project relocation and immutable asynchronous project snapshots pass',
 'An actual OpenCV-trained model is predicted and reused by the training-OFF native executable; all 27 blocks match the installed ON consumer',
 'Generated nullable options/runtime surfaces and positive/negative typing match the installed wheel; all 195 SDK files and imported wheel members authenticate'],
 development=['First native fixture reused a Document in two slots and correctly refused; fixed to independent Documents without changing ownership rules','First relocation fixture compared normal and Windows extended path spelling; fixed to compare actual file identity before mapping the relative bundle','Original development has 64 Python passes/one relocation failure/one skip and three passing native regressions/one ownership fixture failure; corrected source ec46efe has 67 Python passes/one skip and a passing affected native batch suite','First stub-generation launch used a parent-only sys.path injection that its child did not inherit and produced no new declarations; final development interpreter package origin and installed-wheel stub check establish the actual runtime'],
 limitations=['Installed Windows CPython 3.12 training/TIFF/project ON, GUI/GEOS/OpenSlide OFF only; separate backend-OFF checks are native, not an OFF wheel/platform qualification','Fresh wheel package reuses workspace test dependencies through .pth; no clean-machine or release claim','Batch targets are area objects; each raw output traverses its declared global lattice and masks its target ROI. Sparse ROI prediction traversal and process-memory/performance qualification are not claimed','Restart/reuse is at completed-image checkpoints; interrupted current-image work is owned temporary output. Per-file prefix resume remains the separate CLI/native job workflow','The first failing native fixture executable was not separately retained before cache reuse; its original log/XML/source manifest are retained along with original module/CLI products','Desktop/managed viewer, bounded contour frontend apply, broader installed profiles, T5, streamed stitching and scientific/resource/hardware/release gates remain open'],raw_payloads=payloads,retained_artifacts=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-pixel-batch-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),products=len(products),python_passes=136,native_on=4,native_off=1)))
