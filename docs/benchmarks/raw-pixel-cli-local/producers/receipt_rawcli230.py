from pathlib import Path
import collections,datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawcli230-final';off=r/'build/rawcli230-backend-off'
m=json.loads((out/'compiler-inputs.json').read_text(encoding='utf-8'));source=m['source']
assert source==subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in m['files'].items())
cases=list(E.parse(out/'python-tests.xml').iter('testcase'));native=list(E.parse(out/'native-tests.xml').iter('testcase'))
assert len(cases)==105 and all(c.find(x) is None for c in cases for x in ('failure','error'))
cohorts=dict(collections.Counter(c.attrib['classname'] for c in cases));assert cohorts=={'tests.test_raw_pixel_cli':15,'tests.test_raw_pixel_python':10,'tests.test_pixel_stream_cli':46,'tests.test_project_pixel_batch':25,'tests.test_raw_batch':4,'tests.test_public_stubs':5}
skips=[dict(classname=c.attrib['classname'],name=c.attrib['name'],reason=c.find('skipped').attrib['message']) for c in cases if c.find('skipped') is not None]
assert len(skips)==1 and skips[0]['name']=='test_cli_training_backend_off_refuses_before_output'
assert {c.attrib['name'] for c in native}=={'pixel_workflows','pixel_features','classifier'} and all(c.find(x) is None for c in native for x in ('failure','error','skipped'))
commands=json.loads((out/'verification-commands.json').read_text(encoding='utf-8'));assert all(c['exit']==c['expected'] for c in commands)
proof=json.loads((out/'installed-proof.json').read_text(encoding='utf-8'));assert proof['source']==source and proof['fingerprint']==m['fingerprint']
off_proof=json.loads((off/'backend-off-proof.json').read_text(encoding='utf-8'));assert off_proof['no_outputs'] and off_proof['source_project_unchanged'] and off_proof['exit']==1
assert json.loads((off/'compiler-inputs.json').read_text(encoding='utf-8'))['fingerprint']==m['fingerprint']
dest=s/'docs/benchmarks/raw-pixel-cli-local';dest.mkdir(exist_ok=False);payloads=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);data=target.read_bytes();assert data==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
def save(name,data):
 p=r/'build'/name;p.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n');return p
for directory,label in [(out,'final'),(off,'backend-off'),(r/'build/rawcli230-development','development'),(r/'build/rawcli230-development-b','development-b')]:
 for p in sorted(directory.iterdir()):
  if p.is_file():copy(p,dest/label/p.name)
cache=r/'build/ci/rawtraining218-windows';off_cache=r/'build/ci/gridfrontends167-package'
for label,c in [('training-on',cache),('training-off',off_cache)]:
 header=c/'generated/build_manifest.hpp';text=header.read_text(encoding='utf-8');assert source in text and m['fingerprint'] in text;copy(header,dest/label/'build_manifest.hpp')
 raw=(c/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode().splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
 assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if label=='training-on' else 'OFF')
 copy(save('rawcli230-'+label+'-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),dest/label/'configuration.json')
sdk_manifest=r/'build/deps/opencv-sdk/qupathxx-opencv-sdk.json';sdk=json.loads(sdk_manifest.read_text(encoding='utf-8'));assert len(sdk['files'])==195 and all(hashlib.sha256((sdk_manifest.parent/n).read_bytes()).hexdigest()==d for n,d in sdk['files'].items());copy(sdk_manifest,dest/'opencv-sdk-manifest.json')
# Bind every declared compiler input and the actual test consumers to Git blobs.
consumers=['tests/'+n for n in ['test_raw_pixel_cli.py','test_raw_pixel_python.py','test_pixel_stream_cli.py','test_project_pixel_batch.py','test_raw_batch.py','test_public_stubs.py']]+['tools/generate_stubs.py']
paths=list(dict.fromkeys(list(m['files'])+consumers));batch=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in paths).encode(),capture_output=True,check=True).stdout
at=0;files=[]
for name in paths:
 end=batch.index(b'\n',at);header=batch[at:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);at=end+1;blob=batch[at:at+size];at+=size;assert batch[at:at+1]==b'\n';at+=1
 data=(s/name).read_bytes();assert blob.replace(b'\r\n',b'\n')==data.replace(b'\r\n',b'\n');files.append(dict(path=name,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=hashlib.sha256(data).hexdigest(),crlf_normalized=blob!=data))
