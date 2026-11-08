"""Source-bound Windows package-manifest artifact checks for the engine audit."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

spec = importlib.util.spec_from_file_location('qpx_runtime_manifest', Path(__file__).with_name('runtime_manifest.py'))
runtime = importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),runtime.__dict__)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    runtime.require(path.stat().st_size <= 64*1024*1024, 'manifest evidence exceeds byte budget')
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=runtime.unique_object)


def audit(folder, wheel, root, commit, gui=False):
    folder, wheel, root = Path(folder), Path(wheel), Path(root)
    sources = ['tools/ci/run.py', 'tools/ci/runtime_manifest.py', 'tools/ci/pe_dependencies.py',
               'tools/ci/probe_pe_forwarders.py', 'tools/ci/runtime_manifest_evidence.py']
    source_hashes = {}
    for relative in sources:
        committed = subprocess.run(['git', '-C', str(root), 'show', commit+':'+relative], capture_output=True)
        actual = (root/relative).read_bytes()
        runtime.require(committed.returncode == 0 and actual == committed.stdout, 'runtime manifest tooling differs from dispatched source: '+relative)
        source_hashes[relative] = hashlib.sha256(actual).hexdigest()
    observed = runtime.inspect_wheel(wheel, gui=gui)
    manifest_path = folder/'windows-wheel-runtime-manifest.json'
    runtime.require(runtime.canonical_json(read(manifest_path)) == runtime.canonical_json(observed),
                    'wheel runtime manifest differs from actual archive')
    probe_path = folder/'pe-forwarder-native.json'
    probe = read(probe_path)
    runtime.require(probe['schema'] == 'qpx.pe-forwarder-native-probe/1' and
                    type(probe['direct_result']) is int and type(probe['forwarded_result']) is int and
                    probe['direct_result'] == probe['forwarded_result'] == 123 and
                    probe['parser_modules'] == ['qpx_manifest_bridge.dll'] and probe['release_ready'] is False,
                    'native forwarder probe result differs')
    runtime.require(probe['probe_sha256'] == source_hashes['tools/ci/probe_pe_forwarders.py'],
                    'native forwarder probe source differs')
    binaries = probe['binaries_sha256']
    runtime.require(set(binaries) == {'qpx_manifest_bridge.dll', 'qpx_manifest_front.dll'} and
                    all(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value) for value in binaries.values()),
                    'native forwarder binary digests differ')
    result = dict(source=commit, tooling_sha256=source_hashes, wheel_manifest_sha256=digest(manifest_path),
                  wheel_files=len(observed['files']), wheel_binaries=len(observed['binaries']),
                  wheel_dependency_edges=len(observed['dependency_edges']), native_forwarder_probe_sha256=digest(probe_path),
                  static_windows_manifest_accepted=True, dynamic_loading_exhaustively_verified=False,
                  external_runtime_availability_verified=False, release_ready=False,
                  scope='Exact-source offline archive reinspection and static dependency records. Native forwarder execution and binary digests are CI-generated evidence; fixture binary bytes are not uploaded. This does not accept deployment/runtime availability, arbitrary ABI, hardware, signing or a supported release.')
    candidate = folder/'qupathxx-windows-x64-candidate.zip'
    native_manifest = folder/'windows-native-runtime-manifest.json'
    if gui:
        actual = runtime.inspect_archive(candidate, gui=True, native_archive=True)
        runtime.require(runtime.canonical_json(read(native_manifest)) == runtime.canonical_json(actual),
                        'standalone runtime manifest differs from actual archive')
        wheel_files = {item['path']: item['sha256'] for item in observed['files']}
        runtime.require(all(item['path'] == 'README.txt' or wheel_files.get('qupathxx/'+item['path']) == item['sha256']
                            for item in actual['files']), 'standalone archive is not derived from the audited wheel')
        result.update(native_manifest_sha256=digest(native_manifest), native_files=len(actual['files']),
                      native_binaries=len(actual['binaries']), native_dependency_edges=len(actual['dependency_edges']))
    else:
        runtime.require(not candidate.exists() and not native_manifest.exists(), 'standalone GUI archive in headless evidence')
    return result
