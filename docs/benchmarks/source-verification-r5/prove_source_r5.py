from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as ET,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/source-verification-r5';cache=r/'build/ci/gridfrontends167-package'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda name:json.loads((o/name).read_bytes())
inputs=read('compiler-inputs-ui.json');first=read('compiler-inputs-final.json');env=q.build_environment()
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fingerprint
assert all(digest(w/p)==h for p,h in inputs['files'].items())
delta=[p for p,h in inputs['files'].items() if first['files'].get(p)!=h]
assert delta==['cpp/gui.cpp','cpp/tests/project_tests.cpp']
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
cohorts={n:ET.parse(o/(n+'.xml')).findall('.//testcase') for n in ('before','model','native','native-final','native-ui','desktop','desktop-ui','desktop-ui-final','final-identity')}
assert sum(passed(c) for c in cohorts['model'])==687
assert sum(c.find('skipped') is not None for c in cohorts['model'])==10
assert all(c.find('failure') is None and c.find('error') is None for c in cohorts['model'])
assert sum(c.find('failure') is not None for c in cohorts['before'])==2
assert sum(passed(c) for c in cohorts['native'])==4
for n in ('native-final','native-ui'):assert len(cohorts[n])==1 and all(passed(c) for c in cohorts[n])
assert 'Project source verification: historical round trip, reviewed replacement, stale/file-change refusal and codec checks passed' in ''.join(c.findtext('system-out','') for c in cohorts['native-ui'])
desktop={ (c.get('classname'),c.get('name')) for n in ('desktop-ui','desktop-ui-final') for c in cohorts[n] if passed(c)}
assert len(desktop)==3
assert sum(passed(c) for c in cohorts['final-identity'])==18
assert sum(c.find('skipped') is not None for c in cohorts['final-identity'])==3
for n in ('stubs-command.json','stubs-ui-command.json','typing-valid-command.json','final-identity-command.json','native-ui-command.json'):assert read(n)['returncode']==0
assert read('typing-invalid-command.json')['returncode']==1
assert (o/'typing-invalid.log').read_text(encoding='utf-8').count('error:')==3
builds=[read(n) for n in ('build-exit.json','build-final-exit.json','build-ui-exit.json')]
assert [b['returncode'] for b in builds]==[1,0,0]
observer=read('native-observer-build.json');assert observer['returncode']==0 and observer['retained_products_unchanged']
wheel,=list((o/'wheel-ui').glob('*.whl'));site=Path(q.__file__).parent.parent
assert 'source-verification-r5-ui-env' in str(site)
members=[]
with zipfile.ZipFile(wheel) as archive:
 for name in archive.namelist():
  if name.startswith('qupathxx/') and not name.endswith('/'):
   assert archive.read(name)==(site/name).read_bytes();members.append(name)
 assert len(members)==62
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert archive.read(name)==(w/'src'/name).read_bytes()
 record,=list(n for n in archive.namelist() if n.endswith('.dist-info/RECORD'))
 for name,h,size in csv.reader(io.StringIO(archive.read(record).decode())):
  if name==record:assert h==size=='';continue
  data=archive.read(name);assert len(data)==int(size)
  assert h=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
 with zipfile.ZipFile(next((o/'wheel').glob('*.whl'))) as old:
  for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert archive.read(name)==old.read(name)
package=read('package-ui.json');assert package['wheel_sha256']==digest(wheel) and not package['stub_repackaging']
old_package=read('package.json');assert not old_package['native_rebuild_for_stubs']
old_wheel,=list((o/'wheel-before-stubs').glob('*.whl'))
with zipfile.ZipFile(old_wheel) as a, zipfile.ZipFile(next((o/'wheel').glob('*.whl'))) as b:
 name,=list(n for n in a.namelist() if n.endswith('.pyd'))
 assert a.read(name)==b.read(name) and hashlib.sha256(a.read(name)).hexdigest()==old_package['native_sha256']
products=o/'products';products.mkdir(exist_ok=False);retained=[]
for relative in ('Release/_native.cp312-win_amd64.pyd','Release/qupathxx_project_tests.exe','qupathxx_core.dir/Release/project_source.obj','qupathxx_core.dir/Release/provenance.obj'):
 source=cache/relative;target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
summary=dict(native_source=inputs['source'],first_candidate_source=first['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),source_fingerprint=fingerprint,compiler_inputs=len(inputs['files']),changed_inputs_since_first_candidate=delta,installed_members=len(members),native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),record_verified=True,model_scope='687 passes,10 declared skips on a66c8af5; unchanged production core reused, not relabeled',final_identity_source_workflow_passes=18,final_identity_desktop_skips=3,final_native_project_pass=True,other_native_suites_reused=4,desktop_distinct_passes=3,desktop_final_sources=['ca009219 product','7b6f871e observer'],typing_valid=True,typing_intended_errors=3,stub_check_final=True,native_build_attempts=3,successful_native_builds=2,build_elapsed_seconds=[b['elapsed_seconds'] for b in builds],observer_only_build_seconds=observer['elapsed_seconds'],candidate_product_installations=2,final_corrected_installations=1,gui_fix_regenerated_stubs=False,full_platform_campaign=False,scope='Windows local; primary-file hash and compound dependency metadata, not all companion bytes or historical decoder proof',products=retained)
(o/'validation.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
