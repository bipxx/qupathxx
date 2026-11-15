"""Check a frozen desktop ZIP in a new external Windows prefix, without rebuilding.

Development orchestration only: the native probe owns project interpretation,
relocation, exact replay and CSV export. Removal uses remove_native_handoff.ps1.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import zipfile

import check_material_staging as admission
from stage_native_release import compiled_manifest, sha


def files(root):
    return {str(p): sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def check_handoff(release, dependencies, probe, reference, work, report):
    if os.name != 'nt':
        raise ValueError('This installed-route check is Windows x64 only')
    release, dependencies, probe, reference, work, report = (
        Path(p).absolute() for p in (release, dependencies, probe, reference, work, report))
    checkout = Path(__file__).resolve().parents[2]
    if work.is_relative_to(checkout) or work.exists():
        raise ValueError('Need a new validation prefix outside the checkout')
    for part in (work, *work.parents):
        if part.exists() and admission.reparse(part):
            raise ValueError('Validation ancestry contains a link/reparse point')
    receipt = json.loads((release/'admission.json').read_text(encoding='utf8'))
    frozen = release/'source'
    manifest = frozen/'tests/fixtures/manifest.json'
    font_manifest = frozen/'assets/fonts/font-source.json'
    profile = json.loads(manifest.read_text(encoding='utf8'))['release_profiles'][receipt['profile']]
    if (receipt['state'] != 'passed' or profile['kind'] != 'desktop-only'
            or profile['composition'] != receipt['composition']
            or profile['source_inputs_sha256'] != receipt['source_inputs_sha256']):
        raise ValueError('Frozen desktop profile/receipt identity differs')
    generated = json.loads((release/'generated-materials.json').read_text(encoding='utf8'))['records']
    generated_by_hash = {r['sha256']: r['material']['id'] for r in generated}
    entries = admission.inventory([manifest, font_manifest, release/'generated-materials.json',
                                   dependencies/'generated-materials.json'])
    archives = {}
    for name, info in receipt['stages'].items():
        archive = release/info['archive']
        if sha(archive) != info['sha256'] or archive.stat().st_size != info['bytes']:
            raise ValueError('Frozen archive changed: ' + name)
        expected = {m['path']: m['material_id'] for m in profile['stages'][name]}
        metadata = ['RELEASE.json']
        if name == 'source':
            metadata += ['tests/fixtures/manifest.json', 'assets/fonts/font-source.json', 'source-inputs.sha256']
        for relative in metadata:
            expected[relative] = generated_by_hash[sha(release/name/relative)]
        if admission.check_archive(archive, entries, expected=expected) != info['archive_members']:
            raise ValueError('Archive member count differs: ' + name)
        archives[name] = archive
    if sha(frozen/'source-inputs.sha256') != receipt['source_inputs_sha256']:
        raise ValueError('Corresponding-source fingerprint differs')
    if compiled_manifest(frozen) != (frozen/'source-inputs.sha256').read_bytes():
        raise ValueError('Staged corresponding-source inputs changed')
    companion = json.loads((dependencies/'dependency-sources.json').read_text(encoding='utf8'))
    if (companion['state'] != 'passed' or companion['profile'] != receipt['profile']
            or companion['source_sha256'] != receipt['source_inputs_sha256']
            or [r['material_id'] for r in companion['sources']] != profile['sources']):
        raise ValueError('Dependency companion is not bound to this desktop source')
    dependency_zip = dependencies/companion['archive']
    if sha(dependency_zip) != companion['sha256'] or dependency_zip.stat().st_size != companion['bytes']:
        raise ValueError('Dependency companion archive changed')
    expected = {r['filename']: r['material_id'] for r in companion['sources']}
    for record in json.loads((dependencies/'generated-materials.json').read_text(encoding='utf8'))['records']:
        expected[record['filename']] = record['material']['id']
    admission.check_archive(dependency_zip, entries, expected=expected)

    reference_before = files(reference)
    expected_result = json.loads((reference/'real.json').read_text(encoding='utf8'))
    work.mkdir(parents=True, exist_ok=False)
    report.mkdir(parents=True, exist_ok=True)
    native, user = work/'installed/native app', work/'user data'
    native.mkdir(parents=True)
    user.mkdir()
    with zipfile.ZipFile(archives['desktop']) as archive:
        archive.extractall(native)  # exact safe members were checked before extraction
    admission.check_stage(native, entries)
    installed = files(native)
    package_members = len(installed)
    validation_probe = native/'qupathxx_desktop_delivery_probe.exe'
    if validation_probe.exists():
        raise ValueError('Development probe collides with a shipped member')
    shutil.copyfile(probe, validation_probe)
    installed[str(validation_probe)] = sha(validation_probe)
    env = os.environ.copy()
    for key in list(env):
        if key.upper().startswith('PYTHON') or key.upper() in {
                'CMAKE_PREFIX_PATH', 'QUPATHXX_DIR', 'QUPATHXX_UI_LANGUAGE', 'VIRTUAL_ENV'}:
            env.pop(key)
    windows = Path(os.environ['SystemRoot'])
    env['PATH'] = os.pathsep.join([str(windows/'System32'), str(windows)])
    commands = []

    def run(name, argv):
        argv = [str(v) for v in argv]
        commands.append({'name': name, 'argv': argv, 'cwd': str(user)})
        (report/'commands.json').write_text(json.dumps(commands, indent=2)+'\n', encoding='utf8')
        with (report/(name+'.log')).open('wb') as log:
            result = subprocess.run(argv, cwd=user, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=180, creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:
            raise RuntimeError(name+' failed; retained '+str(report/(name+'.log')))
        print(name+': passed', flush=True)

    result_dir = user/'native results'
    run('native-project-use', [validation_probe, reference/'differential.qpx', result_dir])
    result = dict(line.split('=', 1) for line in (result_dir/'result.txt').read_text(encoding='utf8').splitlines())
    for key in ('record', 'result_sha256', 'source', 'pseudobulk', 'features', 'observations',
                'n_a', 'n_b', 'testable_features'):
        if result[key] != str(expected_result[key]):
            raise ValueError('Representative native identity/value differs: ' + key)
    if result['saved_replay'] != 'exact':
        raise ValueError('Saved native project did not replay exactly')
    for name in ('geos.dll', 'geos_c.dll', 'libopenslide-1.dll'):
        if Path(result['module:'+name]).resolve() != (native/name).resolve():
            raise ValueError('Runtime module escaped the installed app: ' + name)
    for name in ('genes.csv', 'replicates.csv'):
        if sha(result_dir/name) != sha(reference/name):
            raise ValueError('Relocated native export differs: ' + name)
    research_before = files(result_dir)
    run('gui-project-open', [native/'qupathxx_app.exe', '--project', result_dir/'study.qpx',
                             '--layout', user/'preferences.ini', '--smoke'])
    if files(result_dir) != research_before or files(reference) != reference_before:
        raise ValueError('Installed use changed a project/input/export')
    sentinel = native/'user scratch/research-notes.txt'
    sentinel.parent.mkdir()
    sentinel.write_text('Research notes must survive manifest-owned removal.\n', encoding='utf8')
    protected = files(user)
    protected[str(sentinel)] = sha(sentinel)
    if any(sha(Path(p)) != digest for p, digest in installed.items()):
        raise ValueError('Installed package/probe changed during use')
    result = {'state': 'install_and_use_passed', 'scope': 'local Windows fresh external per-user prefix; not a fresh VM',
              'work': str(work), 'native': str(native), 'os': platform.platform(),
              'composition': receipt['composition'], 'source_inputs_sha256': receipt['source_inputs_sha256'],
              'archives': receipt['stages'], 'dependency_companion': companion,
              'canonical_inventory_sha256': sha(manifest), 'runtime_PATH': env['PATH'],
              'python_environment': 'removed', 'engine_builds': 0, 'package_members': package_members,
              'validation_probe': {'sha256': sha(probe),
                                   'source_sha256': sha(Path(__file__).with_name('desktop_delivery_probe.cpp')),
                                   'shipped': False},
              'native_result': result, 'export_sha256': {n: sha(result_dir/n) for n in ('genes.csv', 'replicates.csv')},
              'installed_files': installed, 'protected_user_files': protected,
              'install_run_verified': True, 'uninstall_verified': False}
    (report/'handoff.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')
    print('Install/use passed; manifest-owned removal remains required.', flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('release', 'dependencies', 'probe', 'reference', 'work', 'report'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    check_handoff(**vars(args))


if __name__ == '__main__':
    main()
