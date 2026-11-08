from pathlib import Path
import json
import os
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/gui-font-delivery-283'
work = root / 'build/ci/scientific-grid-159'
original = json.loads((root / 'build/record-cohort-observer-275/preflight-command.json').read_text())
tests = [a.replace('\\', '/') for a in original['argv']
         if a.replace('\\', '/').startswith('tests/') and a.endswith('.py')]
assert len(tests) == 20
args = [str(root / 'build/ci/gui-font-delivery-283-env/Scripts/python.exe'),
        '-I', '-X', 'utf8', '-m', 'pytest', *tests, '-q',
        '--junitxml=' + str(out / 'preflight.xml'), '--basetemp', str(out / 'preflight-temp')]
assert not (out / 'preflight.log').exists()
(out / 'preflight-command.json').write_text(json.dumps(dict(argv=args, cwd=str(work),
    note='Normalize both separator styles from the original20-file command; do not omit Windows-path entries.'), indent=2) + '\n')
env = os.environ.copy()
env['QUPATHXX_TEST_GUI'] = '0'
with (out / 'preflight.log').open('w', encoding='utf-8') as stream:
    result = subprocess.run(args, cwd=work, env=env, stdout=stream, stderr=subprocess.STDOUT)
(out / 'preflight-exit.json').write_text(json.dumps(dict(returncode=result.returncode), indent=2) + '\n')
print('Preflight exit', result.returncode, flush=True)
assert result.returncode == 0
