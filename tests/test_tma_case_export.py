"""Installed native file and standalone CLI contracts for case summaries."""
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import struct
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
    bundled = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    executable = os.environ.get('QUPATHXX_TEST_TMA_BATCH', str(bundled))
    return subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True, encoding='utf8', timeout=30)


@pytest.mark.skipif(not q.has_project, reason='native project storage')
@pytest.mark.parametrize('workflow', ['cases', 'cohort'])
def test_tma_source_id_with_legacy_revision_stays_versioned(tmp_path, workflow):
    document, ids, snapshot, _ = setup_case()
    project = q.Project()
    image = project.add_document(document, 'TMA')
    source = tmp_path / 'project.qpx'
    project.save(str(source))
    revision = next(core.revision for core in snapshot.cores if core.id == ids[0])
    inputs = tmp_path / 'measurements.csv'
    output = tmp_path / 'export.csv'
    original = source.read_bytes()

    def export(header, row, allow=False):
        inputs.write_bytes((header + '\r\n' + row + '\r\n').encode())
        args = (f'--export-tma-{workflow}', source, output, '--image', image, '--measurements', inputs,
                '--value-column', 'value')
        return cli(*args, *(['--allow-unversioned'] if allow else []))

    columns = 'source_image_id,source_object_id,object_revision,value'
    stale_row = f'{image},{ids[0]},{revision + 1},2'
    denied = export(columns, stale_row, allow=True)
    assert denied.returncode == 1 and 'core revision' in denied.stderr
    assert not output.exists() and inputs.read_bytes() == (columns + '\r\n' + stale_row + '\r\n').encode()
    denied = export(columns + ',object_id', f'{image},{ids[0]},{revision},2,{ids[1]}', allow=True)
    assert denied.returncode == 1 and 'different object' in denied.stderr
    assert not output.exists()
    accepted = export(columns + ',object_id', f'{image},{ids[0]},{revision},2,{ids[0]}')
    assert accepted.returncode == 0, accepted.stderr
    row = next(value for value in rows(output.read_text(encoding='utf-8')) if value['case_id'] == 'replicate')
    assert row['measurement_revision_status'] == 'verified'
    assert float(row['value_sum']) == 2 and row['input_table_sha256'] == hashlib.sha256(inputs.read_bytes()).hexdigest()
    assert source.read_bytes() == original


@pytest.mark.skipif(not q.has_project, reason='native project storage')
@pytest.mark.parametrize('cohort', [False, True])
def test_tma_measurement_csv_utf8_signature(tmp_path, cohort):
    document, _, _, table = setup_case()
    project = q.Project()
    image = project.add_document(document, 'TMA')
    source = tmp_path / 'source.qpx'
    project.save(str(source))
    header, first, second, _ = table.encode().splitlines(keepends=True)
    if cohort:
        header = b'source_image_id,' + header
        prefix = str(image).encode() + b','
        first, second = prefix + first, prefix + second
    signature = b'\xef\xbb\xbf'
    marked = tmp_path / 'signature.csv'
    marked.write_bytes(signature + header + first)
    plain = tmp_path / 'plain.csv'
    plain.write_bytes(header + second)
    other_marked = tmp_path / 'second-signature.csv'
    other_marked.write_bytes(signature + header + second)
    originals = {path: path.read_bytes() for path in (source, marked, plain, other_marked)}
    command = '--export-tma-cohort' if cohort else '--export-tma-cases'
    scope = ('--all-tma-images',) if cohort else ('--image', image)

    def export(name, inputs, expected_bytes, expected_mean, *options):
        output = tmp_path / f'{name}.csv'
        files = [arg for path in inputs for arg in ('--measurements', path)]
        done = cli(command, source, output, *scope, *files,
                   '--value-column', 'value', '--weight-column', 'weight', *options)
        assert done.returncode == 0, done.stderr
        summary = next(row for row in rows(output.read_text(encoding='utf8'))
                       if row['case_id'] == 'replicate')
        assert float(summary['weighted_mean']) == expected_mean
        assert summary['input_table_sha256'] == hashlib.sha256(expected_bytes).hexdigest()

    export('only-marked', [marked], signature + header + first, 2.0)
    export('marked-first', [marked, plain], signature + header + first + second, 3.5)
    export('marked-second', [plain, marked], header + second + first, 3.5)
    export('both-marked', [marked, other_marked], signature + header + first + second, 3.5)
    reordered = tmp_path / 'reordered.csv'
    reordered.write_bytes(signature + move_first_fixture_column(header + second))
    export('marked-reordered', [marked, reordered], signature + header + first + second,
           3.5, '--match-columns-by-name')
    quoted = tmp_path / 'quoted.csv'
    column, remainder = header.split(b',', 1)
    quoted_header = b'"' + column + b'",' + remainder
    quoted.write_bytes(signature + quoted_header + first)
    export('quoted-marked', [quoted], signature + quoted_header + first, 2.0)
    assert all(path.read_bytes() == original for path, original in originals.items())


