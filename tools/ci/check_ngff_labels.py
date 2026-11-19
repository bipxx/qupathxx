"""Native optional NGFF labels: typed source/frame, workflow and legacy checks.

Development-only orchestration. Fixtures and scientific expectations are owned
by the native test executables; this controller records commands and identities.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binaries', 'work', 'sdk', 'cmake', 'png_manifest', 'old_png_packet'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--standalone-fixtures', type=Path,
                        help='Reuse passing native fixtures for only the missing fresh-process capture routes')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binaries = args.binaries.resolve()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items()
           if not k.upper().startswith(('PYTHON', 'VIRTUAL_ENV', 'CONDA'))}
    windows = Path(os.environ['SystemRoot'])
    env['PATH'] = os.pathsep.join(map(str, [args.sdk.resolve() / 'bin',
                                          windows / 'System32', windows]))
    results = {'schema': 'qpx.ngff-label-checks/1', 'passed': False,
               'goal_complete': False, 'binaries': {}, 'checks': {}}

    def save():
        (work / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf8')

    def exe(name):
        path = binaries / (name + '.exe')
        results['binaries'][str(path)] = sha(path)
        return path

    def run(name, command, expected=0, parse=False, native=True):
        cwd = work / name
        cwd.mkdir()
        command = list(map(str, command))
        process = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                                 encoding='utf8', errors='strict', timeout=180)
        row = {'argv': command, 'returncode': process.returncode,
               'stdout': process.stdout, 'stderr': process.stderr,
               'native_process': native}
        results['checks'][name] = row
        (cwd / 'stdout.log').write_text(process.stdout, encoding='utf8')
        (cwd / 'stderr.log').write_text(process.stderr, encoding='utf8')
        save()
        if process.returncode != expected:
            raise RuntimeError(f'{name} failed ({process.returncode}); see {cwd}')
        print(name + ': passed', flush=True)
        return json.loads(process.stdout) if parse else process

    def standalone_capture(fixtures):
        fixtures = fixtures.resolve()
        manifest = binaries / 'ngff-label-reader-plugins.json'
        prefix = [exe('qupathxx_labels'), '--reader-plugins', manifest]
        protected = [fixtures / 'input.qpx', fixtures / 'source.ome.tif', fixtures / 'cli.qpx']
        mask = fixtures / 'workflow.zarr'
        protected += [p for p in mask.rglob('*') if p.is_file()]
        before = {str(p): sha(p) for p in protected}
        original_members = sorted(str(p.relative_to(mask)) for p in mask.rglob('*'))
        native = run('capture-reference-record', [*prefix, '--list-labels', fixtures / 'cli.qpx',
                     '--image', '1'], parse=True)['records']
        assert len(native) == 1
        input_info = run('capture-target-input', [*prefix, '--label-input', fixtures / 'input.qpx',
                         '--image', '1', '--roi', '1', '--channels', '3', '--z', '1'], parse=True)
        project = work / 'standalone-import.qpx'
        command = [*prefix, '--import-ngff-label-mask', fixtures / 'input.qpx', project,
                   '--image', '1', '--roi', '1', '--mask-directory', mask,
                   '--label-group', 'labels/cells', '--reference-group', '.',
                   '--origin', 'authored', '--author', 'Independent native author',
                   '--input-sha256', input_info['input_sha256'], '--reference-z', '1',
                   '--z', '1', '--channels', '3', '--max-regions', '16']
        captured = run('standalone-capture', command, parse=True)
        assert captured['record'] == native[0] and captured['regions'] == 2
        assert captured['published_objects'] == 2
        run('standalone-capture-verify', [*prefix, '--verify-labels', project,
                                         '--image', '1', '--record', native[0]])
        exports = {}
        for format_name, reference in [('csv', 'native.csv'), ('raster', 'native.qplm')]:
            output = work / ('standalone-import.' + format_name)
            run('standalone-capture-' + format_name, [*prefix, '--export-labels', project,
                output, '--image', '1', '--record', native[0], '--format', format_name])
            assert output.read_bytes() == (fixtures / reference).read_bytes(), format_name
            exports[format_name] = sha(output)
            run('standalone-protected-' + format_name, [*prefix, '--export-labels', project,
                mask / ('forbidden.' + format_name), '--image', '1', '--record', native[0],
                '--format', format_name], expected=1)
        run('standalone-existing-output', command, expected=1)
        refused = list(command)
        refused[refused.index(project)] = mask / 'forbidden.qpx'
        run('standalone-protected-project', refused, expected=1)
        unavailable = run('standalone-capture-missing-provider', [exe('qupathxx_labels'),
            '--verify-labels', project, '--image', '1', '--record', native[0]], expected=1)
        assert 'org.qupathxx.ngff-labels' in unavailable.stderr
        listed = run('standalone-capture-metadata-only', [exe('qupathxx_labels'),
            '--list-labels', project, '--image', '1'], parse=True)
        assert listed['records'] == native
        for filename, digest in before.items():
            assert sha(Path(filename)) == digest, filename
        assert sorted(str(p.relative_to(mask)) for p in mask.rglob('*')) == original_members
        results['standalone_capture'] = {'record': native[0], 'exports': exports,
                                          'protected_inputs': before, 'members_unchanged': True}

    if args.standalone_fixtures:
        results['scope'] = 'Missing fresh-process capture routes over passing native fixtures'
        standalone_capture(args.standalone_fixtures)
        results['native_only_environment'] = True
        results['passed'] = True
        save()
        return

    for name in ('label_mask_frame', 'ngff_label_description', 'label_mask_archive'):
        command = [exe('ngff_' + name + '_tests')]
        if name == 'label_mask_archive':
            command += [work / 'archive-fixtures']
        run(name, command)
    run('reader-description-off', [exe('ngff_reader_label_offline_tests'), 'off'])
    manifests = {}
    for kind in ('good', 'missing', 'bad', 'short'):
        directory = work / ('module-' + kind)
        directory.mkdir()
        module = directory / ('ngff_label_' + kind + '.dll')
        shutil.copy2(binaries / module.name, module)
        results['binaries'][str(module)] = sha(module)
        definition = {'schema': 'qpx.reader-plugins/1', 'plugins': [{
            'provider_id': 'org.qupathxx.test-label-description',
            'display_name': 'Authored label-description fixture', 'version': '1.0.0',
            'contract': 'authored-store-label-description/1', 'abi': 1,
            'profile': 'windows-x64-msvc1944-release-md',
            'capabilities': {'display_rgba': True, 'raw_samples': True,
                             'source_store': True, 'label_description': True},
            'module': {'path': module.name, 'sha256': sha(module)}, 'dependencies': []}]}
        manifest = directory / 'reader-plugins.json'
        manifest.write_text(json.dumps(definition, indent=2) + '\n', encoding='utf8')
        manifests[kind] = manifest
        if kind == 'good':
            for variant, flag, value in [('no-store', 'source_store', False),
                                          ('no-raw', 'raw_samples', False),
                                          ('files-conflict', 'source_files', True)]:
                altered = copy.deepcopy(definition)
                altered['plugins'][0]['capabilities'][flag] = value
                manifests[variant] = directory / (variant + '.json')
                manifests[variant].write_text(json.dumps(altered, indent=2) + '\n', encoding='utf8')
    run('reader-description', [exe('ngff_reader_label_description_tests'), 'validate',
                                manifests['good'], work / 'reader-fixtures'])
    for kind in ('missing', 'bad', 'short', 'no-store', 'no-raw', 'files-conflict'):
        run('reader-refuse-' + kind, [exe('ngff_reader_label_description_tests'),
                                      'refused', manifests[kind]])
    manifest = binaries / 'ngff-label-reader-plugins.json'
    results['provider_manifest'] = {'path': str(manifest), 'sha256': sha(manifest)}
    provider = json.loads(manifest.read_text(encoding='utf8'))['plugins'][0]
    for member in [provider['module'], *provider['dependencies']]:
        path = manifest.parent / member['path']
        assert sha(path) == member['sha256'], path
        results['binaries'][str(path)] = sha(path)
    run('actual-workflow', [exe('ngff_ngff_label_workflow_tests'), 'validate', manifest,
                            work / 'workflow-fixtures'])
    run('provider-unavailable', [exe('ngff_ngff_label_workflow_tests'), 'unavailable',
                                 work / 'workflow-fixtures/saved.qpx'])
    run('standalone-help', [exe('qupathxx_labels'), '--help'])
    catalog = run('standalone-readers', [exe('qupathxx_labels'), '--reader-plugins',
                                        manifest, '--readers'], parse=True)
    assert not catalog['plugin_diagnostics']
    assert any(p['provider_id'] == provider['provider_id'] and p['available']
               and p['module_sha256'] == provider['module']['sha256'] for p in catalog['readers'])
    prefix = [exe('qupathxx_labels'), '--reader-plugins', manifest]
    project = work / 'workflow-fixtures/saved.qpx'
    records = run('standalone-records', [*prefix, '--list-labels', project, '--image', '1'], parse=True)['records']
    for i, record in enumerate(records):
        run(f'standalone-verify-{i}', [*prefix, '--verify-labels', project,
                                      '--image', '1', '--record', record])
        for format_name in ('csv', 'raster'):
            run(f'standalone-export-{i}-{format_name}', [*prefix, '--export-labels', project,
                 work / f'export-{i}.{format_name}', '--image', '1', '--record', record,
                 '--format', format_name])
    standalone_capture(work / 'workflow-fixtures')
    old = args.old_png_packet.resolve()
    legacy = json.loads((old / 'before.json').read_text(encoding='utf8'))['legacy']
    validation = json.loads((old / 'validation.json').read_text(encoding='utf8'))
    png_project = next(p for p in validation['originals'] if p.endswith('saved.qpx'))
    run('retained-tiff-png-identities', [exe('ngff_label_replay_compat_tests'), legacy['project'],
         png_project, args.png_manifest.resolve(), work / 'retained-identity-fixtures'])
    run('foundation-and-legacy', [sys.executable, '-X', 'utf8', root / 'tools/ci/check_ngff_foundations.py',
         '--binaries', binaries, '--work', work / 'foundations', '--sdk', args.sdk.resolve(),
         '--cmake', args.cmake.resolve(), '--png-manifest', args.png_manifest.resolve()], native=False)
    results['native_only_environment'] = True
    results['passed'] = True
    save()
    print('Native NGFF label workflow and affected legacy controls passed.', flush=True)


if __name__ == '__main__':
    main()
