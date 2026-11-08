from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawpython229-final-b';m=json.loads((out/'compiler-inputs.json').read_text());source=m['source']
assert source==subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in m['files'].items())
native=list(E.parse(out/'native-tests.xml').iter('testcase'));python=list(E.parse(out/'python-tests.xml').iter('testcase'))
assert {c.attrib['name'] for c in native}=={'pixel_workflows','pixel_features','classifier'}
assert all(c.find(t) is None for c in native for t in ('failure','error','skipped'))
assert len(python)==68 and all(c.find(t) is None for c in python for t in ('failure','error'))
skips=[dict(classname=c.attrib['classname'],name=c.attrib['name'],reason=c.find('skipped').attrib['message']) for c in python if c.find('skipped') is not None]
assert len(skips)==1 and skips[0]['name']=='test_real_aperio_sampling_and_tiled_source_prediction_match_direct_context'
raw=[c for c in python if c.attrib['classname'].endswith('test_raw_pixel_python')];assert len(raw)==10 and all(c.find('skipped') is None for c in raw)
for kind,count,errors in [('valid',2,0),('invalid',1,4)]:
 report=json.loads((out/('typing-'+kind+'.log')).read_text());assert report['summary']['filesAnalyzed']==count and report['summary']['errorCount']==errors
commands=json.loads((out/'verification-commands.json').read_text());assert all(c['exit']==c['expected'] for c in commands)
proof=json.loads((out/'installed-proof.json').read_text());assert proof['source']==source and proof['fingerprint']==m['fingerprint'] and len(proof['new_public_exports'])==71
cache=r/'build/ci/rawtraining218-windows';header=cache/'generated/build_manifest.hpp';assert source in header.read_text() and m['fingerprint'] in header.read_text()
dest=s/'docs/benchmarks/raw-python-bindings-local';dest.mkdir(exist_ok=True);payloads=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);b=target.read_bytes();assert b==p.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
def save(name,data):
 p=r/'build'/name;p.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n');return p
for p in sorted(out.iterdir()):
 if p.is_file():copy(p,dest/p.name)
