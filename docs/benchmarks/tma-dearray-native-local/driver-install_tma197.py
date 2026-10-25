from pathlib import Path
import os,subprocess,shutil,hashlib,json,re
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/tma197-evidence';out.mkdir(exist_ok=False)
envdir=r/'build/ci/tma197-python-env';python=envdir/'Scripts/python.exe';pkg=envdir/'Lib/site-packages/qupathxx'
uv=r.parent.parent/'.local/bin/uv.exe'
uv=Path(r'C:/Users/HEQ/.local/bin/uv.exe')
commands=[[str(uv),'venv',str(envdir),'--python',str(r/'build/ci/tma193-python-env/Scripts/python.exe')],[str(uv),'pip','install','--offline','--python',str(python),'numpy==2.5.4','pytest==9.1.1','mypy==2.4.0','nanobind==3.1.0']]
for i,cmd in enumerate(commands):
 with (out/('environment-'+str(i)+'.log')).open('wb') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
shutil.copytree(r/'build/ci/tma193-python-env/Lib/site-packages/qupathxx',pkg,ignore=shutil.ignore_patterns('__pycache__'))
for name in ['_native.cp312-win_amd64.pyd','qupathxx_region.exe','qupathxx_batch.exe']:shutil.copy2(r/'build/ci/gridfrontends167-package/Release'/name,pkg/name)
for name in ['__init__.py','__main__.py','py.typed']:shutil.copy2(s/'src/qupathxx'/name,pkg/name)
manifest=(r/'build/ci/gridfrontends167-package/generated/build_manifest.hpp').read_text(encoding='utf-8');fingerprint=re.search(r'source_sha256\[\]="([a-f0-9]+)"',manifest).group(1)
cmd=[str(python),'-I','-c','import qupathxx as q; from qupathxx import _native; import json; b=q.build_environment(); assert b.git_revision=="ecbf83172ac3b44553d530e140e7109434426ee5"; assert b.source_tree_sha256=="'+fingerprint+'"; print(_native.__file__); print(json.dumps({k:getattr(b,k) for k in ["git_revision","source_tree_sha256","compiler","gui"]},indent=2)); print(q.TmaDearrayOptions().thumbnail.max_width)']
with (out/'installed-probe.log').open('wb') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
print((out/'installed-probe.log').read_text(encoding='utf-8'))
files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.iterdir() if p.is_file()};(out/'installed-files-before-stubs.json').write_text(json.dumps(dict(package=str(pkg),files=files),indent=2)+'\n',encoding='utf-8')
