from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';hosts=('windows','linux','training')
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip();manifests=[json.loads((r/f'build/tiledtraining222b-{h}/compiler-inputs.json').read_text()) for h in hosts];assert all(m==manifests[0] for m in manifests);m=manifests[0];assert m['source']==source
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
markers=[
 'Raw tiled training: full/tiled global reservoir rows, filtered seams, sparse trillion-cell lattice, quotas, owner lifecycle, calibration and compound-source controls passed',
 'Raw tiled training: real TIFF fractional sampling, optional backend prediction and late-file refusal passed',
 'Raw window classifier: real TIFF fractional-window owner prediction, global ROI mask and late-file refusal passed',
 'Raw window classifier: portable recipe/model identity, 42 full/window label-vote seams, huge indexed ROI, physical calibration, pre-IO quotas/series, cancellation/stale/copy/thread and compound-source checks passed',
]
backend_marker='Raw pixel backend training: real TIFF deterministic direct/job training, historical retraining, trained archives/model files and portable prediction votes passed'
destination=s/'docs/benchmarks/raw-tiled-training-local';destination.mkdir(exist_ok=True);retained=r/'build/tiledtraining222-retained-binaries';retained.mkdir(exist_ok=True);payloads=[];binaries=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
targets=dict(pixel_features='pixel_feature',pixel_workflows='pixel_workflow',resampling='resampling',raw_image='raw_image',source_identity='source_identity',project='project',project_save='save',provenance='provenance',classifier='classifier')
for host in hosts:
 folder=r/f'build/tiledtraining222b-{host}';assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
 cases=list(ET.parse(folder/'tests.xml').iter('testcase'));names={'pixel_workflows'}
 assert len(cases)==len(names) and {c.attrib['name'] for c in cases}==names and all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
 output='\n'.join(c.findtext('system-out') or '' for c in cases)
 for marker in markers:assert output.count(marker)==1,(host,marker)
 assert output.count(backend_marker)==(1 if host=='training' else 0)
 assert output.count('Raw tiled training: actual backend full/tiled matrix, portable model bytes and prediction votes passed')==(1 if host=='training' else 0)
 assert json.loads((folder/'output-preflight.json').read_text())['matches_run']
 cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host]);header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text();copy(header,destination/host/'build_manifest.hpp')
 raw=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line};assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if host=='training' else 'OFF')
 copy(save(f'tiledtraining222-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),destination/host/'configuration.json')
 for p in sorted(folder.iterdir()):
  if p.is_file():copy(p,destination/host/p.name)
 for name in sorted(names):
  p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+targets[name]+'_tests'+('' if host=='linux' else '.exe'));target=retained/host/p.name;target.parent.mkdir(exist_ok=True);shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes();binaries.append(dict(host=host,suite=name,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n,d in m['files'].items():
 end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1;checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n');proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=d,crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save('tiledtraining222-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],framed_batch_objects=len(proof),files=proof)),destination/'compiler-git-proof.json')
initial_products=json.loads((r/'build/tiledtraining222-initial-products/products.json').read_text())
assert len(initial_products)==19 and sum(v['passed'] for v in initial_products)==16
for item in initial_products:
 p=r/item['path'];assert p.stat().st_size==item['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==item['sha256']
binaries.extend(initial_products)
initial_manifest=json.loads((r/'build/tiledtraining222-windows/compiler-inputs.json').read_text())
assert all(json.loads((r/f'build/tiledtraining222-{h}/compiler-inputs.json').read_text())==initial_manifest for h in hosts)
raw_initial=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(initial_manifest['source']+':'+n+'\n' for n in initial_manifest['files']).encode(),capture_output=True,check=True).stdout
position_initial=0;initial_proof=[]
for n,digest in initial_manifest['files'].items():
 end=raw_initial.index(b'\n',position_initial);header=raw_initial[position_initial:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position_initial=end+1;blob=raw_initial[position_initial:position_initial+size];position_initial+=size;assert raw_initial[position_initial:position_initial+1]==b'\n';position_initial+=1
 blob_digest=hashlib.sha256(blob).hexdigest()
 if blob_digest!=digest:
  current=(s/n).read_bytes();assert hashlib.sha256(current).hexdigest()==digest and blob.replace(b'\r\n',b'\n')==current.replace(b'\r\n',b'\n'),(n,'initial compiler bytes cannot be authenticated')
 initial_proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=blob_digest,checkout_sha256=digest,crlf_normalized=blob_digest!=digest))
