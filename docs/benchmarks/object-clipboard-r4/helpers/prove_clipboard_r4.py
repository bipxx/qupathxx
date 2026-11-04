from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as E,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-clipboard-r4';cache=r/'build/ci/gridfrontends167-package'
read=lambda n:json.loads((o/n).read_bytes());digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=read('compiler-inputs.json');env=q.build_environment();fp=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fp
assert all(digest(w/p)==h for p,h in inputs['files'].items())
assert 'object-clipboard-r4-env' in str(Path(_native.__file__))
assert not subprocess.check_output(['git','status','--porcelain'],cwd=w)
cohorts={name:list(E.parse(o/(name+'.xml')).iter('testcase')) for name in ('baseline','staged','staged-retry','native','desktop','desktop-retry','public-stubs')}
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
key=lambda c:(c.get('classname'),c.get('name'))
failed={key(c) for c in cohorts['staged'] if c.find('failure') is not None or c.find('error') is not None}
assert len(cohorts['baseline'])==1 and 'no attribute' in cohorts['baseline'][0].find('failure').get('message','')
assert failed=={key(c) for c in cohorts['staged-retry']} and all(map(passed,cohorts['staged-retry']))
latest={key(c):c for c in cohorts['staged']};latest.update({key(c):c for c in cohorts['staged-retry']})
assert all(c.find('failure') is None and c.find('error') is None for c in latest.values())
assert {c.get('name') for c in cohorts['native']}=={'interchange','document_edits','selection','geometry_edits','project'} and all(map(passed,cohorts['native']))
assert len(cohorts['desktop-retry'])==2 and all(map(passed,cohorts['desktop-retry']))
assert all(map(passed,cohorts['public-stubs']))
assert all(read(name)['returncode']==0 for name in ('build-exit.json','native-command.json','staged-retry-command.json','desktop-retry-command.json'))
assert read('staged-command.json')['returncode']==read('desktop-command.json')['returncode']==1
assert read('typing-retry-commands.json')['typing']['returncode']==0
negative=(o/'typing-negative-retry.log').read_text(encoding='utf-8');assert negative.count(': error:')==4 and all(code in negative for code in ('[call-arg]','[list-item]','[arg-type]'))
wheel,=list((o/'wheel-final').glob('*.whl'));site=Path(q.__file__).parent.parent;members=[]
with zipfile.ZipFile(wheel) as z:
 for name in z.namelist():
  if name.startswith('qupathxx/') and not name.endswith('/'):assert z.read(name)==(site/name).read_bytes();members.append(name)
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert z.read(name)==(w/'src'/name).read_bytes()
 record,=list(n for n in z.namelist() if n.endswith('.dist-info/RECORD'))
 for name,h,size in csv.reader(io.StringIO(z.read(record).decode())):
  if name==record:assert h==size=='';continue
  data=z.read(name);assert len(data)==int(size) and h=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
assert read('package.json')['final_product_installs']==read('package.json')['stub_generations']==1
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
prior=json.loads((r/'build/source-plane-replacement-r5/compiler-inputs.json').read_bytes())['files']
added=sorted(set(inputs['files'])-set(prior));removed=sorted(set(prior)-set(inputs['files']));changed=sorted(p for p in set(prior)&set(inputs['files']) if prior[p]!=inputs['files'][p])
assert added==['cpp/clipboard_bindings.cpp'] and not removed
products=o/'products';products.mkdir();retained=[]
for name in ('interchange','document_edits','selection','geometry_edits','project'):
 source=cache/('Release/qupathxx_'+name+'_tests.exe');target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),sha256=digest(target)))
new=[c for c in latest.values() if c.get('classname')=='tests.test_object_clipboard']
summary=dict(native_source=inputs['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),compiler_inputs=len(inputs['files']),source_fingerprint=fp,added_compiler_inputs=added,changed_compiler_inputs=changed,native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),installed_members=len(members),record_verified=True,baseline_failures=1,staged_passes=sum(map(passed,latest.values())),staged_skips=sum(c.find('skipped') is not None for c in latest.values()),new_headless_passes=sum(map(passed,new)),native_suites=len(cohorts['native']),desktop_passes=len(cohorts['desktop-retry']),packaged_stub_passes=len(cohorts['public-stubs']),typing_negative_diagnostics=4,native_builds=1,build_seconds=read('build-exit.json')['elapsed_seconds'],product_installations=1,stub_generations=1,failed_staged_cases_retained=len(failed),failed_desktop_cases_retained=len(cohorts['desktop']),failed_typing_setup_retained=True,full_platform_campaign=False,scope='Local Windows native/Python/ImGui annotation clipboard: explicit selection/descendants, loss-aware GeoJSON, fresh IDs and hierarchy/state, atomic bounds/class/plane refusal, history, saved reopen and system clipboard/current-plane flows. Existing no-automatic-reselection history policy retained. No new scientific, supported-platform or overall speed claim.',products=retained)
(o/'validation.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
