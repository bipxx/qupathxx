"""Install the authenticated wheel durably for request-only metadata inspection."""
from pathlib import Path
import hashlib
import json
import subprocess
import zipfile

root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');out=root/'build/nominal-cell-comparison-296/linux-recording';out.mkdir(exist_ok=False)
environment=root/'build/ci/nominal-cell-comparison296-linux-env';assert not environment.exists()
python=environment/'bin/python';site=environment/'lib/python3.12/site-packages';records=[]
def run(label,args):
    with (out/(label+'.log')).open('w',encoding='utf-8') as log:p=subprocess.run(list(map(str,args)),stdout=log,stderr=subprocess.STDOUT)
    records.append(dict(label=label,argv=list(map(str,args)),returncode=p.returncode));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print(label,p.returncode,flush=True);assert p.returncode==0
run('venv',['python3','-m','venv','--without-pip',environment])
(site/'shared_test_dependencies.pth').write_text(str(root/'build/ci/grid171-linux-python/lib/python3.12/site-packages')+'\n')
product=root/'build/q295-all/Linux-headless-py3.12';result=json.loads((product/'result.json').read_text());wheel=product/'wheels'/result['wheel']
assert hashlib.sha256(wheel.read_bytes()).hexdigest()==result['wheel_sha256']
pip_site=root/'build/ci/nominal-cell-frontends-294-env/Lib/site-packages'
bootstrap='import sys,runpy;sys.path.insert(0,'+repr(str(pip_site))+');runpy.run_module("pip",run_name="__main__")'
run('install',[python,'-I','-c',bootstrap,'install','--no-index','--disable-pip-version-check','--no-deps','--ignore-installed',wheel])
with zipfile.ZipFile(wheel) as archive:
    members=[n for n in archive.namelist() if n.startswith('qupathxx/') and not n.endswith('/')]
    for name in members:assert (site/name).read_bytes()==archive.read(name)
(out/'installation-proof.json').write_text(json.dumps(dict(wheel_sha256=result['wheel_sha256'],members_verified=len(members),package_root=str(site/'qupathxx'),
    previous_tmp_environment_missing=True,shared_test_dependencies=True,clean_machine=False,native_detection_rerun=False),indent=2)+'\n')
run('record-requests',[python,'-I',root/'build/ci/record_nominal_recipe296.py','linux'])
