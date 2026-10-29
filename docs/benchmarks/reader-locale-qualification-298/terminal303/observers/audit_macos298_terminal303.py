from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2];campaign=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T022825Z'
run=json.loads((obs/'macos-run.json').read_text(encoding='utf-8'))
assert run['databaseId']==40792333489 and run['status']=='completed' and run['conclusion']=='success'
assert run['headSha']=='9fbc9315e8c2417c35492cf505a8f676ecdfcf75'
plan=json.loads((campaign/'complete-audit-plan.json').read_text(encoding='utf-8'))
item=next(item for item in plan if item['role']=='macos');cwd=Path(item['cwd'])
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=cwd,text=True).strip()==run['headSha']
assert not subprocess.check_output(['git','status','--porcelain'],cwd=cwd)
for name in ('macos-run-terminal.json','macos-complete-audit.log','macos-audit-exit.json','macos-acceptance.json'):assert not (campaign/name).exists(),name
(campaign/'macos-run-terminal.json').write_text(json.dumps(run,indent=2)+'\n',encoding='utf-8')
with (campaign/'macos-complete-audit.log').open('x',encoding='utf-8') as stream:r=subprocess.run(item['argv'],cwd=cwd,stdout=stream,stderr=subprocess.STDOUT)
(campaign/'macos-audit-exit.json').write_text(json.dumps(dict(argv=item['argv'],cwd=str(cwd),returncode=r.returncode,original_complete_auditor=True,source=run['headSha']),indent=2)+'\n',encoding='utf-8')
print('Original complete Mac298 auditor exit',r.returncode,flush=True)
assert not subprocess.check_output(['git','status','--porcelain'],cwd=cwd)
assert r.returncode==0,campaign/'macos-complete-audit.log'