copy(header,dest/'build_manifest.hpp')
sdk_manifest=r/'build/deps/opencv-sdk/qupathxx-opencv-sdk.json';sdk=json.loads(sdk_manifest.read_text());assert sdk['version']=='4.11.0' and len(sdk['files'])==195 and all(hashlib.sha256((sdk_manifest.parent/n).read_bytes()).hexdigest()==h for n,h in sdk['files'].items());copy(sdk_manifest,dest/'opencv-sdk-manifest.json')
raw_cache=(cache/'CMakeCache.txt').read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in raw_cache.decode().splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
assert flags['QUPATHXX_BUILD_PYTHON']==flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==flags['QUPATHXX_BUILD_TIFF']==flags['QUPATHXX_BUILD_PROJECT']=='ON' and flags['QUPATHXX_BUILD_GUI']==flags['QUPATHXX_BUILD_GEOS']==flags['QUPATHXX_BUILD_OPENSLIDE']=='OFF'
copy(save('rawpython229-configuration.json',dict(cache_sha256=hashlib.sha256(raw_cache).hexdigest(),flags=flags,package_isolation='Fresh CPython 3.12 wheel target; test dependencies reused through a recorded workspace .pth, not a clean-machine claim')),dest/'configuration.json')
batch=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in m['files']).encode(),capture_output=True,check=True).stdout
at=0;files=[]
for name,digest in m['files'].items():
 end=batch.index(b'\n',at);h=batch[at:end].split();assert len(h)==3 and h[1]==b'blob';size=int(h[2]);at=end+1;blob=batch[at:at+size];at+=size;assert batch[at:at+1]==b'\n';at+=1
 data=(s/name).read_bytes();assert blob.replace(b'\r\n',b'\n')==data.replace(b'\r\n',b'\n');files.append(dict(path=name,git_object=h[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=digest,crlf_normalized=blob!=data))
assert at==len(batch)
copy(save('rawpython229-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],files=files)),dest/'compiler-git-proof.json')
qualifiers=['tests/test_raw_pixel_python.py','tests/test_public_stubs.py','tests/test_stub_signature_guard.py','tests/test_scientific_raw_bindings.py','tests/test_pixel_workflows.py','tests/test_raw_records.py','tests/typing/raw_pixel_valid.py','tests/typing/raw_pixel_invalid.py','examples/raw_classifier_workflow.py','tools/generate_stubs.py','src/qupathxx/_native.pyi','src/qupathxx/__init__.pyi']
copy(save('rawpython229-consumer-inputs.json',dict(source=source,files={n:hashlib.sha256((s/n).read_bytes()).hexdigest() for n in qualifiers})),dest/'consumer-inputs.json')
retained=r/'build/rawpython229-retained-binaries';retained.mkdir(exist_ok=True);binaries=[]
for p in list((cache/'Release').glob('_native*.pyd'))+[cache/'Release'/n for n in ('qupathxx_region.exe','qupathxx_batch.exe','qupathxx_pixel_workflow_tests.exe','qupathxx_pixel_feature_tests.exe','qupathxx_classifier_tests.exe')]+list((out/'wheel').glob('*.whl')):
 q=retained/p.name;shutil.copy2(p,q);assert q.read_bytes()==p.read_bytes();binaries.append(dict(path=q.relative_to(r).as_posix(),source=source,fingerprint=m['fingerprint'],bytes=q.stat().st_size,sha256=hashlib.sha256(q.read_bytes()).hexdigest()))
assert len(binaries)==7
copy(save('rawpython229-retained-binaries.json',dict(source=source,products=binaries,development_products='build/rawpython229-development-products/products.json')),dest/'retained-binaries.json')
for phase in ('development','development-b','development-c','development-d'):
 for p in sorted((r/('build/rawpython229-'+phase)).iterdir()):
  if p.is_file():copy(p,dest/'development'/phase/p.name)
for p in sorted((r/'build/rawpython229-final').iterdir()):
 if p.is_file():copy(p,dest/'development'/'first-wheel'/p.name)
for name in ('rawpython229_backend_b.py','build_rawpython229_wheel_b.py','verify_rawpython229_final.py','rawpython229_installed_proof.py','receipt_rawpython229.py'):
 copy(r/'build/ci'/name,dest/name)
receipt=dict(schema='qpx.raw-python-bindings-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='windows_cp312_training_enabled_raw_python_bindings_and_installed_example_locally_verified',new_public_exports=71,installed_python_passes=67,installed_skips=skips,new_raw_workflow_cases=[c.attrib['name'] for c in raw],native_suites=3,positive_typing_files=2,negative_typing_cases=4,profile=flags,wheel=dict(bytes=proof['wheel_bytes'],sha256=proof['wheel_sha256'],members=len(proof['wheel_members'])),scope=[
 'Thin nanobind bindings expose native raw lattice/window sampling, feature options, recipes/models, full/tiled training/prediction, bounded streams, archives/replay/contours, checkpoints, files/links and owner jobs without a Python numeric algorithm',
 'Read-only owning NumPy views and independent mutable copies preserve float64 raw HWC values, float32 feature/vote matrices, uint32 labels, uint8 support, int64 global coordinates and uint64 annotation IDs',
 'Owner job wait joins release GIL while document checks retain it; Python-owned native job destructors release GIL during native cancel/join, including foreign-thread last-reference deletion and Python-owned file captures',
 'Actual Windows OpenCV backend full/tiled model bytes, independent Fraction row values, numeric window seams, archives/project replay, file/checkpoint resume, stale/cancel/thread controls and retained external links pass in the installed wheel',
 'Standalone project-to-project example trains from labelled TIFF areas, checkpoints/resumes both producers, retains outputs and saves/reopens/verifies a new project',
 'Generated PEP 561 exports/members, array dtypes, nullable partial results and non-null synchronous predictions match runtime; valid typing fixture/example pass, four distinct invalid cases reject with nonzero analyzed-file guards',
 'Wheel members, imported noneditable package origin, runtime Git/source fingerprint, SDK manifest and native tests are source-bound; file-version path allocation is charged in retained metadata'],
 development=['Initial development module compiled and eight workflow tests passed','GIL-safe holder forwarding had two compiler-detected adapter mistakes, retained in development-b/development-c logs; corrected without changing the numerical kernel','Development-d passed nine workflow tests, shutdown checks and stub generation; the example later passed separately','Initial Pyright absolute include paths analyzed zero files; those outputs were not accepted and final guards require two positive/one negative analyzed files','A first consumer was mistakenly created with CPython 3.13 and correctly rejected the cp312 wheel; final consumer explicitly uses 3.12 and preserves the rejected install log','Generated stubs now write LF deterministically; the generator resets class context before module functions to prevent spurious nullable predictions'],
 limitations=['Windows CPython 3.12, training ON, TIFF/project ON, GUI/GEOS/OpenSlide OFF only; optional legacy Aperio case remains skipped','Consumer package is fresh and wheel-authenticated; workspace test dependencies are reused via .pth, so no clean-machine/release claim','Official raw CLI/batch, ImGui/managed-viewer adapters and broader installed/platform qualification remain open','Cross-tile contours, automatic checkpoint maintenance, abrupt-process/power-loss and independent biological/model/process-resource/hardware/release acceptance remain open'],raw_payloads=payloads,retained_artifacts=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-python-bindings-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(source=source,compiler_inputs=len(m['files']),payloads=len(payloads),retained_products=len(binaries),python_passes=67,native_passes=3)))
