"""Development-only native Loom/ABI2/QMX and thin-consumer orchestration.

Scientific fixtures, numerical oracles, geometry, ownership and persistence are
native. This controller records processes, exact products and native reports.
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


def load(path):
    return json.loads(path.read_text(encoding='utf8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binaries', 'work', 'legacy-manifest', 'legacy-fixtures', 'legacy-baseline'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--reuse-native-work', type=Path,
                        help='Reuse only seven passing native checks after exact input/product verification')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binaries, work = args.binaries.resolve(), args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    windows = Path(os.environ['SystemRoot'])
    env = {k: v for k, v in os.environ.items()
           if not k.upper().startswith(('PYTHON', 'VIRTUAL_ENV', 'CONDA'))}
    env['PATH'] = os.pathsep.join(map(str, (windows / 'System32', windows)))
    result = {'schema': 'qpx.loom-native-checks/1', 'passed': False, 'goal_complete': False,
              'checks': {}, 'binaries': {}, 'native_only_environment': True}

    def save():
        (work / 'results.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

    def exe(name):
        path = binaries / (name + '.exe')
        result['binaries'][str(path)] = sha(path)
        return path

    def run(name, command, expected=0, parse=False):
        directory = work / 'commands' / name
        directory.mkdir(parents=True)
        command = list(map(str, command))
        process = subprocess.run(command, cwd=directory, env=env, capture_output=True,
                                 encoding='utf8', errors='strict', timeout=180)
        row = {'argv': command, 'returncode': process.returncode,
               'stdout': process.stdout, 'stderr': process.stderr}
        result['checks'][name] = row
        (directory / 'stdout.log').write_text(process.stdout, encoding='utf8')
        (directory / 'stderr.log').write_text(process.stderr, encoding='utf8')
        save()
        assert process.returncode == expected, (name, row)
        print(name + ': passed', flush=True)
        return json.loads(process.stdout) if parse else row

    manifest = binaries / 'loom-format-plugins.json'
    result['provider_manifest'] = {'path': str(manifest), 'sha256': sha(manifest)}
    provider = load(manifest)['plugins'][0]
    assert provider['abi'] == 2 and provider['provider_id'] == 'org.qupathxx.loom'
    module = binaries / provider['module']['path']
    assert sha(module) == provider['module']['sha256']
    result['binaries'][str(module)] = sha(module)
    cli = exe('qupathxx_format_import')
    if args.reuse_native_work:
        previous_work = args.reuse_native_work.resolve()
        previous = load(previous_work / 'results.json')
        names = ['c-header', 'catalog', 'actual-workflow', 'provider-free-metadata',
                 'provider-free-import', 'loader-off-metadata', 'loader-off-import']
        assert all(previous['checks'][name]['returncode'] == 0 for name in names)
        assert previous['provider_manifest'] == result['provider_manifest']
        for name, digest in previous['binaries'].items():
            assert sha(Path(name)) == digest, name
        result['reused_native_checks'] = {'receipt': str(previous_work / 'results.json'),
            'sha256': sha(previous_work / 'results.json'), 'checks': names,
            'verified_binaries': previous['binaries'],
            'scope': 'Explicitly passed checks before a later controller manifest-setup failure; not full prior-controller acceptance'}
        fixtures = previous_work / 'fixtures'
    else:
        run('c-header', [exe('loom_c_header_tests')])
        catalog = run('catalog', [cli, '--format-plugins', manifest, 'catalog'], parse=True)
        assert not catalog['diagnostics'] and len(catalog['providers']) == 1
        assert catalog['providers'][0]['abi'] == 2
        fixtures = work / 'fixtures'
        run('actual-workflow', [exe('loom_tests'), 'validate', manifest, fixtures])
        for name, binary, mode in [('provider-free-metadata', 'loom_tests', 'inspect'),
                                   ('provider-free-import', 'loom_tests', 'unavailable'),
                                   ('loader-off-metadata', 'loom_offline_tests', 'inspect'),
                                   ('loader-off-import', 'loom_offline_tests', 'unavailable')]:
            run(name, [exe(binary), mode, fixtures])

    control_module = binaries / 'loom_matrix_control.dll'
    result['binaries'][str(control_module)] = sha(control_module)
    control_package = work / 'controlled-provider'
    control_package.mkdir()
    shutil.copy2(control_module, control_package / control_module.name)
    shutil.copy2(module, control_package / module.name)
    controlled = {'provider_id': 'org.qupathxx.test.typed-matrix', 'format_id': 'typed-test',
        'domain': 'matrix', 'display_name': 'Typed matrix boundary control', 'version': '1.0.0',
        'contract': 'test-typed-matrix/1', 'abi': 2, 'profile': provider['profile'],
        'module': {'path': control_module.name, 'sha256': sha(control_module)}, 'dependencies': []}
    control_manifest = control_package / 'controlled-format-plugins.json'
    control_manifest.write_text(json.dumps({'schema': 'qpx.format-plugins/1', 'plugins': [controlled]}, indent=2) + '\n', encoding='utf8')
    run('typed-controls', [exe('loom_tests'), 'control', control_manifest, work / 'controls'])

    # Exercise manifest-default ambiguity before DOM normalization. Both entries
    # have valid modules; only the ordinary duplicate-key ABI2 entry must refuse.
    bad = json.dumps(dict(controlled, options={'mode': 'valid-counts'}))
    bad = bad.replace('"mode": "valid-counts"', '"mode": "valid-counts", "mo\\u0064e": "valid-counts"')
    coexist = control_package / 'default-options-duplicate.json'
    absolute_provider = copy.deepcopy(provider)
    coexist.write_text('{"schema":"qpx.format-plugins/1","plugins":[' + bad + ',' + json.dumps(absolute_provider) + ']}\n', encoding='utf8')
    catalog = run('duplicate-default-options', [cli, '--format-plugins', coexist, 'catalog'], parse=True)
    assert [p['provider_id'] for p in catalog['providers']] == ['org.qupathxx.loom']
    assert len(catalog['diagnostics']) == 1 and 'duplicate' in catalog['diagnostics'][0].lower()

    baseline = args.legacy_baseline.resolve()
    run('legacy-schema4-exact', [exe('loom_tests'), 'legacy', args.legacy_manifest.resolve(),
        baseline / 'counts.tsv', baseline / 'positions.tsv', baseline / 'baseline.qmx', work / 'legacy.qmx'])
    assert sha(work / 'legacy.qmx') == load(baseline / 'results.json')['qmx_sha256']
    legacy_source = args.legacy_fixtures.resolve()
    legacy_copy = work / 'legacy-fixtures'
    assert legacy_source.is_relative_to(root)
    protected_legacy = {str(p): sha(p) for p in legacy_source.rglob('*') if p.is_file()}
    shutil.copytree(legacy_source, legacy_copy)
    old_manifest = args.legacy_manifest.resolve()
    old = load(old_manifest)['plugins']
    old_modules = {p['domain']: (old_manifest.parent / p['module']['path']).resolve() for p in old}
    run('legacy-abi', [exe('loom_legacy_host_tests'), '--abi', old_modules['matrix'], old_modules['annotation'], legacy_copy])
    for mode in ('catalog', 'matrix-control', 'receipt', 'source-change', 'cancel', 'ranges', 'annotation-control'):
        run('legacy-host-' + mode, [exe('loom_legacy_host_tests'), '--host-case', mode, old_manifest, legacy_copy])
    for mode in ('matrix', 'annotation', 'annotation-stale', 'annotation-cancel', 'positions', 'limits', 'legacy', 'native-id-limits'):
        output = work / ('legacy-import-' + mode)
        output.mkdir()
        run('legacy-import-' + mode, [exe('loom_legacy_import_tests'), mode, old_manifest, legacy_copy, output])
    assert all(sha(Path(name)) == digest for name, digest in protected_legacy.items())
    result['legacy'] = {'original_fixture_files_unchanged': len(protected_legacy),
                        'schema4_qmx_sha256': sha(work / 'legacy.qmx')}

    reference = load(fixtures / 'native-reference.json')
    consumer = exe('loom_matrix_consumer')
    prefix = [cli, '--format-plugins', manifest]
    protected = {str(fixtures / n): sha(fixtures / n) for n in
                 ('counts.loom', 'continuous.loom', 'positions.tsv', 'image.ppm', 'input.qpx')}
    exports = {}
    for kind in ('counts', 'continuous'):
        ref = reference[kind]
        output = work / ('cli-' + kind + '.qmx')
        command = [*prefix, 'matrix-index', 'org.qupathxx.loom', ref['input'], reference['positions'],
                   reference['image'], output, '--plugin-options-json', ref['options_json']]
        row = run('cli-' + kind, command, parse=True)
        assert output.read_bytes() == Path(ref['path']).read_bytes(), kind
        for field in ('file_sha256', 'source_sha256', 'features', 'observations', 'entries', 'domain', 'value_domain', 'input_profile'):
            assert row[field] == ref[field], (kind, field)
        assert row['frame_sha256'] == reference['frame_sha256']
        assert row['receipt'] == json.loads(ref['reader_receipt_json'])
        info = run('cli-info-' + kind, [cli, 'matrix-info', output], parse=True)
        assert info['source_sha256'] == ref['source_sha256'] and info['value_domain'] == ref['value_domain']
        run('cli-existing-' + kind, command, expected=2)
        assert sha(output) == ref['file_sha256']
        sdk_qmx, sdk_project = work / ('sdk-' + kind + '.qmx'), work / ('sdk-' + kind + '.qpx')
        sdk = run('sdk-' + kind, [consumer, manifest, reference['input_project'], reference['image_id'],
            ref['input'], reference['positions'], sdk_qmx, ref['options_json'], sdk_project])
        assert sdk_qmx.read_bytes() == output.read_bytes() and sdk_project.is_file()
        assert ref['source_sha256'] in sdk['stdout'] and ref['file_sha256'] in sdk['stdout']
        exports[kind] = {'file_sha256': sha(output), 'source_sha256': row['source_sha256'],
                         'reader_receipt': row['receipt'], 'sdk_project_sha256': sha(sdk_project)}
    assert all(sha(Path(name)) == digest for name, digest in protected.items())
    result['consumers'] = exports
    result['native_reference_sha256'] = sha(fixtures / 'native-reference.json')
    result['protected_inputs'] = protected
    result['passed'] = True
    save()
    print(json.dumps({'passed': True, 'checks': len(result['checks']), 'goal_complete': False}))


if __name__ == '__main__':
    main()
