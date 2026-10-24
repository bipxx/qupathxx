"""Installed native file and standalone CLI contracts for case summaries."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

import pytest
import qupathxx as q
from qupathxx import _native
from test_tma import scene, rows
from test_tma_case_summary import measurement_table


def setup_case():
    document, ids = scene()
    document.set_tma_case_ids({ids[0]: 'replicate', ids[1]: 'replicate'})
    document.set_tma_missing([ids[2]], True)
    snapshot = document.tma_grid()
    table = measurement_table(snapshot, [(ids[0], 2, 1), (ids[1], 4, 3), (ids[2], 100, 10)])
    return document, ids, snapshot, table


@pytest.mark.parametrize('measurements', [False, True])
def test_native_file_export_matches_snapshot_and_retains_input(tmp_path, measurements):
    document, ids, snapshot, table = setup_case()
    source, output = tmp_path/'measurements-组织.csv', tmp_path/'cases-组织.csv'
    source.write_bytes(table.encode())
    if measurements:
        size = q.save_tma_case_summary_csv(snapshot, str(output), str(source), 'value', 'weight')
        expected = q.tma_case_summary_csv(snapshot, table, 'value', 'weight')
    else:
        size = q.save_tma_case_summary_csv(snapshot, str(output))
        expected = q.tma_case_summary_csv(snapshot)
    assert output.read_bytes() == expected.encode() and size == output.stat().st_size
    assert source.read_bytes() == table.encode()
    assert not list(tmp_path.glob('*.qpx-tmp-*'))
    document.set_tma_case_ids({ids[0]: 'edited later'})
    second = tmp_path/'captured.csv'
    q.save_tma_case_summary_csv(snapshot, str(second), str(source) if measurements else '', 'value' if measurements else '', 'weight' if measurements else '')
    assert second.read_bytes() == output.read_bytes()


@pytest.mark.parametrize('failure', ['existing', 'input_alias', 'empty_input', 'malformed', 'stale', 'budget', 'missing_file'])
def test_native_file_export_preserves_files_and_cleans_partial_output(tmp_path, failure):
    document, ids, snapshot, table = setup_case()
    source, output = tmp_path/'input.csv', tmp_path/'output.csv'
    source.write_bytes(table.encode())
    kwargs = {}
    if failure == 'existing':
        output.write_bytes(b'keep original')
    elif failure == 'input_alias':
        output = source
    elif failure == 'empty_input':
        source.write_bytes(b'')
    elif failure == 'malformed':
        source.write_bytes(b'"unterminated')
    elif failure == 'stale':
        document.set_tma_missing([ids[0]], True)
        snapshot = document.tma_grid()
    elif failure == 'budget':
        kwargs['byte_limit'] = 1
    elif failure == 'missing_file':
        source = tmp_path/'absent.csv'
    before = {p.name: p.read_bytes() for p in tmp_path.iterdir() if p.is_file()}
    with pytest.raises((ValueError, RuntimeError, OverflowError)):
        q.save_tma_case_summary_csv(snapshot, str(output), str(source), 'value', 'weight', **kwargs)
    assert {p.name: p.read_bytes() for p in tmp_path.iterdir() if p.is_file()} == before


def test_native_file_export_explicit_unversioned_input(tmp_path):
    document, ids, snapshot, _ = setup_case()
    source, output = tmp_path/'input.csv', tmp_path/'output.csv'
    source.write_bytes(measurement_table(snapshot, [(ids[0], 3, 2)], revision=False).encode())
    with pytest.raises(ValueError):
        q.save_tma_case_summary_csv(snapshot, str(output), str(source), 'value')
    q.save_tma_case_summary_csv(snapshot, str(output), str(source), 'value', allow_unversioned=True)
    row = next(r for r in rows(output.read_text(encoding='utf8')) if r['case_id'] == 'replicate')
    assert row['measurement_revision_status'] == 'unversioned'


@pytest.mark.parametrize('below_limit', [False, True])
def test_native_file_input_budget_boundary(tmp_path, below_limit):
    document, ids, snapshot, _ = setup_case()
    table = measurement_table(snapshot, [(ids[0], 2, 1)]*200)
    expected = q.tma_case_summary_csv(snapshot, table, 'value', 'weight').encode()
    raw = table.encode()
    assert len(raw) > len(expected)
    source, output = tmp_path/'input.csv', tmp_path/'cases.csv'
    source.write_bytes(raw)
    if below_limit:
        with pytest.raises((ValueError, RuntimeError)):
            q.save_tma_case_summary_csv(snapshot, str(output), str(source), 'value', 'weight', byte_limit=len(raw)-1)
        assert not output.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))
    else:
        assert q.save_tma_case_summary_csv(snapshot, str(output), str(source), 'value', 'weight', byte_limit=len(raw)) == len(expected)
        assert output.read_bytes() == expected
    assert source.read_bytes() == raw


def cli(*args):
    executable = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    return subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True, encoding='utf8', timeout=30)


@pytest.mark.skipif(not q.has_project, reason='native project storage')
@pytest.mark.parametrize('measurements', [False, True])
def test_standalone_export_matches_api_from_saved_project(tmp_path, measurements):
    document, ids, snapshot, table = setup_case()
    project = q.Project()
    image = project.add_document(document, 'TMA')
    source, inputs, output = tmp_path/'project-组织.qpxproj', tmp_path/'input.csv', tmp_path/'cases.csv'
    project.save(str(source))
    inputs.write_bytes(table.encode())
    project_sha = hashlib.sha256(source.read_bytes()).hexdigest()
    command = ['--export-tma-cases', source, output]
    if measurements:
        command += ['--image', image, '--measurements', inputs, '--value-column', 'value', '--weight-column', 'weight']
    result = cli(*command)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report['schema'] == 'qpx.tma-case-export/1' and report['state'] == 'saved'
    assert report['image_id'] == image and report['bytes'] == output.stat().st_size
    reopened = q.Project.open(str(source)).document(image).tma_grid()
    expected = q.tma_case_summary_csv(reopened, table if measurements else '', 'value' if measurements else '', 'weight' if measurements else '')
    assert output.read_bytes() == expected.encode()
    assert hashlib.sha256(source.read_bytes()).hexdigest() == project_sha
    assert inputs.read_bytes() == table.encode() and not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.skipif(not q.has_project, reason='native project storage')
def test_standalone_requires_explicit_image_for_multiple_grids(tmp_path):
    project = q.Project()
    ids = [project.add_document(setup_case()[0], f'image-{i}') for i in range(2)]
    source, output = tmp_path/'project.qpxproj', tmp_path/'cases.csv'
    project.save(str(source))
    result = cli('--export-tma-cases', source, output)
    assert result.returncode == 1 and not output.exists()
    assert 'select one TMA image' in json.loads(result.stderr)['error']
    result = cli('--export-tma-cases', source, output, '--image', ids[1])
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize('options', [
    ['--image', '0'], ['--csv-bytes', '0'], ['--csv-bytes', str(256*1024*1024+1)],
    ['--unknown'], ['--value-column', 'value'], ['--weight-column', 'weight'],
    ['--measurements', 'table.csv'], ['--image', '1', '--image', '1'], ['--image'],
])
def test_standalone_argument_errors_create_no_output(tmp_path, options):
    output = tmp_path/'cases.csv'
    result = cli('--export-tma-cases', tmp_path/'absent.qpxproj', output, *options)
    assert result.returncode == 2 and not output.exists()
    assert json.loads(result.stderr)['schema'] == 'qpx.batch-error/1'


@pytest.mark.skipif(not q.has_project, reason='native project storage')
@pytest.mark.parametrize('failure', ['existing', 'project_alias', 'input_alias', 'stale', 'no_grid'])
def test_standalone_data_failures_preserve_project_and_measurements(tmp_path, failure):
    document, ids, snapshot, table = setup_case()
    if failure == 'stale':
        document.set_tma_missing([ids[0]], True)
    if failure == 'no_grid':
        document.clear_tma_grid()
    project = q.Project()
    project.add_document(document, 'TMA')
    source, inputs, output = tmp_path/'project.qpxproj', tmp_path/'input.csv', tmp_path/'cases.csv'
    project.save(str(source))
    inputs.write_bytes(table.encode())
    if failure == 'existing':
        output.write_bytes(b'keep original')
    elif failure == 'project_alias':
        output = source
    elif failure == 'input_alias':
        output = inputs
    before = {p.name: p.read_bytes() for p in tmp_path.iterdir() if p.is_file()}
    result = cli('--export-tma-cases', source, output, '--measurements', inputs, '--value-column', 'value', '--weight-column', 'weight')
    assert result.returncode == 1, result.stderr
    assert json.loads(result.stderr)['schema'] == 'qpx.batch-error/1'
    assert {p.name: p.read_bytes() for p in tmp_path.iterdir() if p.is_file()} == before
