from pathlib import Path
import subprocess,json
base='/mnt/c/Users/HEQ/Projects/qupathxx'
root=Path(__file__).resolve().parent/'scientific-grid-159'
out=root.parents[1]/'grid171-linux-local';out.mkdir(exist_ok=False)
linux_env=base+'/build/ci/grid171-linux-python'
old_python=base+'/build/ci/gridci168-linux-python/bin/python'
linux_out=base+'/build/grid171-linux-local'
linux_source=base+'/build/ci/scientific-grid-159'
wheel=base+'/build/gridci169-hosted/artifacts/Linux-headless-py3.12/wheels/qupathxx-0.1.0-cp312-cp312-linux_x86_64.whl'
files=['tests/'+name+'.py' for name in json.loads((root/'tools/ci/scientific-grid-cases.json').read_text(encoding='utf-8'))]
files+=['tests/test_scientific_grid_evidence.py','tests/test_manylinux_qualification.py','tests/test_manylinux_acceptance.py','tests/test_manylinux_body_acceptance.py','tests/test_campaign_inputs.py']
commands=[('venv',['python3','-m','venv','--without-pip',linux_env]),
 ('install',[old_python,'-m','pip','--python',linux_env,'install','--disable-pip-version-check','--no-input','numpy==2.5.4','scipy==1.16.3','Pillow==13.0.0','pytest==9.1.1',wheel]),
 ('identity',[linux_env+'/bin/python','-I','-c','import qupathxx as q,qupathxx._native as n,hashlib,json;print(json.dumps(dict(native=n.__file__,sha256=hashlib.sha256(open(n.__file__,"rb").read()).hexdigest(),revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256,geos=q.has_geos,interchange=q.has_interchange)))']),
 ('installed',[linux_env+'/bin/python','-I','-m','pytest',*files,'-q','--override-ini','addopts=','--junitxml='+linux_out+'/tests.xml']),
 ('roi-cropped',[linux_env+'/bin/python','-I','tools/compare_qupath_grid_cells.py','--grid-scope','reference-roi','--context','0','--report',linux_out+'/roi-cropped.json'])]
for name,cmd in commands:
 print('Linux',name,flush=True)
 with (out/(name+'.log')).open('w',encoding='utf-8') as log:
  p=subprocess.run(['wsl.exe','-d','Ubuntu','--cd',linux_source,'--',*cmd],stdout=log,stderr=subprocess.STDOUT,timeout=300)
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:
  print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-12000:]);raise SystemExit(p.returncode)
