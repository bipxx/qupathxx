"""Dispatch each complete font-source role once; observe uncertain POSTs without retry."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import subprocess
import time
import urllib.parse

root = Path(__file__).resolve().parents[2]
source = root / 'build/ci/gui-font-qualification-285-audit'
out = root / 'build/gui-font-qualification-285'
out.mkdir(exist_ok=False)
commit = '1f71eb59e13fba272113c1119ee2b608fa26d01a'
ref = 'feature/gui-font-qualification-285'
workflow_id = 392658909
def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
def query(args):
    return json.loads(subprocess.check_output(['gh', *args], encoding='utf-8', timeout=45))
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip() == commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
assert subprocess.check_output(['git','ls-remote','origin','refs/heads/'+ref],cwd=source,text=True).split()[0] == commit
workflow = query(['api',f'repos/bipxx/qupathxx/actions/workflows/{workflow_id}'])
assert workflow['id'] == workflow_id and workflow['state'] == 'active' and workflow['path'] == '.github/workflows/ci.yml'
save(out/'workflow.json',{k:workflow[k] for k in ('id','name','path','state')})
spec = importlib.util.spec_from_file_location('raw',source/'tools/ci/raw_classifier_evidence.py')
raw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(raw)
proof = raw.source_contract(source,commit)
assert len(proof) == 547
save(out/'frozen-source-proof.json',proof)
local = root/'build/gui-font-delivery-283'
native = json.loads((local/'compiler-inputs-compatibility.json').read_text())
native_git = json.loads((local/'source-proof.json').read_text())
assert len(native['files']) == 415
assert all(proof[p]['git_blob_sha256'] == native_git[p]['git_blob_sha256'] for p in native['files'])
differences = [p for p,h in native['files'].items() if proof[p]['checked_out_sha256'] != h]
assert all(hashlib.sha256((source/p).read_bytes().replace(b'\r\n',b'\n')).hexdigest() == native_git[p]['git_blob_sha256'] for p in differences)
save(out/'local-native-equivalence.json',dict(native_source=native['source'],frozen_source=commit,
    canonical_compiler_blobs_equal=415,fresh_checkout_newline_only_differences=differences,
    local_binary_relabelled=False,hosted_roles_must_build_own_source=True))
helper = source/'tools/ci/campaign_inputs.py'
spec = importlib.util.spec_from_file_location('profiles',helper)
profiles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profiles)
inputs = {role:(profiles.full_inputs(role),profiles.gh_inputs(role,profiles.full_inputs(role)))
          for role in ('engine','macos','portable')}
state = dict(schema='qpx.qualification-campaign/1',source=commit,ref=ref,
    local_native_source=native['source'],local_binary_provenance_not_relabelled=True,
    source_paths=547,canonical_compiler_blobs_equal=415,roles={})
save(out/'campaign.json',state)
parameters = urllib.parse.urlencode(dict(branch=ref,event='workflow_dispatch',per_page=30))
def runs():
    observed = query(['api',f'repos/bipxx/qupathxx/actions/workflows/{workflow_id}/runs?{parameters}'])
    result = []
    for run in observed['workflow_runs']:
        assert run['workflow_id']==workflow_id and run['head_sha']==commit and run['head_branch']==ref
        result.append(dict(databaseId=run['id'],headSha=run['head_sha'],status=run['status'],
            conclusion=run['conclusion'] or '',url=run['html_url'],createdAt=run['created_at']))
    return result
before = runs()
assert not before,'Existing runs: identify them; never redispatch'
save(out/'initial-no-runs.json',dict(at_utc=datetime.now(timezone.utc).isoformat(),runs=before))
for role,(semantic,wire) in inputs.items():
    previous = {v['databaseId'] for v in runs()}
    entry = dict(state='dispatch_started',semantic_inputs=semantic,gh_inputs=wire,
        at_utc=datetime.now(timezone.utc).isoformat(),known_runs_before=sorted(previous),
        input_helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest(),api_workflow_id=workflow_id)
    state['roles'][role] = entry
    save(out/'campaign.json',state)
    try:
        result = subprocess.run(['gh','api','--method','POST',f'repos/bipxx/qupathxx/actions/workflows/{workflow_id}/dispatches','--input','-'],
            input=json.dumps(dict(ref=ref,inputs=wire)),encoding='utf-8',capture_output=True,timeout=30)
        entry.update(state='dispatch_acknowledged' if result.returncode==0 else 'dispatch_uncertain',
                     returncode=result.returncode,stdout=result.stdout,stderr=result.stderr)
        save(out/'campaign.json',state)
        assert result.returncode==0,'Uncertain POST: observe without retry'
    except BaseException:
        if entry['state']=='dispatch_started':
            entry['state']='dispatch_uncertain'
            save(out/'campaign.json',state)
        raise
    deadline = time.monotonic()+30
    while time.monotonic()<deadline:
        found = [v for v in runs() if v['databaseId'] not in previous]
        assert len(found)<=1,'Ambiguous runs: observe without another POST'
        if found:
            entry.update(state='run_identified',run=found[0])
            save(out/'campaign.json',state)
            print(role,found[0]['databaseId'],found[0]['url'],flush=True)
            break
        time.sleep(2)
    else:
        raise RuntimeError('Acknowledged run not visible yet; observe without redispatch')
print('All three complete font-source roles dispatched once; newline-only differences:',len(differences),flush=True)
