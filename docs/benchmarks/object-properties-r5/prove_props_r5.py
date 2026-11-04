from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as ET,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-properties-r5';cache=r/'build/ci/gridfrontends167-package'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda name:json.loads((o/name).read_bytes())
inputs=read('compiler-inputs-final.json');first=read('compiler-inputs.json');env=q.build_environment()
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fingerprint
assert all(digest(w/p)==h for p,h in inputs['files'].items())
delta=[p for p,h in inputs['files'].items() if first['files'].get(p)!=h];assert delta==['cpp/bindings.cpp']
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
cohorts={n:ET.parse(o/(n+'.xml')).findall('.//testcase') for n in ('before','model','model-corrected','native-first','desktop')}
assert len(cohorts['before'])==2 and all(c.find('failure') is not None for c in cohorts['before'])
failed={(c.get('classname'),c.get('name')) for c in cohorts['model'] if c.find('failure') is not None}
assert len(failed)==2 and all(c.find('error') is None for c in cohorts['model'])
assert {(c.get('classname'),c.get('name')) for c in cohorts['model-corrected']}==failed and all(passed(c) for c in cohorts['model-corrected'])
assert sum(passed(c) for c in cohorts['model'])==724 and sum(c.find('skipped') is not None for c in cohorts['model'])==12
properties=[c for c in cohorts['model'] if c.get('classname')=='tests.test_object_properties']
assert len(properties)==26 and sum(passed(c) for c in properties)==25 and sum(c.find('skipped') is not None for c in properties)==1
assert len(cohorts['native-first'])==5 and all(passed(c) for c in cohorts['native-first'])
assert 'Object properties: immutable history, special numbers, checksummed codec and lossless GeoJSON round trip passed' in ''.join(c.findtext('system-out','') for c in cohorts['native-first'])
assert len(cohorts['desktop'])==2 and all(passed(c) for c in cohorts['desktop'])
assert read('candidate-probe-command.json')['returncode']==1 and 'bad cast' in (o/'candidate-probe.log').read_text(encoding='utf-8')
assert read('build-exit.json')['returncode']==read('build-final-exit.json')['returncode']==0
for n in ('stubs-command.json','typing-valid-command.json','model-corrected-command.json','desktop-command.json'):assert read(n)['returncode']==0
assert read('model-command.json')['returncode']==1
assert read('typing-invalid-command.json')['returncode']==1
assert (o/'typing-invalid.log').read_text(encoding='utf-8').count('error:')==4
retained=read('native-first-products.json');assert retained['source']==first['source'] and retained['unchanged_tested_executables_retained_before_final_relink']
for product in retained['products']:assert digest(o/product['path'])==product['sha256']
wheel,=list((o/'wheel-final').glob('*.whl'));site=Path(q.__file__).parent.parent
assert 'object-properties-r5-env' in str(site)
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
package=read('package.json');assert package['final_product_installs']==1 and not package['native_rebuild_for_stubs']
assert package['final_wheel_sha256']==digest(wheel)
old,=list((o/'wheel-before-stubs').glob('*.whl'))
with zipfile.ZipFile(old) as a,zipfile.ZipFile(wheel) as b:
 name,=list(n for n in a.namelist() if n.endswith('.pyd'));assert a.read(name)==b.read(name)
 assert hashlib.sha256(a.read(name)).hexdigest()==package['native_sha256']
products=o/'products';products.mkdir(exist_ok=False);final=[]
for relative in ('Release/_native.cp312-win_amd64.pyd','qupathxx_core.dir/Release/object_properties.obj','qupathxx_core.dir/Release/provenance.obj'):
 source=cache/relative;target=products/source.name;shutil.copyfile(source,target);final.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
assert digest(Path(_native.__file__))==digest(cache/'Release/_native.cp312-win_amd64.pyd')
summary=dict(native_source=inputs['source'],native_test_source=first['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),source_fingerprint=fingerprint,compiler_inputs=len(inputs['files']),changed_inputs_since_native_tests=delta,installed_members=len(members),native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),record_verified=True,model_passes=726,initial_model_passes=724,corrected_benchmark_passes=2,initial_benchmark_failures_preserved=True,model_declared_skips=sum(c.find('skipped') is not None for c in cohorts['model']),new_property_model_passes=25,new_property_model_gui_skips=1,native_suites=5,native_test_products=retained['products'],desktop_passes=2,typing_intended_errors=4,stub_check=True,native_build_attempts=2,successful_native_builds=2,first_candidate_import_failure=True,invalid_candidate_installed=False,build_elapsed_seconds=[read(n)['elapsed_seconds'] for n in ('build-exit.json','build-final-exit.json')],final_product_installations=1,full_platform_campaign=False,scope='Local Windows supported object metadata/measurements, not all foreign property kinds, locks, planes, biology or release',products=final)
(o/'validation.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in summary.items() if k not in ('products','native_test_products')},indent=2))
