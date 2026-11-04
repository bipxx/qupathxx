from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as E,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/root-facts-r5';cache=r/'build/ci/gridfrontends167-package'
read=lambda n:json.loads((o/n).read_bytes());digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=read('compiler-inputs-store.json');env=q.build_environment();fp=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fp
assert all(digest(w/p)==h for p,h in inputs['files'].items())
assert 'root-facts-r5-env' in str(Path(_native.__file__)) and q.has_project and q.has_tiff and q.has_interchange
assert not subprocess.check_output(['git','status','--porcelain'],cwd=w)
cohorts={n:list(E.parse(o/(n+'.xml')).iter('testcase')) for n in ('baseline','ownership-before','staged','staged-collect-fixed','staged-store','staged-report-fixed','native','native-owned','native-store','desktop','public-stubs')}
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
key=lambda c:(c.get('classname'),c.get('name'))
latest={key(c):c for c in cohorts['staged-collect-fixed']};current={key(c):c for c in cohorts['staged-store']}
reused=set(latest)-set(current)
assert all(cls=='tests.test_document_edits' or (cls=='tests.test_classification_edits' and name=='test_classification_batch_is_atomic_and_preserves_redo') for cls,name in reused)
latest.update(current);latest.update({key(c):c for c in cohorts['staged-report-fixed']})
assert all(c.find('failure') is None and c.find('error') is None for c in latest.values())
assert len(cohorts['baseline'])==1 and 'root_properties_dropped' in cohorts['baseline'][0].find('failure').text
assert len(cohorts['ownership-before'])==1 and 'cleanup timed out' in cohorts['ownership-before'][0].find('failure').text
assert digest(o/'ownership-before-test.py')==read('ownership-before-command.json')['observer_file_sha256']
assert read('staged-command.json')['returncode']==4 and not cohorts['staged']
assert sum(c.find('failure') is not None for c in cohorts['staged-collect-fixed'])==6
assert sum(c.find('failure') is not None for c in cohorts['staged-store'])==2
assert all(map(passed,cohorts['native-owned'])) and all(map(passed,cohorts['native-store']))
assert {c.get('name') for c in cohorts['native-owned']}=={'object_classifier','document_edits','qupath_project','project','interchange','project_save'}
assert {c.get('name') for c in cohorts['native-store']}=={'project','qupath_project','project_save'}
assert len(cohorts['desktop'])==1 and all(map(passed,cohorts['desktop'])) and all(map(passed,cohorts['public-stubs']))
assert all(read(n)['returncode']==0 for n in ('build-exit.json','build-owned-exit.json','build-store-exit.json','native-owned-command.json','native-store-command.json','staged-report-fixed-command.json','desktop-command.json'))
assert read('typing-commands.json')['typing']['returncode']==0 and read('typing-commands.json')['public-stubs']['returncode']==0
negative=(o/'typing-negative.log').read_text(encoding='utf-8');assert negative.count(': error:')==3 and all(c in negative for c in ('[arg-type]','[assignment]','[misc]'))
new=[c for c in latest.values() if c.get('classname')=='tests.test_image_properties']
for scope in ('image','object'):
 assert any(c.get('name')=='test_async_image_facts_final_release_needs_no_python_on_save_worker['+scope+']' and passed(c) for c in new)
owned=read('compiler-inputs-owned.json')['files'];assert owned.keys()==inputs['files'].keys()
assert [p for p,h in inputs['files'].items() if owned[p]!=h]==['cpp/project.cpp']
assert not any('Project' in (w/'cpp/tests'/name).read_text(encoding='utf-8') for name in ('document_edits_tests.cpp','object_classifier_tests.cpp','interchange_tests.cpp'))
wheel,=list((o/'wheel-final').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
with zipfile.ZipFile(wheel) as z:
 for name in z.namelist():
  if name.startswith('qupathxx/') and not name.endswith('/'):assert z.read(name)==(site/name).read_bytes();members.append(name)
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert z.read(name)==(w/'src'/name).read_bytes()
 record,=list(n for n in z.namelist() if n.endswith('.dist-info/RECORD'))
 for name,h,size in csv.reader(io.StringIO(z.read(record).decode())):
  if name==record:assert h==size=='';continue
  data=z.read(name);assert len(data)==int(size) and h=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
assert read('package.json')['final_product_installs']==read('package.json')['stub_generations']==1
pin=json.loads((w/'tests/fixtures/qupath-root-facts-0.7.0/manifest.json').read_bytes())
assert read('runtime-verification.json')['runtime_tree_sha256']==pin['upstream']['runtime_tree_sha256']
products=o/'products';products.mkdir();retained=[]
for name in ('document_edits','object_classifier','interchange','qupath_project','project','save'):
 source=cache/('Release/qupathxx_'+name+'_tests.exe');target=products/source.name;shutil.copyfile(source,target)
 origin=inputs['source'] if name in ('qupath_project','project','save') else read('compiler-inputs-owned.json')['source']
 retained.append(dict(path=target.relative_to(o).as_posix(),native_source=origin,sha256=digest(target)))
builds=[read(n) for n in ('build-exit.json','build-owned-exit.json','build-store-exit.json')]
summary=dict(native_source=inputs['source'],native_kernel_source=read('compiler-inputs-owned.json')['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),compiler_inputs=len(inputs['files']),source_fingerprint=fp,native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),installed_members=len(members),record_verified=True,project_schema=19,staged_passes=sum(map(passed,latest.values())),staged_skips=sum(c.find('skipped') is not None for c in latest.values()),reused_pure_model_cases=len(reused),new_headless_passes=sum(map(passed,new)),native_suites=6,native_writer_suites_on_final_source=3,desktop_passes=1,packaged_stub_passes=len(cohorts['public-stubs']),typing_negative_diagnostics=3,native_builds=3,build_seconds=[v['elapsed_seconds'] for v in builds],product_installations=1,stub_generations=1,baseline_feature_loss_failures=1,ownership_deadlock_failures=1,collection_failures=1,staged_initial_failures=6,staged_observer_failures=2,failed_inputs_and_results_retained=True,pinned_runtime_files=496,full_platform_campaign=False,scope='Local Windows saved-data root metadata/measurements with immutable image-level facts, native shared image/ROI ownership, borrowed SQLite BLOB lifetime repair, schema19 snapshots/migration/rollback/integrity, thin Python/types and actual ImGui inspector/save/reopen. Native kernel suites retain ce786260 input scope; final writer suites run405ba1ad. No broader ImageData settings, scientific, platform-release or overall speed claim.',products=retained)
(o/'validation.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
