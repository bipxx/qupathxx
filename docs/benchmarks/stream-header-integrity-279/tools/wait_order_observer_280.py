from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'build/ci/scientific-grid-159';OUT=ROOT/'build/stream-header-integrity-279'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=WORK,text=True).strip()
assert source=='128b313d49920350c6c341daa17f93930ebe020c'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=WORK)
spec=importlib.util.spec_from_file_location('raw',WORK/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)
proof=raw.source_contract(WORK,source)
assert len(proof)==540
(OUT/'observer-source-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
native=json.loads((OUT/'compiler-inputs-initial.json').read_text())
assert all(hashlib.sha256((WORK/p).read_bytes()).hexdigest()==h for p,h in native['files'].items())
(OUT/'observer-native-binding.json').write_text(json.dumps(dict(observer=source,native_source=native['source'],unchanged_compiler_inputs=len(native['files']),native_rebuild=False),indent=2)+'\n')
prior=json.loads((ROOT/'build/record-cohort-observer-275/preflight-command.json').read_text())
args=[a.replace('snapshot-waits-274-env','stream-header-integrity-279-env').replace('record-cohort-observer-275','stream-header-integrity-279') for a in prior['argv']]
(OUT/'preflight-final-command.json').write_text(json.dumps(dict(argv=args,cwd=str(WORK),observer=source,native_source=native['source']),indent=2)+'\n')
log=OUT/'preflight-final.log';assert not log.exists()
with log.open('w',encoding='utf-8') as stream:
    result=subprocess.run(args,cwd=WORK,stdout=stream,stderr=subprocess.STDOUT)
print('Observer preflight exit',result.returncode,flush=True)
assert result.returncode==0