def move_first_fixture_column(data):
    result = []
    for line in data.splitlines(keepends=True):
        body = line.rstrip(b'\r\n')
        first, separator, rest = body.partition(b',')
        assert separator
        result.append(rest + separator + first + line[len(body):])
    return b''.join(result)


@pytest.mark.skipif(not q.has_project, reason='native project storage')
def test_single_grid_pools_multiple_measurement_files(tmp_path):
    document, _, snapshot, table = setup_case()
    project = q.Project()
    image = project.add_document(document, 'TMA')
    source = tmp_path / 'single-grid.qpxproj'
    project.save(str(source))
    header, first, second, missing = table.encode().splitlines(keepends=True)
    early, late = tmp_path / 'early.csv', tmp_path / 'late.csv'
    early.write_bytes(header + first)
    late.write_bytes(header + second + missing)
    originals = {path: path.read_bytes() for path in (source, early, late)}

    def export(output, *inputs, options=()):
        args = [arg for path in inputs for arg in ('--measurements', path)]
        return cli('--export-tma-cases', source, output, '--image', image, *args,
                   '--value-column', 'value', '--weight-column', 'weight', *options)

    output = tmp_path / 'cases.csv'
    done = export(output, early, late)
    assert done.returncode == 0, done.stderr
    assert json.loads(done.stdout)['schema'] == 'qpx.tma-case-export/1'
    expected = q.tma_case_summary_csv(snapshot, table, 'value', 'weight')
    assert output.read_bytes() == expected.encode()
    parsed = rows(output.read_text(encoding='utf-8'))
    pooled = next(row for row in parsed if row['case_id'] == 'replicate')
    assert float(pooled['weighted_mean']) == 3.5 and int(pooled['measured_cores']) == 2
    assert sum(int(row['ignored_missing_rows']) for row in parsed) == 1
    assert pooled['input_table_sha256'] == hashlib.sha256(table.encode()).hexdigest()

    reversed_output = tmp_path / 'reversed.csv'
    reversed_result = export(reversed_output, late, early)
    assert reversed_result.returncode == 0, reversed_result.stderr
    digest = hashlib.sha256(header + second + missing + first).hexdigest()
    assert reversed_output.read_bytes() == expected.replace(pooled['input_table_sha256'], digest).encode()

    eof_early = tmp_path / 'early-eof.csv'
    eof_early.write_bytes(early.read_bytes().rstrip(b'\r\n'))
    joined = tmp_path / 'joined-at-eof.csv'
    done = export(joined, eof_early, late)
    assert done.returncode == 0, done.stderr
    assert joined.read_bytes() == output.read_bytes()

    lf_late = tmp_path / 'late-lf.csv'
    lf_body = (second + missing).replace(b'\r\n', b'\n')
    lf_late.write_bytes(header.replace(b'\r\n', b'\n') + lf_body)
    mixed = tmp_path / 'mixed-endings.csv'
    done = export(mixed, eof_early, lf_late)
    assert done.returncode == 0, done.stderr
    mixed_digest = hashlib.sha256(header + first + lf_body).hexdigest()
    assert mixed.read_bytes() == expected.replace(pooled['input_table_sha256'], mixed_digest).encode()

    header_only = tmp_path / 'header-only-eof.csv'
    header_only.write_bytes(header.rstrip(b'\r\n'))
    from_header = tmp_path / 'from-header-only.csv'
    done = export(from_header, header_only, early, late)
    assert done.returncode == 0, done.stderr
    assert from_header.read_bytes() == output.read_bytes()

    unterminated = tmp_path / 'final-record-no-newline.csv'
    final_row, final_weight = missing.rstrip(b'\r\n').rsplit(b',', 1)
    quoted_tail = final_row + b',"' + final_weight + b'"'
    unterminated.write_bytes(header + second + quoted_tail)
    eof_result = tmp_path / 'eof-cases.csv'
    done = export(eof_result, early, unterminated)
    assert done.returncode == 0, done.stderr
    digest = hashlib.sha256(header + first + second + quoted_tail).hexdigest()
    assert eof_result.read_bytes() == expected.replace(pooled['input_table_sha256'], digest).encode()

    reordered = tmp_path / 'reordered-columns.csv'
    reordered.write_bytes(move_first_fixture_column(unterminated.read_bytes()))
    matched = tmp_path / 'matched-columns.csv'
    done = export(matched, early, reordered)
    assert done.returncode == 1 and 'headers differ' in done.stderr and not matched.exists()
    match = ('--match-columns-by-name',)
    done = export(matched, early, reordered, options=match)
    assert done.returncode == 0, done.stderr
    assert matched.read_bytes() == eof_result.read_bytes()

    annotated_header = header.rstrip(b'\r\n') + b',note\r\n'
    note = b'"first line\r\nsecond, ""quoted"""'
    annotated_first = first.rstrip(b'\r\n') + b',"first"\r\n'
    annotated_second = second.rstrip(b'\r\n') + b',' + note
    annotated = tmp_path / 'annotated.csv'
    annotated.write_bytes(annotated_header + annotated_first)
    different_order = tmp_path / 'note-first.csv'
    different_order.write_bytes(b'note,' + header.replace(b'\r\n', b'\n') + note + b',' + second.rstrip(b'\r\n'))
    annotated_output = tmp_path / 'matched-notes.csv'
    done = export(annotated_output, annotated, different_order, options=match)
    assert done.returncode == 0, done.stderr
    note_digest = hashlib.sha256(annotated_header + annotated_first + annotated_second).hexdigest()
    annotated_case = next(row for row in rows(annotated_output.read_text(encoding='utf8'))
                          if row['case_id'] == 'replicate')
    assert annotated_case['weighted_mean'] == pooled['weighted_mean']
    assert annotated_case['input_table_sha256'] == note_digest

    other_header = tmp_path / 'mismatched.csv'
    other_header.write_bytes(late.read_bytes().replace(b',value,', b',other,', 1))
    unfinished = tmp_path / 'incomplete.csv'
    unfinished.write_bytes(unterminated.read_bytes()[:-1])
    duplicate_column = tmp_path / 'duplicate-column.csv'
    duplicate_column.write_bytes(late.read_bytes().replace(b',weight', b',value', 1))
    alias = tmp_path / 'alias.csv'
    os.link(early, alias)
    scoped_header = b'batch_image_id,batch_project_identity,' + header
    identity = project.identity.encode()

    def scoped(name, image_id, project_id, record):
        file = tmp_path / name
        file.write_bytes(scoped_header + str(image_id).encode() + b',' + project_id + b',' + record)
        return file

    scoped_early = scoped('scoped-early.csv', image, identity, first)
    wrong_image = scoped('wrong-image.csv', image + 1, identity, second)
    wrong_project = scoped('wrong-project.csv', image, b'0' * len(identity), second)
    refused = tmp_path / 'refused.csv'
    for inputs, options, message in [
        ((early, other_header), (), 'headers differ'),
        ((early, other_header), match, 'column names differ'),
        ((early, duplicate_column), match, 'duplicate TMA case column'),
        ((early, unfinished), (), 'unterminated quoted field'),
        ((early, alias), (), 'duplicate TMA measurement file'),
        ((scoped_early, wrong_image), (), 'selected image'),
        ((scoped_early, wrong_project), (), 'project identity'),
        ((early, late), ('--csv-bytes', len(originals[early]) + len(originals[late]) - 1),
         'CSV inputs exceed byte limit'),
    ]:
        result = export(refused, *inputs, options=options)
        assert result.returncode == 1 and message in result.stderr and not refused.exists()
    assert {path: path.read_bytes() for path in originals} == originals
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.skipif(not q.has_project, reason='native project storage')
def test_native_batch_measurements_pool_by_project_image_and_core(tmp_path):
    project = q.Project()
    images = []
    for intensity, diameter in [(32, 6), (128, 8)]:
        doc = q.Document(q.Image(48, 24, [intensity] * (48 * 24 * 3)))
        options = q.TmaGridOptions()
        options.rows, options.columns = 1, 3
        options.origin, options.column_step = q.Point(8, 8), q.Point(12, 0)
        options.width = options.height = diameter
        first, second, third = doc.create_tma_grid(options)
        doc.set_tma_case_ids({first: 'paired', second: 'paired'})
        doc.remove_annotation(third)
        if not images:
            doc.set_tma_missing([second], True)
        else:
            doc.remove_annotation(second)
        images.append(project.add_document(doc))
    source = tmp_path / 'source.qpx'
    project.save(str(source))
    original = source.read_bytes()
    run = cli(source, tmp_path / 'batch', '--kind', 'od', '--tma-only', '--include-missing-tma')
    assert run.returncode == 0, run.stderr
    report = json.loads(run.stdout.splitlines()[-1])
    tables = []
    for result in report['images']:
        output = tmp_path / f"image-{result['image_id']}.csv"
        exported = cli('--export-csv', result['path'], output)
        assert exported.returncode == 0, exported.stderr
        tables.append(output)
    a, b = [table.read_bytes() for table in tables]
    first_rows, second_rows = rows(a.decode()), rows(b.decode())
    assert first_rows[0]['source_object_id'] == second_rows[0]['source_object_id']
    assert first_rows[0]['source_object_revision'] == second_rows[0]['source_object_revision']

    value = ('--value-column', 'mean_channel_1', '--weight-column', 'samples')
    single = tmp_path / 'single.csv'
    done = cli('--export-tma-cases', source, single, '--image', images[1], '--measurements', tables[1], *value)
    assert done.returncode == 0, done.stderr
    row, = rows(single.read_text(encoding='utf-8'))
    assert float(row['weighted_mean']) == pytest.approx(math.log10(255 / 128))
    assert row['input_table_sha256'] == hashlib.sha256(b).hexdigest()

    wrong = tmp_path / 'wrong.csv'
    refused = cli('--export-tma-cases', source, wrong, '--image', images[0], '--measurements', tables[1], *value)
    assert refused.returncode == 1 and 'selected image' in refused.stderr and not wrong.exists()

    combined = tmp_path / 'pool-input.csv'
    header_a, _ = a.split(b'\n', 1)
    header_b, body_b = b.split(b'\n', 1)
    assert header_a == header_b
    combined.write_bytes(a + body_b)
    pooled = tmp_path / 'pooled.csv'
    done = cli('--export-tma-cohort', source, pooled, '--all-tma-images', '--measurements', combined, *value)
    assert done.returncode == 0, done.stderr
    row, = rows(pooled.read_text(encoding='utf-8'))
    for field, count in dict(cohort_grids=2, cohort_slots=6, cohort_vacant_slots=3, grids=2, cores=3,
                             present_cores=2, missing_cores=1, measured_cores=2, measurement_rows=2,
                             ignored_missing_rows=1).items():
        assert int(row[field]) == count, field

    def disk_count(diameter):
        return sum((x + 0.5)**2 + (y + 0.5)**2 <= (diameter / 2)**2
                   for x in range(-diameter // 2, diameter // 2) for y in range(-diameter // 2, diameter // 2))

    weights = [disk_count(6), disk_count(8)]
    # The native ROI pipeline exports means of binary32 OD samples.
    means = [struct.unpack('<f', struct.pack('<f', math.log10(255 / v)))[0] for v in (32, 128)]
    expected = sum(w * v for w, v in zip(weights, means)) / sum(weights)
    assert int(row['weight_sum']) == sum(weights)
    assert float(row['weighted_mean']) == pytest.approx(expected, abs=1e-14)
    assert row['input_table_sha256'] == hashlib.sha256(a + body_b).hexdigest()

    def pool(output, *inputs, options=()):
        measurements = [arg for path in inputs for arg in ('--measurements', path)]
        return cli('--export-tma-cohort', source, output, '--all-tma-images', *measurements, *value, *options)

    direct = tmp_path / 'direct.csv'
    done = pool(direct, *tables)
    assert done.returncode == 0, done.stderr
    assert direct.read_bytes() == pooled.read_bytes()
    reversed_output = tmp_path / 'reversed.csv'
    done = pool(reversed_output, *reversed(tables))
    assert done.returncode == 0, done.stderr
    reversed_row, = rows(reversed_output.read_text(encoding='utf-8'))
    assert reversed_row.pop('input_table_sha256') == hashlib.sha256(b + a.split(b'\n', 1)[1]).hexdigest()
    assert reversed_row == {k: v for k, v in row.items() if k != 'input_table_sha256'}

    final_file = tmp_path / 'image-2-eof.csv'
    final_file.write_bytes(b.rstrip(b'\r\n'))
    no_ending = tmp_path / 'pooled-eof.csv'
    done = pool(no_ending, tables[0], final_file)
    assert done.returncode == 0, done.stderr
    exact_digest = hashlib.sha256(a + body_b.rstrip(b'\r\n')).hexdigest()
    assert no_ending.read_bytes() == pooled.read_bytes().replace(row['input_table_sha256'].encode(), exact_digest.encode())
    eof_reversed = tmp_path / 'reversed-eof.csv'
    done = pool(eof_reversed, final_file, tables[0])
    assert done.returncode == 0, done.stderr
    assert eof_reversed.read_bytes() == reversed_output.read_bytes()

    lf_final = tmp_path / 'image-2-lf-eof.csv'
    lf_final.write_bytes(header_b.rstrip(b'\r') + b'\n' + body_b.rstrip(b'\r\n'))
    mixed = tmp_path / 'mixed-endings.csv'
    done = pool(mixed, lf_final, tables[0])
    assert done.returncode == 0, done.stderr
    mixed_digest = hashlib.sha256(lf_final.read_bytes() + b'\n' + a.split(b'\n', 1)[1]).hexdigest()
    original_digest = hashlib.sha256(b + a.split(b'\n', 1)[1]).hexdigest()
    assert mixed.read_bytes() == reversed_output.read_bytes().replace(original_digest.encode(), mixed_digest.encode())

    reordered = tmp_path / 'reordered-image.csv'
    reordered.write_bytes(move_first_fixture_column(final_file.read_bytes()))
    matched = tmp_path / 'matched-columns.csv'
    match = ('--match-columns-by-name',)
    done = pool(matched, tables[0], reordered, options=match)
    assert done.returncode == 0, done.stderr
    assert matched.read_bytes() == no_ending.read_bytes()

    header_mismatch = tmp_path / 'header-mismatch.csv'
    header_mismatch.write_bytes(b.replace(b'mean_channel_1', b'other_mean', 1))
    incomplete = tmp_path / 'incomplete.csv'
    incomplete.write_bytes(b.rstrip(b'\r\n') + b',"unfinished')
    alias = tmp_path / 'same-inode.csv'
    os.link(tables[0], alias)
    for inputs, options, message in [
        ((tables[0], alias), (), 'duplicate TMA measurement file'),
        ((tables[0], header_mismatch), (), 'headers differ'),
        ((tables[0], incomplete, tables[1]), (), 'unterminated quoted field'),
        (tables, ('--csv-bytes', len(a) + len(b) - 1), 'CSV inputs exceed byte limit'),
    ]:
        refused = pool(wrong, *inputs, options=options)
        assert refused.returncode == 1 and message in refused.stderr and not wrong.exists()

    foreign = tmp_path / 'foreign.csv'
    identity = second_rows[0]['batch_project_identity'].encode()
    foreign.write_bytes(b.replace(identity, b'0' * len(identity)))
    refused = cli('--export-tma-cohort', source, wrong, '--all-tma-images', '--measurements', foreign, *value)
    assert refused.returncode == 1 and 'project identity' in refused.stderr and not wrong.exists()
    reordered_foreign = tmp_path / 'reordered-foreign.csv'
    reordered_foreign.write_bytes(move_first_fixture_column(foreign.read_bytes()))
    refused = pool(wrong, tables[0], reordered_foreign, options=match)
    assert refused.returncode == 1 and 'project identity' in refused.stderr and not wrong.exists()
    template = second_rows[0]
    for changed, message in [({'source_object_revision': str(int(template['source_object_revision']) + 1)}, 'core revision'),
                             ({'source_image_id': str(images[0])}, 'conflicting image selectors')]:
        bad_table = tmp_path / f'{next(iter(changed))}.csv'
        edited = {**template, **changed}
        with bad_table.open('w', encoding='utf-8', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=edited.keys())
            writer.writeheader()
            writer.writerow(edited)
        refused = cli('--export-tma-cohort', source, wrong, '--all-tma-images', '--measurements', bad_table, *value)
        assert refused.returncode == 1 and message in refused.stderr and not wrong.exists()
    assert source.read_bytes() == original and tables[0].read_bytes() == a and tables[1].read_bytes() == b
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.skipif(not q.has_project, reason='native project storage')
@pytest.mark.parametrize('workflow', ['cases', 'cohort'])
def test_standalone_export_protects_unselected_label_stores(tmp_path, workflow):
    project = q.Project()
    document, _, _, _ = setup_case()
    image = project.add_document(document, 'TMA')
    linked = q.Document(q.Image(16, 16, [0, 0, 0] * 256))
    online, offline = tmp_path / 'labels.zarr', tmp_path / 'offline.zarr'
    online.mkdir()
    (online / 'sentinel').write_bytes(b'existing label-store file')
    records = {
        str(index): {'ngff_source': True, 'parameters': json.dumps({'method': 3, 'path': str(path)})}
        for index, path in enumerate((online, offline), 1)
    }
    linked.set_image_data_properties(q.ImageDataProperties({'qpx.label-analysis/1': records}))
    project.add_document(linked, 'Unselected image')
    source = tmp_path / 'project.qpxproj'
    project.save(str(source))
    before = source.read_bytes()

    for output in (online / 'cases.csv', offline):
        result = cli(f'--export-tma-{workflow}', source, output, '--image', image)
        assert result.returncode == 1
        assert 'outside the protected input directory' in json.loads(result.stderr)['error']
        assert not output.exists()
    output = tmp_path / 'outside.csv'
    result = cli(f'--export-tma-{workflow}', source, output, '--image', image)
    assert result.returncode == 0, result.stderr
    assert any(row['case_id'] == 'replicate' for row in rows(output.read_text(encoding='utf8')))
    assert source.read_bytes() == before
    assert {p.name: p.read_bytes() for p in online.iterdir()} == {'sentinel': b'existing label-store file'}
    assert not list(tmp_path.rglob('*.qpx-tmp-*'))


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
