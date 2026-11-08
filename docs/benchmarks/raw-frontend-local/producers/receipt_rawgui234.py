from pathlib import Path
import ast,collections,datetime,hashlib,json,shutil,subprocess,sys,xml.etree.ElementTree as E
assert sys.flags.optimize==0
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawgui234-final';off=r/'build/rawgui234-backend-off';geo=r/'build/rawgui234-geos-off';cache=r/'build/ci/gridfrontends167-package';offcache=r/'build/ci/rawtraining218-windows'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=read(out/'compiler-inputs.json');source=m['source'];assert source==subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
assert len(m['files'])==383 and all(digest(s/n)==d for n,d in m['files'].items())
for folder in [off,geo]:assert read(folder/'compiler-inputs.json')==m
cases=list(E.parse(out/'python-tests.xml').iter('testcase'));cohorts=dict(collections.Counter(c.attrib['classname'] for c in cases))
assert cohorts=={'tests.test_raw_pixel_batch':14,'tests.test_project_batch':22,'tests.test_raw_pixel_cli':15,'tests.test_raw_pixel_python':10,'tests.test_pixel_stream_cli':46,'tests.test_project_pixel_batch':25,'tests.test_raw_batch':4,'tests.test_public_stubs':5,'tests.test_gui_raw_pixel':17,'tests.test_gui_pixel_classifier':20,'tests.test_viewer_session':20}
assert len(cases)==198 and all(c.find(x) is None for c in cases for x in ['failure','error'])
skips=[dict(classname=c.attrib['classname'],name=c.attrib['name'],reason=c.find('skipped').attrib['message']) for c in cases if c.find('skipped') is not None]
assert {v['name'] for v in skips}=={'test_digest_coarse_grid_independent[1]','test_digest_coarse_grid_independent[2]','test_cli_training_backend_off_refuses_before_output'}
new=[c.attrib['name'] for c in cases if c.attrib['classname']=='tests.test_gui_raw_pixel'];assert len(new)==17
for folder,names in [(out,{'project_batch','pixel_features','pixel_workflows','classifier','viewer_session','gui_replay','overlay_surface'}),(off,{'viewer_session','project_batch'}),(geo,{'viewer_session'})]:
 suite=list(E.parse(folder/'native-tests.xml').iter('testcase'));assert {c.attrib['name'] for c in suite}==names
 assert all(c.find(x) is None for c in suite for x in ['failure','error','skipped'])
 viewer=next(c for c in suite if c.attrib['name']=='viewer_session').findtext('system-out')
 assert 'Raw pixel viewer:' in viewer and 'active shutdown join passed' in viewer
 assert ('training=0' if folder==off else 'training=1') in viewer
 if folder==out:assert 'Raw managed contours: historical preview, fresh revision-guarded apply' in viewer and 'GEOS=1' in viewer
 else:assert 'GEOS=0' in viewer
for kind,n,errors in [('valid',4,0),('invalid',3,12)]:
 report=read(out/('typing-'+kind+'.log'));assert report['summary']['filesAnalyzed']==n and report['summary']['errorCount']==errors
assert all(v['exit']==v['expected'] for v in read(out/'verification-commands.json'))
proof=read(out/'installed-proof.json');portable=read(off/'portable-backend-off-proof.json');shutdown=read(out/'contour-shutdown-c/proof.json')
for value in [proof,portable,shutdown]:assert value['source']==source and value['fingerprint']==m['fingerprint']
assert proof['gui'] and proof['geos'] and proof['training'] and portable['saved']==portable['reused']==2 and portable['verified_numeric_blocks']==27
assert read(off/'training-refusal-proof.json')['no_outputs']
assert len(shutdown['cases'])==2 and all(c['observed_running'] and c['state']=='cancelled' and not c['committed'] and c['objects']==3 and c['reader_released'] for c in shutdown['cases'])
dest=s/'docs/benchmarks/raw-frontend-local';dest.mkdir(exist_ok=True);payloads=[]
def copy(p,target):
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);assert p.read_bytes()==target.read_bytes();payloads.append(dict(path=target.relative_to(s).as_posix(),bytes=target.stat().st_size,sha256=digest(target)))
def save(name,value):
 p=r/'build'/name;p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8');return p
