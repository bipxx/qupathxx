from pathlib import Path
import hashlib
import json
import os
import subprocess
import zipfile
root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');work=root/'build/ci/scientific-grid-159';out=root/'build/qupath-input-stages-301/linux';out.mkdir(exist_ok=False)
environment=root/'build/ci/reader-inputs301-linux-env';assert not environment.exists();python=environment/'bin/python';site=environment/'lib/python3.12/site-packages';records=[]
def run(label,args):
    args=list(map(str,args))
    with (out/(label+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=work,stdout=stream,stderr=subprocess.STDOUT)
    records.append(dict(label=label,argv=args,returncode=r.returncode));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print(label,r.returncode,flush=True);assert r.returncode==0
run('venv',['python3','-m','venv','--without-pip',environment])
(site/'shared_test_dependencies.pth').write_text(str(root/'build/ci/grid171-linux-python/lib/python3.12/site-packages')+'\n')
product=root/'build/q298-all/Linux-headless-py3.12';result=json.loads((product/'result.json').read_text());wheel=product/'wheels'/result['wheel']
assert hashlib.sha256(wheel.read_bytes()).hexdigest()==result['wheel_sha256']
pip_site=root/'build/ci/nominal-cell-frontends-294-env/Lib/site-packages';bootstrap='import sys,runpy;sys.path.insert(0,'+repr(str(pip_site))+');runpy.run_module("pip",run_name="__main__")'
run('install',[python,'-I','-c',bootstrap,'install','--no-index','--disable-pip-version-check','--no-deps','--ignore-installed',wheel])
members=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            raw=archive.read(name);assert (site/name).read_bytes()==raw
            members.append(dict(path=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
(out/'installed-proof.json').write_text(json.dumps(dict(wheel_sha256=result['wheel_sha256'],package_members=members,shared_test_dependencies=True,clean_machine=False),indent=2)+'\n')
run('identity',[python,'-I','-c',"import sys,json,numpy,PIL,qupathxx as q;assert q.build_environment().git_revision=='9fbc9315e8c2417c35492cf505a8f676ecdfcf75';print(json.dumps(dict(executable=sys.executable,native_source=q.build_environment().git_revision,fingerprint=q.build_environment().source_tree_sha256,numpy=numpy.__version__,pillow=PIL.__version__)))"])
run('api',[python,'-I','-X','utf8','-m','pytest','tests/test_qupath_cell_inputs.py','tests/test_reader_locale.py','-q','--junitxml='+str(out/'api.xml')])
run('native-comparison',[python,'-I','-X','utf8','tools/qupath_cell_inputs.py','--native','--report',out/'native-comparison.json'])
