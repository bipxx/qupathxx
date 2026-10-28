from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/gui-font-qualification-285'
observation=root/'build/q285-all/20261028T190527Z'
state=json.loads((out/'campaign.json').read_text())
run=json.loads((observation/'engine-run.json').read_text())
assert run['databaseId']==state['roles']['engine']['run']['databaseId']
assert run['headSha']==state['source'] and run['status']=='completed' and run['conclusion']=='failure'
verified=json.loads((observation/'verified.json').read_text())
assert len([v for v in verified['files'] if v['run_id']==run['databaseId']])==5
item,=[v for v in json.loads((out/'complete-audit-plan.json').read_text()) if v['role']=='engine']
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=item['cwd'],text=True).strip()==state['source']
assert not subprocess.check_output(['git','status','--porcelain'],cwd=item['cwd'])
log=out/'engine-complete-audit.log'
assert not log.exists()
with log.open('w',encoding='utf-8') as stream:
    result=subprocess.run(item['argv'],cwd=item['cwd'],stdout=stream,stderr=subprocess.STDOUT)
(out/'engine-audit-exit.json').write_text(json.dumps(dict(argv=item['argv'],cwd=item['cwd'],
    returncode=result.returncode,expected_to_accept=False),indent=2)+'\n')
print('Original frozen font285 full engine auditor exit',result.returncode,flush=True)
assert result.returncode!=0
