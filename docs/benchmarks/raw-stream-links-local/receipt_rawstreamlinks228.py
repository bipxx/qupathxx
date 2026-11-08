from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';hosts=('windows','linux','training')
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip();manifests=[json.loads((r/f'build/rawstreamlinks228b-{h}/compiler-inputs.json').read_text()) for h in hosts];assert all(m==manifests[0] for m in manifests);m=manifests[0];assert m['source']==source
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
markers=[
 'Raw stream links: real TIFF optional trained export retention, mixed raw/RGB/project records, missing-file metadata reopen, save-as/snapshot/recovery and verified relocation passed',
 'Raw stream links: verified prefix/complete retention, independent QWL1 bounds, global geometry, historical authority refusal, exact-content relink and external lifetime controls passed',
 'Raw file jobs: owner export/checkpoint transfer, one-slot historical reads, cancellation/staleness/output guards, annotation resume refusal, thread/lifetime and retry controls passed',
 'Raw training checkpoint: independent QTC1 bounds, balanced/unbalanced multistage replay, exact rows/hashes/bytes, input tamper, limits, cancellation and concurrent-writer refusal passed',
]
backend_marker='Raw training checkpoint: actual backend uninterrupted/resumed table and model bytes passed'

destination=s/'docs/benchmarks/raw-stream-links-local';destination.mkdir(exist_ok=True);retained=r/'build/rawstreamlinks228-retained-binaries';retained.mkdir(exist_ok=True);payloads=[];binaries=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
targets=dict(pixel_features='pixel_feature',pixel_workflows='pixel_workflow',resampling='resampling',raw_image='raw_image',source_identity='source_identity',project='project',project_save='save',provenance='provenance',classifier='classifier')
for host in hosts:
 folder=r/f'build/rawstreamlinks228b-{host}';assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
 cases=list(ET.parse(folder/'tests.xml').iter('testcase'));names={'pixel_workflows'}
 assert len(cases)==len(names) and {c.attrib['name'] for c in cases}==names and all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
 output='\n'.join(c.findtext('system-out') or '' for c in cases)
 for marker in markers:assert output.count(marker)==1,(host,marker)
 assert output.count(backend_marker)==(2 if host=='training' else 0)
 assert output.count('Raw tiled training: actual backend full/tiled matrix, portable model bytes and prediction votes passed')==(1 if host=='training' else 0)
 assert json.loads((folder/'output-preflight.json').read_text())['matches_run']
 cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host]);header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text();copy(header,destination/host/'build_manifest.hpp')
 raw=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line};assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('ON' if host=='training' else 'OFF')
 copy(save(f'rawstreamlinks228-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(raw).hexdigest(),flags=flags)),destination/host/'configuration.json')
 for p in sorted(folder.iterdir()):
  if p.is_file():copy(p,destination/host/p.name)
 for name in sorted(names):
  p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+targets[name]+'_tests'+('' if host=='linux' else '.exe'));target=retained/host/p.name;target.parent.mkdir(exist_ok=True);shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes();binaries.append(dict(host=host,suite=name,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n,d in m['files'].items():
 end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1;checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n');proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=d,crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save('rawstreamlinks228-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],framed_batch_objects=len(proof),files=proof)),destination/'compiler-git-proof.json')
initial_products=json.loads((r/'build/rawstreamlinks228-initial-products/products.json').read_text())
assert len(initial_products)==19 and sum(v['passed'] for v in initial_products)==16
for item in initial_products:
 p=r/item['path'];assert p.stat().st_size==item['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==item['sha256']
binaries.extend(initial_products)
initial_manifest=json.loads((r/'build/rawstreamlinks228-windows/compiler-inputs.json').read_text())
assert all(json.loads((r/f'build/rawstreamlinks228-{h}/compiler-inputs.json').read_text())==initial_manifest for h in hosts)
raw_initial=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(initial_manifest['source']+':'+n+'\n' for n in initial_manifest['files']).encode(),capture_output=True,check=True).stdout
position_initial=0;initial_proof=[]
for n,digest in initial_manifest['files'].items():
 end=raw_initial.index(b'\n',position_initial);header=raw_initial[position_initial:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position_initial=end+1;blob=raw_initial[position_initial:position_initial+size];position_initial+=size;assert raw_initial[position_initial:position_initial+1]==b'\n';position_initial+=1
 blob_digest=hashlib.sha256(blob).hexdigest()
 if blob_digest!=digest:
  current=(s/n).read_bytes();assert hashlib.sha256(current).hexdigest()==digest and blob.replace(b'\r\n',b'\n')==current.replace(b'\r\n',b'\n'),(n,'initial compiler bytes cannot be authenticated')
 initial_proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=blob_digest,checkout_sha256=digest,crlf_normalized=blob_digest!=digest))
