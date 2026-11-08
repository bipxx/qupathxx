from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cells-291'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source=='37a7df5195af47e0f5c1965e09f8dbec4e469a60' and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
old=json.loads((out/'compiler-inputs-final.json').read_text(encoding='utf-8'))
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in old['files']}
assert [p for p,h in files.items() if h!=old['files'][p]]==['cpp/provenance_codec.cpp','cpp/tests/nominal_cell_tests.cpp']
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
(out/'compiler-inputs-repaired.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n',encoding='utf-8')
backend=root/'build/ci/nominal_cells291_repaired_backend.py';assert not backend.exists()
backend.write_text((root/'build/ci/nominal_cells291_backend.py').read_text(encoding='utf-8').replace('/initial-wheel','/repaired-wheel'),encoding='utf-8')
args=[str(root/'.venv/Scripts/python.exe'),'-X','utf8',str(backend)];assert not (out/'build-repaired.log').exists()
with (out/'build-repaired.log').open('w',encoding='utf-8') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
(out/'build-repaired-exit.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
print('Repaired native291 build exit',result.returncode,flush=True);assert result.returncode==0
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
