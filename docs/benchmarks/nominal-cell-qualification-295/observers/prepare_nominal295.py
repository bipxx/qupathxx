from pathlib import Path
import hashlib
import importlib.util
import json
import os
import subprocess
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];source=root/'build/ci/nominal-cell-qualification-295-audit';out=root/'build/nominal-cell-qualification-295'
commit='a7eb78de75387bce2da7b3d59b7f0a2ab6126cee';ref='feature/nominal-cell-qualification-295'
out.mkdir(exist_ok=False)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
assert subprocess.check_output(['git','ls-remote','origin','refs/heads/'+ref],cwd=source,text=True).split()[0]==commit
def save(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,source/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(source,commit);save(name+'-source-proof.json',proof)
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
assert len(merged)==894
save('frozen-source-proof.json',merged)
local=root/'build/nominal-cell-frontends-294';native=json.loads((local/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
native_git=json.loads((local/'raw_classifier_evidence-source-proof.json').read_text(encoding='utf-8'))
native_git.update(json.loads((local/'scientific_grid_evidence-source-proof.json').read_text(encoding='utf-8')))
assert len(native['files'])==424
for p in native['files']:assert merged[p]['git_blob_sha256']==native_git[p]['git_blob_sha256'],p
differences=[p for p,h in native['files'].items() if merged[p]['checked_out_sha256']!=h]
for p in differences:assert hashlib.sha256((source/p).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==native_git[p]['git_blob_sha256'],p
save('local-native-equivalence.json',dict(native_source=native['source'],frozen_source=commit,source_paths=len(merged),canonical_compiler_blobs_equal=424,
    fresh_checkout_newline_only_differences=differences,local_binary_relabelled=False,hosted_roles_must_build_own_source=True))
prior=json.loads((local/'preflight-command.json').read_text(encoding='utf-8'))
args=[v.replace('nominal-cell-frontends-294','nominal-cell-qualification-295') if i else v for i,v in enumerate(prior['argv'])]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
with (out/'preflight.log').open('w',encoding='utf-8') as log:result=subprocess.run(args,cwd=source,env=env,stdout=log,stderr=subprocess.STDOUT)
save('preflight-command.json',dict(argv=args,cwd=str(source),returncode=result.returncode,compiled_runtime_source=native['source'],clean_machine=False))
assert result.returncode==0
cases=ET.parse(out/'preflight.xml').findall('.//testcase');assert len(cases)==1086 and all(all(c.find(t) is None for t in ('failure','error','skipped')) for c in cases)
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
save('preflight-admission.json',dict(frozen_source=commit,source_paths=len(merged),canonical_compiler_inputs=424,controls_passed=len(cases),shared_test_dependencies=True,compiled_runtime_source=native['source'],source_unchanged=True))
print('Frozen295:894 sources,424 canonical native inputs and1086 fresh-checkout controls pass',flush=True)
