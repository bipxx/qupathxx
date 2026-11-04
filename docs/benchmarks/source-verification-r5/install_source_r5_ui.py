from pathlib import Path
import hashlib,json,subprocess,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/source-verification-r5';python=r/'build/ci/source-verification-r5-ui-env/Scripts/python.exe'
assert json.loads((o/'build-ui-exit.json').read_bytes())['returncode']==0
assert not python.exists()
subprocess.run([str(r/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
(python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(r/'.venv/Lib/site-packages')+'\n')
wheel,=list((o/'wheel-ui').glob('*.whl'))
with zipfile.ZipFile(wheel) as archive:
    for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert archive.read(name)==(w/'src'/name).read_bytes()
argv=[str(python),'-I','-m','pip','install','--ignore-installed','--no-index','--no-deps',str(wheel)]
with (o/'install-ui.log').open('x') as f:subprocess.run(argv,cwd=w,stdout=f,stderr=subprocess.STDOUT,check=True)
(o/'package-ui.json').write_text(json.dumps(dict(final_product_installs=1,wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest(),public_bindings_unchanged=True,stub_repackaging=False,install_argv=argv),indent=2))
print('Installed final verified relink wheel once; unchanged public stubs reused.')
