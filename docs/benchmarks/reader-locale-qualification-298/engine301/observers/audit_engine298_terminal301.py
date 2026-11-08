from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T005538Z'
verified=json.loads((obs/'verified.json').read_text());assert len(verified['files'])==10
assert len([v for v in verified['files'] if v['run_id']==40792323979])==5
run=json.loads((obs/'engine-run.json').read_text());assert run['status']=='completed' and run['conclusion']=='failure'
plan=next(v for v in json.loads((out/'complete-audit-plan.json').read_text()) if v['role']=='engine')
assert not (out/'engine-audit-exit.json').exists()
(out/'engine-run-terminal.json').write_text(json.dumps(run,indent=2)+'\n')
with (out/'engine-complete-audit.log').open('x',encoding='utf-8') as stream:r=subprocess.run(plan['argv'],cwd=plan['cwd'],stdout=stream,stderr=subprocess.STDOUT)
(out/'engine-audit-exit.json').write_text(json.dumps(dict(argv=plan['argv'],cwd=plan['cwd'],returncode=r.returncode),indent=2)+'\n')
print('Original frozen298 engine complete audit rejects with exit',r.returncode,flush=True);assert r.returncode!=0
