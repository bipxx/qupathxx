from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];source=r/'build/ci/scientific-grid-159'
revision='9c992171d9361ea2a438ed2ed1fae94547edf46d'
fingerprint='dc46c0c08b9a3ec95061c93e4f3fe3e1ac997232b5942363ba499b45c8ffd129'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==revision
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
out=source/'docs/benchmarks/raw-grid-batch-local';out.mkdir(exist_ok=False)
manifest={}
def copy(path,name):
 data=path.read_bytes();p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
 return data
def passing(path,name,markers=()):
 copy(path,name);cases=list(ET.parse(path).getroot().iter('testcase'));assert cases
 assert all(c.find(t) is None for c in cases for t in ('failure','error','skipped'))
 text='\n'.join(c.findtext('system-out','') for c in cases)
 for marker in markers:assert text.splitlines().count(marker)==1,(path,marker)
 return len(cases)
counts={}
for mode in ['windows','linux','tiff-linux']:
 folder=r/f'build/raw180-{mode}'
 commands=json.loads((folder/'commands.json').read_text(encoding='utf-8'));assert all(v['exit']==0 for v in commands)
 for p in folder.iterdir():
  if p.is_file():copy(p,f'{mode}/{p.name}')
 counts[mode]=passing(folder/'native-tests.xml',f'{mode}/native-tests.xml',[
  'Typed raw sampling engine identity '+fingerprint+' '+revision,
  'Shared ROI grid raw count/context planning and pre-IO admission passed',
  '188 independent derived raw grid statistics references, ROI/core support, hash-only digest and native jobs passed',
  'QRA2 raw grid retention, canonical re-encoding, semantic corruption and fresh replay passed'])
 counts[mode]+=passing(folder/'cells-tests.xml',f'{mode}/cells-tests.xml',[
  'Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed'])
 if mode=='tiff-linux':
  counts[mode]+=passing(folder/'raw-image-tests.xml',f'{mode}/raw-image-tests.xml',[
   'Actual UInt16 TIFF scientific grid analysis, ROI upsampling and hash-only digest passed',
   'Independent deflate TIFF typed grid analysis through prefixed zlib passed',
   'QRA2 TIFF grid SQLite save/reopen, explicit replay and second save passed',
   'Raw grid TIFF batch distinct recipe, interrupted checkpoint resume and hash-only typed CSV passed'])
  counts[mode]+=passing(folder/'project-batch-tests.xml',f'{mode}/project-batch-tests.xml',[
   'Grid ROI planning, typed batch recipe, per-image calibration, interrupted resume and QCL5/CSV checkpoints passed'])
 for p in (r/f'build/raw179-{mode}').iterdir():
  if p.is_file():copy(p,f'rejected-179/{mode}/{p.name}')
 rejected=list(ET.parse(r/f'build/raw179-{mode}/native-tests.xml').getroot().iter('testcase'))
 assert len(rejected)==1 and rejected[0].find('failure') is not None
 assert 'ROI must lie inside the image' in rejected[0].findtext('system-out','')
 cache=r/('build/ci/gridfrontends167-package' if mode=='windows' else 'build/ci/raw174-linux-native' if mode=='linux' else 'build/ci/raw176-tiff-linux-native')
 copy(cache/'generated/build_manifest.hpp',f'{mode}/build_manifest.hpp')
 copy(cache/'CMakeCache.txt',f'{mode}/CMakeCache.txt')
 assert fingerprint in (cache/'generated/build_manifest.hpp').read_text(encoding='utf-8')
 copy(r/f'build/ci/build_raw180_{mode.replace("-","_")}.py',f'{mode}/runner.py')
assert counts==dict(windows=2,linux=2,**{'tiff-linux':4}),counts
assert passing(r/'build/raw180-auditor-tests.xml','auditor-tests.xml')==99
for name in ['source-proof.json','compiler-input-proof.json']:
 data=json.loads(copy(r/('build/raw180-'+name),name));assert data['source']==revision
results=json.loads(copy(r/'build/raw180-cli-valid/results.json','cli/results.json'));assert len(results)==4
for case in ['uint64','axes']:
 values=[v for v in results if v['case']==case];assert [v['image']['state'] for v in values]==['saved','reused']
 assert [v['image']['raw_tiles'] for v in values]==[1,0] and [v['image']['raw_hashed_tiles'] for v in values]==[0,1]
 for p in (r/'build/raw180-cli-valid'/case).iterdir():
  if p.suffix in ('.stdout','.stderr'):copy(p,'cli/'+case+'/'+p.name)
copy(r/'build/ci/raw180_cli_valid.py','cli/check.py')
copy(r/'build/ci/raw180_cli_check.py','rejected-ome-uint64-check.py')
copy(r/'build/ci/raw178-before-raw179-native/inventory.json','prior-native-inventory.json')
copy(Path(__file__),'record_evidence.py')
cli=r/'build/ci/gridfrontends167-package/Release/qupathxx_batch.exe'
module=r/'build/ci/raw178-python-env/Lib/site-packages/qupathxx/_native.cp312-win_amd64.pyd'
identity=lambda p:dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
receipt=dict(schema='qpx.raw-grid-batch-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
 compiled_source=revision,source_tree_sha256=fingerprint,compiler_inputs=310,source_contract_files=490,
 native_selected_suites=counts,auditor_cases_passed=99,
 native_cli=identity(cli),checkpoint_fixture_reader_module=identity(module),
 checkpoint_fixture_reader_scope='Previously accepted fd32e9d Python module creates input projects and reads compatible QRA2; it does not expose or test the new producer API.',
 exact_uint64_cli_checked=True,ome_channel_z_time_cli_checked=True,unicode_checkpoint_cli_checked=True,
 cli_compute_hash_only_resume_checked=True,cli_flag_conflicts_checked=True,
 two_image_interrupted_resume_checked=True,raw_grid_recipe_separate_from_natural_checked=True,
 typed_csv_qra2_checked=True,cell_project_batch_regressions_checked=True,
 rejected_attempts=['Three source845 resampling failures from impossible outside Document annotation setup; independent later suites were not run by fail-fast runners.',
 'Initial CLI fixture writer rejected OME uint64 before CLI execution; corrected tests use ordinary uint64 TIFF and separate float64 OME axes fixture.'],
 limits=['Selected local native suites only; no complete native/platform/wheel campaign.',
 'RawGridSampling Python producer bindings syntax checked earlier; runtime and generated stubs pending grouped candidate.',
 'No new GUI producer controls/interaction acceptance, workspace/performance budget or biological/release acceptance.'],
 overall_project_complete=False,release_ready=False,milestones={f'M{i}':'Partial' for i in range(8)},artifacts=manifest)
(source/'docs/benchmarks/raw-grid-batch-local-windows-linux.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf-8'))
print('Raw grid batch receipt verified',len(manifest),'payloads',counts)
