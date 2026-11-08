from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,re,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((r/'build/raw177-source-proof.json').read_text(encoding='utf-8'));compiler=json.loads((r/'build/raw177-compiler-input-proof.json').read_text(encoding='utf-8'))
assert proof['source']==compiler['source']==source
spec=importlib.util.spec_from_file_location('raw177_audit',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
assert audit.source_contract(root,source)==proof['files']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert all(sha(root/n)==v for n,v in compiler['files'].items())
folder=root/'docs/benchmarks/raw-grid-retention-local';folder.mkdir(exist_ok=False)
def copy(p,name):shutil.copyfile(p,folder/name)
def cases(p):
 result=list(ET.parse(p).getroot().iter('testcase'))
 assert all(c.find(t) is None for c in result for t in ('failure','error','skipped'))
 return result
platforms={}
for side,cache in [('windows','gridfrontends167-package'),('linux','raw174-linux-native'),('tiff-linux','raw176-tiff-linux-native')]:
 out=r/('build/raw177-final-'+side);commands=json.loads((out/'commands.json').read_text(encoding='utf-8'));assert all(c['exit']==0 for c in commands)
 native=cases(out/'native-tests.xml');assert len(native)==1 and native[0].get('name')=='resampling';lines=native[0].findtext('system-out').splitlines()
 for marker in audit.BASE['resampling']:assert lines.count(marker)==1
 expected='Typed raw sampling engine identity '+compiler['source_tree_sha256']+' '+source
 assert lines.count(expected)==1 and not any('removed since it exceeds' in line for line in lines)
 build=r/'build/ci'/cache;manifest=build/'generated/build_manifest.hpp'
 assert re.search(r'source_sha256\[\]="([0-9a-f]{64})"',manifest.read_text(encoding='utf-8')).group(1)==compiler['source_tree_sha256']
 binary=build/('Release/qupathxx_resampling_tests.exe' if side=='windows' else 'qupathxx_resampling_tests')
 platforms[side]={'identity':expected,'selected_suites':['resampling'],'resampling_executable_sha256':sha(binary),'commands':commands}
 for name in ['commands.json','configure.log','build.log','native-tests.xml']+(['tests-resampling.log','tests-raw-image.log','raw-image-tests.xml'] if side=='tiff-linux' else ['tests.log']):copy(out/name,side+'-'+name)
 copy(manifest,side+'-build_manifest.hpp');copy(build/'CMakeCache.txt',side+'-CMakeCache.txt')
 if side=='tiff-linux':
  raw=cases(out/'raw-image-tests.xml');assert len(raw)==1 and raw[0].get('name')=='raw_image'
  lines=raw[0].findtext('system-out').splitlines()
  for marker in ['QRA2 TIFF grid SQLite save/reopen, explicit replay and second save passed','Independent deflate TIFF typed grid analysis through prefixed zlib passed','Actual UInt16 TIFF scientific grid analysis, ROI upsampling and hash-only digest passed']:assert lines.count(marker)==1
  platforms[side]['selected_suites'].append('raw_image');platforms[side]['raw_image_executable_sha256']=sha(build/'qupathxx_raw_image_tests')
san=r/'build/raw177-sanitize';commands=json.loads((san/'commands.json').read_text(encoding='utf-8'));assert all(c['exit']==0 for c in commands)
lines=(san/'tests.log').read_text(encoding='utf-8').splitlines();assert lines.count(expected)==1
for marker in audit.BASE['resampling'][2:]:assert lines.count(marker)==1
for name in ['commands.json','build.log','tests.log']:copy(san/name,'selective-sanitizer-'+name)
assert len(cases(r/'build/raw177-python-final.xml'))==97
copy(r/'build/raw177-python-final.xml','python-tests.xml');copy(r/'build/raw177-source-proof.json','source-proof.json');copy(r/'build/raw177-compiler-input-proof.json','compiler-input-proof.json')
copy(r/'build/ci/source176-before-raw177-native/inventory.json','retained-source176-artifacts.json')
copy(r/'build/raw177-windows/native-tests.xml','rejected-truncated-native-tests.xml')
for name in ['build_raw177_final_windows.py','build_raw177_final_linux.py','build_raw177_final_tiff_linux.py','sanitize_raw177.py','record_raw177.py']:copy(r/'build/ci'/name,name)
copy(r/'build/raw177-sanitize-driver.cpp','selective-sanitizer-driver.cpp')
pyd=list((r/'build/ci/gridfrontends167-package/Release').glob('*.pyd'));assert len(pyd)==1 and sha(pyd[0])=='bc286d4d160c59fbccf853173820ed88280ddc4ef28e0ce31cc3553e9c9755d0'
record={'schema':'qpx.raw-grid-retention-native-local/1','observed_at_utc':datetime.now(timezone.utc).isoformat(),'source':source,'source_tree_sha256':compiler['source_tree_sha256'],'compiler_inputs':compiler['compiler_inputs'],'source_contract_files':len(proof['files']),
 'platforms':platforms,'sample_types':10,'sampling_reference_cases':94,'channel_results':3732,'statistics_retention_replay_cases':188,'independent_archive_vectors':2,'independent_archive_versions':['QRA1','QRA2'],'affected_python_passed':97,'oracle_python_passed_included':12,
 'scope':'Native typed grid result retention, canonical QRA2 and legacy QRA1 bytes, hash/metadata/support validation, immutable snapshots, fresh replay and schema-2 grid/mixed CSV; actual UInt16 TIFF project save/reopen/replay and second save',
 'sanitizer_scope':'New sampling, analysis, record, archive and CSV translation units plus fake-reader/mean/retention tests instrumented with ASan/UBSan at this source. Remaining linked minimal core is unsanitized; no TIFF/project codec instrumentation or full sanitizer campaign.',
 'selective_sanitizer_executable_sha256':sha(r/'build/ci/raw177-sanitized-reader'),'native_grid_retention_replay_csv_implemented':True,'local_native_retention_accepted':True,'legacy_qra1_reference_preserved':True,'actual_uint16_tiff_project_replay_accepted':True,
 'baseline_module_unchanged':True,'baseline_module_sha256':sha(pyd[0]),'prior_native_artifacts_retained':13,
 'full_native_suite_accepted':False,'complete_raw_analysis_workflow_accepted':False,'installed_python_api_accepted':False,'cli_batch_grid_delivery_accepted':False,'gui_accepted':False,'platform_campaign_accepted':False,'scientific_acceptance':False,'release_ready':False,'overall_project_complete':False,
 'rejected_observations':['The initial df21113 native tests passed execution, but default CTest output truncation removed required markers; retained JUnit is unaccepted as full semantic evidence. Final ab206a7 runs retain complete output.'],
 'remaining':['Nanobind ownership and typed grid descriptors; generated stubs and installed validation','Raw CLI/batch grid options, typed checkpoint identity and hash-only resume','ImGui and managed viewer producer/retained-grid presentation','Broader TIFF dtypes/vendors/pyramids and performance budgets','Complete installed/native/platform campaign and all remaining M0-M7 criteria'],
 'evidence':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(folder.iterdir())}}
(root/'docs/benchmarks/raw-grid-retention-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps({'source':source,'fingerprint':compiler['source_tree_sha256'],'evidence_files':len(record['evidence'])}))
