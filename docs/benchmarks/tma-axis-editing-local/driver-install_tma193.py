from pathlib import Path
import hashlib,json,shutil,subprocess,os
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/tma193-evidence';envdir=r/'build/ci/tma193-python-env';pkg=envdir/'Lib/site-packages/qupathxx';base=r/'build/ci/raw190-python-env/Lib/site-packages/qupathxx';build=r/'build/ci/gridfrontends167-package/Release';python=envdir/'Scripts/python.exe'
assert not pkg.exists()
# Copy dependency/facade baseline, then replace every newly built executable.
shutil.copytree(base,pkg,ignore=shutil.ignore_patterns('__pycache__'))
for name in ['_native.cp312-win_amd64.pyd','qupathxx_region.exe','qupathxx_batch.exe']:
    shutil.copy2(build/name,pkg/name)
for name in ['__init__.py','__main__.py','py.typed']:
    assert (s/'src/qupathxx'/name).read_bytes()==(pkg/name).read_bytes()
process_env=os.environ.copy();process_env['PYTHONUTF8']='1';process_env['PYTHONIOENCODING']='utf-8'
probe=[str(python),'-I','-c','import qupathxx as q; from qupathxx import _native; import json; print(_native.__file__); b=q.build_environment(); assert b.source_tree_sha256=="12de03f213d3084399c8afa7cdcf1090b73c01226e6a8e12065fc0fc06e4fde4"; print(json.dumps({k:getattr(b,k) for k in ["git_revision","source_tree_sha256","compiler","gui"]},indent=2))']
with (out/'installed-probe.log').open('wb') as log:subprocess.run(probe,env=process_env,stdout=log,stderr=subprocess.STDOUT,check=True)
print((out/'installed-probe.log').read_text(encoding='utf-8'))
# Generation touches declarations only, after both native build handles are terminal.
subprocess.run([str(python),'-I',str(s/'tools/generate_stubs.py')],env=process_env,check=True)
for name in ['_native.pyi','__init__.pyi']:shutil.copy2(s/'src/qupathxx'/name,pkg/name)
subprocess.run([str(python),'-I',str(s/'tools/generate_stubs.py'),'--check'],env=process_env,check=True)
files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.iterdir() if p.is_file()}
(out/'installed-files.json').write_bytes((json.dumps(dict(package=str(pkg),files=files),indent=2)+'\n').encode())
