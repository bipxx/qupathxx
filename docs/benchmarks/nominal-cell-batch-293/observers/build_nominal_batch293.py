from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-batch-293'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source.startswith('a7c8e4f4') and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
assert out.exists() and not (out/'compiler-inputs-initial.json').exists()
names={'CMakeLists.txt','pyproject.toml'}
for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    names.update(p.relative_to(work).as_posix() for p in (work/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(names)};assert len(files)==424
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
(out/'compiler-inputs-initial.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n',encoding='utf-8')
old=json.loads((root/'build/nominal-cell-python-292/final-products.json').read_text(encoding='utf-8'));assert len(old)==71
for p in old:
    assert hashlib.sha256(Path(p['retained']).read_bytes()).hexdigest()==p['sha256']
    assert hashlib.sha256((root/'build/ci/gridfrontends167-package'/p['path']).read_bytes()).hexdigest()==p['sha256']
(out/'prior-products.json').write_text(json.dumps(old,indent=2)+'\n',encoding='utf-8')
backend=root/'build/ci/nominal_batch293_backend.py';assert not backend.exists()
backend.write_text((root/'build/ci/rgb_sdk289_backend.py').read_text(encoding='utf-8').replace('rgb-nominal-sdk-289','nominal-cell-batch-293'),encoding='utf-8')
args=[str(root/'.venv/Scripts/python.exe'),'-X','utf8',str(backend)]
with (out/'build.log').open('w',encoding='utf-8') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
(out/'build-exit.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
print('Nominal Python292 build exit',result.returncode,flush=True);assert result.returncode==0
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
