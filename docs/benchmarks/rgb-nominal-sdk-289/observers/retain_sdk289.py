from pathlib import Path
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
from PIL import Image

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-sdk-289'
digest=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((out/'compiler-inputs-initial.json').read_text())
assert len(manifest['files'])==418
assert all(digest(work/p)==h for p,h in manifest['files'].items())
products=[]
for prior in json.loads((out/'prior-products.json').read_text()):
    assert digest(Path(prior['retained']))==prior['sha256']
    relative=prior['path']
    source=root/'build/ci/gridfrontends167-package'/relative
    target=out/'final-products'/relative
    assert not target.exists()
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,target)
    assert digest(target)==digest(source)
    products.append(dict(path=relative,bytes=target.stat().st_size,sha256=digest(target),retained=str(target)))
assert len(products)==71
(out/'final-products.json').write_text(json.dumps(products,indent=2)+'\n')
source,=list((out/'preview-temp').rglob('preview.ppm'))
target=out/'preview.png'
assert not target.exists()
with Image.open(source) as original:
    original.save(target)
    with Image.open(target) as saved:
        assert original.mode==saved.mode and original.size==saved.size and original.tobytes()==saved.tobytes()
(out/'preview-image-proof.json').write_text(json.dumps(dict(source=str(source),source_sha256=digest(source),png_sha256=digest(target),lossless=True),indent=2)+'\n')
spec=importlib.util.spec_from_file_location('raw',work/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)
python=root/'build/ci/rgb-nominal-sdk-289-env/Scripts/python.exe'
commands=[(raw.CONTRACT['typing_positive_log'],0,raw.CONTRACT['typing_positive'])]
commands += [(v['log'],1,['tests/typing/'+name]) for name,v in raw.CONTRACT['typing_negative'].items()]
env=os.environ.copy();env.pop('MYPYPATH',None)
results=[]
for name,expected,files in commands:
    log=out/('raw-'+name);assert not log.exists()
    args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental',*files]
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(argv=args,returncode=result.returncode,log=log.name))
    assert result.returncode==expected,log
# The established auditor takes a directory with its original log names.
audit=out/'raw-typing';audit.mkdir()
for name,_,_ in commands:shutil.copy2(out/('raw-'+name),audit/name)
(out/'raw-typing-admission.json').write_text(json.dumps(raw.typing(audit),indent=2)+'\n')
(out/'raw-typing-commands.json').write_text(json.dumps(results,indent=2)+'\n')
print('Retained71 products; compiler418 unchanged; preview lossless; raw typing admitted',flush=True)
