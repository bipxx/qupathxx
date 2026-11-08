from pathlib import Path
import datetime,json,subprocess
root=Path(__file__).resolve().parents[2];state_path=root/'build/tmaci202-campaign.json';state=json.loads(state_path.read_text(encoding='utf-8'));out=root/'build/tmaci202-hosted';out.mkdir(exist_ok=True);summary=[]
for attempt in state['attempts']:
 assert attempt['state']=='run_identified';role=attempt['role'];run_id=attempt['run']['databaseId']
 p=subprocess.run(['gh','run','view',str(run_id),'--repo','bipxx/qupathxx','--json','databaseId,headSha,status,conclusion,url,jobs,createdAt,updatedAt'],capture_output=True,timeout=30,check=True)
 value=json.loads(p.stdout);assert value['headSha']==state['source'] and value['databaseId']==run_id
 (out/(role+'-run.json')).write_bytes(p.stdout)
 current=[dict(id=j['databaseId'],name=j['name'],status=j['status'],conclusion=j['conclusion'],step=next((s['name'] for s in j.get('steps',[]) if s['status']=='in_progress'),None)) for j in value['jobs'] if j['conclusion']!='skipped']
 summary.append(dict(role=role,run_id=run_id,status=value['status'],conclusion=value['conclusion'],url=value['url'],jobs=current));attempt['last_observation']=summary[-1]
state['last_polled_at_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();state['all_terminal']=all(s['status']=='completed' for s in summary);state_path.write_bytes((json.dumps(state,indent=2)+'\n').encode('utf-8'));(out/'summary.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode('utf-8'))
print(json.dumps(summary,indent=2))