assert position_initial==len(raw_initial)
copy(save('rawstreamlinks228-initial-git-proof.json',dict(source=initial_manifest['source'],fingerprint=initial_manifest['fingerprint'],files=initial_proof)),destination/'initial/compiler-git-proof.json')
changed=subprocess.check_output(['git','-C',str(s),'diff','--name-only',initial_manifest['source'],source],text=True).splitlines()
assert changed==['cpp/tests/raw_pixel_stream_link_checks.hpp'],changed
for host in hosts:
 for p in sorted((r/f'build/rawstreamlinks228-{host}').iterdir()):
  if p.is_file():copy(p,destination/'initial'/host/p.name)
 initial=r/'build/rawstreamlinks228-initial-products'/host
 copy(initial/'build_manifest.hpp',destination/'initial'/host/'build_manifest.hpp')
 cache=(initial/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in cache.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
 copy(save(f'rawstreamlinks228-initial-{host}-configuration.json',dict(cache_sha256=hashlib.sha256(cache).hexdigest(),flags=flags)),destination/'initial'/host/'configuration.json')
 copy(save(f'rawstreamlinks228-source-delta.json',dict(initial_source=initial_manifest['source'],final_source=source,changed_inputs=changed,reason='RGB coexistence fixture omitted required sampling resolution and correctly refused; add explicit level-zero PixelResolution without changing source or record validation',initial_workflow_suites_failed=3,unchanged_regressions_passed=16)),destination/'source-delta.json') if host=='windows' else None
copy(save('rawstreamlinks228-retained-binaries.json',dict(source=source,binaries=binaries)),destination/'retained-binaries.json')
for name in ('build_rawstreamlinks228.py','build_rawstreamlinks228b.py','preserve_rawstreamlinks228.py','receipt_rawstreamlinks228.py'):copy(r/'build/ci'/name,destination/name)
receipt=dict(schema='qpx.raw-stream-links-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='verified_external_raw_stream_links_and_project_retention_locally_verified',native=dict(windows_suites=1,linux_suites=1,windows_training_suites=1,unchanged_regressions_at_initial_source=16,required_markers=markers,backend_marker=backend_marker,backend_marker_count=2),initial_regression_source=initial_manifest['source'],initial_fingerprint=initial_manifest['fingerprint'],development_failure='The initial RGB coexistence fixture omitted its sampling resolution. All three workflow suites correctly refused; 16 other suites passed. Final source changes only that fixture to an explicit level-zero resolution. Original failures and binaries remain source-bound.',scope=[
 'Distinct QWL1 bounded raw stream link records in the existing stream-record table, with SQL schema 13 and legacy RGB QSL1/raw QXR1 grammars unchanged',
 'Historical links preserve original global rational lattice, region/tile/work/vote policy, source object/revision history, model/request/header identities, exact prefix coverage/totals/counts and input/result/content commitments',
 'Shared QWS1 framing arithmetic validates expected file bytes and global prefix coverage without admitting a whole ScientificGrid raster',
 'RawPixelStreamFile.link and prepare_raw_pixel_stream_link require full prefix verification and a current file observation; no model/header raster is retained in the metadata descriptor',
 'Owner export and historical read jobs transfer verified links; open_link asynchronously verifies actual file contents and compares complete saved metadata before returning a historical file or link',
 'Native Document keep/relink require checked verification observations; decoded/project-restored links have no observation, capture or live result authority',
 'Record removal leaves external files untouched, exact-content relocation preserves record ID/name and increments one document revision, mismatched/stale/unverified links refuse without mutation',
 'Project save/open, background snapshot/save-as and autosave/recovery preserve mixed QWL1/QSL1/QXR1 records and rebase external paths without copying/scanning output files',
 'Real TIFF optional actual trained model export/retention, missing-file metadata reopen, verified relocation and legacy RGB stream coexistence pass',
 'Independent QWL1 framing/count/geometry/file-size tamper and truncation/byte limits, empty/partial/complete prefixes, old-prefix mismatch, late file mutation and trillion-cell zero-IO link controls pass'],
 limitations=['Links reference external files; project save/recovery does not copy them or promise a self-contained bundle','Historical metadata reload does not verify file availability/content; asynchronous full verification is required before keep/relink or linked file use','Native links are historical references, not restored live classification authority or stitched contours','Installed nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui adapters, automatic checkpoint maintenance, abrupt-process/power-loss and integrated qualification remain open','Independent biological/model/process-resource/hardware and release acceptance remain separate'],raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-stream-links-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_binaries=len(binaries))))
