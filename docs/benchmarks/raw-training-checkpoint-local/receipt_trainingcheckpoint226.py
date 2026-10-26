from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';hosts=('windows','linux','training')
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip();manifests=[json.loads((r/f'build/trainingcheckpoint226-{h}/compiler-inputs.json').read_text()) for h in hosts];assert all(m==manifests[0] for m in manifests);m=manifests[0];assert m['source']==source
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
markers=[
 'Raw training checkpoint: real TIFF project close/reopen, fresh replay, optional trained model, calibration/file and source-alias refusal passed',
 'Raw training checkpoint: independent QTC1 bounds, balanced/unbalanced multistage replay, exact rows/hashes/bytes, input tamper, limits, cancellation and concurrent-writer refusal passed',
 'Raw tiled archive: independent QTT1 geometry/rows, balanced/unbalanced and huge-lattice round trips, semantic tamper, all-selected record bounds, authority/replay and atomic files passed',
 'Raw tiled training: full/tiled global reservoir rows, filtered seams, sparse trillion-cell lattice, quotas, owner lifecycle, calibration and compound-source controls passed',
]
backend_marker='Raw training checkpoint: actual backend uninterrupted/resumed table and model bytes passed'

destination=s/'docs/benchmarks/raw-training-checkpoint-local';destination.mkdir(exist_ok=True);retained=r/'build/trainingcheckpoint226-retained-binaries';retained.mkdir(exist_ok=True);payloads=[];binaries=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
targets=dict(pixel_features='pixel_feature',pixel_workflows='pixel_workflow',resampling='resampling',raw_image='raw_image',source_identity='source_identity',project='project',project_save='save',provenance='provenance',classifier='classifier')
for host in hosts:
 folder=r/f'build/trainingcheckpoint226-{host}';assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
 cases=list(ET.parse(folder/'tests.xml').iter('testcase'));names=set(targets)-{'classifier'} if host!='training' else {'pixel_features','pixel_workflows','classifier'}
 assert len(cases)==len(names) and {c.attrib['name'] for c in cases}==names and all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
 output='\n'.join(c.findtext('system-out') or '' for c in cases)
 for marker in markers:assert output.count(marker)==1,(host,marker)
 assert output.count(backend_marker)==(2 if host=='training' else 0)
 assert output.count('Raw tiled training: actual backend full/tiled matrix, portable model bytes and prediction votes passed')==(1 if host=='training' else 0)
 assert json.loads((folder/'output-preflight.json').read_text())['matches_run']
 cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host]);header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text();copy(header,destination/host/'build_manifest.hpp')
 raw=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line};assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if host=='training' else 'OFF')
 copy(save(f'trainingcheckpoint226-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),destination/host/'configuration.json')
 for p in sorted(folder.iterdir()):
  if p.is_file():copy(p,destination/host/p.name)
 for name in sorted(names):
  p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+targets[name]+'_tests'+('' if host=='linux' else '.exe'));target=retained/host/p.name;target.parent.mkdir(exist_ok=True);shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes();binaries.append(dict(host=host,suite=name,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n,d in m['files'].items():
 end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1;checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n');proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=d,crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save('trainingcheckpoint226-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],framed_batch_objects=len(proof),files=proof)),destination/'compiler-git-proof.json')
copy(save('trainingcheckpoint226-retained-binaries.json',dict(source=source,binaries=binaries)),destination/'retained-binaries.json')
for name in ('build_trainingcheckpoint226.py','receipt_trainingcheckpoint226.py'):copy(r/'build/ci'/name,destination/name)
receipt=dict(schema='qpx.raw-training-checkpoint-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='deterministic_bounded_training_prefix_checkpoint_replay_locally_verified',native=dict(windows_suites=8,linux_suites=8,windows_training_suites=3,required_markers=markers,backend_marker=backend_marker,backend_marker_count=2),scope=[
 'Distinct bounded QTC1 historical training prefix commitments; no serialized reservoir, hash implementation state or restored live authority',
 'Request identity binds recipe, original lattice/axes, ordered immutable annotation IDs/revisions/kinds/classes/geometry, sampling/tile/work policy, source reader/metadata epoch and effective calibration',
 'Replay traverses completed cores through the same sampler/features/global reservoir and original input-hash grammar; validates cumulative prefix before advancing',
 'Balanced/unbalanced multistage resumed rows, labels, owners, coordinates, counts, input/result hashes and final checkpoint bytes equal uninterrupted execution',
 'Actual Windows backend model bytes agree after sampling resume with identical explicitly supplied effective parameters; forest fitting itself is not checkpointed',
 'Real TIFF project close/reopen resumes under fresh capture with original file observations and independently reconstructed complete table',
 'Empty prefix performs no pixel IO; partial prefixes publish no table; completed-prefix resume replays all cores instead of trusting a historical completion flag',
 'Independent little-endian framing/count controls, rehashed prefix tamper, changed decoded input, request/calibration/file mismatch, truncation, quotas and cancellation controls',
 'Atomic explicit checkpoint publication protects prior output on failure/cancellation and a competing newly created destination even when replacement is allowed; source paths/hardlink aliases refuse output'],
 limitations=['Resume repeats prefix decoding and feature extraction; memory is bounded but IO/time are not constant','Synchronous native API; owner file jobs, retained prediction-file references, installed nanobind/Python/CLI/batch/ImGui/viewer adapters remain open','Caller trains the completed table with explicit forest parameters; no interrupted forest-fitting state is saved','Automatic checkpoint scheduling/maintenance, abrupt-process/power-loss, integrated platform, biological/model, process-resource/hardware and release acceptance remain open'],raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-training-checkpoint-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_binaries=len(binaries))))
