"""Development-only controller for native NGFF interoperability prerequisites.

The authored store module is a transport test, not an NGFF format provider.
Native executables own parsing, validation, exact labels and project behavior.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binaries', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--cmake', type=Path, required=True)
    parser.add_argument('--png-manifest', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    binaries = args.binaries.resolve()
    env = {k: v for k, v in os.environ.items()
           if not k.upper().startswith(('PYTHON', 'VIRTUAL_ENV', 'CONDA'))}
    windows = Path(os.environ['SystemRoot'])
    env['PATH'] = os.pathsep.join(map(str, [args.sdk.resolve() / 'bin',
                                          windows / 'System32', windows]))
    results = {'schema': 'qpx.ngff-foundation-checks/1', 'passed': False,
               'full_ngff_delivered': False, 'goal_complete': False,
               'binaries': {}, 'checks': {}}

    def save():
        (work / 'results.json').write_text(json.dumps(results, indent=2) + '\n',
                                           encoding='utf8')

    def run(name, command):
        cwd = work / name
        cwd.mkdir()
        command = list(map(str, command))
        result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                                encoding='utf8', errors='strict', timeout=90)
        row = {'argv': command, 'returncode': result.returncode,
               'stdout': result.stdout, 'stderr': result.stderr}
        results['checks'][name] = row
        (cwd / 'stdout.log').write_text(result.stdout, encoding='utf8')
        (cwd / 'stderr.log').write_text(result.stderr, encoding='utf8')
        save()
        if result.returncode != 0:
            raise RuntimeError(f'{name} failed ({result.returncode}); see {cwd}')
        print(name + ': passed', flush=True)

    def exe(name):
        path = binaries / ('ngff_' + name + '.exe')
        results['binaries'][str(path)] = sha(path)
        return path

    manifests = {}
    for kind in ('good', 'missing', 'bad'):
        directory = work / ('module-' + kind)
        directory.mkdir()
        module = directory / ('ngff_store_' + kind + '.dll')
        shutil.copy2(binaries / module.name, module)
        results['binaries'][str(module)] = sha(module)
        definition = {'schema': 'qpx.reader-plugins/1', 'plugins': [{
            'provider_id': 'org.qupathxx.test-store',
            'display_name': 'Authored directory-store fixture', 'version': '1.0.0',
            'contract': 'authored-store-uint64/1', 'abi': 1,
            'profile': 'windows-x64-msvc1944-release-md',
            'capabilities': {'display_rgba': True, 'raw_samples': True,
                             'source_store': True},
            'module': {'path': module.name, 'sha256': sha(module)},
            'dependencies': []}]}
        manifest = directory / 'reader-plugins.json'
        manifest.write_text(json.dumps(definition, indent=2) + '\n', encoding='utf8')
        manifests[kind] = manifest
        if kind == 'good':
            conflict = copy.deepcopy(definition)
            conflict['plugins'][0]['capabilities']['source_files'] = True
            manifests['conflict'] = directory / 'conflicting-plugins.json'
            manifests['conflict'].write_text(json.dumps(conflict, indent=2) + '\n',
                                             encoding='utf8')

    run('c-header', [exe('reader_c_header_tests')])
    run('exact-label-codes', [exe('label_code_tests')])
    run('directory-on', [exe('directory_store_tests'), work / 'store-on'])
    run('directory-off', [exe('directory_store_offline_tests'), work / 'store-off'])
    run('bounded-open-off', [exe('bounded_offline_tests')])
    run('saved-reader-off', [exe('reader_plugin_offline_offline_tests')])
    run('store-validate', [exe('reader_store_tests'), 'validate', manifests['good'],
                           work / 'store-workflow'])
    for kind in ('missing', 'bad', 'conflict'):
        run('store-refuse-' + kind, [exe('reader_store_tests'), 'refused', manifests[kind]])
    run('store-provider-unavailable', [exe('reader_store_tests'), 'unavailable',
                                      work / 'store-workflow/saved.qpproj'])
    run('legacy-reader-boundary', [exe('reader_plugin_tests')])
    run('legacy-loader-cases', [args.cmake.resolve(),
                               '-DTEST=' + str(exe('reader_plugin_tests')),
                               '-DWORK=' + str(work / 'loader-fixtures'), '-P',
                               root / 'tools/ci/check_reader_plugin_cases.cmake'])
    run('legacy-project-cases', [args.cmake.resolve(),
                                '-DTEST=' + str(exe('reader_plugin_project_tests')),
                                '-DWORK=' + str(work / 'project-fixtures'), '-P',
                                root / 'tools/ci/check_reader_plugin_project.cmake'])
    for name in ('label_analysis', 'label_workflow', 'label_cli'):
        run('legacy-' + name, [exe(name + '_tests')])
    run('png-unavailable', [exe('label_mask_plugin_tests')])
    run('png-configured', [exe('label_mask_plugin_tests'), '--reader-plugins',
                            args.png_manifest.resolve()])
    results['native_only_environment'] = True
    results['passed'] = True
    save()
    print('Native foundation checks passed; NGFF workflow remains in progress.', flush=True)


if __name__ == '__main__':
    main()
