from pathlib import Path
import datetime
import hashlib
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET

r = Path(__file__).resolve().parents[2]
s = r / 'build/ci/scientific-grid-159'
source = subprocess.check_output(['git', '-C', str(s), 'rev-parse', 'HEAD'], text=True).strip()
manifests = [json.loads((r / f'build/rawarchive217f-{h}/compiler-inputs.json').read_text()) for h in ('windows', 'linux')]
assert manifests[0] == manifests[1] and manifests[0]['source'] == source
m = manifests[0]
for name, digest in m['files'].items():
    assert hashlib.sha256((s / name).read_bytes()).hexdigest() == digest
prior = json.loads((r / 'build/rawarchive217e-windows/compiler-inputs.json').read_text())
changed = [name for name in set(prior['files']) | set(m['files']) if prior['files'].get(name) != m['files'].get(name)]
assert changed == ['cpp/tests/raw_pixel_archive_checks.hpp']
markers = [
    'Owner-captured raw training/prediction: deterministic labels, rational axes, document calibration, bounded IO, cancellation, stale compound sources and owner-thread publication passed',
    'Raw pixel archives: real TIFF retention, project save/reopen, snapshot save, stale-file refusal and historical-only decode passed',
    'Raw pixel archives: sampling/prediction semantic round trips, rational axes, authority refusal, tamper/cancel and atomic replacement passed',
]
for host in ('windows', 'linux'):
    folder = r / f'build/rawarchive217f-{host}'
    assert all(c['exit'] == 0 for c in json.loads((folder / 'commands.json').read_text()))
    cases = [c for xml in folder.glob('*.xml') for c in ET.parse(xml).iter('testcase')]
    assert len(cases) == 1 and cases[0].attrib['name'] == 'pixel_workflows'
    assert all(cases[0].find(tag) is None for tag in ('failure', 'error', 'skipped'))
    output = cases[0].findtext('system-out') or ''
    assert all(output.count(marker) == 1 for marker in markers)
    previous = [c for xml in (r / f'build/rawarchive217e-{host}').glob('*.xml') for c in ET.parse(xml).iter('testcase')]
    assert len(previous) == 11
    assert [c.attrib['name'] for c in previous if c.find('failure') is not None] == ['pixel_workflows']

# Authenticate consumed source through framed Git objects, retaining checkout hashes.
request = ''.join(source + ':' + name + '\n' for name in m['files']).encode()
raw = subprocess.run(['git', '-C', str(s), 'cat-file', '--batch'], input=request, capture_output=True, check=True).stdout
position = 0
proof = []
for name, digest in m['files'].items():
    end = raw.index(b'\n', position)
    header = raw[position:end].split()
    assert len(header) == 3 and header[1] == b'blob'
    size = int(header[2]); position = end + 1
    blob = raw[position:position + size]; position += size
    assert raw[position:position + 1] == b'\n'; position += 1
    checkout = (s / name).read_bytes()
    assert blob.replace(b'\r\n', b'\n') == checkout.replace(b'\r\n', b'\n')
    proof.append(dict(path=name, git_object=header[0].decode(), git_blob_sha256=hashlib.sha256(blob).hexdigest(), checkout_sha256=digest, crlf_normalized=blob != checkout))
assert position == len(raw)

