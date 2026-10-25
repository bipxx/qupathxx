from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,re,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((r/'build/raw178-source-proof.json').read_text(encoding='utf-8'));compiler=json.loads((r/'build/raw178-compiler-input-proof.json').read_text(encoding='utf-8'));assert proof['source']==compiler['delivery_source']==source
spec=importlib.util.spec_from_file_location('raw178_evidence',root/'tools/ci/scientific_grid_evidence.py');a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
assert a.source_contract(root,source)==proof['files']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert all(sha(root/n)==v for n,v in compiler['files'].items())
folder=root/'docs/benchmarks/raw-grid-python-local';folder.mkdir(exist_ok=False)
def copy(p,name):shutil.copyfile(p,folder/name)
def cases(path,count):
 result=list(ET.parse(path).getroot().iter('testcase'));assert len(result)==count and all(c.find(t)is None for c in result for t in ('failure','error','skipped'));return result
identity=json.loads((r/'build/raw178-python-isolated/identity.log').read_text(encoding='utf-8'));assert identity['revision']==compiler['compiled_source'] and identity['source_tree_sha256']==compiler['source_tree_sha256'] and identity['has_tiff']
module=Path(identity['module']);installed=r/'build/ci/raw178-python-env/Lib/site-packages/qupathxx'/module.name
assert sha(module)==sha(installed)==sha(r/'build/ci/gridfrontends167-package/Release'/module.name)
for folder_name in ['raw178-native','raw178-gui-compile']:
 data=json.loads((r/'build'/folder_name/'commands.json').read_text(encoding='utf-8'));assert all(c['exit']==0 for c in data)
 for name in ['commands.json','configure.log','build.log']:copy(r/'build'/folder_name/name,folder_name+'-'+name)
assert json.loads((r/'build/raw178-cli/commands.json').read_text(encoding='utf-8'))['exit']==0
for name in ['commands.json','build.log']:copy(r/'build/raw178-cli'/name,'cli-'+name)
assert json.loads((r/'build/raw178-binding-syntax/commands.json').read_text(encoding='utf-8'))['exit']==0
for name in ['commands.json','syntax.log']:copy(r/'build/raw178-binding-syntax'/name,'gcc-'+name)
runtime=cases(r/'build/raw178-regression-final/python-tests.xml',137);cases(r/'build/raw178-pure-tests-final.xml',110);cases(r/'build/raw178-cohort-audit-final.xml',89)
for module_name in ['test_scientific_raw_bindings','test_public_stubs']:
 assert sorted(c.get('name') for c in runtime if c.get('classname','').endswith(module_name))==a.CASES[module_name]
