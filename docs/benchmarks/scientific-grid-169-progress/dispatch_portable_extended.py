from pathlib import Path
import datetime,json,subprocess,time
root=Path(r'C:/Users/HEQ/Projects/qupathxx');path=root/'build/gridci169-campaign.json';state=json.loads(path.read_text(encoding='utf-8'));assert state['source']=='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062'
assert not any(a['role']=='portable_extended' for a in state['attempts']),'Attempt already recorded: poll it, do not redispatch'
def save():path.write_bytes((json.dumps(state,indent=2)+'\n').encode())
def query():return json.loads(subprocess.check_output(['gh','run','list','--repo','bipxx/qupathxx','--branch',state['ref'],'--event','workflow_dispatch','--limit','20','--json','databaseId,headSha,status,conclusion,url'],text=True,encoding='utf-8',timeout=30))
old=next(a for a in state['attempts'] if a['role']=='portable');old['required_scope_accepted']=False;old['scope_failure']='extended_readers omitted; required native NDPI project and builder NDPI cases not exercised'
previous={r['databaseId'] for r in query()};inputs=dict(portable_linux=True,extended_readers=True)
record=dict(role='portable_extended',inputs=inputs,attempted_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),state='dispatch_started',known_runs_before=sorted(previous));state['attempts'].append(record);save()
try:
 p=subprocess.run(['gh','workflow','run','ci.yml','--repo','bipxx/qupathxx','--ref',state['ref'],'--json'],input=json.dumps({k:str(v).lower() for k,v in inputs.items()}),capture_output=True,text=True,encoding='utf-8',timeout=30)
 record.update(returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,state='dispatch_acknowledged' if p.returncode==0 else 'dispatch_uncertain');save();assert p.returncode==0,'Dispatch uncertain; inspect before retry'
except BaseException:
 if record['state']=='dispatch_started':record['state']='dispatch_uncertain';save()
 raise
for _ in range(15):
 runs=[r for r in query() if r['databaseId'] not in previous and r['headSha']==state['source']]
 assert len(runs)<=1,'Ambiguous response; inspect instead of redispatch'
 if runs:
  record.update(state='run_identified',run=runs[0]);state['required_portable_run']=runs[0]['databaseId'];save();print(json.dumps(runs[0]));break
 time.sleep(1)
else:raise RuntimeError('Acknowledged attempt not identified yet: poll, do not redispatch')
