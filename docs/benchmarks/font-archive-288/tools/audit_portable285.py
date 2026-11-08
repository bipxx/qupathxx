from pathlib import Path
import json
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
out=root/'build/gui-font-qualification-285'
observation=root/'build/q285-all/20261028T185757Z'
run=json.loads((observation/'portable-run.json').read_text())
state=json.loads((out/'campaign.json').read_text())
assert run['databaseId']==state['roles']['portable']['run']['databaseId']
assert run['headSha']==state['source'] and run['status']=='completed' and run['conclusion']=='success'
verified=json.loads((observation/'verified.json').read_text())
assert verified['source']==state['source']
assert {v['name'] for v in verified['files'] if v['run_id']==run['databaseId']}=={
    'manylinux-3.11','manylinux-3.12','manylinux-consumer-3.11','manylinux-consumer-3.12'}
target=out/'portable-run-terminal.json'
assert not target.exists()
shutil.copyfile(observation/'portable-run.json',target)
plan=json.loads((out/'complete-audit-plan.json').read_text())
item,=[v for v in plan if v['role']=='portable']
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=item['cwd'],text=True).strip()==state['source']
assert not subprocess.check_output(['git','status','--porcelain'],cwd=item['cwd'])
assert not (out/'portable-acceptance.json').exists()
with (out/'portable-complete-audit.log').open('w',encoding='utf-8') as log:
    result=subprocess.run(item['argv'],cwd=item['cwd'],stdout=log,stderr=subprocess.STDOUT)
(out/'portable-audit-exit.json').write_text(json.dumps(dict(argv=item['argv'],cwd=item['cwd'],
    returncode=result.returncode),indent=2)+'\n')
print('Frozen original font285 portable complete auditor exit',result.returncode,flush=True)
assert result.returncode==0