assert position_initial==len(raw_initial)
copy(save('tiledtraining222-initial-git-proof.json',dict(source=initial_manifest['source'],fingerprint=initial_manifest['fingerprint'],files=initial_proof)),destination/'initial/compiler-git-proof.json')
changed=subprocess.check_output(['git','-C',str(s),'diff','--name-only',initial_manifest['source'],source],text=True).splitlines()
assert changed==['cpp/tests/raw_tiled_training_checks.hpp'],changed
for host in hosts:
 for p in sorted((r/f'build/tiledtraining222-{host}').iterdir()):
  if p.is_file():copy(p,destination/'initial'/host/p.name)
 initial=r/'build/tiledtraining222-initial-products'/host
 copy(initial/'build_manifest.hpp',destination/'initial'/host/'build_manifest.hpp')
 cache=(initial/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in cache.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
 copy(save(f'tiledtraining222-initial-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(cache).hexdigest(),flags=flags)),destination/'initial'/host/'configuration.json')
 copy(save(f'tiledtraining222-source-delta.json',dict(initial_source=initial_manifest['source'],final_source=source,changed_inputs=changed,reason='Restore original metadata vector allocation after quota rejection; assigning the original text retained the oversized string capacity',initial_workflow_suites_failed=3,unchanged_regressions_passed=16)),destination/'source-delta.json') if host=='windows' else None
copy(save('tiledtraining222-retained-binaries.json',dict(source=source,binaries=binaries)),destination/'retained-binaries.json')
for name in ('build_tiledtraining222.py','build_tiledtraining222b.py','preserve_tiledtraining222.py','receipt_tiledtraining222.py'):copy(r/'build/ci'/name,destination/name)
receipt=dict(schema='qpx.raw-tiled-training-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='sparse_owner_captured_tiled_raw_training_locally_verified',native=dict(windows_suites=1,linux_suites=1,windows_training_suites=1,unchanged_regressions_at_initial_source=16,required_markers=markers,backend_marker=backend_marker),scope=['ScientificRawLattice stores global geometry without whole-raster admission; equivalent recipes preserve QXS1/QRM1 bytes','Sparse ROI tile union admits unique candidate scans before decoding and retains original rational phase','One shared global-coordinate per-class reservoir for full and tiled raw training; balance only after global selection','Exact selected feature rows, classes, coordinates, minimum annotation IDs, eligible/ambiguous counts across three tile policies with raw and filtered features','Million-by-million lattice selects four rows from one 4096-core candidate tile, without a full-grid allocation','Actual Windows OpenCV backend full/tiled model bytes and votes agree with explicit identical effective parameters','Real TIFF fractional sampling, backend-positive prediction and fresh file refusal; compound companion refusal at delivery','Owner identity/revision/calibration, borrowed reader lifetime, cancellation during and after IO, failure/retry and thread controls','Scan/window/snapshot limits and actual per-window feature/sampler quotas before each window IO; changed series/metadata refuse before decoder calls'],legacy_archive_grammars_changed=False,initial_regression_source=initial_manifest['source'],initial_fingerprint=initial_manifest['fingerprint'],development_failure='All three initial workflow suites fail because fixture text restoration retains oversized string capacity; raw logs/XML and binaries preserved. Final source changes only that test helper.',tiled_prediction_or_resume_complete=False,remaining=['Bounded tiled prediction over declared coverage with output sinks and resource checks','Versioned tiled training/stream archives and source/model/selection-safe interrupted resume','Cross-tile contours and nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui workflows','Integrated platform and independent biological/model/process-resource/hardware/release acceptance'],baselines=['docs/benchmarks/raw-window-classifier-local.json','docs/benchmarks/raw-lattice-windows-local.json','docs/benchmarks/raw-pixel-replay-apply-local.json'],raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-tiled-training-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_binaries=len(binaries))))
