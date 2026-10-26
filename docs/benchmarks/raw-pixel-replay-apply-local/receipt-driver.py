from pathlib import Path
import datetime,hashlib,json,re,shutil,subprocess,sys,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';prefix=sys.argv[1]
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
hosts=('windows','linux','training');manifests=[json.loads((r/f'build/{prefix}-{h}/compiler-inputs.json').read_text()) for h in hosts]
assert all(m==manifests[0] for m in manifests);m=manifests[0];assert m['source']==source
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
markers=[
 'Raw pixel replay: pre-IO capture, source-bit comparison, historical/cross-document refusal, recipe/calibration/annotation checks and owner-job verification passed',
 'Raw pixel replay: real TIFF reopened training/prediction records verify through fresh owner jobs without promoting historical authority passed',
 'Raw pixel contours: rational axes/context, raster membership/area, bounded conversion, cancel, atomic apply/undo/redo and stale authority passed',
 'Raw pixel contours: GEOS-unavailable direct/job refusal passed',
 'Raw pixel backend training: real TIFF deterministic direct/job training, historical retraining, trained archives/model files and portable prediction votes passed',
]
suite_targets=dict(pixel_workflows='pixel_workflow',pixel_features='pixel_feature',classifier='classifier',classifier_io='classifier_io',source_identity='source_identity',raw_image='raw_image',provenance='provenance',project='project',project_save='save',recovery='recovery',tissue_contours='tissue_contour',tissue='tissue',document_edits='document_edits',geometry_edits='geometry_edits',resampling='resampling')
destination=s/'docs/benchmarks/raw-pixel-replay-apply-local';destination.mkdir(exist_ok=True);payloads=[];binaries=[]
assert not (s/'docs/benchmarks/raw-pixel-replay-apply-local.json').exists()
retained=r/f'build/{prefix}-retained-binaries';retained.mkdir(exist_ok=True)
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True)
 if target.exists():assert target.read_bytes()==p.read_bytes(),target
 shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
for host in hosts:
 folder=r/f'build/{prefix}-{host}';assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
 cases=list(ET.parse(folder/'tests.xml').iter('testcase'));names={'pixel_workflows','classifier','classifier_io'} if host=='training' else {'pixel_workflows','tissue_contours','document_edits','source_identity'}
 assert len(cases)==len(names) and {c.attrib['name'] for c in cases}==names
 assert all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
 output='\n'.join(c.findtext('system-out') or '' for c in cases)
 for i,marker in enumerate(markers):assert output.count(marker)==(1 if i<2 or (i==2 and host=='windows') or (i==3 and host!='windows') or (i==4 and host=='training') else 0),(host,marker)
 cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host])
 header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text();copy(header,destination/host/'build_manifest.hpp')
 raw=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
 assert flags['QUPATHXX_BUILD_GEOS']==('ON' if host=='windows' else 'OFF')
 assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if host=='training' else 'OFF')
 copy(save(f'{prefix}-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),destination/host/'configuration.json')
 for p in sorted(folder.iterdir()):
  if p.is_file():copy(p,destination/host/p.name)
 for name in sorted(names):
  p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+suite_targets[name]+'_tests'+('' if host=='linux' else '.exe'));target=retained/host/p.name;target.parent.mkdir(exist_ok=True);shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes();binaries.append(dict(host=host,suite=name,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n,d in m['files'].items():
 end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1
 checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n');proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=d,crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save(prefix+'-compiler-git-proof.json',dict(source=source,fingerprint=m['fingerprint'],framed_batch_objects=len(proof),files=proof)),destination/'compiler-git-proof.json')
copy(save(prefix+'-binaries.json',dict(source=source,binaries=binaries)),destination/'retained-binaries.json')
copy(r/f'build/ci/build_{prefix}.py',destination/'build-driver.py');copy(Path(__file__),destination/'receipt-driver.py')
# Preserve all completed/failed development rounds without changing their bytes.
for development in ('rawreplay219','rawreplay219b','rawreplay219c'):
 if development==prefix:continue
 for host in hosts:
  folder=r/f'build/{development}-{host}'
  if folder.exists():
   for p in sorted(folder.iterdir()):
    if p.is_file() and not (development=='rawreplay219' and host=='linux' and p.name=='tests.xml'):copy(p,destination/'development'/f'{development}-{host}'/p.name)
development=[]
for old in ('rawreplay219','rawreplay219b'):
 for host in hosts:
  folder=r/f'build/{old}-{host}'
  if host=='linux':
   entries=re.findall(r'^\s*\d+/\d+\s+Test\s+#\d+:\s+(\w+)\s+\.{2,}\s+(Passed|\*+Failed)',(folder/'tests.log').read_text(encoding='utf-8'),re.M)
   assert len(entries)==15 and all(outcome=='Passed' for _,outcome in entries)
   development.append(dict(round=old,host=host,source=json.loads((folder/'compiler-inputs.json').read_text())['source'],passed=[name for name,_ in entries],failed=[],evidence='Original CTest text log with fifteen terminal suite records; original XML overwritten by copied-driver path reuse. No original XML or old semantic-marker acceptance claimed.'))
  else:
   cases=list(ET.parse(folder/'tests.xml').iter('testcase'))
   development.append(dict(round=old,host=host,source=json.loads((folder/'compiler-inputs.json').read_text())['source'],passed=[c.attrib['name'] for c in cases if all(c.find(t) is None for t in ('failure','error','skipped'))],failed=[c.attrib['name'] for c in cases if c.find('failure') is not None or c.find('error') is not None]))
receipt=dict(schema='qpx.raw-pixel-replay-apply-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(proof),status='native_live_replay_verification_and_bounded_rational_grid_contour_apply_locally_verified',native=dict(windows_geos_on_suites=4,linux_geos_off_suites=4,windows_training_on_geos_off_suites=3,required_markers=markers),verification='Fresh owner-captured request, source/recipe/annotation/calibration validation, exact input/result commitments and repeated actual file guards; boolean observation, no persistent current flag or transferable authority',geometry='Existing bounded GEOS converter borrows core labels and maps exact rational edges on each axis; existing atomic ObjectEdit command supplies apply/undo/redo. Apply additionally requires the captured document revision, preventing duplicate publication; verify alone does not require equal document revisions.',archive_protocol_changed=False,sql_schema=13,scope=['Real TIFF reopened sampled/trained and predicted records reverify through owner jobs','Changed raw input bits reject despite unchanged model/reader metadata','Decoded historical/cross-document/wrong-capture results cannot verify or apply','Recipe, selected annotation, threshold and calibration mismatches reject','Rational axes with context preserve contour area and raster class membership','Cancellation before delivery, bounded conversion, stale authority and late-file refusal','One atomic annotation command with parent/class and object IDs preserved by undo/redo'],development_regressions=development,regression_applicability='Broader 15-suite development runs retain their original source IDs and outcomes. Final source exercises the directly affected workflow/converter/edit/source paths plus actual backend training; earlier results are not relabeled.',remaining=['Phase-preserving tiled raw sampling/features, bounded streams and checkpoint resume','Cross-tile stitched contour semantics and qualification remain a separate workflow','Nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui integration','Linux/Mac GEOS-positive and backend-enabled/platform qualification','Independent biological/model, process-resource/hardware and release acceptance'],baselines=['docs/benchmarks/raw-pixel-archives-local.json','docs/benchmarks/raw-pixel-training-local.json'],raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-pixel-replay-apply-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(proof),payloads=len(payloads),retained_binaries=len(binaries))))
