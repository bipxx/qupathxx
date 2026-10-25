from pathlib import Path

import hashlib,importlib.util,json,os,re,shutil,subprocess,time,xml.etree.ElementTree as ET

r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/tma198-evidence';python=r/'build/ci/tma198-python-env/Scripts/python.exe';pkg=python.parent.parent/'Lib/site-packages/qupathxx'

for host,count in [('windows',7),('linux',3)]:

 records=json.loads((r/f'build/tma198-{host}/commands.json').read_text(encoding='utf-8'));assert len(records)==count and all(v['exit']==0 for v in records)

paths=sorted([p for folder,extensions in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})] for p in (s/folder).rglob('*') if p.is_file() and p.suffix in extensions]+[s/'CMakeLists.txt',s/'pyproject.toml'])

files=dict(sorted((p.relative_to(s).as_posix(),hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths));manifest=''.join(name+'='+digest+'\n' for name,digest in files.items());fingerprint=hashlib.sha256(manifest.encode()).hexdigest()

assert fingerprint=='2e217fbd3e36b7cfe58a99adff1615cd22d6e0141a256a3a89bf680f282e20ff'

for build in ['gridfrontends167-package','raw176-tiff-linux-native']:

 assert 'source_sha256[]="'+fingerprint+'"' in (r/'build/ci'/build/'generated/build_manifest.hpp').read_text(encoding='utf-8')

source='a492ea5a6f7cea2f069fcc5899b6f4a20cd9554e';(out/'compiler-inputs.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n',encoding='utf-8')

spec=importlib.util.spec_from_file_location('qptma_evidence',s/'tools/ci/qptma_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

proof=m.source_contract(s,source,list(files),label='TMA dearray compiler inputs');(out/'compiler-git-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')

env=os.environ.copy();env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8';env.pop('QUPATHXX_TEST_GUI',None)

commands=[('generate-stubs',[str(python),'-I',str(s/'tools/generate_stubs.py')],0,env)]

results=[]

def run(name,cmd,expected,e):

 start=time.perf_counter()

 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,cwd=s,env=e,stdout=log,stderr=subprocess.STDOUT,timeout=600)

 results.append(dict(name=name,command=cmd,exit=p.returncode,expected_exit=expected,seconds=time.perf_counter()-start));(out/'commands.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8');print(name,p.returncode,flush=True)

 if p.returncode!=expected:print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace'));raise SystemExit(1)

for args in commands:run(*args)

for name in ['_native.pyi','__init__.pyi']:shutil.copy2(s/'src/qupathxx'/name,pkg/name)

run('check-stubs',[str(python),'-I',str(s/'tools/generate_stubs.py'),'--check'],0,env)

files=['test_tma_dearray_cli.py','test_gui_tma_dearray.py','test_tma_dearray.py','test_tma_dearray_reference.py','test_tma_axes.py','test_tma.py','test_tma_case_summary.py','test_tma_case_export.py','test_tma_cohort.py','test_qptma.py','test_qptma_project.py','test_viewer_session.py','test_public_stubs.py','test_stub_signature_guard.py']

run('runtime',[str(python),'-I','-m','pytest',*[str(s/'tests'/n) for n in files],'-q','--tb=short','--junitxml='+str(out/'runtime.xml'),'--basetemp='+str(out/'runtime-tmp')],0,env)

gui=env.copy();gui['QUPATHXX_TEST_GUI']='1'

run('dearray-desktop',[str(python),'-I','-m','pytest',str(s/'tests/test_gui_tma_dearray.py'),'-q','--tb=short','--junitxml='+str(out/'dearray-desktop.xml'),'--basetemp='+str(out/'dearray-desktop-tmp')],0,gui)
run('desktop',[str(python),'-I','-m','pytest',*[str(s/'tests'/n) for n in ['test_gui_tma.py','test_gui_qptma.py','test_viewer_session.py']],'-q','--tb=short','--junitxml='+str(out/'desktop.xml'),'--basetemp='+str(out/'desktop-tmp')],0,gui)

positive=['tests/typing/tma_dearray_valid.py','tests/typing/raw_grid_valid.py','tests/typing/valid_api.py','tests/typing/hierarchy_valid.py','tests/typing/grid_cells_valid.py','examples/live_workflow.py','examples/region_workflow.py']

run('typing-positive',[str(python),'-I','-m','mypy','--strict','--no-incremental',*positive],0,env)

run('typing-dearray-negative',[str(python),'-I','-m','mypy','--strict','--no-incremental','tests/typing/tma_dearray_invalid.py'],1,env)

text=(out/'typing-dearray-negative.log').read_text(encoding='utf-8');assert sorted(map(int,re.findall(r'tma_dearray_invalid.py:(\d+): error:',text)))==[4,5,6] and text.count('error:')==3

for suite in ['runtime','desktop','dearray-desktop']:

 cases=list(ET.parse(out/(suite+'.xml')).getroot().iter('testcase'));assert not any(c.find(t) is not None for c in cases for t in ['failure','error']);print(suite,'cases',len(cases),'skips',sum(c.find('skipped') is not None for c in cases))

files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.iterdir() if p.is_file()};(out/'installed-files.json').write_text(json.dumps(dict(package=str(pkg),files=files),indent=2)+'\n',encoding='utf-8')

