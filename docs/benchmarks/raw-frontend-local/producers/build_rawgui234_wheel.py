from pathlib import Path
import hashlib,json,os,subprocess,time
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawgui234-final';out.mkdir(exist_ok=False);(out/'wheel').mkdir()
source=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip();assert not subprocess.check_output(['git','-C',str(s),'status','--porcelain']).strip()
paths=sorted([p for folder,exts in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})] for p in (s/folder).rglob('*') if p.is_file() and p.suffix in exts]+[s/'CMakeLists.txt',s/'pyproject.toml'],key=lambda p:p.relative_to(s).as_posix())
files={p.relative_to(s).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths};fingerprint=hashlib.sha256(''.join(n+'='+d+'\n' for n,d in files.items()).encode()).hexdigest()
(out/'compiler-inputs.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n',encoding='utf-8')
sdk=r/'build/deps/opencv-sdk';sdk_manifest=json.loads((sdk/'qupathxx-opencv-sdk.json').read_text(encoding='utf-8'));sdk_files={sdk/n:d for n,d in sdk_manifest['files'].items()};assert len(sdk_files)==195
geos=r/'build/deps/geos-sdk';geos_files={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in geos.rglob('*') if p.is_file()};assert len(geos_files)==559
(out/'geos-sdk-files.json').write_text(json.dumps({p.relative_to(geos).as_posix():d for p,d in geos_files.items()},indent=2)+'\n',encoding='utf-8')
env=os.environ.copy();env['PATH']=str(r/'.venv/Scripts')+os.pathsep+env['PATH'];env['CMAKE_GENERATOR']='Visual Studio 17 2022';env['CMAKE_GENERATOR_PLATFORM']='x64';env['CMAKE_BUILD_PARALLEL_LEVEL']='2'
cmd=[str(r/'.venv/Scripts/python.exe'),'-I',str(r/'build/ci/rawgui234_backend.py')]
assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in files.items()) and all(hashlib.sha256(p.read_bytes()).hexdigest()==d for p,d in sdk_files.items()) and all(hashlib.sha256(p.read_bytes()).hexdigest()==d for p,d in geos_files.items())
print('Building pinned Windows training-enabled wheel',source,flush=True);start=time.perf_counter()
with (out/'wheel-build.log').open('wb') as log:done=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=2400)
(out/'wheel-command.json').write_text(json.dumps(dict(command=cmd,exit=done.returncode,seconds=time.perf_counter()-start),indent=2)+'\n');print('Wheel build exit',done.returncode,flush=True)
assert all(hashlib.sha256((s/n).read_bytes()).hexdigest()==d for n,d in files.items()) and all(hashlib.sha256(p.read_bytes()).hexdigest()==d for p,d in sdk_files.items()) and all(hashlib.sha256(p.read_bytes()).hexdigest()==d for p,d in geos_files.items())
raise SystemExit(done.returncode)
