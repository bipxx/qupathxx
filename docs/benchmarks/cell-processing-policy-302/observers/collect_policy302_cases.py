from pathlib import Path
import json
import os
import subprocess

root = Path(__file__).resolve().parents[2]
work = root/'build/ci/scientific-grid-159'
out = root/'build/cell-processing-policy-302'
python = root/'build/ci/reader-locale-297-env/Scripts/python.exe'
env = os.environ.copy(); env['PYTEST_DISABLE_PLUGIN_AUTOLOAD'] = '1'
modules = ['test_imagej_cell_processing', 'test_nominal_cell_frontends']
manifest = work/'tools/ci/scientific-grid-cases.json'
cases = json.loads(manifest.read_text())
for module in modules:
    args = [str(python), '-I', '-m', 'pytest', 'tests/'+module+'.py', '--collect-only', '-q', '--noconftest', '--color=no', '-o', 'addopts=']
    result = subprocess.run(args, cwd=work, env=env, capture_output=True, text=True, encoding='utf-8')
    (out/(module+'-collection.log')).write_text(result.stdout+result.stderr, encoding='utf-8')
    assert result.returncode == 0
    names = [line.split('::',1)[1] for line in result.stdout.splitlines() if module+'.py::' in line]
    assert names and len(names) == len(set(names))
    cases[module] = sorted(names)
manifest.write_text(json.dumps(cases, indent=2)+'\n', encoding='utf-8')
print({module:len(cases[module]) for module in modules})
