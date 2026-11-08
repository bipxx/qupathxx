"""One dispatch per complete role; uncertain POSTs must only be observed."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import subprocess
import time
import urllib.parse

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'build/ci/stream-header-qualification-281-audit'
OUT=ROOT/'build/stream-header-qualification-281';OUT.mkdir(exist_ok=False)
COMMIT='3f15c2be8722ec6559d8657ecd69a839e1f200a5'
REF='feature/stream-header-qualification-281';WORKFLOW=392658909
def save(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def query(arguments):return json.loads(subprocess.check_output(['gh',*arguments],encoding='utf-8',timeout=45))
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=SOURCE,text=True).strip()==COMMIT
assert not subprocess.check_output(['git','status','--porcelain'],cwd=SOURCE)
assert subprocess.check_output(['git','ls-remote','origin','refs/heads/'+REF],cwd=SOURCE,text=True).split()[0]==COMMIT
workflow=query(['api',f'repos/bipxx/qupathxx/actions/workflows/{WORKFLOW}'])
assert workflow['id']==WORKFLOW and workflow['state']=='active' and workflow['path']=='.github/workflows/ci.yml'
save(OUT/'workflow.json',{k:workflow[k] for k in ['id','name','path','state']})
spec=importlib.util.spec_from_file_location('raw',SOURCE/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)
proof=raw.source_contract(SOURCE,COMMIT);assert len(proof)==540
save(OUT/'frozen-source-proof.json',proof)
local=ROOT/'build/stream-header-integrity-279'
native=json.loads((local/'compiler-inputs-initial.json').read_text())
native_git=json.loads((local/'source-proof.json').read_text())
assert all(proof[p]['git_blob_sha256']==native_git[p]['git_blob_sha256'] for p in native['files'])
differences=[p for p,h in native['files'].items() if proof[p]['checked_out_sha256']!=h]
assert all(hashlib.sha256((SOURCE/p).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==native_git[p]['git_blob_sha256'] for p in differences)
save(OUT/'local-native-equivalence.json',dict(native_source=native['source'],frozen_source=COMMIT,canonical_compiler_blobs_equal=410,fresh_checkout_newline_only_differences=differences,local_binary_relabelled=False,hosted_roles_must_build_own_source=True))
helper=SOURCE/'tools/ci/campaign_inputs.py'
spec=importlib.util.spec_from_file_location('profiles',helper);profiles=importlib.util.module_from_spec(spec);spec.loader.exec_module(profiles)
state=dict(schema='qpx.qualification-campaign/1',source=COMMIT,ref=REF,
           local_native_source=native['source'],local_wait_observer='128b313d49920350c6c341daa17f93930ebe020c',
           local_fixture_observer='73b2f8ed6906b9f0610cc295c700dd08437420e7',
           local_compiler_blobs_equivalent=True,local_binary_provenance_not_relabelled=True,roles={})
path=OUT/'campaign.json';save(path,state)
parameters=urllib.parse.urlencode(dict(branch=REF,event='workflow_dispatch',per_page=30))
def runs():
    observed=query(['api',f'repos/bipxx/qupathxx/actions/workflows/{WORKFLOW}/runs?{parameters}'])
    result=[]
    for run in observed['workflow_runs']:
        assert run['workflow_id']==WORKFLOW and run['head_sha']==COMMIT and run['head_branch']==REF
        result.append(dict(databaseId=run['id'],headSha=run['head_sha'],status=run['status'],conclusion=run['conclusion'] or '',url=run['html_url'],createdAt=run['created_at']))
    return result
before=runs();assert not before,'Existing run: identify it; do not redispatch'
save(OUT/'initial-no-runs.json',dict(at_utc=datetime.now(timezone.utc).isoformat(),runs=before))
for role in ['engine','macos','portable']:
    semantic=profiles.full_inputs(role);wire=profiles.gh_inputs(role,semantic)
    previous={run['databaseId'] for run in runs()}
    entry=dict(state='dispatch_started',semantic_inputs=semantic,gh_inputs=wire,
               at_utc=datetime.now(timezone.utc).isoformat(),known_runs_before=sorted(previous),
               input_helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest(),api_workflow_id=WORKFLOW)
    state['roles'][role]=entry;save(path,state)
    try:
        result=subprocess.run(['gh','api','--method','POST',f'repos/bipxx/qupathxx/actions/workflows/{WORKFLOW}/dispatches','--input','-'],input=json.dumps(dict(ref=REF,inputs=wire)),encoding='utf-8',capture_output=True,timeout=30)
        entry.update(state='dispatch_acknowledged' if result.returncode==0 else 'dispatch_uncertain',returncode=result.returncode,stdout=result.stdout,stderr=result.stderr);save(path,state)
        assert result.returncode==0,'Uncertain POST: observe without retry'
    except BaseException:
        if entry['state']=='dispatch_started':entry['state']='dispatch_uncertain';save(path,state)
        raise
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        found=[run for run in runs() if run['databaseId'] not in previous]
        assert len(found)<=1,'Ambiguous run: observe without another POST'
        if found:
            entry.update(state='run_identified',run=found[0]);save(path,state)
            print(role,found[0]['databaseId'],found[0]['url'],flush=True);break
        time.sleep(2)
    else:raise RuntimeError('Acknowledged run not visible yet; observe without redispatch')
print('Dispatched three complete own-source roles once; source paths540, compiler blobs410, newline-only differences',len(differences),flush=True)
