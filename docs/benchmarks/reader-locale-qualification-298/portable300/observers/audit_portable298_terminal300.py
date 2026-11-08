from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T004206Z'
verified=json.loads((obs/'verified.json').read_text());assert len(verified['files'])==8
assert len([v for v in verified['files'] if v['name'].startswith('manylinux')])==4
run=json.loads((obs/'portable-run.json').read_text());assert run['status']=='completed' and run['conclusion']=='success'
plan=next(v for v in json.loads((out/'complete-audit-plan.json').read_text()) if v['role']=='portable')
assert not (out/'portable-audit-exit.json').exists()
(out/'portable-run-terminal.json').write_text(json.dumps(run,indent=2)+'\n')
with (out/'portable-complete-audit.log').open('x',encoding='utf-8') as stream:r=subprocess.run(plan['argv'],cwd=plan['cwd'],stdout=stream,stderr=subprocess.STDOUT)
(out/'portable-audit-exit.json').write_text(json.dumps(dict(argv=plan['argv'],cwd=plan['cwd'],returncode=r.returncode),indent=2)+'\n')
print('Original frozen298 portable complete audit exit',r.returncode,flush=True);assert r.returncode==0
