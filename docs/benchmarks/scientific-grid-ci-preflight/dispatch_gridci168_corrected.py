from pathlib import Path
import datetime,json,subprocess,time
root=Path(r'C:/Users/HEQ/Projects/qupathxx');path=root/'build/gridci168-campaign.json'
assert path.exists()
ref='feature/scientific-workflow-168';commit='5ebdb961e7c9c2bad8c82367ad090eb23ba4ec57'
state=json.loads(path.read_text(encoding='utf-8'))
assert len(state['attempts'])==1
prior=state['attempts'][0]
assert prior['state']=='dispatch_uncertain' and prior['stderr'].startswith('could not parse provided JSON: json: cannot unmarshal bool into Go value of type string')
prior['state']='rejected_before_request'
prior['reason']='gh local JSON input parsing failed before workflow dispatch; remote branch run list remained empty'

def save():path.write_bytes((json.dumps(state,indent=2)+'\n').encode('utf-8'))
def query():
    p=subprocess.run(['gh','run','list','--repo','bipxx/qupathxx','--branch',ref,'--event','workflow_dispatch','--limit','20','--json','databaseId,headSha,status,conclusion,url,createdAt'],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
    return json.loads(p.stdout)
assert not query(),'Unexpected remote run; inspect instead of retrying'
save()
for role,inputs in [('engine',dict(live_reference=True,extended_readers=True)),('macos',dict(macos_native=True)),('portable',dict(portable_linux=True))]:
    previous={r['databaseId'] for r in query()}
    record=dict(role=role,inputs=inputs,attempted_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),state='dispatch_started',known_runs_before=sorted(previous));state['attempts'].append(record);save()
    try:
        result=subprocess.run(['gh','workflow','run','ci.yml','--repo','bipxx/qupathxx','--ref',ref,'--json'],input=json.dumps({k:str(v).lower() for k,v in inputs.items()}),capture_output=True,text=True,encoding='utf-8',timeout=30)
        record.update(returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,state='dispatch_acknowledged' if result.returncode==0 else 'dispatch_uncertain');save()
        if result.returncode:raise RuntimeError('Dispatch response uncertain; inspect this recorded attempt and remote runs before any further action')
    except BaseException:
        if record['state']=='dispatch_started':record['state']='dispatch_uncertain';save()
        raise
    for _ in range(15):
        runs=[r for r in query() if r['databaseId'] not in previous and r['headSha']==commit]
        if len(runs)>1:raise RuntimeError('Ambiguous new runs; preserve state and inspect without redispatch')
        if runs:
            record.update(state='run_identified',run=runs[0]);save();print(role,runs[0]['databaseId'],runs[0]['url'],flush=True);break
        time.sleep(1)
    else:raise RuntimeError('No run identified yet; poll this exact acknowledged attempt without redispatch')
print('All three exact-source dispatches identified',flush=True)
