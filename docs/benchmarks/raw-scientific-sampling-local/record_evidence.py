from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159';win=r/'build/raw174-windows';linux=r/'build/raw174-linux';san=r/'build/raw174-sanitize'
folder=root/'docs/benchmarks/raw-scientific-sampling-local';folder.mkdir(exist_ok=True)
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((win/'source-proof.json').read_text(encoding='utf-8'));compiler=json.loads((win/'compiler-input-proof.json').read_text(encoding='utf-8'))
assert proof['source']==compiler['source']==source
spec=importlib.util.spec_from_file_location('typed_raw_evidence',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
assert audit.source_contract(root,source)==proof['files']
assert all(sha(root/p)==v['checked_out_sha256'] for p,v in compiler['files'].items())
expected_identity='Typed raw sampling engine identity '+compiler['source_tree_sha256']+' '+source
platforms={}
for side,path in [('windows',win),('linux',linux)]:
 commands=json.loads((path/'commands.json').read_text(encoding='utf-8'));assert all(c['exit']==0 for c in commands)
 cases=list(ET.parse(path/'native-tests.xml').getroot().iter('testcase'));assert len(cases)==1 and cases[0].get('name')=='resampling'
 assert all(cases[0].find(k)is None for k in ['failure','error','skipped'])
 lines=cases[0].findtext('system-out').splitlines()
 for marker in audit.BASE['resampling']: assert lines.count(marker)==1
 assert lines.count(expected_identity)==1 and lines.count('94 independent typed raw grid references, 3732 channel results and canonical source/input hashes passed')==1
 platforms[side]=dict(selected_native_suites=1,name='resampling',identity=expected_identity,commands=commands)
for p in [san/'commands.json']:
 assert all(c['exit']==0 for c in json.loads(p.read_text(encoding='utf-8')))
assert expected_identity in (san/'tests.log').read_text(encoding='utf-8').splitlines()
python=list(ET.parse(r/'build/raw174-auditor-tests.xml').getroot().iter('testcase'));assert len(python)==123 and all(c.find(k)is None for c in python for k in ['failure','error','skipped'])
for side,path in [('windows',win),('linux',linux)]:
 for name in ['commands.json','build.log','tests.log','native-tests.xml']:
  shutil.copyfile(path/name,folder/(side+'-'+name))
if (linux/'configure.log').exists():shutil.copyfile(linux/'configure.log',folder/'linux-configure.log')
for name in ['commands.json','build.log','tests.log']:
 shutil.copyfile(san/name,folder/('sanitizer-'+name))
for name in ['source-proof.json','compiler-input-proof.json']:
 shutil.copyfile(win/name,folder/name)
shutil.copyfile(r/'build/raw174-auditor-tests.xml',folder/'python-auditor-tests.xml')
shutil.copyfile(r/'build/ci/source169-before-raw174-native/inventory.json',folder/'retained-source169-artifacts.json')
for side,build in [('windows',r/'build/ci/gridfrontends167-package'),('linux',r/'build/ci/raw174-linux-native')]:
 shutil.copyfile(build/'generated/build_manifest.hpp',folder/(side+'-build_manifest.hpp'))
 shutil.copyfile(build/'CMakeCache.txt',folder/(side+'-CMakeCache.txt'))
for name in ['build_raw174_windows.py','build_raw174_linux.py','sanitize_raw174.py']:
 shutil.copyfile(Path(__file__).with_name(name),folder/name)
shutil.copyfile(r/'build/raw174-sanitize-driver.cpp',folder/'sanitizer-driver.cpp')
shutil.copyfile(Path(__file__),folder/'record_evidence.py')
binaries=dict(windows=r/'build/ci/gridfrontends167-package/Release/qupathxx_resampling_tests.exe',linux=r/'build/ci/raw174-linux-native/qupathxx_resampling_tests',sanitizer=r/'build/ci/raw174-sanitized-reader')
pyd=list((r/'build/ci/gridfrontends167-package/Release').glob('*.pyd'));assert len(pyd)==1 and sha(pyd[0])=='bc286d4d160c59fbccf853173820ed88280ddc4ef28e0ce31cc3553e9c9755d0'
record=dict(schema='qpx.scientific-raw-sampling-native-local/1',observed_at_utc=datetime.now(timezone.utc).isoformat(),source=source,
 source_tree_sha256=compiler['source_tree_sha256'],compiler_inputs=compiler['compiler_inputs'],source_contract_files=len(proof['files']),
 platforms=platforms,native_reference_cases=94,channel_results_per_platform=3732,canonical_hash_pairs_per_platform=94,sample_types=10,
 exact_mean_vectors_per_platform=2891,exact_mean_orders=['forward','reverse'],legacy_area_oracle_shapes_per_platform=2205,
 affected_python_passed=123,oracle_python_passed_included=6,
 sanitized_scope='New raw sampler, native fake reader/tests and exact-mean tests instrumented with ASan/UBSan; linked inherited core remains unsanitized, not a full sanitizer campaign',
 executable_hashes={k:dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p)) for k,p in binaries.items()},
 baseline_native_cache_artifacts_retained=63,old_installed_module_unchanged=True,build_cache_state='Windows core library and resampling executable are current; other cached executables/module remain older and are not relabelled',
 scope='Native bounded level-zero typed grid reader, all dtypes, channel/Z/T selection, per-channel finite/support, canonical source/input hashes and pre-pixel-IO admission using independent fake raw readers',
 native_raw_grid_reader_implemented=True,native_resampling_target_accepted=True,full_native_suite_accepted=False,
 new_tiff_sampling_integration_accepted=False,raw_analysis_workflow_accepted=False,installed_python_api_accepted=False,gui_accepted=False,
 platform_campaign_accepted=False,scientific_acceptance=False,release_ready=False,overall_project_complete=False,
 remaining=['Native immutable raw analysis request/result/jobs and versioned retention/replay','Shared hash-only raw grid input scan for resume','Python CLI batch ImGui adapters and installed wheel validation','Actual TIFF-backed sampling and wider vendor/pyramid/tiled admission','All remaining original M0-M7 criteria'],
 evidence={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.iterdir()) if p.is_file()})
(root/'docs/benchmarks/raw-scientific-sampling-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps(dict(source=source,native_cases=94,channel_results=3732,python=123,compiler_inputs=compiler['compiler_inputs'],source_files=len(proof['files']),receipt=str(root/'docs/benchmarks/raw-scientific-sampling-local-windows-linux.json'))))
