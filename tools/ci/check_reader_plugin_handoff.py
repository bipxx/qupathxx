"""Install/use/remove one explicitly admitted development plugin package.

Development controller only: decoding, project/review, RegionAnalysis and replay
run in installed C++. No engine build, download, wheel or publication occurs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

import check_material_staging as admission
import stage_native_release


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def member(profile, group, input_name):
    matches = [item['path'] for item in profile['stages'][group] if item['input'] == input_name]
    if len(matches) != 1:
        raise ValueError('Need one explicit admitted ' + group + ' member for ' + input_name)
    return matches[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True, help='Frozen reader-plugin-dev stage_release output')
    parser.add_argument('--dependencies', type=Path, required=True, help='Matching collected dependency source output')
    parser.add_argument('--consumer', type=Path, required=True, help='Built SDK consumer; bytes must match admitted host member')
    parser.add_argument('--work', type=Path, required=True, help='New external per-user proof directory')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt': parser.error('Reader plugin development handoff requires Windows x64')
    source, work, report = args.input.resolve(), args.work.absolute(), args.report.absolute()
    frozen = source / 'source'
    inventories = [frozen / 'tests/fixtures/manifest.json']
    if (frozen / 'assets/fonts/font-source.json').is_file(): inventories.append(frozen / 'assets/fonts/font-source.json')
    entries = admission.inventory([*inventories, source / 'generated-materials.json', args.dependencies / 'generated-materials.json'])
    receipt = json.loads((source / 'admission.json').read_text(encoding='utf8'))
    profiles = [json.loads(p.read_text(encoding='utf8')).get('release_profiles', {}).get(receipt['profile']) for p in inventories]
    profiles = [p for p in profiles if p]
    if receipt.get('state') != 'passed' or len(profiles) != 1 or profiles[0].get('kind') != 'reader-plugin-dev':
        raise ValueError('Need one frozen admitted development-plugin profile')
    profile = profiles[0]
    if receipt['source_sha256'] != profile['compiled_source_sha256']:
        raise ValueError('Host/source profile identities differ')
    admission.check_stage(frozen, entries)
    for item in profile['stages']['source']:
        expected = entries[item['material_id']]
        if sha(frozen / item['path']) != expected['sha256']:
            raise ValueError('Frozen corresponding-source member changed: ' + item['path'])
    if hashlib.sha256(stage_native_release.compiled_manifest(frozen)).hexdigest() != receipt['source_sha256']:
        raise ValueError('Frozen corresponding source no longer matches the native host')
    dependency = json.loads((args.dependencies / 'dependency-sources.json').read_text(encoding='utf8'))
    if (dependency.get('state') != 'passed' or dependency['source_sha256'] != receipt['source_sha256']
            or {item['material_id'] for item in dependency['sources']} != set(profile['sources'])):
        raise ValueError('Dependency source companion differs from the selected native profile')
    dependency_archive = args.dependencies / dependency['archive']
    if sha(dependency_archive) != dependency['sha256'] or dependency_archive.stat().st_size != dependency['bytes']:
        raise ValueError('Dependency source archive changed after collection')
    admission.check_archive(dependency_archive, entries)
    archives = {}
    for name, item in receipt['stages'].items():
        archive = source / item['archive']
        if sha(archive) != item['sha256'] or archive.stat().st_size != item['bytes']:
            raise ValueError('Admitted development archive changed: ' + name)
        if admission.check_archive(archive, entries) != item['archive_members']:
            raise ValueError('Admitted archive member count changed: ' + name)
        archives[name] = archive
    work.mkdir(parents=True, exist_ok=False)
    for part in (work, *work.parents):
        if admission.reparse(part): raise ValueError('New proof prefix contains a link/reparse point')
    report.mkdir(parents=True, exist_ok=True)
    installed = {}
    roots = {}
    for name in ['host', 'kit', 'module']:
        target = work / 'installed' / name; target.mkdir(parents=True)
        with zipfile.ZipFile(archives[name]) as archive: archive.extractall(target)
        admission.check_stage(target, entries)
        for item in profile['stages'][name]:
            expected = entries[item['material_id']]
            if sha(target / item['path']) != expected['sha256']:
                raise ValueError('Installed reviewed path changed: ' + item['path'])
        installed.update({str(p): sha(p) for p in target.rglob('*') if p.is_file()})
        roots[name] = target
    host = roots['host'] / member(profile, 'host', profile['host_executable_input'])
    consumer = roots['host'] / member(profile, 'host', profile['host_consumer_input'])
    if sha(consumer) != sha(args.consumer): raise ValueError('SDK consumer differs from its admitted installed member')
    manifest = roots['module'] / member(profile, 'module', profile['plugin_descriptor_input'])
    environment = os.environ.copy()
    for key in list(environment):
        if key.upper() in {'PYTHONHOME', 'PYTHONPATH', 'CMAKE_PREFIX_PATH', 'QUPATHXX_DIR', 'QUPATHXX_UI_LANGUAGE', 'PSMODULEPATH'}:
            environment.pop(key)
    windows = Path(os.environ['SystemRoot'])
    environment['PATH'] = os.pathsep.join([str(windows / 'System32'), str(windows)])
    commands = []

    def run(name, argv, expect=0):
        command = [str(value) for value in argv]
        result = subprocess.run(command, cwd=windows / 'System32', env=environment,
                                capture_output=True, timeout=45)
        (report / (name + '.stdout')).write_bytes(result.stdout)
        (report / (name + '.stderr')).write_bytes(result.stderr)
        commands.append({'name': name, 'argv': command, 'cwd': str(windows / 'System32'), 'returncode': result.returncode})
        (report / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf8')
        if result.returncode != expect:
            raise RuntimeError(f'{name}: expected exit {expect}, got {result.returncode}; retained stdout/stderr')
        return result.stdout

    catalog = json.loads(run('installed-catalog', [host, '--reader-plugins', manifest, '--readers']))
    providers = [item for item in catalog['readers'] if item['provider_id'] == 'org.qupathxx.p6']
    if len(providers) != 1 or not providers[0]['available'] or not providers[0]['display_rgba'] or providers[0]['raw_samples']:
        raise ValueError('Installed P6 provider lacks its admitted display-only capability')
    user = work / 'user data'; user.mkdir()
    source_image = user / 'input.ppm'
    pixels = bytes(range(12)); source_image.write_bytes(b'P6\n2 2\n255\n' + pixels)
    project = user / 'retained.qpx'
    created = json.loads(run('native-project-create', [consumer, 'create', manifest, source_image, project]))
    project_before = sha(project)
    reopened = json.loads(run('native-project-reopen', [consumer, 'reopen', manifest, project]))
    for mode, value in [('create', created), ('reopen', reopened)]:
        if (value.get('schema') != 'qpx.plugin-project-handoff/1' or value.get('mode') != mode
                or value.get('provider') != 'org.qupathxx.p6' or value.get('images') != 1
                or value.get('records') != 1 or not value.get('verified') or value.get('samples') != 4
                or value.get('source_sha256') != receipt['source_sha256']
                or not re.fullmatch('[0-9a-f]{64}', value.get('input_sha256', ''))):
            raise ValueError('Installed native saved RegionAnalysis contract failed: ' + mode)
    if created['input_sha256'] != reopened['input_sha256'] or sha(project) != project_before:
        raise ValueError('Installed replay changed the exact retained input identity/project bytes')
    output = user / 'rgba.pam'
    run('installed-module-rgba', [host, '--reader-plugins', manifest, '--reader', 'org.qupathxx.p6', source_image, 0, 0, 0, 2, 2, output])
    expected = b'P7\nWIDTH 2\nHEIGHT 2\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n'
    expected += b''.join(pixels[i:i+3] + b'\xff' for i in range(0, 12, 3))
    if output.read_bytes() != expected: raise ValueError('Installed module changed exact independent P6/PAM bytes')
    note = roots['host'] / 'user scratch' / 'research-notes.txt'; note.parent.mkdir()
    note.write_bytes(b'Authored in-prefix research notes must survive manifest-owned removal.\n')
    protected = {str(p): sha(p) for p in user.rglob('*') if p.is_file()}; protected[str(note)] = sha(note)
    if any(sha(Path(p)) != digest for p, digest in installed.items()):
        raise ValueError('Installed members changed during native use')
    handoff = {'state': 'install_and_use_passed', 'scope': 'local Windows x64 development plugin host/kit/module; no publication',
               'profile': receipt['profile'], 'source_sha256': receipt['source_sha256'], 'work': str(work),
               'install_run_verified': True, 'uninstall_verified': False, 'installed_files': installed,
               'protected_user_files': protected, 'consumer': reopened, 'runtime_PATH': environment['PATH'],
               'engine_builds': 0, 'python_application_logic': False}
    handoff_path = report / 'handoff.json'; handoff_path.write_text(json.dumps(handoff, indent=2) + '\n', encoding='utf8')
    removal = frozen / 'tools/ci/remove_native_handoff.ps1'
    if not removal.is_file(): raise ValueError('Corresponding source omits the reviewed removal controller')
    powershell = windows / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    run('manifest-owned-removal', [powershell, '-NoProfile', '-NonInteractive', '-File', removal, '-Receipt', handoff_path])
    removed = json.loads(handoff_path.read_text(encoding='utf-8-sig'))
    if not removed.get('uninstall_verified') or any(Path(p).exists() for p in installed):
        raise ValueError('Manifest-owned installed member remains after removal')
    if any(sha(Path(p)) != digest for p, digest in protected.items()):
        raise ValueError('Research files changed during package removal')
    print(json.dumps({'state': 'passed', 'profile': receipt['profile'], 'source_sha256': receipt['source_sha256'],
                      'installed_members_removed': len(installed), 'protected_user_files': len(protected),
                      'saved_rgb_replay': True, 'exact_rgba': True, 'handoff': str(handoff_path)}, indent=2))


if __name__ == '__main__': main()
