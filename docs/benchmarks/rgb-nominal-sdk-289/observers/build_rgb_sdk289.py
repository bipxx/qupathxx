from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-sdk-289'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source=='11f70cd1133612fd059fd0c294f6b7eb60103679'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
paths=set()
for directory,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    paths.update(p.relative_to(work).as_posix() for p in (work/directory).rglob('*') if p.is_file() and p.suffix in suffixes)
paths.update(['CMakeLists.txt','pyproject.toml'])
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(paths)}
assert len(files)==418
fingerprint=hashlib.sha256(''.join(f'{p}={h}\n' for p,h in files.items()).encode()).hexdigest()
(out/'compiler-inputs-initial.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n')
old=json.loads((root/'build/gui-font-delivery-283/final-products.json').read_text())
assert len(old)==71
for v in old:
    assert hashlib.sha256(Path(v['retained']).read_bytes()).hexdigest()==v['sha256']
    assert hashlib.sha256((root/'build/ci/gridfrontends167-package'/v['path']).read_bytes()).hexdigest()==v['sha256']
(out/'prior-products.json').write_text(json.dumps(old,indent=2)+'\n')
backend=root/'build/ci/rgb_sdk289_backend.py'
assert not backend.exists()
template=(root/'build/ci/gui_fonts_283_backend_source_override.py').read_text()
assert template.count('gui-font-delivery-283')==1
backend.write_text(template.replace('gui-font-delivery-283','rgb-nominal-sdk-289'),encoding='utf-8')
args=[str(root/'.venv/Scripts/python.exe'),str(backend)]
with (out/'build.log').open('w',encoding='utf-8') as log:
    result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
(out/'build-exit.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
print('SDK289 build exit',result.returncode,flush=True)
assert result.returncode==0
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
print('Native/GUI/Python/TIFF/GEOS package built once;418 source inputs unchanged',flush=True)
