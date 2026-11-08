from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as E,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-planes-r5';cache=r/'build/ci/gridfrontends167-package'
read=lambda n:json.loads((o/n).read_bytes())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=read('compiler-inputs-compat.json');prior=read('compiler-inputs-final.json');env=q.build_environment()
fp=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fp
assert all(digest(w/p)==h for p,h in inputs['files'].items())
assert inputs['files'].keys()==prior['files'].keys()
assert [p for p,h in inputs['files'].items() if prior['files'][p]!=h]==['cpp/interchange.cpp']
assert q.has_tiff and q.has_project and q.has_interchange and q.has_gui and q.ObjectPlane(1,1,1).c==1
assert 'object-planes-r5-env' in str(Path(_native.__file__))
cohorts={name:list(E.parse(o/(name+'.xml')).iter('testcase')) for name in ('first-probe','first-probe-harness','final-probe','model','native','desktop','compat-probe','compat-native')}
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
for name in ('final-probe','native','desktop','compat-probe','compat-native'):assert all(c.find('failure') is None and c.find('error') is None for c in cohorts[name]),name
model_failed=[c for c in cohorts['model'] if c.find('failure') is not None or c.find('error') is not None]
assert len(model_failed)==1 and len(cohorts['compat-probe'])==1 and passed(cohorts['compat-probe'][0])
assert (model_failed[0].get('classname'),model_failed[0].get('name'))==(cohorts['compat-probe'][0].get('classname'),cohorts['compat-probe'][0].get('name'))
assert 'IndexError' in model_failed[0].find('failure').get('message','')
new_passed={c.get('name') for c in cohorts['final-probe'] if passed(c)}
assert {'test_import_view_mask_analysis_restart_and_export_use_exact_plane','test_saved_data_plane_uses_a_valid_replacement_source_not_the_invalid_original','test_raw_contour_publication_uses_recipe_plane_while_another_plane_is_displayed[False]','test_raw_contour_publication_uses_recipe_plane_while_another_plane_is_displayed[True]','test_schema17_default_plane_upgrade_rolls_back_without_modifying_old_project','test_unchanged_plane_metadata_edit_does_not_open_missing_link','test_axisless_qptma_import_preserves_default_plane_and_export_refuses_plane_loss'}<=new_passed
model_passed={c.get('name') for c in cohorts['model'] if passed(c)}
assert {f'test_legacy_schemas_load_and_transactionally_upgrade_to_schema18[{v}]' for v in range(1,18)}<=model_passed
assert 'test_nondefault_saved_plane_is_explicitly_refused' in model_passed
assert {c.get('name') for c in cohorts['native']}=={'object_classifier','file_fingerprint','analysis','core','qupath_project','spatial_records','selection','project','interchange'} and all(map(passed,cohorts['native']))
assert {c.get('name') for c in cohorts['desktop']}=={'test_desktop_plane_switch_filters_selection_and_measures_matching_pixels','test_managed_merge_keeps_hidden_result_plane_without_failing_automatic_selection','test_desktop_raw_batch_captures_requested_axes_and_reuses_checkpoint'} and all(map(passed,cohorts['desktop']))
assert len(cohorts['compat-native'])==1 and cohorts['compat-native'][0].get('name')=='interchange' and passed(cohorts['compat-native'][0])
for name in ('build-exit.json','build-final-exit.json','build-compat-exit.json'):assert read(name)['returncode']==0
for name in ('stubs','typing-valid','native','desktop','compat-probe','compat-native'):assert read(name+'-command.json')['returncode']==0
assert read('typing-invalid-command.json')['returncode']==1 and (o/'typing-invalid.log').read_text(encoding='utf-8').count('error:')==13
wheel,=list((o/'wheel-compat').glob('*.whl'));prior_wheel,=list((o/'wheel-final').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
with zipfile.ZipFile(wheel) as a,zipfile.ZipFile(prior_wheel) as b:
 for name in a.namelist():
  if name.startswith('qupathxx/') and not name.endswith('/'):assert a.read(name)==(site/name).read_bytes();members.append(name)
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert a.read(name)==b.read(name)==(w/'src'/name).read_bytes()
 record,=list(n for n in a.namelist() if n.endswith('.dist-info/RECORD'))
 for name,h,size in csv.reader(io.StringIO(a.read(record).decode())):
  if name==record:assert h==size=='';continue
  data=a.read(name);assert len(data)==int(size) and h=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
assert read('package-compat.json')['final_product_installs']==2 and read('package-compat.json')['total_stub_generations']==1
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
products=o/'products';products.mkdir(exist_ok=False);retained=[]
for relative in ('Release/qupathxx_interchange_tests.exe','qupathxx_core.dir/Release/interchange.obj','qupathxx_core.dir/Release/provenance.obj'):
 source=cache/relative;target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
identity=lambda c:(c.get('classname'),c.get('name'))
effective={identity(c) for n in ('final-probe','model','compat-probe') for c in cohorts[n] if passed(c)}
summary=dict(native_source=inputs['source'],workflow_validation_source=prior['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),compiler_inputs=len(inputs['files']),source_fingerprint=fp,compatibility_delta=['cpp/interchange.cpp'],native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),installed_members=len(members),record_verified=True,staged_headless_passes=sum(map(passed,cohorts['final-probe'])),staged_desktop_skips=sum(c.find('skipped') is not None for c in cohorts['final-probe']),model_passes_at_workflow_source=sum(map(passed,cohorts['model'])),model_declared_skips=sum(c.find('skipped') is not None for c in cohorts['model']),model_failure_preserved=True,corrected_compatibility_cases=1,distinct_headless_pass_identities=len(effective),native_suites_at_workflow_source=9,affected_native_suites_at_current_source=1,desktop_passes_at_workflow_source=3,pre_build_tool_only_controls=60,typing_intended_errors=13,stub_generations=1,product_installations=2,native_builds=3,build_seconds=[read(n)['elapsed_seconds'] for n in ('build-exit.json','build-final-exit.json','build-compat-exit.json')],full_platform_campaign=False,scope='Declared local Windows object-plane import/view/selection/raw analysis/publication/storage/export workflow; source695158a8 corrects only the imported-plane exception contract from774abdc4. Unaffected checks retain their recorded source/configuration scope. Broader QuPath semantics/science/resources/release remain open.',products=retained)
(o/'validation.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