positive=a.positive_typing(r/'build/raw178-typing-cohort/typing.log')
negative={name:a.negative_typing(r/'build/raw178-negative-cohort'/name,*spec) for name,spec in a.NEGATIVE.items()}
for name in ['commands.json','tests.log','python-tests.xml']:copy(r/'build/raw178-regression-final'/name,'runtime-'+name)
copy(r/'build/raw178-pure-tests-final.xml','pure-tests.xml');copy(r/'build/raw178-cohort-audit-final.xml','final-cohort-audit.xml')
copy(r/'build/raw178-typing-cohort/typing.log','typing.log');copy(r/'build/raw178-typing-cohort/commands.json','positive-typing-commands.json')
for name in a.NEGATIVE:copy(r/'build/raw178-negative-cohort'/name,name)
copy(r/'build/raw178-negative-cohort/commands.json','negative-typing-commands.json')
for name in ['commands.json','generate.log','check.log']:copy(r/'build/raw178-refined-stubs'/name,'stubs-'+name)
copy(r/'build/raw178-python-isolated/identity.log','runtime-identity.json');copy(r/'build/raw178-stubs/isolated-subprocess-identity.log','isolated-subprocess-identity.log')
copy(r/'build/raw178-source-proof.json','source-proof.json');copy(r/'build/raw178-compiler-input-proof.json','compiler-input-proof.json')
copy(r/'build/ci/raw178-before-python-build/inventory.json','retained-native-artifacts.json');copy(r/'build/ci/gridfrontends167-package/CMakeCache.txt','python-CMakeCache.txt');copy(r/'build/ci/gridfrontends167-package/generated/build_manifest.hpp','python-build_manifest.hpp')
copy(r/'build/raw178-python/identity.log','rejected-editable-import.log');copy(r/'build/raw178-stubs/tests.log','rejected-missing-cli-tests.log');copy(r/'build/raw178-typing/raw_grid_valid.log','rejected-numpy-shadow-typing.log')
for name in ['build_raw178_python.py','build_raw178_gui.py','build_raw178_cli.py','check_raw178_bindings.py','test_raw178_candidate_isolated.py','test_raw178_regression_final.py','refresh_raw178_stubs.py','check_raw178_typing_final.py','record_raw178.py']:copy(r/'build/ci'/name,name)
assert (root/'src/qupathxx/_native.pyi').read_bytes()==(module.parent/'_native.pyi').read_bytes()
assert (root/'src/qupathxx/__init__.pyi').read_bytes()==(module.parent/'__init__.pyi').read_bytes()
record={'schema':'qpx.raw-grid-python-windows-local/1','observed_at_utc':datetime.now(timezone.utc).isoformat(),'delivery_source':source,'compiled_source':compiler['compiled_source'],'source_tree_sha256':compiler['source_tree_sha256'],'compiler_inputs':compiler['compiler_inputs'],'source_contract_files':len(proof['files']),
 'compiler_input_reuse':'All 309 compiler inputs equal tested fd32e9d bytes; later commits change generated stubs, typing/tests/cohort and documentation only. Compiled revision is preserved, not relabelled.',
 'native_module':{'path':str(module),'bytes':module.stat().st_size,'sha256':sha(module)},'isolated_candidate_runtime_passed':137,'new_raw_binding_cases_included':14,'actual_tiff_sample_types':10,'source_zt_channel_nonfinite_selection_checked':True,'array_owner_reader_close_gc_checked':True,'qra2_project_replay_checked':True,
 'typing_positive':positive,'typing_negative':negative,'pure_checks_passed':110,'final_registry_auditor_passed':89,'guard_reuse_scope':'Final registry-only cohort correction rechecked all 89 auditor cases; the twelve oracle and nine signature cases remain unchanged from the 110-case run.',
 'stubs_generated_from_tested_module':True,'numpy_shadow_and_optional_descriptor_fixed':True,'windows_gui_library_compile_accepted':True,'linux_binding_syntax_accepted':True,
 'isolated_windows_python_api_accepted':True,'wheel_installed_accepted':False,'gui_behavior_accepted':False,'new_grid_cli_batch_delivery_accepted':False,'complete_raw_workflow_accepted':False,'full_native_suite_accepted':False,'platform_campaign_accepted':False,'scientific_acceptance':False,'release_ready':False,'overall_project_complete':False,
 'candidate_installation':'New CPython 3.12 environment has the tested native package copied into its site-packages and independently identified under -I; primary tests use a matching isolated staged package. This is not an installed wheel qualification.',
 'rejected_observations':['Root editable import redirected to older source/module; rejected before candidate tests; separate interpreter removed editable hook','First broader candidate lacked packaged batch executable: 135 passed, one fixture failed before CLI execution; matching current CLI built/copied and full cohort rerun','Initial generated grid descriptor lacked optionality; generated NumPy module name was shadowed by numpy methods under strict typing; regenerated using private alias without binary rebuild'],
 'rejected_log_encodings':{'rejected-missing-cli-tests.log':'Original Windows GBK stdout retained byte-for-byte; final logs explicitly UTF-8'},
 'remaining':['Raw CLI/batch grid count planning, recipe/checkpoint identity and hash-only resume','ImGui and managed viewer grid producers and runtime/visual replay','Owner-thread ROI validation and complete workspace/performance budgets','Complete wheel/native/platform campaign and remaining original M0-M7 requirements'],
 'evidence':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(folder.iterdir())}}
(root/'docs/benchmarks/raw-grid-python-local-windows.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps({'compiled':compiler['compiled_source'],'delivery':source,'runtime':137,'new_binding_cases':14,'evidence_files':len(record['evidence'])}))
