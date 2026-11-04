from pathlib import Path
import base64,csv,hashlib,io,json,os,subprocess,time,zipfile,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/root-facts-r5';stage=o/'probe-stage'
cohorts={name:list(E.parse(o/(name+'.xml')).iter('testcase')) for name in ('staged-collect-fixed','staged-store','staged-report-fixed')}
key=lambda c:(c.get('classname'),c.get('name'))
latest={key(c):c for c in cohorts['staged-collect-fixed']}
store_keys={key(c) for c in cohorts['staged-store']}
reused=set(latest)-store_keys
assert all(cls=='tests.test_document_edits' or (cls=='tests.test_classification_edits' and name=='test_classification_batch_is_atomic_and_preserves_redo') for cls,name in reused)
for name in ('staged-store','staged-report-fixed'):latest.update({key(c):c for c in cohorts[name]})
assert all(c.find('failure') is None and c.find('error') is None for c in latest.values())
assert json.loads((o/'staged-report-fixed-command.json').read_bytes())['returncode']==0
owned=json.loads((o/'compiler-inputs-owned.json').read_bytes())['files'];current=json.loads((o/'compiler-inputs-store.json').read_bytes())['files']
assert owned.keys()==current.keys() and [p for p,h in current.items() if owned[p]!=h]==['cpp/project.cpp']
inputs=json.loads((o/'compiler-inputs-store.json').read_bytes())
assert all(hashlib.sha256((w/p).read_bytes()).hexdigest()==h for p,h in inputs['files'].items())
source=json.loads((o/'compiler-inputs-store.json').read_bytes())['source']
python=r/'build/ci/root-facts-r5-env/Scripts/python.exe'
assert not python.exists()
subprocess.run([str(r/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
(python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(r/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
env=os.environ.copy();env['PYTHONPATH']=str(stage)
code="import qupathxx as q;assert q.build_environment().git_revision=="+repr(source)+";import runpy;runpy.run_path("+repr(str(w/'tools/generate_stubs.py'))+",run_name='__main__')"
argv=[str(python),'-X','utf8','-c',code]
with (o/'stub-generation.log').open('x',encoding='utf-8') as log:subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
wheel,=list((o/'wheel-store').glob('*.whl'));target=o/'wheel-final'/wheel.name;target.parent.mkdir()
with zipfile.ZipFile(wheel) as z:contents={name:z.read(name) for name in z.namelist()}
original=contents.copy()
for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):contents[name]=(w/'src'/name).read_bytes()
record,=list(n for n in contents if n.endswith('.dist-info/RECORD'))
rows=[]
for name,data in contents.items():
 if name!=record:rows.append((name,'sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('='),str(len(data))))
rows.append((record,'',''));out=io.StringIO(newline='');csv.writer(out,lineterminator='\n').writerows(rows);contents[record]=out.getvalue().encode()
with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED) as z:
 for name,data in contents.items():z.writestr(name,data)
assert all(contents[n]==data for n,data in original.items() if n not in ('qupathxx/_native.pyi','qupathxx/__init__.pyi',record))
argv=[str(python),'-I','-m','pip','install','--ignore-installed','--no-index','--no-deps',str(target)]
with (o/'install.log').open('x',encoding='utf-8') as log:subprocess.run(argv,cwd=w,stdout=log,stderr=subprocess.STDOUT,check=True)
(o/'package.json').write_text(json.dumps(dict(argv=argv,final_product_installs=1,stub_generations=1,native_bytes_reused=True,wheel_sha256=hashlib.sha256(target.read_bytes()).hexdigest()),indent=2),encoding='utf-8')
print('Generated final stubs once; repackaged unchanged native bytes; installed once.',flush=True)
