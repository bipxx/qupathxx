from pathlib import Path
import base64,csv,hashlib,io,json,os,subprocess,time,zipfile,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-clipboard-r4';stage=o/'probe-stage'
initial=list(E.parse(o/'staged.xml').iter('testcase'));retry=list(E.parse(o/'staged-retry.xml').iter('testcase'))
key=lambda c:(c.get('classname'),c.get('name'))
failed={key(c) for c in initial if c.find('failure') is not None or c.find('error') is not None}
assert failed=={key(c) for c in retry} and len(failed)==3
assert all(all(c.find(k) is None for k in ('failure','error','skipped')) for c in retry)
assert json.loads((o/'staged-retry-command.json').read_bytes())['returncode']==0
inputs=json.loads((o/'compiler-inputs.json').read_bytes())
assert all(hashlib.sha256((w/p).read_bytes()).hexdigest()==h for p,h in inputs['files'].items())
source=json.loads((o/'compiler-inputs.json').read_bytes())['source']
python=r/'build/ci/object-clipboard-r4-env/Scripts/python.exe'
assert not python.exists()
subprocess.run([str(r/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
(python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(r/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
env=os.environ.copy();env['PYTHONPATH']=str(stage)
code="import qupathxx as q;assert q.build_environment().git_revision=="+repr(source)+";import runpy;runpy.run_path("+repr(str(w/'tools/generate_stubs.py'))+",run_name='__main__')"
argv=[str(python),'-X','utf8','-c',code]
with (o/'stub-generation.log').open('x',encoding='utf-8') as log:subprocess.run(argv,cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
wheel,=list((o/'wheel').glob('*.whl'));target=o/'wheel-final'/wheel.name;target.parent.mkdir()
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
