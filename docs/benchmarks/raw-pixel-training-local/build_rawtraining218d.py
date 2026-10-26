from pathlib import Path
import hashlib,json,os,subprocess,sys,time
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159'
mode=sys.argv[1];assert mode in ('on','off')
out=r/f'build/rawtraining218d-{mode}';out.mkdir(exist_ok=False)
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()
assert source.startswith('e7e4b66') and not subprocess.check_output(['git','-C',str(s),'status','--porcelain']).strip()
paths=sorted([p for folder,exts in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})] for p in (s/folder).rglob('*') if p.is_file() and p.suffix in exts]+[s/'CMakeLists.txt',s/'pyproject.toml'],key=lambda p:p.relative_to(s).as_posix())
files={p.relative_to(s).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths};fingerprint=hashlib.sha256(''.join(n+'='+d+'\n' for n,d in files.items()).encode()).hexdigest()
(out/'compiler-inputs.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n',encoding='utf-8')
cmake=r/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';env=os.environ.copy();env['PATH']=str(r/'.venv/Scripts')+os.pathsep+env['PATH'];commands=[]
if mode=='on':
    sdk=r/'build/deps/opencv-sdk';sdk_manifest=json.loads((sdk/'qupathxx-opencv-sdk.json').read_text())
    assert sdk_manifest['version']=='4.11.0'
    hashes=sdk_manifest['files']
    for n,d in hashes.items():assert hashlib.sha256((sdk/n).read_bytes()).hexdigest()==d,n
    (out/'sdk-verification.json').write_text(json.dumps(dict(manifest_sha256=hashlib.sha256((sdk/'qupathxx-opencv-sdk.json').read_bytes()).hexdigest(),verified_files=len(hashes),version=sdk_manifest['version'],commit=sdk_manifest['commit']),indent=2)+'\n',encoding='utf-8')
    build=r/'build/ci/rawtraining218-windows'
    arguments=['-G','Visual Studio 17 2022','-A','x64','-DQUPATHXX_BUILD_PYTHON=OFF','-DQUPATHXX_BUILD_GUI=OFF','-DQUPATHXX_BUILD_TESTS=ON','-DQUPATHXX_BUILD_TIFF=ON','-DQUPATHXX_BUILD_PROJECT=ON','-DQUPATHXX_BUILD_CLASSIFIER_TRAINING=ON','-DOpenCV_DIR='+str(sdk),'-DQUPATHXX_OPENCV_NOTICES='+str(sdk/'share/licenses/opencv'),'-DFETCHCONTENT_SOURCE_DIR_SQLITE='+str(r/'build/deps/sqlite-amalgamation-3540100')]
    for name in ('qpx_zlib','qpx_tiff','qpx_xml'):
        arguments.append('-DFETCHCONTENT_SOURCE_DIR_'+name.upper()+'='+str(r/'build/ci/gridfrontends167-package/_deps'/f'{name}-src'))
    candidates=[r/'build/cp312-cp312-win_amd64/_deps/json-src',r/'build/ci/raw176-tiff-linux-native/_deps/json-src']
    json_source=next(p for p in candidates if (p/'CMakeLists.txt').is_file())
    arguments.append('-DFETCHCONTENT_SOURCE_DIR_JSON='+str(json_source))
    targets=['qupathxx_pixel_workflow_tests','qupathxx_classifier_tests','qupathxx_classifier_io_tests'];pattern='^(pixel_workflows|classifier|classifier_io)$'
else:
    build=r/'build/ci/gridfrontends167-package';arguments=['-DQUPATHXX_BUILD_CLASSIFIER_TRAINING=OFF'];targets=['qupathxx_pixel_workflow_tests'];pattern='^pixel_workflows$'
commands=[('configure',[str(cmake),'-S',str(s),'-B',str(build),*arguments]),('build',[str(cmake),'--build',str(build),'--config','Release','--target',*targets,'--parallel','2']),('tests',[str(cmake.with_name('ctest.exe')),'--test-dir',str(build),'-C','Release','-R',pattern,'--output-on-failure','--test-output-size-passed','1048576','--no-tests=error','--output-junit',str(out/'tests.xml')])]
results=[]
for name,cmd in commands:
    assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in files.items())
    print(mode,name,flush=True);start=time.perf_counter()
    with (out/(name+'.log')).open('wb') as log:done=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=2400)
    results.append(dict(name=name,command=cmd,exit=done.returncode,seconds=time.perf_counter()-start));(out/'commands.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8');print(mode,name,'exit',done.returncode,flush=True)
    if done.returncode:raise SystemExit(done.returncode)
assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in files.items())
