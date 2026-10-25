from pathlib import Path
import datetime,hashlib,importlib.util,json,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];source=r/'build/ci/scientific-grid-159';revision='8ece80e52f1b93bd62e19f0c4cc5215d6357fd68';delivery='283a1972140f7de738ef460861faaa65498ae49f';fingerprint='5297728aecff15fb76489a9dc9ab450eeb98d1378b37b36e4281979cabf2f951'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==delivery
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
out=source/'docs/benchmarks/raw-grid-frontends-local';out.mkdir(exist_ok=False);manifest={}
def copy(p,name):
 data=p.read_bytes();target=out/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest());return data
def cases(p,passed,skipped=0,failed=0):
 values=list(ET.parse(p).getroot().iter('testcase'));assert len(values)==passed+skipped+failed,(p,len(values))
 assert sum(c.find('skipped') is not None for c in values)==skipped
 assert sum(any(c.find(k) is not None for k in ['failure','error']) for c in values)==failed
 return values
def folder(p,name):
 for file in sorted(p.iterdir()):
  if file.is_file():copy(file,name+'/'+file.name)
markers={
 'resampling':['Typed raw sampling engine identity '+fingerprint+' '+revision,'Shared ROI grid raw count/context planning and pre-IO admission passed'],
 'cells':['Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed'],
 'raw_image':['Raw grid TIFF batch distinct recipe, interrupted checkpoint resume and hash-only typed CSV passed'],
 'viewer_session':['Raw grid viewer copied counts/options, owner calibration, retention/replay, cancellation and stale delivery passed'],
 'project_batch':['Grid ROI planning, typed batch recipe, per-image calibration, interrupted resume and QCL5/CSV checkpoints passed']}
for mode,dirname,names in [
 ('windows','raw181-native',{k:k+'.xml' for k in markers}),
 ('tiff-linux','raw181-tiff-linux',dict(resampling='native-tests.xml',cells='cells-tests.xml',raw_image='raw-image-tests.xml',viewer_session='viewer-session-tests.xml',project_batch='project-batch-tests.xml'))]:
 p=r/'build'/dirname;commands=json.loads((p/'commands.json').read_text(encoding='utf-8'));assert len(commands)==7 and all(c['exit']==0 for c in commands)
 for suite,name in names.items():
  value=cases(p/name,1)[0];assert value.get('name')==suite
  lines=value.findtext('system-out','').splitlines()
  for marker in markers[suite]:assert lines.count(marker)==1,(suite,marker)
 folder(p,mode)
 cache=r/('build/ci/gridfrontends167-package' if mode=='windows' else 'build/ci/raw176-tiff-linux-native')
 copy(cache/'CMakeCache.txt',mode+'/CMakeCache.txt');copy(cache/'generated/build_manifest.hpp',mode+'/build_manifest.hpp')
 assert fingerprint in (cache/'generated/build_manifest.hpp').read_text(encoding='utf-8')
