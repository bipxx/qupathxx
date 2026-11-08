from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159';out=r/'build/raw173-mean-final'
folder=root/'docs/benchmarks/raw-scientific-means-local';folder.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((out/'source-proof.json').read_text(encoding='utf-8'));assert proof['source']==source
spec=importlib.util.spec_from_file_location('raw_math_proof',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
assert proof['files']==audit.source_contract(root,source)
commands=json.loads((out/'commands.json').read_text(encoding='utf-8'));assert len(commands['commands'])==7 and all(c['exit']==0 for c in commands['commands'])
assert all(sha(root/p)==h for p,h in commands['compiler_input_hashes'].items())
marker='Native exact typed weighted means, subnormal/cancellation, atomic rejection and nearest-even rounding passed'
cohort='2891 independent exact raw mean vectors across all ten sample types in both orders passed'
cases=list(ET.parse(out/'msvc-tests.xml').getroot().iter('testcase'));assert len(cases)==1 and all(cases[0].find(k)is None for k in ['failure','error','skipped'])
assert cases[0].findtext('system-out').splitlines()==[cohort,marker]
for name in ['gcc-test.log','sanitize-test.log']:
 assert (out/name).read_text(encoding='utf-8').splitlines()==[cohort,marker]
for name,count in [('oracle-tests.xml',3),('auditor-tests.xml',118)]:
 selected=list(ET.parse(out/name).getroot().iter('testcase'));assert len(selected)==count and all(c.find(k)is None for c in selected for k in ['failure','error','skipped'])
for name in ['commands.json','msvc-build.log','msvc-test.log','msvc-tests.xml','gcc-build.log','gcc-test.log','sanitize-build.log','sanitize-test.log','oracle.log','oracle-tests.xml','auditor-tests.xml','source-proof.json']:
 shutil.copyfile(out/name,folder/name)
for name in ['build.log','before-exact_weighted_mean.hpp','commands.json']:
 shutil.copyfile(r/'build/raw173-mean-windows'/name,folder/('initial-msvc-'+name))
shutil.copyfile(r/'build/ci/raw173-mean-windows/CMakeCache.txt',folder/'standalone-CMakeCache.txt')
compiler=list((r/'build/ci/raw173-mean-windows/CMakeFiles').rglob('CMakeCXXCompiler.cmake'));assert len(compiler)==1
shutil.copyfile(compiler[0],folder/'CMakeCXXCompiler.cmake')
shutil.copyfile(r/'build/ci/raw173-mean-source/CMakeLists.txt',folder/'standalone-CMakeLists.txt')
shutil.copyfile(r/'build/raw173-mean-initial/inventory.json',folder/'initial-dynamic-table-inventory.json')
for name in ['final_raw173_mean.py','verify_raw173_mean.py']:
 shutil.copyfile(Path(__file__).with_name(name),folder/name)
shutil.copyfile(Path(__file__),folder/'record_evidence.py')
(folder/'gcc-version.log').write_bytes(subprocess.check_output(['wsl.exe','-d','Ubuntu','--','g++','--version']))
executables={'msvc':r/'build/ci/raw173-mean-windows/Release/raw_scientific_mean_probe.exe','gcc':r/'build/ci/raw173-mean-final','gcc_asan_ubsan':r/'build/ci/raw173-mean-sanitize'}
record=dict(schema='qpx.raw-scientific-means-local/1',observed_at_utc=datetime.now(timezone.utc).isoformat(),source=source,
 source_contract_files=len(proof['files']),compiler_inputs=commands['compiler_input_hashes'],
 reference_vectors=2891,source_sample_types=10,input_orders=['forward','reverse'],
 numerical_configurations=['MSVC Release W4 WX fp:precise','GCC O2 Wall Wextra Werror pedantic','GCC O1 ASan UBSan'],
 executable_hashes={k:dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p)) for k,p in executables.items()},
 affected_python_passed=118,oracle_python_passed_included=3,
 scope='Private exact finite weighted mean kernel and independent all-dtype oracle; no raw sampler/application workflow acceptance',
 checks=['Bit-exact integer and floating weighted means','Subnormal/normal ties and negative underflow zero','Large finite mean without intermediate floating overflow','Exact cancellation and input order invariance','INT64_MIN/UInt64 range and atomic invalid-weight/nonfinite rejection','Independent Fraction regeneration and overwrite protection','New native marker omission/duplication rejects'],
 integrated_native_resampling_wired=True,integrated_native_resampling_built=False,
 raw_grid_reader_implemented=False,raw_analysis_workflow_accepted=False,installed_native_application_accepted=False,
 platform_campaign_accepted=False,scientific_acceptance=False,release_ready=False,overall_project_complete=False,
 remaining=['Bounded typed raw grid reader and per-channel support/finite reasons','Raw source bit/axis/calibration provenance','Immutable analysis/jobs/records and save/reopen/replay','Python CLI batch ImGui adapters','Full original M0-M7 criteria'],
 evidence={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.iterdir()) if p.is_file()})
(root/'docs/benchmarks/raw-scientific-means-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps(dict(source=source,vectors=2891,numeric_configurations=3,python_passed=118,source_files=len(proof['files']),receipt=str(root/'docs/benchmarks/raw-scientific-means-local-windows-linux.json'))))
