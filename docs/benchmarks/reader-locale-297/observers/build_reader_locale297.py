from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/reader-locale-297'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
names={'CMakeLists.txt','pyproject.toml'}
for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    names.update(p.relative_to(work).as_posix() for p in (work/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(names)}
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
assert len(files)==425
prior=json.loads((root/'build/nominal-cell-frontends-294/final-products.json').read_text())
assert len(prior)==71
for p in prior:
    assert hashlib.sha256(Path(p['retained']).read_bytes()).hexdigest()==p['sha256']
    assert hashlib.sha256((root/'build/ci/gridfrontends167-package'/p['path']).read_bytes()).hexdigest()==p['sha256']
(out/'prior-products.json').write_text(json.dumps(prior,indent=2)+'\n')
(out/'compiler-inputs-initial.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n')
backend=root/'build/ci/reader_locale297_backend.py'
assert not backend.exists()
backend.write_text((root/'build/ci/rgb_sdk289_backend.py').read_text().replace('rgb-nominal-sdk-289','reader-locale-297'),encoding='utf-8')
args=[str(root/'.venv/Scripts/python.exe'),'-X','utf8',str(backend)]
with (out/'build.log').open('x',encoding='utf-8') as stream:
    result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
(out/'build-exit.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
print('Reader locale297 build exit',result.returncode,flush=True)
assert result.returncode==0
