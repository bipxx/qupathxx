from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import xml.etree.ElementTree as ET
import zipfile

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/font-archive-288'
observation=root/'build/q285-all/20261028T190527Z'
campaign=root/'build/gui-font-qualification-285'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source=='01c3b9aadd36ee4e06ba759929c19a9e2073507c'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
verified=json.loads((observation/'verified.json').read_text())
assert verified['source']=='1f71eb59e13fba272113c1119ee2b608fa26d01a' and len(verified['files'])==9
for entry in verified['files']:
    path=root/'build/q285-all'/(entry['name']+'.zip')
    with path.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').hexdigest()
    assert path.stat().st_size==entry['bytes'] and 'sha256:'+digest==entry['digest']
assert json.loads((campaign/'engine-audit-exit.json').read_text())['returncode']==1
assert json.loads((campaign/'portable-audit-exit.json').read_text())['returncode']==0
artifact=root/'build/q285-all/Windows-gui-py3.12'
counts={}
for name in ('native-tests','python-tests'):
    cases=ET.parse(artifact/(name+'.xml')).findall('.//testcase')
    assert not any(c.find('failure') is not None or c.find('error') is not None for c in cases)
    counts[name]=dict(passed=sum(c.find('skipped') is None for c in cases),
        skipped=sum(c.find('skipped') is not None for c in cases),
        expected_failures=sum(c.find('skipped') is not None and c.find('skipped').get('type')=='pytest.xfail' for c in cases))
assert counts['native-tests']['passed']==55
assert counts['python-tests']==dict(passed=4866,skipped=14,expected_failures=3)
wheel,=list((artifact/'wheels').glob('*.whl'))
package=out/'ci-wheel-package'
package.mkdir(exist_ok=False)
with zipfile.ZipFile(wheel) as z:
    for entry in z.infolist():assert (package/entry.filename).resolve().is_relative_to(package.resolve())
    z.extractall(package)
code='''import json,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import qupathxx as q
assert Path(q.__file__).resolve().parent==Path(sys.argv[1],'qupathxx').resolve()
assert q.build_environment().git_revision=='1f71eb59e13fba272113c1119ee2b608fa26d01a'
assert q.has_gui and q.has_openslide
print(json.dumps(dict(source=q.build_environment().git_revision,fingerprint=q.build_environment().source_tree_sha256,package=q.__file__,gui=q.has_gui,openslide=q.has_openslide)))
'''
args=[str(root/'build/ci/gui-font-delivery-283-env/Scripts/python.exe'),'-I','-X','utf8','-c',code,str(package)]
result=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',timeout=30)
(out/'ci-wheel-runtime.log').write_text(result.stdout+result.stderr,encoding='utf-8')
assert result.returncode==0
runtime=json.loads(result.stdout)
spec=importlib.util.spec_from_file_location('raw',work/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec)
spec.loader.exec_module(raw)
proof=raw.source_contract(work,source)
(out/'observer-source-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
summary=dict(observer_source=source,product_source=verified['source'],
    authenticated_archives=verified['files'],windows_gui_counts=counts,ci_wheel_runtime=runtime,
    engine_terminal='failure',original_engine_auditor_returncode=1,original_portable_auditor_returncode=0,
    macos_last_observed='queued',combined_candidate_accepted=False,source_paths=len(proof),
    local_repack_only=True,hosted_jobs_repeated=False,binary_relabelled=False,clean_machine=False)
(out/'source-runtime-proof.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Font288 observer source proof',len(proof),'paths; original CI wheel self-identifies1f71eb5;55 native/4866 Python passes before standalone resource failure',flush=True)
