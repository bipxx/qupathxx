"""Supplementary candidate257 arm64 inspection; never all-Mac acceptance."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import xml.etree.ElementTree as ET

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
commit = 'd57df0166888eb335d989d404bdae4d1b63c66a4'
folder = out / 'macos-native-arm64'


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name):
    path = root / 'tools/ci' / name
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    exec(compile(path.read_bytes(), str(path), 'exec', dont_inherit=True),
         module.__dict__)
    return module


auditor = load('accept_macos_native.py')
data = json.loads((folder / 'result.json').read_text())
require(data['commit'] == commit and data['architecture'] == 'arm64', 'source/architecture')
xml = folder / 'native-tests.xml'
cases = list(ET.parse(xml).getroot().iter('testcase'))
require(len(cases) == data['tests']['passed'] == 50, 'suite count')
require(sorted(c.get('name') for c in cases) == sorted(data['tests']['names']), 'suite identities')
require(not any(c.find(k) is not None for c in cases
                for k in ['failure', 'error', 'skipped']), 'failed/skipped suite')
require(hashlib.sha256(xml.read_bytes()).hexdigest() == data['tests']['junit_sha256'], 'JUnit hash')
report = dict(source=commit, architecture='arm64', native_suites=50,
              overall_mac_accepted=False,
              scope='Supplementary single-architecture checks only; full two-architecture auditor remains required')
for name in ['multifile', 'raw_classifier', 'resampling', 'axis', 'dearray',
             'qptma', 'hierarchy', 'scientific']:
    report[name] = getattr(auditor, name).native(cases)
report['qptma_project'] = auditor.qptma.project_native(cases)
report['frontier'] = load('frontier_evidence.py').native(cases)
report['tma_cases'] = load('tma_case_evidence.py').native(cases, file_export=True, cohort=True)
report['inventory'] = load('macos_inventory_evidence.py').audit(folder, root, commit, 'arm64', data)
report['relocation'] = load('macos_relocation_evidence.py').audit(folder, root, commit, 'arm64', data)
sources = auditor.raw_classifier.source_contract(root, commit)
auditor.qptma.verify_source_snapshot(root, sources)
report['raw_source_paths'] = len(sources)
(out / 'arm64-supplementary-checks.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print('Arm64 supplementary checks pass; full Mac acceptance remains pending.')
