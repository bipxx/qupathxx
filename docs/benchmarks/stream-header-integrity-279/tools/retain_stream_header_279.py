from pathlib import Path
import base64
import csv
import hashlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/ci/scientific-grid-159'
OUT = ROOT / 'build/stream-header-integrity-279'
spec = importlib.util.spec_from_file_location('raw',WORK/'tools/ci/raw_classifier_evidence.py')
raw = importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)
commit = subprocess.check_output(['git','rev-parse','HEAD'],cwd=WORK,text=True).strip()
assert commit == '3bd6591930baa64ae5826d8ba3e2efaee37e8376'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=WORK)
proof = raw.source_contract(WORK,commit)
(OUT/'source-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
manifest = json.loads((OUT/'compiler-inputs-initial.json').read_text())
assert all(hashlib.sha256((WORK/p).read_bytes()).hexdigest()==h for p,h in manifest['files'].items())
products=[]
for original in json.loads((OUT/'prior-products.json').read_text()):
    relative=original['path'];source=ROOT/'build/ci/gridfrontends167-package'/relative
    target=OUT/'final-products'/relative
    assert not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,target)
    assert target.read_bytes()==source.read_bytes()
    products.append(dict(path=relative,bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest(),retained=str(target)))
assert len(products)==69
(OUT/'final-products.json').write_text(json.dumps(products,indent=2)+'\n')
wheel,=list((OUT/'initial-wheel').glob('*.whl'))
with zipfile.ZipFile(wheel) as archive:
    record,=[n for n in archive.namelist() if n.endswith('.dist-info/RECORD')]
    records=list(csv.reader(io.StringIO(archive.read(record).decode())))
    assert len(records)==37
    for name,checksum,size in records:
        data=archive.read(name)
        if checksum:
            assert checksum=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
            assert len(data)==int(size)
(OUT/'wheel-record-proof.json').write_text(json.dumps(dict(entries=len(records),wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()),indent=2)+'\n')
python=ROOT/'build/ci/stream-header-integrity-279-env/Scripts/python.exe'
commands=[(raw.CONTRACT['typing_positive_log'],0,[str(WORK/p) for p in raw.CONTRACT['typing_positive']])]
commands += [(v['log'],1,[str(WORK/'tests/typing'/name)]) for name,v in raw.CONTRACT['typing_negative'].items()]
results=[]
environment=os.environ.copy();environment.pop('MYPYPATH',None)
for name,expected,files in commands:
    args=[str(python),'-I','-X','utf8','-m','mypy','--strict','--no-incremental',*files]
    log=OUT/name;assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=ROOT,env=environment,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(argv=args,returncode=result.returncode))
    assert result.returncode==expected,log
(OUT/'typing-commands.json').write_text(json.dumps(results,indent=2)+'\n')
(OUT/'typing-admission.json').write_text(json.dumps(raw.typing(OUT),indent=2)+'\n')
print('Retained 69 products, 37 wheel records, exact source and 7/5 typing files',flush=True)
