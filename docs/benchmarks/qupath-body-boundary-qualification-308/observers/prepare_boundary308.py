from pathlib import Path
import hashlib
import importlib.util
import json
import os
import subprocess
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
source=root/'build/ci/qupath-body-boundary-qualification-308-audit'
out=root/'build/qupath-body-boundary-qualification-308';out.mkdir(exist_ok=False)
commit='ae7a41481a5932123562c8fbc241de48eaf0b3d5'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
def save(name,value):(out/name).write_bytes((json.dumps(value,indent=2)+'\n').encode('utf-8'))
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,source/'tools/ci'/(name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(source,commit);save(name+'-source-proof.json',proof)
    for path,value in proof.items():assert path not in merged or merged[path]==value;merged[path]=value
assert len(merged)==1449;save('frozen-source-proof.json',merged)
local=root/'build/real-ndpi-307';native=json.loads((local/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
native_git=json.loads((local/'raw_classifier_evidence-source-proof.json').read_text(encoding='utf-8'))
native_git.update(json.loads((local/'scientific_grid_evidence-source-proof.json').read_text(encoding='utf-8')))
assert len(native['files'])==434
for path in native['files']:assert merged[path]['git_blob_sha256']==native_git[path]['git_blob_sha256'],path
differences=[path for path,value in native['files'].items() if merged[path]['checked_out_sha256']!=value]
for path in differences:assert hashlib.sha256((source/path).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==native_git[path]['git_blob_sha256'],path
save('local-native-equivalence.json',dict(native_source=native['source'],frozen_source=commit,source_paths=len(merged),canonical_compiler_blobs_equal=434,
    fresh_checkout_newline_only_differences=differences,local_binary_relabelled=False,hosted_roles_must_build_own_source=True))
prior=json.loads((local/'preflight-command.json').read_text(encoding='utf-8'))
tests=[v.replace('\\','/') for v in prior['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')];assert len(tests)==20
args=[str(root/'build/ci/boundary307-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest',*tests,'-q','--junitxml='+str(out/'preflight.xml'),'--basetemp',str(out/'preflight-temp')]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
with (out/'preflight.log').open('x',encoding='utf-8') as stream:result=subprocess.run(args,cwd=source,env=env,stdout=stream,stderr=subprocess.STDOUT)
save('preflight-command.json',dict(argv=args,cwd=str(source),returncode=result.returncode,compiled_runtime_source=native['source'],clean_machine=False))
assert result.returncode==0,out/'preflight.log'
cases=ET.parse(out/'preflight.xml').findall('.//testcase')
assert len(cases)==1113 and all(all(c.find(tag) is None for tag in ('failure','error','skipped')) for c in cases)
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
save('preflight-admission.json',dict(frozen_source=commit,source_paths=len(merged),canonical_compiler_inputs=434,controls_passed=len(cases),
    shared_test_dependencies=True,compiled_runtime_source=native['source'],source_unchanged=True,required_added_native_suites=list(module.ADDED_NATIVE_SUITES)))
print('Frozen308:1449 source paths,434 canonical compiler inputs and1113 fresh-checkout controls pass',flush=True)
