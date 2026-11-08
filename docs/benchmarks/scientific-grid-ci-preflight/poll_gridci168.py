from pathlib import Path
import concurrent.futures,json,subprocess,datetime
root=Path(r'C:/Users/HEQ/Projects/qupathxx');path=root/'build/gridci168-campaign.json';state=json.loads(path.read_text(encoding='utf-8'));out=root/'build/gridci168-hosted';out.mkdir(exist_ok=True)
active=[a for a in state['attempts'] if a.get('run')]
def poll(attempt):
    run=attempt['run']['databaseId'];p=subprocess.run(['gh','run','view',str(run),'--repo','bipxx/qupathxx','--json','databaseId,headSha,url,status,conclusion,jobs'],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
    result=json.loads(p.stdout);assert result['headSha']==state['source'] and result['databaseId']==run
    (out/(attempt['role']+'-run.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return attempt,result
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for attempt,result in pool.map(poll,active):
        attempt['run']=result
        print(attempt['role'],result['databaseId'],result['status'],result['conclusion'])
        for job in result['jobs']:
            if job['conclusion']!='skipped':print(' ',job['name'],job['status'],job['conclusion'])
state['last_polled_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();path.write_bytes((json.dumps(state,indent=2)+'\n').encode('utf-8'))
