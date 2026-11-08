from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/rgb-nominal-sdk-289'
commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
proofs={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proofs[name]=module.source_contract(work,commit)
    (out/(name+'-source-proof.json')).write_text(json.dumps(proofs[name],indent=2)+'\n')
manifest=json.loads((out/'compiler-inputs-initial.json').read_text())
linux=json.loads((out/'linux-compiler-inputs.json').read_text())
assert manifest['source']==linux['source']=='11f70cd1133612fd059fd0c294f6b7eb60103679'
assert len(linux['files'])==len(manifest['files'])==418
assert set(linux['files'])==set(manifest['files'])
merged={}
for proof in proofs.values():
    for p,value in proof.items():
        assert p not in merged or merged[p]==value,p
        merged[p]=value
for p,h in manifest['files'].items():
    assert hashlib.sha256((work/p).read_bytes()).hexdigest()==h
    assert merged[p]['git_blob_sha256']==linux['files'][p]['git_blob_sha256'],p
assert 'examples/rgb_nominal_preview.py' in merged
assert 'tests/test_rgb_nominal_preview.py' in merged
assert 'tests/typing/rgb_nominal_invalid.py' in merged
(out/'source-proof-summary.json').write_text(json.dumps(dict(observer_source=commit,native_source=manifest['source'],windows_fingerprint=manifest['fingerprint'],canonical_compiler_inputs=418,windows_bytes_unchanged=True,linux_canonical_inputs_match=True,source_paths=len(merged),domain_counts={k:len(v) for k,v in proofs.items()},default_sampling_changed=False),indent=2)+'\n')
print('Current observer sources and418 Windows/Linux compiler identities verified:',len(merged),flush=True)
