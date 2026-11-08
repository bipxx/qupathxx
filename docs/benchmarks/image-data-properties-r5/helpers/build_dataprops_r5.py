from pathlib import Path
import hashlib,json,subprocess,time
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/image-data-properties-r5'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=w)
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip()
names={'CMakeLists.txt','pyproject.toml'}
for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    names.update(p.relative_to(w).as_posix() for p in (w/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files={p:hashlib.sha256((w/p).read_bytes()).hexdigest() for p in sorted(names)}
with (o/'compiler-inputs.json').open('x') as f:json.dump(dict(source=source,files=files),f,indent=2)
argv=[str(r/'.venv/Scripts/python.exe'),'-X','utf8',str(r/'build/ci/dataprops_r5_backend.py')]
started=time.perf_counter()
with (o/'build.log').open('x',encoding='utf-8') as f:result=subprocess.run(argv,stdout=f,stderr=subprocess.STDOUT)
(o/'build-exit.json').write_text(json.dumps(dict(argv=argv,returncode=result.returncode,elapsed_seconds=time.perf_counter()-started),indent=2))
print('ImageData properties cached build',source,result.returncode,flush=True)
raise SystemExit(result.returncode)
