"""Exercise the native attachment retry after an actual project-save failure."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--packet', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    cli, packet, work = (x.resolve() for x in (args.cli, args.packet, args.work))
    work.mkdir(parents=True, exist_ok=False)
    runtime = packet / 'package-final/bin'
    consumer = runtime / 'anndata_zarr_consumer.exe'
    manifest = runtime / 'anndata-zarr-format-plugins.json'
    offline_runtime = work / 'offline-runtime'
    offline_runtime.mkdir()
    for name in (consumer.name, 'geos.dll', 'geos_c.dll', 'libopenslide-1.dll'):
        shutil.copyfile(runtime / name, offline_runtime / name)
        assert sha(offline_runtime / name) == sha(runtime / name)
    inspector = offline_runtime / consumer.name
    assert not (cli.parent / 'qpx_anndata_zarr_plugin.dll').exists()
    reference = json.loads((packet / 'integration-01/fixtures/native-reference.json').read_text(encoding='utf8'))
    report = {'schema': 'qpx.matrix-attachment-checks/1', 'passed': False, 'checks': {},
              'products': {str(p): sha(p) for p in (cli, consumer, manifest)}}
    windows = Path(os.environ['SYSTEMROOT'])
    env = dict(os.environ, PATH=os.pathsep.join(map(str, (windows / 'System32', windows))))
    for name in list(env):
        if name.startswith(('PYTHON', 'QUPATHXX_', 'QPX_')):
            del env[name]

    def save():
        (work / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, *command, expected=0):
        argv = list(map(str, command))
        result = subprocess.run(argv, cwd=work, env=env, capture_output=True,
                                encoding='utf8', errors='replace', timeout=60)
        report['checks'][name] = {'argv': argv, 'returncode': result.returncode,
            'expected_returncode': expected, 'stdout': result.stdout, 'stderr': result.stderr}
        save()
        assert result.returncode == expected, (name, result.stderr)
        print(f'{name}: passed', flush=True)
        return result

    template = work / 'template'
    template.mkdir()
    project = template / 'input.qpx'
    shutil.copyfile(reference['input_project'], project)
    shutil.copyfile(reference['image'], template / Path(reference['image']).name)
    original_project = sha(project)
    image = reference['image_id']
    destinations = work / 'outputs'
    destinations.mkdir()
    for kind in ('counts', 'continuous'):
        ref = reference[kind]
        store = work / (kind + '.zarr')
        positions = work / (kind + '-positions.tsv')
        shutil.copytree(ref['input'], store)
        shutil.copyfile(reference['positions'], positions)
        qmx = work / (kind + '.qmx')
        blocked = work / 'absent' / (kind + '.qpx')
        failed = run('failed-save-' + kind, consumer, manifest, project, image, store,
                     reference['primary_key'], positions, qmx, ref['options_json'], blocked, expected=2)
        assert qmx.is_file() and not blocked.exists()
        assert 'completed indexed matrix remains' in failed.stderr.lower(), failed.stderr
        assert 'absent' in failed.stderr.lower(), failed.stderr
        store.rename(work / (kind + '.retained-offline'))
        positions.rename(work / (kind + '-positions.retained-offline'))
        assert not store.exists() and not positions.exists()
        metadata = json.loads(run('offline-metadata-' + kind, cli, 'matrix-info', qmx).stdout)
        file_hash = sha(qmx)
        assert file_hash == metadata['file_sha256']
        assert metadata['domain'] == kind and metadata['entries'] == 12

        output = destinations / (kind + '.qpx')
        arguments = (cli, 'matrix-attach', project, image, qmx, file_hash, output)
        if kind == 'counts':
            bad_hash = ('0' if file_hash[0] != '0' else '1') + file_hash[1:]
            wrong = run('wrong-qmx-hash', *arguments[:-2], bad_hash, output, expected=2)
            assert 'exact file digest differs' in wrong.stderr.lower() and not output.exists()
            # Retained Loom template points at a different image with the same dimensions.
            other_ref = json.loads((packet.parent / 'loom/integration-03/fixtures/native-reference.json').read_text(encoding='utf8'))
            wrong = run('wrong-target-frame', cli, 'matrix-attach', other_ref['input_project'],
                        other_ref['image_id'], qmx, file_hash, output, expected=2)
            assert 'does not match the stored qmx frame' in wrong.stderr.lower() and not output.exists()
            run('qmx-destination-alias', *arguments[:-1], qmx, expected=2)
            run('template-destination-alias', *arguments[:-1], project, expected=2)

        saved = json.loads(run('attach-' + kind, *arguments).stdout)
        assert saved['owned'] and saved['file_sha256'] == file_hash
        assert saved['source_sha256'] == metadata['source_sha256']
        assert saved['frame_sha256'] == metadata['frame_sha256']
        managed = destinations / '.qpx-expression' / (file_hash + '.qmx')
        assert sha(managed) == file_hash == sha(qmx)
        retained = json.loads(run('owned-metadata-' + kind, cli, 'matrix-info', managed).stdout)
        for field in ('receipt', 'source_sha256', 'frame_sha256', 'store_content_sha256', 'store_observed_sha256'):
            assert retained[field] == metadata[field], field
        run('fresh-reopen-' + kind, inspector, '--inspect', output, image, saved['association_id'], file_hash,
            metadata['source_sha256'], ref['reader_receipt_sha256'], metadata['frame_sha256'])
        saved_hash = sha(output)
        existing = run('existing-project-' + kind, *arguments, expected=2)
        assert 'new project destination' in existing.stderr.lower()
        assert sha(output) == saved_hash and sha(managed) == file_hash == sha(qmx)
        assert sha(project) == original_project
    report['passed'] = True
    save()


if __name__ == '__main__':
    main()
