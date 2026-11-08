from pathlib import Path
import datetime,hashlib,importlib.util,json,subprocess,time
root=Path(__file__).resolve().parents[2];source=root/'build/ci/raw-classifier-235';path=root/'build/rawt5-235/campaign.json'
if path.exists():raise RuntimeError('Dispatch state exists: inspect recorded attempts and poll exact runs; do not dispatch twice')
ref='feature/raw-classifier-qualification-235';commit='176ec42b274bb12429cef603b59ce704e12c6e96'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
remote=subprocess.check_output(['git','ls-remote','origin','refs/heads/'+ref],cwd=source,text=True).split()[0];assert remote==commit
import xml.etree.ElementTree as E
preflight=json.loads((root/'build/rawt5-235/frozen-source-proofs.json').read_text(encoding='utf-8'));assert preflight['source']==commit and preflight['unique_source_files']==718
for name,count in [('admission-final.xml',515),('inherited-remaining.xml',286)]:
 cases=list(E.parse(root/'build/rawt5-235'/name).iter('testcase'));assert len(cases)==count and not any(c.find(k) is not None for c in cases for k in ['failure','error','skipped'])
helper=source/'tools/ci/campaign_inputs.py';spec=importlib.util.spec_from_file_location('profiles',helper);profiles=importlib.util.module_from_spec(spec);exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),profiles.__dict__)
state=dict(schema='qpx.scientific-grid-campaign/2',source=commit,ref=ref,frozen_checkout='build/ci/raw-classifier-235',profiles_sha256=hashlib.sha256(helper.read_bytes()).hexdigest(),preflight='docs/benchmarks/raw-classifier-preflight-local.json',preflight_controls=801,frozen_source_proofs_sha256=hashlib.sha256((root/'build/rawt5-235/frozen-source-proofs.json').read_bytes()).hexdigest(),attempts=[],platform_campaign_accepted=False,release_ready=False,overall_project_complete=False)
def save():path.write_bytes((json.dumps(state,indent=2)+'\n').encode('utf-8'))
def query():
 p=subprocess.run(['gh','run','list','--repo','bipxx/qupathxx','--workflow','ci.yml','--branch',ref,'--event','workflow_dispatch','--limit','20','--json','databaseId,headSha,status,conclusion,url,createdAt'],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
 return json.loads(p.stdout)
if any(run['status']!='completed' for run in query()):raise RuntimeError('Existing live campaign on this ref; inspect without dispatch')
save()
for role in ['engine','macos','portable']:
 semantic=profiles.full_inputs(role);wire=profiles.gh_inputs(role,semantic);previous={r['databaseId'] for r in query()}
 record=dict(role=role,semantic_inputs=semantic,wire_inputs=wire,attempted_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),state='dispatch_started',known_runs_before=sorted(previous));state['attempts'].append(record);save()
 try:
  p=subprocess.run(['gh','workflow','run','ci.yml','--repo','bipxx/qupathxx','--ref',ref,'--json'],input=json.dumps(wire),capture_output=True,text=True,encoding='utf-8',timeout=30)
  record.update(returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,state='dispatch_acknowledged' if p.returncode==0 else 'dispatch_uncertain');save()
  if p.returncode:raise RuntimeError('Dispatch response uncertain: inspect this attempt and remote runs; do not redispatch')
 except BaseException:
  if record['state']=='dispatch_started':record['state']='dispatch_uncertain';save()
  raise
 for _ in range(15):
  found=[r for r in query() if r['databaseId'] not in previous and r['headSha']==commit]
  if len(found)>1:raise RuntimeError('Ambiguous new runs: inspect without redispatch')
  if found:
   record.update(state='run_identified',run=found[0]);save();print(role,found[0]['databaseId'],found[0]['url'],flush=True);break
  time.sleep(1)
 else:raise RuntimeError('Acknowledged attempt not yet identified: poll this attempt without redispatch')
print('All three exact-source dispatches identified',flush=True)
