from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';hosts=('windows','linux','training')
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip();manifests=[json.loads((r/f'build/windowmodel221-{h}/compiler-inputs.json').read_text()) for h in hosts];assert all(m==manifests[0] for m in manifests);m=manifests[0];assert m['source']==source
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
markers=[
 'Raw window classifier: real TIFF fractional-window owner prediction, global ROI mask and late-file refusal passed',
 'Raw window classifier: portable recipe/model identity, 42 full/window label-vote seams, huge indexed ROI, physical calibration, pre-IO quotas/series, cancellation/stale/copy/thread and compound-source checks passed',
 'Raw lattice windows: 13 Fraction wide means, 20 large irreducible uint64 samples, huge dense lattice, bounded chunk mutation, admission/cancel and 9072 exact tiled feature slots passed',
 'Raw classifier QPR1 independent framing, channel/grid/physical contracts, forest votes, streams, exact budgets and atomic model files passed',
]
backend_marker='Raw pixel backend training: real TIFF deterministic direct/job training, historical retraining, trained archives/model files and portable prediction votes passed'
destination=s/'docs/benchmarks/raw-window-classifier-local';destination.mkdir(exist_ok=False);retained=r/'build/windowmodel221-retained-binaries';retained.mkdir(exist_ok=False);payloads=[];binaries=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
targets=dict(pixel_features='pixel_feature',pixel_workflows='pixel_workflow',resampling='resampling',raw_image='raw_image',source_identity='source_identity',project='project',project_save='save',provenance='provenance',classifier='classifier')
for host in hosts:
 folder=r/f'build/windowmodel221-{host}';assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
 cases=list(ET.parse(folder/'tests.xml').iter('testcase'));names={'pixel_features','pixel_workflows','classifier'} if host=='training' else set(targets)-{'classifier'}
 assert len(cases)==len(names) and {c.attrib['name'] for c in cases}==names and all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
 output='\n'.join(c.findtext('system-out') or '' for c in cases)
 for marker in markers:assert output.count(marker)==1,(host,marker)
 assert output.count(backend_marker)==(1 if host=='training' else 0)
 assert json.loads((folder/'output-preflight.json').read_text())['matches_run']
 cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host]);header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text();copy(header,destination/host/'build_manifest.hpp')
 raw=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line};assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if host=='training' else 'OFF')
 copy(save(f'windowmodel221-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),destination/host/'configuration.json')
 for p in sorted(folder.iterdir()):
  if p.is_file():copy(p,destination/host/p.name)
 for name in sorted(names):
  p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+targets[name]+'_tests'+('' if host=='linux' else '.exe'));target=retained/host/p.name;target.parent.mkdir(exist_ok=True);shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes();binaries.append(dict(host=host,suite=name,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n,d in m['files'].items():
 end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1;checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n');proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=d,crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save('windowmodel221-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],framed_batch_objects=len(proof),files=proof)),destination/'compiler-git-proof.json')
copy(save('windowmodel221-retained-binaries.json',dict(source=source,binaries=binaries)),destination/'retained-binaries.json')
for name in ('build_windowmodel221.py','receipt_windowmodel221.py'):copy(r/'build/ci'/name,destination/name)
receipt=dict(schema='qpx.raw-window-classifier-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='window_aware_raw_models_classification_and_owner_prediction_locally_verified',native=dict(windows_suites=8,linux_suites=8,windows_training_suites=3,required_markers=markers,backend_marker=backend_marker),scope=['RawPixelRecipe captures/matches global window sampling ratios and preserves existing QXS1/QRM1 bytes','Distinct owning RawPixelWindowClassification shares existing schema checks, bounded feature plan and forest vote batching','42 local full/window support-label-vote seam comparisons, plus inherited 9072 feature comparisons and Fraction references','Owner prediction on a million-by-million global lattice masks actual global sample centers','Real TIFF window owner job and fresh primary/companion file refusal after worker completion','Document calibration overrides preserved separately from actual raw metadata epoch','Owner-thread controller, cross-thread cancellation, borrowed reader lifetime, cancellation/stale/error retry and copied-document refusal','Captured metadata payload admitted before copying; actual sampler quotas capped by remaining owner budget and series checked before IO for window and full-grid jobs'],model_recipe_and_legacy_archive_grammars_changed=False,window_training_or_streams_complete=False,remaining=['Owner-captured training over indexed windows with shared global-coordinate reservoirs and portable model linkage','Bounded tiled training/prediction over a global lattice without constructing a full ScientificGrid','Versioned window/stream archives and source/model-safe interrupted checkpoint resume','Cross-tile contours and nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui workflows','Integrated platform and independent biological/model/process-resource/hardware/release acceptance'],baselines=['docs/benchmarks/raw-lattice-windows-local.json','docs/benchmarks/raw-pixel-replay-apply-local.json','docs/benchmarks/raw-pixel-training-local.json'],raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-window-classifier-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_binaries=len(binaries))))
