"""Check the native AnnData Zarr import and project routes on Windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import threading
import time


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binaries', 'work', 'legacy-manifest', 'loom-manifest'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--producer-fixtures', type=Path)
    parser.add_argument('--legacy-store-manifest', type=Path, required=True)
    parser.add_argument('--resume-producer', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    loom = root / 'build/gui-aggregation/catalogue-closure/loom'
    work, binaries = args.work.resolve(), args.binaries.resolve()
    if not args.resume_producer:
        work.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PYTHON', 'VIRTUAL_ENV', 'CONDA'))}
    windows = Path(os.environ['SYSTEMROOT'])
    env['PATH'] = os.pathsep.join(map(str, (windows / 'System32', windows)))
    report = {'schema': 'qpx.anndata-zarr-native-checks/1', 'passed': False,
              'native_only_environment': True, 'checks': {}, 'binaries': {}}
    if args.resume_producer:
        report = load(work / 'results.json')
        if not args.producer_fixtures or not report.get('native_passed') or report['passed']:
            raise ValueError('Resume requires a passing native phase and pending producer checks')
        assert all(digest(path) == sha for path, sha in report['binaries'].items())
        assert digest(report['provider_manifest']['path']) == report['provider_manifest']['sha256']

    def save():
        (work / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, exe, *argv, expected=0, sample_disk=None):
        exe = binaries / (exe + '.exe')
        report['binaries'][str(exe)] = digest(exe)
        command = [str(exe), *map(str, argv)]
        done = threading.Event()
        sampled = []

        def observe():
            while not done.is_set():
                sizes = []
                for p in sample_disk.rglob('*'):
                    try:
                        if p.is_file():
                            sizes.append(p.stat().st_size)
                    except FileNotFoundError:
                        pass
                sampled.append(sum(sizes))
                done.wait(.05)

        began = time.perf_counter()
        sampler = threading.Thread(target=observe) if sample_disk else None
        if sampler:
            sampler.start()
        try:
            result = subprocess.run(command, cwd=work, env=env, capture_output=True,
                                    encoding='utf-8', errors='replace', timeout=180)
        finally:
            done.set()
            if sampler:
                sampler.join()
        report['checks'][name] = dict(argv=command, returncode=result.returncode,
            expected_returncode=expected, stdout=result.stdout, stderr=result.stderr,
            elapsed_seconds=time.perf_counter()-began)
        if sampler:
            report['checks'][name]['peak_sampled_output_bytes'] = max(sampled, default=0)
        save()
        if result.returncode != expected:
            raise RuntimeError(f'{name}: expected exit {expected}, got {result.returncode}: {result.stderr}')
        print(f'{name}: passed', flush=True)
        return result.stdout

    cli, tests = 'qupathxx_format_import', 'anndata_zarr_tests'
    manifest = binaries / 'anndata-zarr-format-plugins.json'
    report['provider_manifest'] = dict(path=str(manifest), sha256=digest(manifest))
    provider, = load(manifest)['plugins']
    module = binaries / provider['module']['path']
    assert digest(module) == provider['module']['sha256'] and provider['abi'] == 4
    report['binaries'][str(module)] = digest(module)
    prefix = ('--format-plugins', manifest)
    fixtures = work / 'fixtures'
    if args.resume_producer:
        reference_path = fixtures / 'native-reference.json'
        assert digest(reference_path) == report['native_reference_sha256']
        producer_checks(args.producer_fixtures.resolve(), work, prefix, cli, load(reference_path), run, report)
        report['passed'] = True
        save()
        return
    run('c-header', 'anndata_c_header_tests')
    catalog = json.loads(run('catalog', cli, *prefix, 'catalog'))
    assert not catalog['diagnostics'] and len(catalog['providers']) == 1
    assert catalog['providers'][0]['module_sha256'] == digest(module)
    run('actual-workflow', tests, 'validate', manifest, fixtures)
    legacy_store_path = root / 'build/gui-aggregation/catalogue-closure/anndata-zarr/integration-blosc-01/results.json'
    legacy_store = load(legacy_store_path)
    legacy_store_command = legacy_store['checks']['upstream-default-blosc']['argv']
    legacy_store_info = json.loads(legacy_store['checks']['upstream-default-blosc']['stdout'])
    # Re-import the original paths because the persisted observation binds the
    # directory location and file versions. Keep the prior package and QMX intact.
    legacy_output = work / 'previous-abi3-store.qmx'
    run('previous-abi3-store-exact', cli, '--format-plugins', args.legacy_store_manifest.resolve(),
        *legacy_store_command[3:9], legacy_output, *legacy_store_command[10:])
    assert digest(legacy_output) == legacy_store_info['file_sha256']
    for prefix_name, executable in [('provider-free', tests), ('loader-off', 'anndata_zarr_offline_tests')]:
        run(prefix_name + '-metadata', executable, 'inspect', fixtures)
        run(prefix_name + '-import', executable, 'unavailable', fixtures)

    control = binaries / 'anndata_matrix_control.dll'
    report['binaries'][str(control)] = digest(control)
    control_dir = work / 'control-package'
    control_dir.mkdir()
    shutil.copy2(control, control_dir / control.name)
    control_manifest = control_dir / 'manifest.json'
    control_manifest.write_text(json.dumps({'schema': 'qpx.format-plugins/1', 'plugins': [{
        'provider_id': 'org.qupathxx.test.matrix-store', 'format_id': 'store-test', 'domain': 'matrix',
        'display_name': 'Matrix directory-store boundary control', 'version': '1.0.0',
        'contract': 'test-matrix-store/1', 'abi': 3, 'profile': provider['profile'],
        'module': {'path': control.name, 'sha256': digest(control)}, 'dependencies': []}]}), encoding='utf-8')
    run('store-controls', tests, 'control', control_manifest, work / 'controls')

    baseline = loom / 'legacy-baseline'
    run('legacy-schema4-exact', 'anndata_loom_tests', 'legacy', args.legacy_manifest.resolve(),
        baseline / 'counts.tsv', baseline / 'positions.tsv', baseline / 'baseline.qmx', work / 'legacy.qmx')
    old = load(loom / 'integration-03/fixtures/native-reference.json')
    for kind in ('counts', 'continuous'):
        ref = old[kind]
        output = work / (kind + '-schema5.qmx')
        run('legacy-schema5-exact' if kind == 'counts' else 'legacy-schema5-continuous', cli,
            '--format-plugins', args.loom_manifest.resolve(), 'matrix-index', 'org.qupathxx.loom',
            ref['input'], old['positions'], old['image'], output, '--plugin-options-json', ref['options_json'])
        assert digest(output) == ref['file_sha256']

    reference_path = fixtures / 'native-reference.json'
    reference = load(reference_path)
    report['native_reference_sha256'] = digest(reference_path)
    ref = reference['counts']
    input_store = Path(ref['input'])
    forbidden_project = input_store / 'forbidden-project.qpx'
    destination = work / 'must-not-publish.qmx'
    before = {p.relative_to(input_store): digest(p) for p in input_store.rglob('*') if p.is_file()}
    run('sdk-reject-project-inside-store', 'anndata_zarr_consumer', manifest, reference['input_project'],
        reference['image_id'], input_store, reference['primary_key'], reference['positions'],
        destination, ref['options_json'], forbidden_project, expected=2)
    assert 'protected input directory' in report['checks']['sdk-reject-project-inside-store']['stderr']
    assert not destination.exists() and not forbidden_project.exists()
    assert before == {p.relative_to(input_store): digest(p) for p in input_store.rglob('*') if p.is_file()}
    sandbox_store = work / 'input-project-contained.zarr'
    shutil.copytree(input_store, sandbox_store)
    nested_project = sandbox_store / 'input.qpx'
    shutil.copyfile(reference['input_project'], nested_project)
    run('sdk-reject-input-managed-inside-store', 'anndata_zarr_consumer', manifest, nested_project,
        reference['image_id'], sandbox_store, reference['primary_key'], reference['positions'],
        destination, ref['options_json'], work / 'must-not-publish.qpx', expected=2)
    assert 'protected input directory' in report['checks']['sdk-reject-input-managed-inside-store']['stderr']
    assert not destination.exists() and not (sandbox_store / '.qpx-expression').exists()
    for kind in ('counts', 'continuous'):
        ref = reference[kind]
        output = work / (kind + '.qmx')
        index_args = (*prefix, 'matrix-store-index', provider['provider_id'], ref['input'],
            reference['primary_key'], reference['positions'], reference['image'], output,
            '--plugin-options-json', ref['options_json'])
        index = json.loads(run('cli-' + kind, cli, *index_args))
        info = json.loads(run(f'cli-{kind}-info', cli, 'matrix-info', output))
        for observation in (index, info):
            for field in ('file_sha256', 'source_sha256', 'features', 'observations', 'entries', 'domain',
                          'value_domain', 'input_profile', 'positions_sha256', 'store_content_sha256',
                          'store_observed_sha256', 'store_files', 'store_bytes'):
                assert observation[field] == ref[field], (kind, field)
            assert observation['receipt'] == json.loads(ref['reader_receipt_json'])
            assert observation['frame_sha256'] == reference['frame_sha256']
        assert digest(output) == ref['file_sha256']
        run(f'cli-{kind}-existing', cli, *index_args, expected=2)
        assert digest(output) == ref['file_sha256']
        output_qmx, project = work / f'sdk-{kind}.qmx', work / f'sdk-{kind}.qpx'
        sdk = run('sdk-' + kind, 'anndata_zarr_consumer', manifest, reference['input_project'],
            reference['image_id'], ref['input'], reference['primary_key'], reference['positions'],
            output_qmx, ref['options_json'], project)
        fields = dict(row.split(' ', 1) for row in sdk.splitlines())
        assert digest(output_qmx) == ref['file_sha256'] and project.is_file()
        assert fields['owned_file_sha256'] == ref['file_sha256']
        assert fields['owned_source_sha256'] == ref['source_sha256']
        assert fields['frame_sha256'] == reference['frame_sha256']
        assert json.loads(fields['receipt']) == json.loads(ref['reader_receipt_json'])
        inspected = run('sdk-inspect-' + kind, 'anndata_zarr_consumer', '--inspect', project,
                        reference['image_id'], fields['association_id'], ref['file_sha256'],
                        ref['source_sha256'], ref['reader_receipt_sha256'], reference['frame_sha256'])
        reopened = dict(row.split(' ', 1) for row in inspected.splitlines())
        assert reopened['owned_file_sha256'] == ref['file_sha256']
        assert reopened['owned_source_sha256'] == ref['source_sha256']
    report['native_passed'] = True
    save()
    if args.producer_fixtures:
        producer_checks(args.producer_fixtures.resolve(), work, prefix, cli, reference, run, report)
        report['passed'] = True
    save()


def producer_checks(fixtures, work, prefix, cli, native_reference, run, report):
    image = native_reference['image']
    import_args = ('matrix-store-index', 'org.qupathxx.anndata-zarr')
    opts = dict(matrix='X', observation_ids='obs/_index', feature_ids='var/_index',
                domain='counts', value_domain='raw-counts')
    snapshots = {p.relative_to(fixtures).as_posix(): digest(p) for p in fixtures.rglob('*') if p.is_file()}
    producer_receipt = fixtures / 'producer-report.json'
    report['producer_reference'] = {'path': str(producer_receipt), 'sha256': digest(producer_receipt)}

    def check(name, store, positions, options, output, expected=0, sample_disk=None):
        return run(name, cli, *prefix, *import_args, fixtures / store, '.zattrs', positions,
                   image, output, '--plugin-options-json', json.dumps(options), expected=expected,
                   sample_disk=sample_disk)

    tiny_positions = work / 'upstream-positions.tsv'
    def positions(path, ids):
        path.write_text('observation_id\tx_px\ty_px\n' + ''.join(
            f'{value}\t{i % 32}\t{i // 32}\n' for i, value in enumerate(ids)), encoding='utf-8')

    observations = ['spot-α', 'spot-B', 'spot-C', 'spot-D']
    positions(tiny_positions, observations)
    tiny = work / 'upstream.qmx'
    check('upstream-zlib', 'tiny-counts.zarr', tiny_positions,
          dict(opts, feature_names='var/feature_name'), tiny)
    with sqlite3.connect(tiny.as_uri() + '?mode=ro', uri=True) as db:
        counts = [int.from_bytes(row[0], 'little') for row in db.execute(
            'SELECT value FROM counts ORDER BY observation,feature')]
        assert counts == [9007199254740993, 0, 18446744073709551615, 0, 7, 0, 3, 0, 0, 0, 0, 11]
        assert [value for value, in db.execute('SELECT id FROM observations ORDER BY idx')] == observations
        assert list(db.execute('SELECT id,name FROM features ORDER BY idx')) == [
            ('GENE-A', 'Marker A'), ('GENE-β', 'Marker B'), ('GENE-C', 'Marker C')]

    default = work / 'upstream-default-blosc.qmx'
    check('upstream-default-blosc', 'default-blosc.zarr', tiny_positions,
          dict(opts, feature_names='var/feature_name'), default)
    with sqlite3.connect(default.as_uri() + '?mode=ro', uri=True) as db, \
            sqlite3.connect(tiny.as_uri() + '?mode=ro', uri=True) as reference:
        for table, order in [('counts', 'observation,feature'), ('observations', 'idx'), ('features', 'idx')]:
            query = f'SELECT * FROM {table} ORDER BY {order}'
            assert db.execute(query).fetchall() == reference.execute(query).fetchall()

    sdk = 'anndata_zarr_consumer'
    sdk_store = work / 'sdk-blosc.zarr'
    shutil.copytree(fixtures / 'default-blosc.zarr', sdk_store)
    sdk_positions = work / 'sdk-blosc-positions.tsv'
    shutil.copyfile(tiny_positions, sdk_positions)
    sdk_qmx, sdk_project = work / 'sdk-blosc.qmx', work / 'sdk-blosc.qpx'
    image_id = native_reference['image_id']
    fields = dict(line.split(' ', 1) for line in run('sdk-default-blosc', sdk, prefix[1],
        native_reference['input_project'], image_id, sdk_store, '.zattrs', sdk_positions,
        sdk_qmx, json.dumps(dict(opts, feature_names='var/feature_name')), sdk_project).splitlines())
    assert digest(sdk_qmx) == fields['owned_file_sha256']
    cli_result = json.loads(report['checks']['upstream-default-blosc']['stdout'])
    assert json.loads(fields['receipt']) == cli_result['receipt']
    assert fields['frame_sha256'] == cli_result['frame_sha256']
    sdk_store.rename(work / 'sdk-blosc-store-away')
    sdk_positions.rename(work / 'sdk-blosc-positions-away.tsv')
    sdk_qmx.rename(work / 'sdk-blosc-export-away.qmx')
    reopened = dict(line.split(' ', 1) for line in run('sdk-reopen-default-blosc', sdk, '--inspect',
        sdk_project, image_id, fields['association_id'], fields['owned_file_sha256'], fields['owned_source_sha256'],
        fields['receipt_sha256'], fields['frame_sha256']).splitlines())
    assert reopened['owned_file_sha256'] == fields['owned_file_sha256']
    assert reopened['receipt'] == fields['receipt']

    block_positions = work / 'compressed-positions.tsv'
    block_obs = [f'repeated-prefix-spot-{i:03}' for i in range(128)]
    block_features = [f'repeated-prefix-gene-{i:03}' for i in range(64)]
    positions(block_positions, block_obs)
    for codec in ('lz4', 'blosclz'):
        name = f'compressed-{codec}'
        output = work / f'{name}.qmx'
        check(name, f'{name}.zarr', block_positions, opts, output)
        with sqlite3.connect(output.as_uri() + '?mode=ro', uri=True) as db:
            assert [v for v, in db.execute('SELECT id FROM observations ORDER BY idx')] == block_obs
            assert [v for v, in db.execute('SELECT id FROM features ORDER BY idx')] == block_features
            assert [int.from_bytes(v, 'little') for v, in db.execute(
                'SELECT value FROM counts ORDER BY observation,feature')] == [i % 5 for i in range(64)] * 128

    for name, table, explanation in [('truncated-blosc', block_positions, 'blosc'),
                                     ('conflict', tiny_positions, 'consolidated')]:
        path = work / f'{name}.qmx'
        check(name, f'{name}.zarr', table, opts, path, expected=2)
        assert explanation in report['checks'][name]['stderr'].lower() and not path.exists()

    medium = work / 'sparse-workload'
    medium.mkdir()
    medium_positions = work / 'sparse-positions.tsv'
    positions(medium_positions, [f'obs-{i:04}' for i in range(1024)])
    output = medium / 'moderate.qmx'
    medium_store = work / 'csr-source.zarr'
    shutil.copytree(fixtures / 'moderate-csr.zarr', medium_store)
    info = json.loads(check('upstream-moderate-csr', medium_store, medium_positions,
                            opts, output, sample_disk=medium))
    assert info['entries'] == 1024 and info['store_files'] > 64
    assert info['input_profile'] == 'external-sparse-store/1'
    with sqlite3.connect(output.as_uri() + '?mode=ro', uri=True) as db:
        rows = db.execute('SELECT observation,feature,value FROM counts ORDER BY observation')
        assert [(o, f, int.from_bytes(v, 'little')) for o, f, v in rows] == [(i, i, i+1) for i in range(1024)]
        assert [row[0] for row in db.execute('SELECT id FROM observations ORDER BY idx')] == [
            f'obs-{i:04}' for i in range(1024)]
        assert [row[0] for row in db.execute('SELECT id FROM features ORDER BY idx')] == [
            f'feature-{i:04}' for i in range(1024)]
    report['sparse_workload'] = {'logical_entries': 1024 ** 2, 'stored_input_entries': info['entries'],
        'store_files': info['store_files'], 'input_bytes': info['store_bytes'], 'qmx_bytes': output.stat().st_size,
        'managed_copy_bytes_per_distinct_project_folder': output.stat().st_size,
        'import_seconds': report['checks']['upstream-moderate-csr']['elapsed_seconds'],
        'peak_sampled_import_bytes': report['checks']['upstream-moderate-csr']['peak_sampled_output_bytes']}
    assert output.stat().st_size < 2 * 1024 * 1024
    medium_store.rename(work / 'csr-source-away')
    medium_positions.rename(work / 'sparse-positions-away.tsv')
    recovered = json.loads(run('attach-sparse-without-provider', cli, 'matrix-attach',
        native_reference['input_project'], native_reference['image_id'], output,
        info['file_sha256'], work / 'sparse-recovered.qpx'))
    assert recovered['file_sha256'] == info['file_sha256'] and recovered['frame_sha256'] == info['frame_sha256']

    wide = work / 'wide-csc.qmx'
    wide_positions = work / 'wide-positions.tsv'
    wide_positions.write_text('observation_id\tx_px\ty_px\n' + ''.join(
        f'axis-{i:05}\t{i % 32}\t{(i // 32) % 32}\n' for i in range(10240)), encoding='utf-8')
    wide_info = json.loads(check('upstream-wide-csc', 'wide-csc.zarr', wide_positions, opts, wide))
    assert (wide_info['features'], wide_info['observations'], wide_info['entries']) == (10240, 10240, 10240)
    with sqlite3.connect(wide.as_uri() + '?mode=ro', uri=True) as db:
        rows = db.execute('SELECT observation,feature,value FROM counts ORDER BY observation')
        assert [(o, f, int.from_bytes(v, 'little')) for o, f, v in rows] == [(i, i, i+1) for i in range(10240)]
    assert snapshots == {p.relative_to(fixtures).as_posix(): digest(p) for p in fixtures.rglob('*') if p.is_file()}


if __name__ == '__main__':
    main()
