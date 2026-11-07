"""Freeze candidate5 from its installed wheel; reuse the candidate4 packaging path."""
import base64
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile
import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]
WORK = Path(sys.argv[1]).resolve()
DELIVERY = WORK / 'delivery'
CACHE = (ROOT / '../../ci/gridfrontends167-package/Release').resolve()


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def digest(data):
    return hashlib.sha256(data).hexdigest()


environment = q.build_environment()
revision = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
assert environment.git_revision == revision
assert not subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], text=True).strip()
wheel = DELIVERY / 'qupathxx-0.1.0-5-cp312-cp312-win_amd64.whl'
package = Path(q.__file__).parent
native = WORK / 'native'
native.mkdir(exist_ok=False)
verified = []
with zipfile.ZipFile(wheel) as source, zipfile.ZipFile(DELIVERY / 'qupathxx-windows-x64-candidate.zip', 'x', zipfile.ZIP_DEFLATED) as target:
    metadata = source.read('qupathxx-0.1.0.dist-info/METADATA').decode()
    assert 'License-Expression: AGPL-3.0-only' in metadata
    assert source.read('qupathxx-0.1.0.dist-info/licenses/LICENSE') == (ROOT / 'LICENSE').read_bytes()
    record = next(name for name in source.namelist() if name.endswith('.dist-info/RECORD'))
    for name, value, count in csv.reader(io.StringIO(source.read(record).decode())):
        if value:
            data = source.read(name)
            assert value == 'sha256=' + base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
            assert len(data) == int(count)
        if not name.endswith('.dist-info/RECORD'):
            assert (package.parent / name).read_bytes() == source.read(name), name
            verified.append(name)
        if not name.startswith('qupathxx/'):
            continue
        relative = name.removeprefix('qupathxx/')
        data = source.read(name)
        if relative.endswith(('.exe', '.pyd')):
            assert data == (CACHE / Path(relative).name).read_bytes(), name
            if not relative.endswith('qupathxx_region.exe'):
                assert (revision + '\0').encode() in data
                assert (environment.source_tree_sha256 + '\0').encode() in data
        if relative.endswith(('.exe', '.dll')) or relative.startswith(('licenses/', 'fonts/')):
            target.writestr(relative, data)
            destination = native / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
    target.writestr('RUN.txt', (ROOT / 'docs/benchmarks/windows-candidate5/RUN.md').read_bytes())
assert (native / 'licenses/qupathxx/LICENSE').read_bytes() == (ROOT / 'LICENSE').read_bytes()
tracked = [name for name in subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '-z']).decode().split('\0') if name]
inputs = sorted(name for name in tracked if
    (name.startswith('cpp/') and name.endswith(('.cpp', '.hpp'))) or
    (name.startswith('cmake/') and name.endswith(('.cmake', '.in'))) or
    (name.startswith('src/') and name.endswith('.py')) or name in ('CMakeLists.txt', 'pyproject.toml'))
manifest = ''.join(name + '=' + sha(ROOT / name) + '\n' for name in inputs).encode()
assert digest(manifest) == environment.source_tree_sha256
source_archive = DELIVERY / ('qupathxx-source-' + revision[:8] + '.zip')
with zipfile.ZipFile(source_archive, 'x', zipfile.ZIP_DEFLATED) as target:
    for name in tracked:
        target.write(ROOT / name, name)
    target.writestr('compiler-inputs.sha256', manifest)
    target.writestr('BUILD-CANDIDATE.txt',
        'Exact raw-byte source for native revision ' + revision + '.\n'
        'CPython3.12.8, MSVC19.44.35225, CMake4.4.4, nanobind3.1.0, scikit-build-core1.1.1, Release.\n'
        'Use pyproject.toml/scikit-build-core, wheel.build-tag=5 and the supplied dependency sources.\n'
        'Enable GUI, PYTHON, OPENSLIDE, TIFF, GEOS, PROJECT, INTERCHANGE and CLASSIFIER_TRAINING.\n'
        'Provide OpenSlide_ROOT, GEOS_DIR and OpenCV_DIR for the pinned SDKs; see tools/build_*_sdk.py.\n'
        'Build flags and dependency identities are recorded in candidate.json/dependency-sources.json.\n'
        'Compiler-input hashes preserve the actual source line endings.\n')
with zipfile.ZipFile(source_archive) as source:
    for name in inputs:
        assert digest(source.read(name)) == sha(ROOT / name), name
old = ROOT / '../../windows-workflow-checkpoint/delivery-agpl'
for name in ['qupathxx-dependency-sources.zip', 'dependency-sources.json']:
    os.link(old / name, DELIVERY / name)
dependencies = json.loads((DELIVERY / 'dependency-sources.json').read_text(encoding='utf-8'))
assert sha(DELIVERY / dependencies['archive']) == dependencies['sha256']
with zipfile.ZipFile(DELIVERY / dependencies['archive']) as source:
    for item in dependencies['sources']:
        assert digest(source.read(item['filename'])) == item['sha256']
for source, name in [(ROOT / 'docs/benchmarks/windows-candidate5/RUN.md', 'RUN.md'),
        (ROOT / 'docs/signed-distances.md', 'signed-distances.md'),
        (ROOT / 'docs/gui-project-density.md', 'project-density.md'),
        (WORK / 'run/signed.qpxproj', 'signed-example.qpxproj'),
        (WORK / 'run/source.qpxproj', 'signed-input.qpxproj'),
        (WORK / 'run/python.csv', 'signed-example.csv'),
        (WORK / 'run/workflow.json', 'installed-workflow.json'),
        (WORK / 'run/desktop-report.json', 'installed-desktop.json')]:
    with (DELIVERY / name).open('xb') as target:
        target.write(source.read_bytes())
candidate = dict(source_revision=revision, source_tree_sha256=environment.source_tree_sha256,
    wheel=wheel.name, wheel_sha256=sha(wheel), immutable_wheel_members_verified=len(verified),
    compiler_inputs_verified=len(inputs), dependency_archives_verified=len(dependencies['sources']),
    source_archive=source_archive.name, installed_package=str(package), license='AGPL-3.0-only',
    environment={name: getattr(environment, name) for name in ['git_revision', 'source_tree_sha256',
        'compiler', 'compiler_flags', 'configuration', 'platform', 'geos_version', 'openslide_version', 'sqlite_version']},
    support='Local Windows x64 CPython3.12 engineering install/run/removal; no fresh-host, signing, additional-platform, biology or speed qualification')
(DELIVERY / 'candidate.json').write_text(json.dumps(candidate, indent=2), encoding='utf-8')
print(json.dumps(candidate, indent=2))
