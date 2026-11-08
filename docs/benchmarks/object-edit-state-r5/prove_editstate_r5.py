from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as ET,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-edit-state-r5';cache=r/'build/ci/gridfrontends167-package'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda name:json.loads((o/name).read_bytes())
inputs=read('compiler-inputs-final.json');first=read('compiler-inputs.json');env=q.build_environment()
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fingerprint
assert all(digest(w/p)==h for p,h in inputs['files'].items())
delta=[p for p,h in inputs['files'].items() if first['files'].get(p)!=h]
assert delta==['cpp/classification_edit.hpp','cpp/document_classifications.cpp']
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
cohorts={n:ET.parse(o/(n+'.xml')).findall('.//testcase') for n in ('before','history-budget-before','history-budget-staged','model','native','desktop','observer-before-build')}
assert len(cohorts['before'])==2 and all(c.find('failure') is not None for c in cohorts['before'])
assert len(cohorts['history-budget-before'])==1 and all(passed(c) for c in cohorts['history-budget-before'])
assert len(cohorts['history-budget-staged'])==1 and all(c.find('failure') is not None for c in cohorts['history-budget-staged'])
assert len(cohorts['observer-before-build'])==58 and all(passed(c) for c in cohorts['observer-before-build'])
assert all(c.find('failure') is None and c.find('error') is None for c in cohorts['model'])
assert sum(passed(c) for c in cohorts['model'])==764 and sum(c.find('skipped') is not None for c in cohorts['model'])==16
state=[c for c in cohorts['model'] if c.get('classname')=='tests.test_object_edit_state']
assert len(state)==31 and sum(passed(c) for c in state)==27 and sum(c.find('skipped') is not None for c in state)==4
assert any(c.get('name')=='test_plain_classification_keeps_the_existing_compact_history_budget' and passed(c) for c in state)
assert {c.get('name') for c in cohorts['native']}=={'object_classifier','file_fingerprint','qupath_project','spatial_records','selection','project','interchange'} and all(passed(c) for c in cohorts['native'])
assert 'Object editing state: shared facts, exact version2 codec and version1 compatibility refusal passed' in ''.join(c.findtext('system-out','') for c in cohorts['native'])
assert len(cohorts['desktop'])==7 and all(passed(c) for c in cohorts['desktop'])
for name in ('stubs','typing-valid','model','native','desktop'):assert read(name+'-command.json')['returncode']==0
assert read('typing-invalid-command.json')['returncode']==1 and (o/'typing-invalid.log').read_text(encoding='utf-8').count('error:')==9
for name in ('build-exit.json','build-final-exit.json'):assert read(name)['returncode']==0
assert 'RawTmaContext' in (o/'history-budget-first.log').read_text(encoding='utf-8')
probe=(o/'history-budget-staged.log').read_text(encoding='utf-8');assert first['source'] in probe and 'history payload budget' in probe
first_wheel,=list((o/'wheel').glob('*.whl'))
with zipfile.ZipFile(first_wheel) as a:
 name,=list(n for n in a.namelist() if n.endswith('.pyd'))
 assert hashlib.sha256(a.read(name)).hexdigest() in probe
wheel,=list((o/'wheel-final').glob('*.whl'));site=Path(q.__file__).parent.parent;assert 'object-edit-state-r5-env' in str(site)
members=[]
with zipfile.ZipFile(wheel) as archive:
 for name in archive.namelist():
  if name.startswith('qupathxx/') and not name.endswith('/'):
   assert archive.read(name)==(site/name).read_bytes();members.append(name)
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert archive.read(name)==(w/'src'/name).read_bytes()
 record,=list(n for n in archive.namelist() if n.endswith('.dist-info/RECORD'))
 for name,h,size in csv.reader(io.StringIO(archive.read(record).decode())):
  if name==record:assert h==size=='';continue
  data=archive.read(name);assert len(data)==int(size)
  assert h=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
package=read('package.json');assert package['final_product_installs']==1 and not package['native_rebuild_for_stubs'] and package['final_wheel_sha256']==digest(wheel)
old,=list((o/'wheel-before-stubs').glob('*.whl'))
with zipfile.ZipFile(old) as a,zipfile.ZipFile(wheel) as b:
 name,=list(n for n in a.namelist() if n.endswith('.pyd'));assert a.read(name)==b.read(name)
 assert hashlib.sha256(a.read(name)).hexdigest()==package['native_sha256']
products=o/'products';products.mkdir(exist_ok=False);retained=[]
for relative in ('Release/_native.cp312-win_amd64.pyd','Release/qupathxx_project_tests.exe','Release/qupathxx_interchange_tests.exe','Release/qupathxx_object_classifier_tests.exe','qupathxx_core.dir/Release/object_properties.obj','qupathxx_core.dir/Release/document_classifications.obj','qupathxx_core.dir/Release/provenance.obj'):
 source=cache/relative;target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
summary=dict(native_source=inputs['source'],first_candidate_source=first['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),source_fingerprint=fingerprint,compiler_inputs=len(inputs['files']),private_history_correction_inputs=delta,installed_members=len(members),native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),record_verified=True,model_passes=764,model_declared_skips=16,new_edit_state_model_passes=27,new_edit_state_gui_skips=4,desktop_passes=7,new_desktop_cases=4,native_suites=7,pre_build_tool_only_controls=58,typing_intended_errors=9,stub_check=True,native_build_attempts=2,successful_native_builds=2,build_elapsed_seconds=[read(n)['elapsed_seconds'] for n in ('build-exit.json','build-final-exit.json')],first_candidate_budget_regression=True,uninstalled_staged_budget_probe=True,wrong_interpreter_probe_preserved=True,compact_history_budget='1000 unchanged-geometry classifications fit the existing64KiB budget after sparse property-owner patch',final_product_installations=1,full_platform_campaign=False,scope='Local Windows editing hints,24-bit color and reported probability; planes, full QuPath property semantics, science/performance/release remain open',products=retained)
(o/'validation.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
