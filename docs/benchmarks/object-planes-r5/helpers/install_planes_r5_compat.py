from pathlib import Path
import hashlib,json,subprocess,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-planes-r5';python=r/'build/ci/object-planes-r5-env/Scripts/python.exe'
assert json.loads((o/'compat-probe-command.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel-compat').glob('*.whl'));previous,=list((o/'wheel-final').glob('*.whl'))
with zipfile.ZipFile(wheel) as a,zipfile.ZipFile(previous) as b:
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):
  assert a.read(name)==b.read(name)==(w/'src'/name).read_bytes()
argv=[str(python),'-I','-m','pip','install','--ignore-installed','--no-index','--no-deps',str(wheel)]
with (o/'install-compat.log').open('x',encoding='utf-8') as log:subprocess.run(argv,cwd=w,stdout=log,stderr=subprocess.STDOUT,check=True)
(o/'package-compat.json').write_text(json.dumps(dict(install_argv=argv,final_product_installs=2,total_stub_generations=1,unchanged_stub_bytes_reused=True,wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()),indent=2),encoding='utf-8')
print('Installed compatibility correction; reused unchanged generated stubs.',flush=True)
