from pathlib import Path
import importlib.util
import json
import os
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
work = root / 'build/ci/scientific-grid-159'
out = root / 'build/gui-font-delivery-283'
python = root / 'build/ci/gui-font-delivery-283-env/Scripts/python.exe'
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=work, text=True).strip()
assert source == '7231d95c25fbc9515d937036986e8c92b19da261'
assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=work)
spec = importlib.util.spec_from_file_location('raw', work / 'tools/ci/raw_classifier_evidence.py')
raw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(raw)

def run(label, tests, gui, extra=()):
    log = out / (label + '.log')
    assert not log.exists(), log
    args = [str(python), '-I', '-X', 'utf8', '-m', 'pytest', *tests, '-q',
            '--junitxml=' + str(out / (label + '.xml')),
            '--basetemp', str(out / (label + '-temp')), *extra]
    env = os.environ.copy()
    env['QUPATHXX_TEST_GUI'] = '1' if gui else '0'
    (out / (label + '-command.json')).write_text(json.dumps(dict(argv=args, cwd=str(work),
        source=source, gui=gui), indent=2) + '\n')
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(args, cwd=work, env=env, stdout=stream, stderr=subprocess.STDOUT)
    print(label, result.returncode, flush=True)
    assert result.returncode == 0, log
    return ET.parse(out / (label + '.xml'))

for mode, gui in [('gui', True), ('nonrendering', False)]:
    tree = run(mode + '-final-cohort', ['tests/' + name + '.py' for name in raw.CONTRACT['installed']], gui)
    admitted = raw.python(tree.findall('.//testcase'), gui=gui)
    assert (admitted['passed'], admitted['declared_skips']) == ((395, 1) if gui else (348, 48))
    (out / (mode + '-final-cohort-admission.json')).write_text(json.dumps(admitted, indent=2) + '\n')

gui_files = ['tests/test_gui_' + name + '.py' for name in
             ('layout', 'docking', 'scale', 'overlay', 'lifecycle', 'workspace', 'window', 'replay', 'retirement')]
skip = ['--deselect=tests/test_gui_scale.py::' + name for name in (
    'test_unicode_labels_render_actual_glyphs_instead_of_question_marks',
    'test_unicode_dynamic_fonts_preserve_cached_pixels_across_scale_changes',
    'test_packaged_fonts_are_independent_of_working_directory',
    'test_packaged_fonts_reject_corrupt_assets_and_recover')]
run('desktop-regression', gui_files, True, skip)
original = json.loads((root / 'build/record-cohort-observer-275/preflight-command.json').read_text())
tests = [arg.replace('\\', '/') for arg in original['argv']
         if arg.replace('\\', '/').startswith('tests/') and arg.endswith('.py')]
assert len(tests) == 20, len(tests)
run('preflight', tests, False)
