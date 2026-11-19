"""Exercise one admitted AnnData Zarr package and remove its owned files."""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess

from check_loom_delivery import checked_path, member, snapshot
from check_anndata_zarr import digest, load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('stage', 'admission', 'work', 'reference', 'cache-binaries'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--producer-fixtures', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    stage, work = checked_path(args.stage), checked_path(args.work)
    installation = checked_path(work / 'installation')
    assert work.is_relative_to(root / 'build') and installation.parent == work
    assert not work.exists() and not stage.is_relative_to(work) and not work.is_relative_to(stage)
    admission = load(args.admission)
    expected = admission['members']
    assert admission['admission_exit'] == 0 and set(load(stage / 'PACKAGE.json')['owned_files']) == set(expected)
    before, _ = snapshot(stage)
    assert before == {name: row['sha256'] for name, row in expected.items()}
    work.mkdir()
    shutil.copytree(stage, installation)

    windows = Path(os.environ['SYSTEMROOT'])
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PYTHON', 'VIRTUAL_ENV', 'CONDA'))}
    env['PATH'] = os.pathsep.join(map(str, (windows / 'System32', windows)))
    report = {'schema': 'qpx.anndata-zarr-deployment/1', 'passed': False, 'checks': {},
              'installed_members': len(expected), 'admission_sha256': digest(args.admission)}

    def save():
        (work / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, *argv, expected_exit=0):
        command = list(map(str, argv))
        p = subprocess.run(command, cwd=work, env=env, capture_output=True, encoding='utf-8', errors='replace', timeout=120)
        report['checks'][name] = dict(argv=command, returncode=p.returncode, expected_returncode=expected_exit,
                                      stdout=p.stdout, stderr=p.stderr)
        save()
        assert p.returncode == expected_exit, (name, p.stderr)
        return p.stdout

    reference = load(args.reference)
    reference_root = checked_path(args.reference).parent
    assert not work.is_relative_to(reference_root) and not reference_root.is_relative_to(work)
    original_files, original_members = snapshot(reference_root)
    runtime = installation / 'bin'
    cli, sdk = runtime / 'qupathxx_format_import.exe', runtime / 'anndata_zarr_consumer.exe'
    manifest = runtime / 'anndata-zarr-format-plugins.json'
    prefix = (cli, '--format-plugins', manifest)
    provider = 'org.qupathxx.anndata-zarr'
    outputs = installation / 'research'
    outputs.mkdir()
    (outputs / 'user-notes.txt').write_text('Preserve this user file.\n', encoding='utf-8')
    catalog = json.loads(run('installed-catalog', *prefix, 'catalog'))
    assert [row['provider_id'] for row in catalog['providers']] == [provider] and not catalog['diagnostics']
    refs = {}
    for kind in ('counts', 'continuous'):
        ref = reference[kind]
        qmx, sdk_qmx, project = (outputs / f'{kind}{ext}' for ext in ('.qmx', '-sdk.qmx', '.qpx'))
        import_args = (*prefix, 'matrix-store-index', provider, ref['input'], reference['primary_key'],
                       reference['positions'], reference['image'], qmx, '--plugin-options-json', ref['options_json'])
        row = json.loads(run(f'installed-{kind}', *import_args))
        for key in ('file_sha256', 'source_sha256', 'store_observed_sha256', 'store_content_sha256'):
            assert row[key] == ref[key]
        assert digest(qmx) == ref['file_sha256']
        run(f'installed-{kind}-existing', *import_args, expected_exit=2)
        assert digest(qmx) == ref['file_sha256']
        sdk_report = run(f'installed-sdk-{kind}', sdk, manifest, reference['input_project'], reference['image_id'],
                         ref['input'], reference['primary_key'], reference['positions'], sdk_qmx, ref['options_json'], project)
        values = dict(row.split(' ', 1) for row in sdk_report.splitlines())
        managed = outputs / '.qpx-expression' / (ref['file_sha256'] + '.qmx')
        assert digest(sdk_qmx) == digest(managed) == ref['file_sha256'] and project.is_file()
        assert values['owned_source_sha256'] == ref['source_sha256']
        refs[kind] = (project, values['association_id'])

    producer = args.producer_fixtures.resolve()
    producer_report = load(producer / 'producer-report.json')['fixtures']
    # Keeping a new QMX also writes ownership beside the input project. Use a
    # per-deployment copy so qualification fixtures remain read-only.
    private_input = work / 'producer-input-project'
    private_input.mkdir()
    input_project = Path(reference['input_project'])
    input_image = Path(reference['image'])
    shutil.copyfile(input_project, private_input / input_project.name)
    shutil.copyfile(input_image, private_input / input_image.name)
    producer_refs = {}
    for name, dimensions, profile in [
            ('default-blosc', ['4', '3', '12'], 'external-dense-store/1'),
            ('moderate-csr', ['1024', '1024', '1024'], 'external-sparse-store/1')]:
        store = work / f'{name}.zarr'
        shutil.copytree(producer / store.name, store)
        positions = work / f'{name}-positions.tsv'
        with positions.open('w', encoding='utf-8', newline='') as stream:
            writer = csv.writer(stream, delimiter='\t', lineterminator='\n')
            writer.writerow(('observation_id', 'x_px', 'y_px'))
            for index, label in enumerate(producer_report[name]['expected']['observations']):
                writer.writerow((label, index % 32, index // 32))
        options = json.dumps(producer_report[name]['options'])
        qmx, project = outputs / f'{name}.qmx', outputs / f'{name}.qpx'
        row = run(f'installed-sdk-{name}', sdk, manifest, private_input / input_project.name, reference['image_id'],
                  store, '.zattrs', positions, qmx, options, project)
        imported = dict(line.split(' ', 1) for line in row.splitlines())
        assert digest(qmx) == imported['owned_file_sha256']
        assert json.loads(imported['receipt'])['provider']['version'] == '1.2.0'
        assert [imported['observations'], imported['features'], imported['entries']] == dimensions
        assert imported['input_profile'] == profile
        store.rename(work / f'{name}-input-away')
        positions.rename(work / f'{name}-positions-away.tsv')
        producer_refs[name] = imported

    files, _ = snapshot(installation)
    assert snapshot(reference_root) == (original_files, original_members)
    unowned = {name: sha for name, sha in files.items() if name not in expected}
    deletion = [member(installation, name) for name in expected]
    assert all(p.is_relative_to(installation) and p.is_file() and digest(p) == expected[p.relative_to(installation).as_posix()]['sha256'] for p in deletion)
    assert len(unowned) == 15
    process_env = dict(env, QPX_ANNDATA_TEST_INSTALL=str(installation))
    pwsh = windows / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    query = "$ErrorActionPreference='Stop'; $p=$env:QPX_ANNDATA_TEST_INSTALL + '\\'; @(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($p,[StringComparison]::OrdinalIgnoreCase) }).Count"
    active = subprocess.check_output([str(pwsh), '-NoProfile', '-NonInteractive', '-Command', query], env=process_env, text=True, timeout=30)
    assert active.strip() == '0'
    for path in deletion:
        path.unlink()
    assert snapshot(installation)[0] == unowned
    assert snapshot(reference_root) == (original_files, original_members)
    for kind, (project, association) in refs.items():
        ref = reference[kind]
        row = run(f'retained-project-{kind}', args.cache_binaries / sdk.name, '--inspect', project,
                  reference['image_id'], association, ref['file_sha256'], ref['source_sha256'],
                  ref['reader_receipt_sha256'], reference['frame_sha256'])
        fields = dict(line.split(' ', 1) for line in row.splitlines())
        assert fields['owned_source_sha256'] == reference[kind]['source_sha256']
    for name, imported in producer_refs.items():
        row = run(f'retained-project-{name}', args.cache_binaries / sdk.name, '--inspect',
                  outputs / f'{name}.qpx', reference['image_id'], imported['association_id'],
                  imported['owned_file_sha256'], imported['owned_source_sha256'], imported['receipt_sha256'], imported['frame_sha256'])
        assert dict(line.split(' ', 1) for line in row.splitlines())['receipt'] == imported['receipt']
    assert snapshot(installation)[0] == unowned
    report.update(passed=True, removed_members=len(deletion), preserved_files=unowned)
    save()
    print(f'Removed {len(deletion)} exact owned members and preserved {len(unowned)} user files')


if __name__ == '__main__':
    main()
