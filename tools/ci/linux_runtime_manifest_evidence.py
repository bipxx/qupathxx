"""Source-bound Linux wheel manifest checks for engine/portable artifact audits."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location('qpx_linux_runtime_evidence', Path(__file__).with_name('linux_runtime_manifest.py'))
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

SOURCES = ['tools/ci/run.py', 'tools/ci/run_manylinux.py', 'tools/ci/linux_runtime_manifest.py',
           'tools/ci/elf_dependencies.py', 'tools/ci/runtime_manifest.py', 'tools/ci/pe_dependencies.py',
           'tools/ci/linux_runtime_manifest_evidence.py', '.github/workflows/ci.yml',
           'tools/ci/linux-runtime-manifest-cases.json']


def python_cohort(path, root, commit):
    root, path = Path(root), Path(path)
    relative = 'tools/ci/linux-runtime-manifest-cases.json'
    contract = (root/relative).read_bytes()
    committed = subprocess.run(['git','-C',str(root),'show',commit+':'+relative], capture_output=True)
    runtime.require(committed.returncode == 0 and committed.stdout == contract, 'Linux manifest case contract differs from source')
    expected = json.loads(contract)
    runtime.require(expected['schema'] == 'qpx.linux-manifest-python-cases/1', 'invalid Linux manifest case schema')
    cases = list(ET.parse(path).getroot().iter('testcase'))
    groups = []
    for classname, names in expected['classes'].items():
        selected = [case for case in cases if case.get('classname','').rsplit('.',1)[-1] == classname]
        runtime.require(sorted(case.get('name') for case in selected) == names, 'Linux manifest Python cohort missing/duplicate/changed: '+classname)
        runtime.require(all(not any(case.find(tag) is not None for tag in ['failure','error','skipped']) for case in selected),
                        'Linux manifest Python cohort failed or skipped: '+classname)
        groups.append(dict(classname=classname, passed=len(selected)))
    return dict(groups=groups, passed=sum(row['passed'] for row in groups),
                case_contract_sha256=hashlib.sha256(contract).hexdigest(), junit_sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def audit(folder, wheel, root, commit, manifest_name='linux-wheel-runtime-manifest.json'):
    folder, wheel, root = Path(folder), Path(wheel), Path(root)
    runtime.require(manifest_name in ['linux-wheel-runtime-manifest.json', 'qualified-linux-runtime-manifest.json',
                                    'consumer-linux-runtime-manifest.json'], 'unsupported Linux evidence name')
    hashes = {}
    for relative in SOURCES:
        committed = subprocess.run(['git','-C',str(root),'show',commit+':'+relative], capture_output=True)
        actual = (root/relative).read_bytes()
        runtime.require(committed.returncode == 0 and actual == committed.stdout,
                        'Linux manifest tooling differs from dispatched source: '+relative)
        hashes[relative] = hashlib.sha256(actual).hexdigest()
    path = folder/manifest_name
    runtime.require(path.stat().st_size <= 64*1024*1024, 'Linux manifest evidence exceeds budget')
    recorded = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=runtime.common.unique_object)
    observed = runtime.inspect_wheel(wheel)
    runtime.require(runtime.common.canonical_json(recorded) == runtime.common.canonical_json(observed),
                    'Linux manifest differs from actual wheel')
    return dict(source=commit, tooling_sha256=hashes, archive_sha256=observed['archive_sha256'],
                manifest_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                files=len(observed['files']), binaries=len(observed['binaries']),
                dependency_edges=len(observed['dependency_edges']), static_linux_manifest_accepted=True,
                symbol_version_availability_verified=False, external_runtime_availability_verified=False,
                dynamic_loading_exhaustively_verified=False, release_ready=False,
                scope='Exact-source wheel bytes reinspected under explicit direct origin RUNPATH packaging policy. '
                      'This does not accept installed prerequisites, symbol versions/ABI, arbitrary loading or a supported release.')
