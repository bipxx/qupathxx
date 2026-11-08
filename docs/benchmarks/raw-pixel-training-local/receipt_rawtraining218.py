from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159'
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
marker='Raw pixel backend training: real TIFF deterministic direct/job training, historical retraining, trained archives/model files and portable prediction votes passed'
manifests=[json.loads((r/f'build/rawtraining218d-{mode}/compiler-inputs.json').read_text()) for mode in ('on','off')]
assert manifests[0]==manifests[1] and manifests[0]['source']==source
m=manifests[0]
for n,d in m['files'].items():assert hashlib.sha256((s/n).read_bytes()).hexdigest()==d
payload_dir=s/'docs/benchmarks/raw-pixel-training-local';payload_dir.mkdir(exist_ok=False)
payloads=[]
def copy(p,target):
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes()
    payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,value):
    p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n');return p
retained=r/'build/rawtraining218-retained-binaries';retained.mkdir(exist_ok=False);binaries=[]
for mode in ('on','off'):
    folder=r/f'build/rawtraining218d-{mode}'
    assert all(c['exit']==0 for c in json.loads((folder/'commands.json').read_text()))
    cases=list(ET.parse(folder/'tests.xml').iter('testcase'))
    expected={'pixel_workflows','classifier','classifier_io'} if mode=='on' else {'pixel_workflows'}
    assert len(cases)==len(expected) and {c.attrib['name'] for c in cases}==expected
    assert all(c.find(tag) is None for c in cases for tag in ('failure','error','skipped'))
    assert sum((c.findtext('system-out') or '').count(marker) for c in cases)==(1 if mode=='on' else 0)
    cache=r/('build/ci/rawtraining218-windows' if mode=='on' else 'build/ci/gridfrontends167-package')
    header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text()
    copy(header,payload_dir/mode/'build_manifest.hpp')
    data=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in data.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
    assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==mode.upper()
    copy(save(f'rawtraining218-{mode}-configuration.json',dict(cache_sha256=hashlib.sha256(data).hexdigest(),flags=flags)),payload_dir/mode/'configuration.json')
    for p in sorted(folder.iterdir()):
        if p.is_file():copy(p,payload_dir/mode/p.name)
    names=['qupathxx_pixel_workflow_tests']+(['qupathxx_classifier_tests','qupathxx_classifier_io_tests'] if mode=='on' else [])
    for name in names:
        original=cache/'Release'/(name+'.exe');target=retained/mode/original.name;target.parent.mkdir(exist_ok=True);shutil.copy2(original,target)
        assert target.read_bytes()==original.read_bytes();binaries.append(dict(configuration=mode,source=source,fingerprint=m['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
# Framed Git reads authenticate the compiler inputs, preserving actual checkout hashes.
raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
position=0;proof=[]
for n,d in m['files'].items():
    end=raw.index(b'\n',position);header=raw[position:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);position=end+1;blob=raw[position:position+size];position+=size;assert raw[position:position+1]==b'\n';position+=1
    checkout=(s/n).read_bytes();assert blob.replace(b'\r\n',b'\n')==checkout.replace(b'\r\n',b'\n')
    proof.append(dict(path=n,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=d,crlf_normalized=blob!=checkout))
assert position==len(raw)
copy(save('rawtraining218-compiler-git-proof.json',dict(source=source,fingerprint=m['fingerprint'],framed_batch_objects=len(proof),files=proof)),payload_dir/'compiler-git-proof.json')
copy(save('rawtraining218-binaries.json',dict(source=source,binaries=binaries)),payload_dir/'retained-binaries.json')
for name in ('build_rawtraining218d.py','receipt_rawtraining218.py'):copy(r/'build/ci'/name,payload_dir/name)
for prefix,modes in [('rawtraining218',('on','off')),('rawtraining218b',('on',)),('rawtraining218c',('on',))]:
    for mode in modes:
        for p in sorted((r/f'build/{prefix}-{mode}').iterdir()):
            if p.is_file():copy(p,payload_dir/'development'/f'{prefix}-{mode}'/p.name)
copy(save('rawtraining218b-termination.json',dict(status='deliberately_stopped_after_wrong_cache_selection',reason='Diagnostic driver accidentally changed cache directory as well as output prefix; the original Windows process tree was identified and terminated. Corrected run218c reused the existing cache. This was not an observation timeout or successful test result.',original_session=88937,windows_driver_pid=113104)),payload_dir/'development/rawtraining218b-on/termination.json')
sdk=r/'build/deps/opencv-sdk';copy(sdk/'qupathxx-opencv-sdk.json',payload_dir/'sdk/manifest.json')
for p in sorted((sdk/'share/licenses/opencv').iterdir()):
    if p.is_file():copy(p,payload_dir/'sdk/licenses'/p.name)
receipt=dict(schema='qpx.raw-pixel-training-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(proof),status='windows_native_backend_enabled_raw_training_and_trained_archive_locally_verified',native=dict(windows_backend_on_suites=3,windows_backend_off_suites=1,required_positive_marker=marker),backend=dict(name='OpenCV',version='4.11.0',sdk_verified_files=195,linux_backend_training_accepted=False),scope=['Real two-channel uint16 TIFF sampled on a non-unit grid','Deterministic direct and owner-job raw training agree','Trained archive matches recipe/classes/rows/training-matrix hash and round-trips','Historical decoded table retrains deterministically without gaining live publication authority','Atomic trained model and archive files replay; project/snapshot/recovery retain the trained model','Native predictions match direct portable forest row votes; pre-cancelled training refuses','Backend-OFF archive and workflow regressions still pass'],remaining=['Linux/Mac and integrated platform backend-enabled qualification','Live source/model replay verification and atomic owner-stage apply/undo','Phase-preserving tiled feature/sampler execution and checkpoint resume','Nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui integration','Independent biological/model, process-resource/hardware and supported-release acceptance'],archive_baseline='docs/benchmarks/raw-pixel-archives-local.json',raw_payloads=payloads,retained_binaries=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-pixel-training-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(payloads=len(payloads),retained_binaries=len(binaries),source=source,compiler_inputs=len(proof))))
