from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as E,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/source-plane-replacement-r5';cache=r/'build/ci/gridfrontends167-package'
read=lambda n:json.loads((o/n).read_bytes());digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=read('compiler-inputs.json');prior=json.loads((r/'build/object-planes-r5/compiler-inputs-compat.json').read_bytes());env=q.build_environment()
fp=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fp
assert all(digest(w/p)==h for p,h in inputs['files'].items())
assert inputs['files'].keys()==prior['files'].keys() and [p for p,h in inputs['files'].items() if prior['files'][p]!=h]==['cpp/project.cpp']
assert 'source-plane-replacement-r5-env' in str(Path(_native.__file__))
cohorts={n:list(E.parse(o/(n+'.xml')).iter('testcase')) for n in ('before','staged','native','desktop','desktop-retry','desktop-save','desktop-save-fixed')}
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
assert len(cohorts['before'])==6 and all(c.find('failure') is not None and 'DID NOT RAISE' in c.find('failure').get('message','') for c in cohorts['before'])
assert all(c.find('failure') is None and c.find('error') is None for c in cohorts['staged'])
assert {c.get('name') for c in cohorts['native']}=={'project','qupath_project','file_fingerprint'} and all(map(passed,cohorts['native']))
new=[c for c in cohorts['staged'] if c.get('classname')=='tests.test_source_plane_replacement']
assert sum(map(passed,new))==15 and sum(c.find('skipped') is not None for c in new)==2
for axis in ('z','t','c'):
 for route in ('relink','review'):
  for prefix in ('test_reduced_axes_refuse_without_publishing_reader_path_or_baseline','test_compatible_reduction_keeps_reader_handoff_planes_and_saved_replay'):
   assert any(c.get('name')==prefix+'['+axis+'-'+route+']' and passed(c) for c in new)
assert any(c.get('name')=='test_plane_edit_after_review_is_checked_at_apply_and_refusal_does_not_consume_review' and passed(c) for c in new)
assert any(c.get('name')=='test_undo_restored_plane_is_checked_before_source_adoption' and passed(c) for c in new)
desktop=[c for c in cohorts['desktop-retry'] if c.get('name').endswith('[False]')]+cohorts['desktop-save-fixed']
assert len(desktop)==2 and all(map(passed,desktop))
assert read('build-exit.json')['returncode']==0 and read('staged-command.json')['returncode']==0 and read('native-command.json')['returncode']==0
assert read('desktop-command.json')['returncode']==1 and read('desktop-retry-command.json')['returncode']==1 and read('desktop-save-command.json')['returncode']==1 and read('desktop-save-fixed-command.json')['returncode']==0
wheel,=list((o/'wheel').glob('*.whl'));previous,=list((r/'build/object-planes-r5/wheel-compat').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
with zipfile.ZipFile(wheel) as a,zipfile.ZipFile(previous) as b:
 for name in a.namelist():
  if name.startswith('qupathxx/') and not name.endswith('/'):assert a.read(name)==(site/name).read_bytes();members.append(name)
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert a.read(name)==b.read(name)==(w/'src'/name).read_bytes()
 record,=list(n for n in a.namelist() if n.endswith('.dist-info/RECORD'))
 for name,h,size in csv.reader(io.StringIO(a.read(record).decode())):
  if name==record:assert h==size=='';continue
  data=a.read(name);assert len(data)==int(size) and h=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
assert read('package.json')['final_product_installs']==1 and read('package.json')['stub_generations']==0
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
products=o/'products';products.mkdir(exist_ok=False);retained=[]
for relative in ('Release/qupathxx_project_tests.exe','Release/qupathxx_qupath_project_tests.exe','Release/qupathxx_file_fingerprint_tests.exe','qupathxx_core.dir/Release/project.obj','qupathxx_core.dir/Release/provenance.obj'):
 source=cache/relative;target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
summary=dict(native_source=inputs['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),compiler_inputs=len(inputs['files']),source_fingerprint=fp,changed_compiler_inputs=['cpp/project.cpp'],native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),installed_members=len(members),record_verified=True,baseline_failures=6,staged_passes=sum(map(passed,cohorts['staged'])),staged_declared_skips=sum(c.find('skipped') is not None for c in cohorts['staged']),new_headless_passes=15,native_suites=3,desktop_passes=2,failed_desktop_attempts_preserved=3,native_builds=1,build_seconds=read('build-exit.json')['elapsed_seconds'],product_installations=1,stub_generations=0,unchanged_stubs_and_typing_inputs_reused=True,full_platform_campaign=False,scope='Local Windows retained Z/time/channel compatibility on relink/review publication, current-plane/undo races, atomic refusal, compatible reduction, reader handoff, save/replay and real ImGui actions. GUI retry evidence distinguishes viewport state and copied input-vector semantics; no native correction/rebuild was needed.',products=retained)
(o/'validation.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