payload_dir = s / 'docs/benchmarks/raw-pixel-archives-local'
payload_dir.mkdir(exist_ok=False)
payloads = []
def copy(original, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(original, target)
    data = target.read_bytes()
    assert data == original.read_bytes()
    payloads.append(dict(path=target.relative_to(s).as_posix(), bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
def save(name, value):
    path = r / 'build' / name
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')
    return path

retained = r / 'build/rawarchive217f-retained-binaries'
retained.mkdir(exist_ok=False)
binaries = []
for host in ('windows', 'linux'):
    cache = r / ('build/ci/gridfrontends167-package' if host == 'windows' else 'build/ci/raw176-tiff-linux-native')
    folder = r / f'build/rawarchive217f-{host}'
    header = cache / 'generated/build_manifest.hpp'
    assert source in header.read_text() and m['fingerprint'] in header.read_text()
    copy(header, payload_dir / host / 'build_manifest.hpp')
    data = (cache / 'CMakeCache.txt').read_bytes()
    flags = {line.split(':')[0]: line.split('=', 1)[1] for line in data.decode('utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
    assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING'] == 'OFF'
    config = save(f'rawarchive217f-{host}-configuration.json', dict(cache_sha256=hashlib.sha256(data).hexdigest(), flags=flags))
    copy(config, payload_dir / host / 'configuration.json')
    original = cache / ('Release/qupathxx_pixel_workflow_tests.exe' if host == 'windows' else 'qupathxx_pixel_workflow_tests')
    target = retained / host / original.name
    target.parent.mkdir()
    shutil.copy2(original, target)
    assert target.read_bytes() == original.read_bytes()
    binaries.append(dict(host=host, source=source, fingerprint=m['fingerprint'], path=target.relative_to(r).as_posix(), bytes=target.stat().st_size, sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    for file in sorted(folder.iterdir()):
        if file.is_file(): copy(file, payload_dir / host / file.name)

for prefix in ('rawarchive217', 'rawarchive217b', 'rawarchive217c', 'rawarchive217d', 'rawarchive217e'):
    for host in ('windows', 'linux'):
        for file in sorted((r / f'build/{prefix}-{host}').iterdir()):
            if file.is_file(): copy(file, payload_dir / 'development' / f'{prefix}-{host}' / file.name)
copy(save('rawarchive217f-compiler-git-proof.json', dict(source=source, fingerprint=m['fingerprint'], framed_batch_objects=len(proof), files=proof)), payload_dir / 'compiler-git-proof.json')
copy(save('rawarchive217f-retained-binaries.json', dict(source=source, binaries=binaries)), payload_dir / 'retained-binaries.json')
for name in ('build_rawarchive217f.py', 'receipt_rawarchive217.py'):
    copy(r / 'build/ci' / name, payload_dir / name)
receipt = dict(
    schema='qpx.raw-pixel-archives-local/1', observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    source=source, compiled_fingerprint=m['fingerprint'], compiler_inputs=len(proof),
    status='native_sampling_prediction_archives_and_document_project_retention_locally_verified',
    native=dict(windows_suites=1, linux_suites=1, required_markers=markers),
    inherited_regressions=dict(source=prior['source'], fingerprint=prior['fingerprint'], passing_suites_per_host=10, failed_suite='pixel_workflows', failure='test-owned recovery sidecars prevented empty-directory removal', applicability='Only raw_pixel_archive_checks.hpp changed; it is consumed by pixel_workflows. Other ten results retain their original source, not relabeled as the final source.', changed_compiler_inputs=changed),
    formats=dict(new_raw_model='QRM1', legacy_raw_model='explicit strict QPR1 read preserving original bytes/hash', recipe='QXS1', table='QXT1', prediction='QXP1', record='QXR1', source_only_provenance='QPV4 with explicit codec opt-in', sql_schema=13),
    scope=['Sampling and prediction semantic round trips with independent model framing and original forest authority', 'Real TIFF Document retention, project save/reopen, snapshot save and recovery readback', 'Historical decode lacks publication authority; owner/source/capture identity required for keep', 'Unsupported source footprint, corrupted/versioned/truncated/hash-tampered archives reject', 'Atomic archive replacement preserves unrelated and cancelled destinations', 'Strict source-only provenance does not weaken existing object consumers'],
    remaining=['Positive backend-enabled raw forest training and trained archive acceptance', 'Explicit live source/model replay verification and owner-stage apply/undo', 'Phase-preserving tiled sampling/features, streaming and checkpoint resume', 'Nanobind/Python/NumPy, CLI/batch, managed viewer and ImGui integration', 'Integrated source-bound platform and independent biological/model/hardware/release acceptance'],
    backend_enabled_training_accepted=False, raw_payloads=payloads, retained_binaries=retained.relative_to(r).as_posix(),
    main_changed=False, milestones='M0-M7 Partial')
(s / 'docs/benchmarks/raw-pixel-archives-local.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(payloads=len(payloads), compiler_inputs=len(proof), retained_binaries=len(binaries), source=source)))
