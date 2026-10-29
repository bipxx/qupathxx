from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-305'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
names={'CMakeLists.txt','pyproject.toml'}
for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    names.update(p.relative_to(work).as_posix() for p in (work/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(names)}
assert len(files)==434
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
prior=json.loads((root/'build/postlog-trace-304/final-products.json').read_text(encoding='utf-8'));assert len(prior)==79
for product in prior:
    assert hashlib.sha256(Path(product['retained']).read_bytes()).hexdigest()==product['sha256']
    assert hashlib.sha256((root/'build/ci/gridfrontends167-package'/product['path']).read_bytes()).hexdigest()==product['sha256']
with (out/'prior-products.json').open('x',encoding='utf-8') as stream:json.dump(prior,stream,indent=2);stream.write('\n')
with (out/'compiler-inputs-initial.json').open('x',encoding='utf-8') as stream:json.dump(dict(source=source,fingerprint=fingerprint,files=files),stream,indent=2);stream.write('\n')
backend=root/'build/ci/original305_backend.py';assert not backend.exists()
backend.write_text((root/'build/ci/postlog304_backend.py').read_text(encoding='utf-8').replace('postlog-trace-304','original-source-305'),encoding='utf-8')
args=[str(root/'.venv/Scripts/python.exe'),'-X','utf8',str(backend)]
with (out/'app-build.log').open('x',encoding='utf-8') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
with (out/'app-build-exit.json').open('x',encoding='utf-8') as stream:json.dump(dict(argv=args,returncode=result.returncode),stream,indent=2);stream.write('\n')
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
print('Original-source305 build exit',result.returncode,flush=True)
raise SystemExit(result.returncode)
