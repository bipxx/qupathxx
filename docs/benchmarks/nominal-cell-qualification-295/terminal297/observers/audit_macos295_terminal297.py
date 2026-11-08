from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/nominal-cell-qualification-295'
observation=root/'build/q295-all/20261028T235925Z'
verified=json.loads((observation/'verified.json').read_text())
assert len(verified['files'])==11
plan=next(p for p in json.loads((out/'complete-audit-plan.json').read_text()) if p['role']=='macos')
run=json.loads((observation/'macos-run.json').read_text())
assert run['status']=='completed' and run['conclusion']=='success'
assert not (out/'macos-audit-exit.json').exists()
(out/'macos-run-terminal.json').write_text(json.dumps(run,indent=2)+'\n')
with (out/'macos-complete-audit.log').open('x',encoding='utf-8') as stream:
    result=subprocess.run(plan['argv'],cwd=plan['cwd'],stdout=stream,stderr=subprocess.STDOUT)
(out/'macos-audit-exit.json').write_text(json.dumps(dict(argv=plan['argv'],cwd=plan['cwd'],returncode=result.returncode),indent=2)+'\n')
print('Original frozen295 Mac audit exit',result.returncode)
assert result.returncode==0
