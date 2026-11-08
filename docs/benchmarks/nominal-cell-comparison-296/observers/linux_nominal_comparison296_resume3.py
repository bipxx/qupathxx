from pathlib import Path
import hashlib
import json
import os
import subprocess
import zipfile

root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-comparison-296/linux';assert out.is_dir()
product=root/'build/q295-all/Linux-headless-py3.12';result=json.loads((product/'result.json').read_text());wheel=product/'wheels'/result['wheel']
assert hashlib.sha256(wheel.read_bytes()).hexdigest()==result['wheel_sha256']
environment=Path('/tmp/qupathxx-nominal-comparison296-env');assert environment.is_dir()
python=environment/'bin/python';records=json.loads((out/'commands.json').read_text());assert records[-1]['label']=='install' and records[-1]['returncode']==1
def run(label,args,cwd=work):
    with (out/(label+'.log')).open('w',encoding='utf-8') as log:p=subprocess.run(list(map(str,args)),cwd=cwd,stdout=log,stderr=subprocess.STDOUT)
    records.append(dict(label=label,argv=list(map(str,args)),cwd=str(cwd),returncode=p.returncode));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print(label,p.returncode,flush=True);assert p.returncode==0,out/(label+'.log')
assert python.is_file()
site=environment/'lib/python3.12/site-packages';deps=root/'build/ci/grid171-linux-python/lib/python3.12/site-packages';assert deps.is_dir()
(site/'shared_test_dependencies.pth').write_text(str(deps)+'\n')
pip_main=root/'build/ci/nominal-cell-frontends-294-env/Lib/site-packages/pip/__main__.py';assert pip_main.is_file()
bootstrap='import sys,runpy;sys.path.insert(0,'+repr(str(pip_main.parent.parent))+');runpy.run_module("pip",run_name="__main__")'
run('install-shared-pip',[python,'-I','-c',bootstrap,'install','--no-index','--disable-pip-version-check','--no-deps','--ignore-installed',wheel])
script="""import json,platform,sys,numpy,scipy,PIL,pytest,qupathxx as q
assert q.build_environment().git_revision=='a7eb78de75387bce2da7b3d59b7f0a2ab6126cee'
assert q.__file__.startswith('/tmp/qupathxx-nominal-comparison296-env/')
print(json.dumps(dict(native_source=q.build_environment().git_revision,fingerprint=q.build_environment().source_tree_sha256,package=q.__file__,platform=platform.platform(),python=sys.version,numpy=numpy.__version__,scipy=scipy.__version__,pillow=PIL.__version__,pytest=pytest.__version__,geos=q.has_geos,gui=q.has_gui,openslide=q.has_openslide,shared_test_dependencies=True,clean_machine=False)))
"""
run('identity',[python,'-I','-c',script])
identity=json.loads((out/'identity.log').read_text());installed=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if not name.startswith('qupathxx/') or name.endswith('/'):continue
        data=archive.read(name);assert (site/name).read_bytes()==data
        installed.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
(out/'installed-proof.json').write_text(json.dumps(dict(wheel_sha256=result['wheel_sha256'],native_source=identity['native_source'],installed=installed,shared_test_dependencies=True,clean_machine=False),indent=2)+'\n')
run('api',[python,'-I','-m','pytest','tests/test_qupath_nominal_cells_reference.py','-q','--junitxml='+str(out/'api.xml'),'--basetemp',out/'api-temp'])
for label,flags in [('roi-context0',['--context','0']),('roi-context32',['--context','32']),('full-context0',['--context','0','--grid-scope','full-image']),('physical-context32',['--context','32','--units','micrometres'])]:
    run(label,[python,'-I','tools/compare_qupath_nominal_cells.py',*flags,'--report',out/(label+'.json')])
print('Authenticated CI295 Linux wheel passes new nominal comparison; no native rebuild or qualification transfer',flush=True)