assert at==len(batch);copy(save('rawcli230-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],files=files)),dest/'source-proof.json')
retained=r/'build/rawcli230-retained-binaries';retained.mkdir(exist_ok=False);products=[]
original=list((cache/'Release').glob('_native*.pyd'))+[cache/'Release'/n for n in ('qupathxx_region.exe','qupathxx_batch.exe','qupathxx_pixel_workflow_tests.exe','qupathxx_pixel_feature_tests.exe','qupathxx_classifier_tests.exe')]+list((out/'wheel').glob('*.whl'))
for path in original+[off_cache/'Release/qupathxx_batch.exe']:
 target=retained/('backend-off' if path.parent==off_cache/'Release' else 'training-on')/path.name;target.parent.mkdir(exist_ok=True);shutil.copy2(path,target);assert target.read_bytes()==path.read_bytes();products.append(dict(path=target.relative_to(r).as_posix(),source=source,fingerprint=m['fingerprint'],bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
assert len(products)==8;copy(save('rawcli230-retained-products.json',dict(products=products)),dest/'retained-products.json')
for name in ('build_rawcli230.py','build_rawcli230_b.py','build_rawcli230_off.py','rawcli230_backend.py','build_rawcli230_wheel.py','verify_rawcli230_final.py','rawcli230_installed_proof.py','rawcli230_backend_off_proof.py','receipt_rawcli230.py'):
 copy(r/'build/ci'/name,dest/'producers'/name)
receipt=dict(schema='qpx.raw-pixel-cli-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='windows_cp312_installed_raw_cli_locally_verified',installed_python_passes=104,installed_skips=skips,cohorts=cohorts,new_raw_cli_cases=[c.attrib['name'] for c in cases if c.attrib['classname']=='tests.test_raw_pixel_cli' and c.find('skipped') is None],native_suites=3,backend_off_witness=off_proof, wheel=dict(bytes=proof['wheel_bytes'],sha256=proof['wheel_sha256'],members=len(proof['wheel_members'])),scope=[
 'Official native --train-raw-pixels, --export-raw-pixels and --verify-raw-pixels reuse tiled requests, training checkpoints, actual forest backend, owner file jobs and native archives/project retention',
 'Installed Unicode TIFF train/predict/checkpoint/resume workflows preserve input project bytes; recorded model bytes and training rows match independent installed Python APIs',
 'Prediction labels/support/votes, input/result chains and exact retained header/block prefix match; independent fresh-process provenance clocks/generations may differ',
 'Source/project/model/hardlink/product aliases, model-lattice mismatch, changed resume request, tampering, argument bounds and process cancellation/refusal are distinguishing controls',
 'Backend-OFF native executable rejects valid project training before producing files; original source project is preserved',
 'Independent producer, checkpoint metadata, file metadata and forest budgets avoid rejecting valid small producer requests',
 'Installed wheel/package/runtime source and native test products bind the same compiler fingerprint; 195 pinned OpenCV SDK files verify'],
 development=['Initial build succeeded; first test collection failed because the fixture was imported as a nonexistent tests package','Initial CLI coupled work and checkpoint metadata limits; two positive workflows correctly refused, and budgets were separated','Fresh-export test first used the wrong argument index, then incorrectly compared independent provenance bytes; corrected to exact committed-prefix preservation plus numeric/input/result equality without changing product formats','All original failed logs/XML retain their own compiler input/source snapshots'],
 limitations=['Windows CPython 3.12 training/TIFF/project ON, GUI/GEOS/OpenSlide OFF installed profile only; separate training-OFF native witness is not an installed OFF package qualification','Consumer uses a fresh wheel package with workspace test dependencies through .pth; no clean-machine claim','Optional prediction project retention currently performs another bounded full-file verification after the save job; no IO/performance improvement is claimed','Multi-image raw-classifier batches, ImGui/managed viewer, bounded contour frontend apply, broader installed profiles and T5 integrated qualification remain open','Cross-tile contours, automatic maintenance, abrupt-process/power-loss and scientific/resource/hardware/release gates remain open'],raw_payloads=payloads,retained_artifacts=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-pixel-cli-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_products=len(products),python_passes=104,native_passes=3)))
