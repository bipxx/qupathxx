from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,subprocess,time,sys
sys.dont_write_bytecode=True
r=Path(__file__).resolve().parents[2];source=r/'build/ci/spatial-waits-qualification-271-audit';out=r/'build/spatial-waits-271';path=out/'campaign.json';commit='ee19be5743cc131b297bfdbccad393f99b67b7c7';ref='feature/spatial-waits-qualification-271'
def require(value,message):
    if not value:raise ValueError(message)
def query():
    return json.loads(subprocess.check_output(['gh','run','list','--repo','bipxx/qupathxx','--workflow','ci.yml','--branch',ref,'--event','workflow_dispatch','--limit','20','--json','databaseId,headSha,status,conclusion,url,createdAt'],text=True))
require(not path.exists(),'Campaign exists: observe its runs without redispatch')
proof=json.loads((out/'frozen-source-proof.json').read_text());require(proof['candidate_source']==commit and proof['clean'] and proof['all_compiler_git_blobs_identical'] and proof['compiler_input_git_blobs_compared']==410,'frozen source/local admission proof')
require(subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit,'source checkout')
require(not subprocess.check_output(['git','status','--porcelain'],cwd=source),'source dirty')
require(subprocess.check_output(['git','ls-remote','origin','refs/heads/'+ref],cwd=source,text=True).split()[0]==commit,'remote ref')
require(not query(),'Existing dispatches: inspect them without redispatch')
helper=source/'tools/ci/campaign_inputs.py';spec=importlib.util.spec_from_file_location('qpx271_profiles',helper);profiles=importlib.util.module_from_spec(spec);exec(compile(helper.read_bytes(),str(helper),'exec',dont_inherit=True),profiles.__dict__)
state=dict(schema='qpx.qualification-campaign/1',source=commit,ref=ref,local_native_source=proof['local_native_source'],local_observer_source=proof['local_observer_source'],local_compiler_blobs_equivalent=True,local_binary_provenance_not_relabelled=True,roles={},overall_engineering_accepted=False,release_ready=False,scope='Combined spatial workflow and nine Document-taking wait repair. Every role builds its own frozen source; no acceptance transfers from262.')
def save():path.write_text(json.dumps(state,indent=2)+'\n')
save()
for role in ('engine','macos','portable'):
    previous={x['databaseId'] for x in query()};semantic=profiles.full_inputs(role);wire=profiles.gh_inputs(role,semantic)
    record=dict(state='dispatch_started',semantic_inputs=semantic,gh_inputs=wire,at_utc=datetime.now(timezone.utc).isoformat(),known_runs_before=sorted(previous),input_helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest());state['roles'][role]=record;save()
    try:
        result=subprocess.run(['gh','workflow','run','ci.yml','--repo','bipxx/qupathxx','--ref',ref,'--json'],input=json.dumps(wire),capture_output=True,text=True,timeout=30)
        record.update(returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,state='dispatch_acknowledged' if result.returncode==0 else 'dispatch_uncertain');save();require(result.returncode==0,'Uncertain dispatch: observe, never retry blindly')
    except BaseException:
        if record['state']=='dispatch_started':record['state']='dispatch_uncertain';save()
        raise
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        found=[x for x in query() if x['databaseId'] not in previous and x['headSha']==commit];require(len(found)<=1,'Ambiguous new run: observe without redispatch')
        if found:record.update(state='run_identified',run=found[0]);save();print(role,found[0]['databaseId'],found[0]['url'],flush=True);break
        time.sleep(1)
    else:raise RuntimeError('Acknowledged run not yet identified: poll without redispatch')
