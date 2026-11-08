from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';hosts=('windows','linux','training')
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
manifests=[json.loads((r/f'build/rawwindow220-{h}/compiler-inputs.json').read_text()) for h in hosts];assert all(m==manifests[0] for m in manifests);m=manifests[0];assert m['source']==source
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
markers=[
 'Raw lattice windows: 13 Fraction wide means, 20 large irreducible uint64 samples, huge dense lattice, bounded chunk mutation, admission/cancel and 9072 exact tiled feature slots passed',
 'Typed raw pixel features: 12 independent dtype/extreme references, per-channel support, anisotropic scales, stream seams, admission and cancellation passed',
 'Raw classifier QPR1 independent framing, channel/grid/physical contracts, forest votes, streams, exact budgets and atomic model files passed',
 'Owner-captured raw training/prediction: deterministic labels, rational axes, document calibration, bounded IO, cancellation, stale compound sources and owner-thread publication passed',
]
destination=s/'docs/benchmarks/raw-lattice-windows-local';destination.mkdir(exist_ok=False);retained=r/'build/rawwindow220-retained-binaries';retained.mkdir(exist_ok=False);payloads=[];binaries=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
targets=dict(pixel_features='pixel_feature',resampling='resampling',pixel_workflows='pixel_workflow',raw_image='raw_image',source_identity='source_identity')
for host in hosts:
 folder=r/f'build/rawwindow220-{host}';assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
 cases=list(ET.parse(folder/'tests.xml').iter('testcase'));names={'pixel_features','pixel_workflows'} if host=='training' else set(targets)
 assert len(cases)==len(names) and {c.attrib['name'] for c in cases}==names and all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
 output='\n'.join(c.findtext('system-out') or '' for c in cases)
 for marker in markers:assert output.count(marker)==1,(host,marker)
 assert json.loads((folder/'output-preflight.json').read_text())['matches_run']
 cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host])
 header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text();copy(header,destination/host/'build_manifest.hpp')
 raw=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
 assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if host=='training' else 'OFF')
 copy(save(f'rawwindow220-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),destination/host/'configuration.json')
 for p in sorted(folder.iterdir()):
  if p.is_file():copy(p,destination/host/p.name)
 for name in sorted(names):
  p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+targets[name]+'_tests'+('' if host=='linux' else '.exe'));target=retained/host/p.name;target.parent.mkdir(exist_ok=True);shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes();binaries.append(dict(host=host,suite=name,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
references=['tools/reference_raw_window_weights.py','docs/benchmarks/raw-window-fraction-reference.json']
paths=list(m['files'])+references
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in paths).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n in paths:
 end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1
 checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n')
 if n in references:assert blob==checkout
 proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=hashlib.sha256(checkout).hexdigest(),crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save('rawwindow220-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),reference_sources=references,framed_batch_objects=len(proof),files=proof)),destination/'compiler-and-reference-git-proof.json')
copy(save('rawwindow220-retained-binaries.json',dict(source=source,binaries=binaries)),destination/'retained-binaries.json')
for name in ('build_rawwindow220.py','receipt_rawwindow220.py'):copy(r/'build/ci'/name,destination/name)
copy(r/'build/rawwindow220-reference-regeneration.json',destination/'reference-regeneration.json')
for n in references:copy(s/n,destination/'reference'/Path(n).name)
sdk=r/'build/deps/opencv-sdk';sdk_manifest=json.loads((sdk/'qupathxx-opencv-sdk.json').read_text());assert sdk_manifest['version']=='4.11.0'
for n,d in sdk_manifest['files'].items():assert hashlib.sha256((sdk/n).read_bytes()).hexdigest()==d,n
copy(save('rawwindow220-sdk-observation.json',dict(version=sdk_manifest['version'],verified_files=len(sdk_manifest['files']),observed='post-run; actual training regression uses the retained SDK configuration',manifest_sha256=hashlib.sha256((sdk/'qupathxx-opencv-sdk.json').read_bytes()).hexdigest())),destination/'training-sdk-observation.json')
receipt=dict(schema='qpx.raw-lattice-windows-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='phase_preserving_raw_window_sampling_and_shared_numeric_features_locally_verified',native=dict(windows_suites=5,linux_suites=5,windows_training_regression_suites=2,required_markers=markers),geometry='Original image dimensions and global integer-origin/extent/sample lattice plus bounded core first/count and per-axis context; no full ScientificGrid or whole source/output raster admission required',numeric='Window-only gcd-reduced area weights; shared exact accumulator has checked 72-bit denominator mode, unchanged 28-bit legacy mode, 2176 numerator bits and one binary64 nearest/even rounding. Window division traversal charged at 68 or 2304 steps per channel sample.',independent_reference=dict(wide_weight_means=13,large_irreducible_uint64_context_samples=20,authority='Python stdlib Fraction, exact regeneration',exact_tiled_feature_slots=9072,dtype_cases=12,original_feature_reference_slots=12600),scope=['Public distinct owning ScientificRawWindowData cannot enter legacy full-grid consumers','One-million by one-million global sample lattice supports bounded dense and irreducible windows without whole-raster allocation','Exact sampled HWC means, support/reasons and float32 feature bits match full-grid cores at every tested tile seam','Shared bounded source chunk commitments reject mutation between capture and materialization','Pre-IO budget and cancellation refusal; outside-source support does not decode pixels','Legacy full-grid sampler/feature/model/workflow references and actual Windows forest training regressions pass'],legacy_archive_and_hash_grammars_changed=False,window_classifier_or_stream_complete=False,remaining=['Window-aware raw recipe/model matching and classifier outputs','Owner-captured tiled training/prediction requests and bounded jobs on the global lattice','Versioned stream files, source/model-safe interruption and checkpoint resume','Cross-tile contour stitching, nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui integration','Integrated platform and independent biological/model/process-resource/hardware/release acceptance'],raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-lattice-windows-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_binaries=len(binaries))))