history=['rawgui232-development','rawgui232-development-b','rawgui232-development-c','rawgui232-development-d','rawgui232-development-e','rawgui232-development-f','rawgui232-development-g','rawgui232-development-h','rawgui233-development','rawgui233-geos-off','rawgui234-development','rawgui234-development-b']
for folder,label in [(out,'final'),(off,'backend-off'),(geo,'geometry-off')]+[(r/'build'/n,n) for n in history]:
 for p in sorted(folder.iterdir()):
  if p.is_file() and p.suffix in ['.json','.xml','.log','.stdout','.stderr']:copy(p,dest/label/p.name)
for name in ['contour-shutdown-b','contour-shutdown-c']:
 for p in (out/name).rglob('*.json'):copy(p,dest/'final'/p.relative_to(out))
for name in ['panel.png','report.json']:copy(r/'build/rawgui234-development-b/panel-preview'/name,dest/'visual'/name)
for label,path in [('training-on',cache/'CMakeCache.txt'),('training-off',offcache/'CMakeCache.txt'),('geometry-off',geo/'built-cache.txt')]:
 data=path.read_bytes();flags={line.split(':')[0]:line.split('=',1)[1] for line in data.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
 assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']==('OFF' if label=='training-off' else 'ON') and flags['QUPATHXX_BUILD_GEOS']==('ON' if label=='training-on' else 'OFF')
 copy(save('rawgui234-'+label+'-configuration.json',dict(cache_sha256=hashlib.sha256(data).hexdigest(),flags=flags)),dest/label/'configuration.json')
for label,c in [('training-on',cache),('training-off',offcache)]:
 header=c/'generated/build_manifest.hpp';assert source in header.read_text(encoding='utf-8') and m['fingerprint'] in header.read_text(encoding='utf-8');copy(header,dest/label/'build_manifest.hpp')
sdk=r/'build/deps/opencv-sdk/qupathxx-opencv-sdk.json';sdk_data=read(sdk);assert len(sdk_data['files'])==195 and all(digest(sdk.parent/n)==d for n,d in sdk_data['files'].items());copy(sdk,dest/'opencv-sdk-manifest.json')
geos=read(out/'geos-sdk-files.json');assert len(geos)==559 and all(digest(r/'build/deps/geos-sdk'/n)==d for n,d in geos.items())
copy(r/'build/rawpython229-public-api.json',dest/'public-exports.json')
consumers={c.attrib['classname'].replace('.','/')+'.py' for c in cases};pending=list(consumers);modules={p.stem:p.relative_to(s).as_posix() for p in (s/'tests').glob('*.py')}
while pending:
 name=pending.pop()
 for node in ast.walk(ast.parse((s/name).read_text(encoding='utf-8'))):
  names=[node.module.split('.')[0]] if isinstance(node,ast.ImportFrom) and node.module else [v.name.split('.')[0] for v in node.names] if isinstance(node,ast.Import) else []
  for module in names:
   path=modules.get(module)
   if path and path not in consumers:consumers.add(path);pending.append(path)
consumers.update(['tools/generate_stubs.py','examples/raw_classifier_workflow.py','tests/data/pixel-streams/rectangle-v1-windows.qps','src/qupathxx/_native.pyi','src/qupathxx/__init__.pyi']+['tests/typing/raw_'+n+'_'+k+'.py' for n in ['pixel','batch','frontend'] for k in ['valid','invalid']])
paths=list(dict.fromkeys(list(m['files'])+sorted(consumers)));raw=subprocess.run(['git','-C',str(s),'cat-file','--batch'],input=''.join(source+':'+n+'\n' for n in paths).encode(),capture_output=True,check=True).stdout;at=0;entries=[]
for name in paths:
 end=raw.index(b'\n',at);header=raw[at:end].split();assert len(header)==3 and header[1]==b'blob';size=int(header[2]);at=end+1;blob=raw[at:at+size];at+=size;assert raw[at:at+1]==b'\n';at+=1;data=(s/name).read_bytes();assert blob.replace(b'\r\n',b'\n')==data.replace(b'\r\n',b'\n');entries.append(dict(path=name,git_object=header[0].decode(),git_blob_sha256=hashlib.sha256(blob).hexdigest(),checkout_sha256=digest(s/name),crlf_normalized=blob!=data))
assert at==len(raw);copy(save('rawgui234-source-proof.json',dict(source=source,fingerprint=m['fingerprint'],files=entries,test_consumer_closure=sorted(consumers))),dest/'source-proof.json')
retained=r/'build/rawgui234-retained-binaries';retained.mkdir(exist_ok=True);products=[]
def retain(path,label,identity):
 target=retained/label/path.name;target.parent.mkdir(exist_ok=True);assert not target.exists() or digest(target)==digest(path);shutil.copy2(path,target);assert digest(path)==digest(target);products.append(dict(path=target.relative_to(r).as_posix(),source=identity['source'],fingerprint=identity['fingerprint'],bytes=target.stat().st_size,sha256=digest(target)))
 if path.name.lower() in ['geos.dll','geos_c.dll']:
  entry=products[-1];entry['artifact_role']='pinned_sdk_runtime_dependency';entry['consumer_source']=entry.pop('source');entry['consumer_fingerprint']=entry.pop('fingerprint');entry['sdk_manifest']='docs/benchmarks/raw-frontend-local/final/geos-sdk-files.json';entry['sdk_member']='bin/'+path.name
for n in ['_native.cp312-win_amd64.pyd','qupathxx_region.exe','qupathxx_batch.exe','qupathxx_project_batch_tests.exe','qupathxx_pixel_workflow_tests.exe','qupathxx_pixel_feature_tests.exe','qupathxx_classifier_tests.exe','qupathxx_viewer_session_tests.exe','qupathxx_gui_replay_tests.exe','qupathxx_overlay_tests.exe','geos.dll','geos_c.dll']:retain(cache/'Release'/n,'final-on',m)
wheel,=list((out/'wheel').glob('*.whl'));retain(wheel,'final-on',m)
for n in ['qupathxx_viewer_session_tests.exe','qupathxx_batch.exe','qupathxx_project_batch_tests.exe']:retain(offcache/'Release'/n,'final-training-off',m)
for folder,label in [(r/'build/rawgui234-development','development-initial'),(r/'build/rawgui234-development-b','development-refined'),(geo,'geometry-off'),(r/'build/rawgui233-development','shared-adapter-on'),(r/'build/rawgui233-geos-off','shared-adapter-off')]:
 identity=read(folder/'compiler-inputs.json')
 for p in (folder/'products').iterdir():retain(p,label,identity)
assert len(products)==26;copy(save('rawgui234-retained-products.json',dict(products=products)),dest/'retained-products.json')
producers=['receipt_rawgui234.py','build_rawgui234.py','build_rawgui234_b.py','verify_rawgui234_development.py','verify_rawgui234_development_b.py','preview_rawgui234.py','preview_rawgui234_b.py','build_rawgui234_wheel.py','rawgui234_backend.py','verify_rawgui234_final.py','rawgui234_installed_proof.py','build_rawgui234_off.py','build_rawgui234_backend_off.py','rawgui234_backend_off_proof.py','rawgui234_contour_shutdown_probe.py','rawgui234_contour_shutdown_probe_b.py','rawgui234_contour_shutdown_probe_c.py']
for name in producers:copy(r/'build/ci'/name,dest/'producers'/name)
receipt=dict(schema='qpx.raw-frontend-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,compiled_fingerprint=m['fingerprint'],compiler_inputs=len(m['files']),status='windows_cp312_installed_raw_desktop_managed_contours_locally_verified',installed_python_passes=195,installed_skips=skips,cohorts=cohorts,new_raw_frontend_cases=new,native_training_on_suites=7,native_training_off_suites=2,native_geometry_off_training_on_suites=1,portable_backend_off=portable,managed_active_contour_shutdown=shutdown,positive_typing_files=4,negative_typing_files=3,negative_typing_errors=12,wheel=dict(bytes=proof['wheel_bytes'],sha256=proof['wheel_sha256'],members=len(proof['wheel_members'])),scope=[
 'Desktop and managed raw train/checkpoint resume, owned bounded window preview, model IO, QWS1 export/resume and QTT1/QWL1 retention reuse native owner jobs and codecs',
 'Historical model/file browsing does not restore live contour apply authority; real disabled controls, mixed records and external-file preservation are exercised',
 'Bounded full-grid contour preview and atomic apply preserve rational axes/context, use fresh owner prediction and existing undo commands, and refuse stale/cancelled/oversized/budget-exceeding work without mutation',
 'Managed contour results reserve conservative snapshot/model/prediction/geometry payload plus anticipated edit IDs before owner mutation; a quota refusal leaves the document unchanged',
 'Real desktop apply/undo/redo, class-colored geometry preview, image/edit retirement, invalid model recovery and installed managed saved-project reopen pass',
 'Two independent installed active-contour cancel/close probes observe running tickets, reject mutation, release retained results and permit source-reader rename after shutdown',
 'Training-OFF native and CLI checks refuse training before outputs and save/reuse an ON-trained model with all 27 numeric blocks matching the installed ON consumer',
 'All imported wheel members, source/fingerprint/capabilities, loaded GEOS DLL paths, required notices, 195 OpenCV SDK files and 559 GEOS SDK files authenticate; generated declarations and nonempty exact typing counts match'],
 development=['Initial native/GUI compile fixture and replay failures remain at their original Git sources with input manifests/logs/XML and product manifests where available','ImGui overlap-enabled tabs required a prior hover frame in replay; actual move/down/up fixed the interaction without forcing selection','Initial partial-file comparison incorrectly included the replaceable QWS1 footer; corrected checks preserve exact header/committed block bytes and verify the complete file','A fresh shutdown fixture initially used an old workspace epoch and correctly refused; corrected owner setup adopted the current epoch','The first additional contour shutdown probes exceeded feature scratch admission before cancellation; the corrected probe pre-admits a valid grid and exercises active cancellation and close'],
 limitations=['Local Windows CPython 3.12 GUI/GEOS/training/TIFF/project ON and OpenSlide OFF; separate native OFF profiles are not OFF wheels or cross-platform qualification','The fresh installed package reuses workspace test dependencies through .pth; no clean-machine or supported-release claim','Bounded full-grid contour apply is distinct from indexed-window preview and stitched streamed contours; large whole-slide apply, sparse ROI prediction traversal and process RSS/performance acceptance remain open','Source/data/lifetime controls are engineering evidence, not independent biological/model or hardware acceptance','Historical rawgui232 observations retain their original manifests and products; no old executable is relabeled as the final candidate. The passing 6a1bb0e native executable was not separately retained before cache reuse','T5 integrated producer/auditor/platform qualification, automatic maintenance, abrupt-process/power-loss, wider reader support, scientific/resource/hardware and release requirements remain open'],raw_payloads=payloads,retained_artifacts=retained.relative_to(r).as_posix(),main_changed=False,milestones='M0-M7 Partial')
(s/'docs/benchmarks/raw-frontend-local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(source=source,payloads=len(payloads),products=len(products),python_passes=195,native_on=7,native_training_off=2,native_geometry_off=1)))