copy(r/'build/raw181-source-proof.json','compiled-source-proof.json');copy(r/'build/raw182-delivery-source-proof.json','delivery-source-proof.json')
proof=json.loads(copy(r/'build/raw181-compiler-input-proof.json','compiler-input-proof.json'));assert proof['source']==revision and proof['compiler_inputs']==311
for name,digest in proof['files'].items():assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest,name
spec=importlib.util.spec_from_file_location('evidence',source/'tools/ci/scientific_grid_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
assert len(json.loads((r/'build/raw182-delivery-source-proof.json').read_text(encoding='utf-8'))['files'])==492
pure=cases(r/'build/raw181-pure-tests.xml',120);copy(r/'build/raw181-pure-tests.xml','pure-tests.xml');copy(r/'build/raw181-auditor-tests.xml','auditor-tests.xml')
runtime=cases(r/'build/raw182-runtime/python-tests.xml',142,3);desktop=cases(r/'build/raw182-desktop/desktop-tests.xml',33)
for values,gui in [(runtime,False),(desktop,True)]:
 selected=[c for c in values if c.get('classname','').rsplit('.',1)[-1]=='test_raw_grid_frontends']
 assert sorted(c.get('name') for c in selected)==m.CASES['test_raw_grid_frontends']
 for c in selected:
  if c.get('name') in m.GUI_CASES and not gui:assert c.find('skipped') is not None and c.find('skipped').get('message')=='opt-in raw grid desktop'
  else:assert all(c.find(k) is None for k in ['failure','error','skipped'])
folder(r/'build/raw182-runtime','runtime');folder(r/'build/raw182-desktop','desktop');folder(r/'build/raw181-typing','typing');typing=m.typing(r/'build/raw181-typing')
folder(r/'build/raw181-python-isolated','stubs-and-identity')
# Keep failed observations separately; no failed result is counted as passing.
cases(r/'build/raw181-runtime/python-tests.xml',141,3,1);cases(r/'build/raw181-desktop/desktop-tests.xml',32,0,1)
folder(r/'build/raw181-runtime','rejected/runtime');folder(r/'build/raw181-desktop','rejected/desktop');folder(r/'build/raw181-binding-syntax','syntax')
for name in ['raw181-msvc-observation.json','raw181-msvc-tail-observation.json']:copy(r/'build'/name,name)
copy(r/'build/ci/source180-before-raw181/inventory.json','prior-artifact-inventory.json')
for name in ['build_raw181_windows.py','build_raw181_tiff_linux.py','stage_raw181_python.py','test_raw182_runtime.py','test_raw182_desktop.py','check_raw181_typing.py','capture_raw182_controls_final.py','capture_raw182_controls.py']:copy(r/'build/ci'/name,'runners/'+name)
copy(r/'build/raw182-controls-final/controls.png','visual/controls.png');copy(r/'build/raw182-controls-final/report.json','visual/controls-report.json');copy(r/'build/raw182-grid-records.png','visual/records.png')
copy(Path(__file__),'record_evidence.py')
module=r/'build/ci/raw181-python-env/Lib/site-packages/qupathxx/_native.cp312-win_amd64.pyd';cli=module.with_name('qupathxx_batch.exe')
identity=lambda p:dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
receipt=dict(schema='qpx.raw-grid-frontends-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),compiled_source=revision,delivery_source=delivery,source_tree_sha256=fingerprint,compiler_inputs=311,source_contract_files=492,
 compiler_input_reuse='All compiler-input bytes equal the tested8ece80e source. Delivery changes generated stubs and a Python exception expectation only; module revision remains8ece80e.',
 native_module=identity(module),native_cli=identity(cli),candidate_inventory={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(module.parent.iterdir()) if p.is_file()},windows_native_selected_suites=list(markers),tiff_project_linux_native_selected_suites=list(markers),
 isolated_windows_runtime_passed=142,declared_raw_desktop_skips=3,desktop_enabled_cases_passed=33,new_producer_cases=8,new_desktop_workflows=3,pure_checks_passed=120,typing=typing,
 gui_controls_and_records_visually_inspected=True,gui_overflow_items=0,
 accepted_scope=['Local Windows raw-grid factory/batch/CLI bindings, generated stubs and strict typing','Per-ROI ImGui grid measure/keep/QRA2 CSV, managed queue retain/verify/save and desktop batch hash-only resume','Selected Windows and TIFF/project Linux native correctness, copied options, owner calibration, stale/cancelled publication and sampler-work rejection'],
 rejected_observations=['Initial runtime141pass/1fail/3skip and desktop32pass/1fail used ValueError for an established out-of-range channel error mapped to IndexError; fixed test expectation, no native rebuild.','A screenshot-only helper requested TARGET_READY on MOVE, which replay preflight rejects; final helper renders the Data tab without that invalid event.'],
 limits=['No full native/platform/wheel campaign, Linux Python runtime, Mac Python/GUI/OpenSlide acceptance or new sanitizer campaign.','No full ROI/context workspace ledger or owner-thread performance/hardware-budget acceptance.','No new biological equivalence or supported-release acceptance.'],
 release_ready=False,overall_project_complete=False,milestones={f'M{i}':'Partial' for i in range(8)},artifacts=manifest)
(source/'docs/benchmarks/raw-grid-frontends-local-windows-linux.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf-8'))
print('Verified frontend receipt:',len(manifest),'payloads; Windows/Linux5native each,142runtime+3declaredskips,33desktop,120pure,6positive/4negative typing')
