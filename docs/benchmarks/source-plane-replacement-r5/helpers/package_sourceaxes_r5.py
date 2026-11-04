from pathlib import Path
import hashlib,json,subprocess,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/source-plane-replacement-r5';python=r/'build/ci/source-plane-replacement-r5-env/Scripts/python.exe'
assert json.loads((o/'staged-command.json').read_bytes())['returncode']==0
before=json.loads((r/'build/object-planes-r5/compiler-inputs-compat.json').read_bytes());now=json.loads((o/'compiler-inputs.json').read_bytes())
assert now['files'].keys()==before['files'].keys()
assert [p for p,h in now['files'].items() if before['files'][p]!=h]==['cpp/project.cpp']
wheel,=list((o/'wheel').glob('*.whl'));prior,=list((r/'build/object-planes-r5/wheel-compat').glob('*.whl'))
with zipfile.ZipFile(wheel) as a,zipfile.ZipFile(prior) as b:
 for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert a.read(name)==b.read(name)==(w/'src'/name).read_bytes()
assert not python.exists();subprocess.run([str(r/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
(python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(r/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
argv=[str(python),'-I','-m','pip','install','--ignore-installed','--no-index','--no-deps',str(wheel)]
with (o/'install.log').open('x',encoding='utf-8') as log:subprocess.run(argv,cwd=w,stdout=log,stderr=subprocess.STDOUT,check=True)
(o/'package.json').write_text(json.dumps(dict(argv=argv,final_product_installs=1,stub_generations=0,unchanged_stubs_reused=True,wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()),indent=2),encoding='utf-8')
print('Installed once after staged contract checks; reused byte-identical stubs.',flush=True)
