from pathlib import Path
import hashlib
import json
import shutil
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'build/gaussian-characterization-299';repeat=out/'repeat';repeat.mkdir(exist_ok=False)
source=[p for p in out.iterdir() if p.name.endswith('-input.f32le') or p.name in ('cases.json','probe.groovy')]
for p in source:shutil.copyfile(p,repeat/p.name)
args=json.loads((out/'command.json').read_text())['argv'];args=[v.replace(str(out),str(repeat)) for v in args]
with (repeat/'driver.log').open('xb') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
(repeat/'command.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n');assert result.returncode==0
files=[]
for path in out.iterdir():
    if path.name.endswith(('-input.f32le','-blur.f32le','-log.f32le','-kernel.json')) or path.name in ('cases.json','probe.groovy'):
        assert path.read_bytes()==(repeat/path.name).read_bytes(),path.name
        files.append(dict(path=path.name,bytes=path.stat().st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
(out/'repeat-proof.json').write_text(json.dumps(dict(cases=37,repeated_payloads=len(files),byte_identical=True,files=files),indent=2)+'\n')
print('Second original-runtime capture matches all',len(files),'payloads',flush=True)
