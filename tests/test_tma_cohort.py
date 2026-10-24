"""Captured multi-grid pooling: raw-row statistics, identities and file protection."""
import csv
from fractions import Fraction
import gc
import hashlib
import io
import json

import pytest
import qupathxx as q


def setup_cohort(namespaces=(11, 22)):
    documents = []
    for at in range(2):
        doc = q.Document(q.Image(256, 192, [32]*256*192*3))
        options = q.TmaGridOptions()
        options.rows, options.columns = 1, 3
        options.origin = q.Point(30, 30)
        options.row_step, options.column_step = q.Point(0, 60), q.Point(60, 0)
        options.width, options.height = 30, 24
        ids = doc.create_tma_grid(options)
        doc.set_tma_case_ids({ids[0]: 'P'})
        if at == 0:
            doc.remove_annotation(ids[2])
        else:
            doc.set_tma_case_ids({ids[1]: 'P'})
            doc.set_tma_missing([ids[1]], True)
        documents.append(doc)
    snapshots = {image: doc.tma_grid() for image, doc in zip(namespaces, documents)}
    return documents, snapshots, measurements(snapshots)


def measurements(snapshots):
    entries = list(sorted(snapshots.items()))
    text = 'image_id,object_id,object_revision,value,weight\r\n'
    for at, slot, value, weight in [(0, 0, 2, 1), (1, 0, 4, 3), (0, 0, 8, 2),
                                   (1, 1, 100, 20), (0, 1, 10, 0), (1, 2, 12, 2)]:
        image, grid = entries[at]
        core = grid.core(0, slot)
        text += f'{image},{core.id},{core.revision},{value},{weight}\r\n'
    return text


def rows(text):
    return list(csv.DictReader(io.StringIO(text)))


def test_cohort_pools_raw_rows_with_independent_rational_statistics():
    _, snapshots, table = setup_cohort()
    result = rows(q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight'))
    assert len(result) == 3
    pooled = next(row for row in result if row['case_id'] == 'P')
    for column, expected in [('grids', 2), ('cores', 3), ('present_cores', 2), ('missing_cores', 1),
                             ('measured_cores', 2), ('measurement_rows', 3), ('ignored_missing_rows', 1),
                             ('cohort_grids', 2), ('cohort_slots', 6), ('cohort_vacant_slots', 1)]:
        assert int(pooled[column]) == expected
    for column, expected in [('value_sum', Fraction(14)), ('weight_sum', Fraction(6)),
                             ('weighted_sum', Fraction(30)), ('mean', Fraction(14, 3)),
                             ('weighted_mean', Fraction(5)), ('minimum', Fraction(2)), ('maximum', Fraction(8))]:
        assert float(pooled[column]) == pytest.approx(float(expected), rel=1e-15, abs=0)
    assert pooled['measurement_revision_status'] == 'verified'
    unassigned = [row for row in result if not row['case_id']]
    assert {row['unassigned_image_id'] for row in unassigned} == {'11', '22'}
    assert all(row['cores'] == row['grids'] == '1' for row in unassigned)
    assert next(row for row in unassigned if row['unassigned_image_id'] == '11')['weighted_mean'] == ''
    assert next(row for row in unassigned if row['unassigned_image_id'] == '22')['weighted_mean'] == '12'


def test_cohort_manifest_binds_full_grid_metadata_and_input_bytes():
    _, snapshots, table = setup_cohort()
    quote = lambda value: '"'+value.replace('"', '""')+'"'
    expected = 'table_schema,image_id,metadata_document_revision,grid_name,grid_slots,grid_vacant_slots,cores,metadata_csv_sha256\r\n'
    for image, grid in sorted(snapshots.items()):
        digest = hashlib.sha256(q.tma_metadata_csv(grid).encode('utf-8')).hexdigest()
        slots = grid.info.rows*grid.info.columns
        expected += (f'"qpx.tma-cohort-sources/1",{image},{grid.document_revision},'+quote(grid.info.name)+
                     f',{slots},{slots-len(grid.cores)},{len(grid.cores)},"{digest}"\r\n')
    assert q.tma_cohort_manifest_csv(snapshots) == expected
    output = rows(q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight'))
    assert {row['source_manifest_sha256'] for row in output} == {hashlib.sha256(expected.encode('utf-8')).hexdigest()}
    assert {row['input_table_sha256'] for row in output} == {hashlib.sha256(table.encode('utf-8')).hexdigest()}
    reverse = dict(reversed(list(snapshots.items())))
    assert q.tma_cohort_manifest_csv(reverse) == expected
    assert q.tma_cohort_case_summary_csv(reverse, table, 'value', 'weight') == q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight')


def test_cohort_captures_survive_edits_and_document_destruction():
    documents, snapshots, table = setup_cohort()
    original = q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight')
    manifest = q.tma_cohort_manifest_csv(snapshots)
    core = snapshots[22].core(0, 0)
    documents[1].set_tma_case_ids({core.id: 'later'})
    changed = {11: snapshots[11], 22: documents[1].tma_grid()}
    assert q.tma_cohort_manifest_csv(changed) != manifest
    with pytest.raises(ValueError):
        q.tma_cohort_case_summary_csv(changed, table, 'value', 'weight', allow_unversioned=True)
    documents.clear()
    gc.collect()
    assert q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight') == original


@pytest.mark.parametrize('selectors', ['image_id,object_id,object_revision',
                                      'source_image_id,source_object_id,source_object_revision'])
def test_cohort_selector_namespaces(selectors):
    _, snapshots, table = setup_cohort()
    renamed = table.replace('image_id,object_id,object_revision', selectors, 1)
    result = next(row for row in rows(q.tma_cohort_case_summary_csv(snapshots, renamed, 'value', 'weight')) if row['case_id'] == 'P')
    assert result['weighted_mean'] == '5' and result['measurement_rows'] == '3'


@pytest.mark.parametrize('failure', ['missing_image', 'unknown_image', 'unknown_core', 'stale', 'noncanonical_image',
                                     'duplicate_header', 'truncated', 'negative_weight', 'nan', 'overflow', 'underflow',
                                     'empty_value', 'field_count', 'missing_revision', 'zero_image'])
def test_cohort_rejects_invalid_rows(failure):
    _, snapshots, _ = setup_cohort()
    core = snapshots[11].core(0, 0)
    header = 'image_id,object_id,object_revision,value,weight\n'
    fields = ['11', str(core.id), str(core.revision), '2', '1']
    if failure == 'missing_image':
        header = 'other,object_id,object_revision,value,weight\n'
    elif failure == 'unknown_image': fields[0] = '33'
    elif failure == 'unknown_core': fields[1] = str(snapshots[22].core(0, 2).id)
    elif failure == 'stale': fields[2] = str(core.revision+1)
    elif failure == 'noncanonical_image': fields[0] = '011'
    elif failure == 'duplicate_header': header = header.replace('weight', 'value')
    elif failure == 'negative_weight': fields[4] = '-1'
    elif failure == 'nan': fields[3] = 'nan'
    elif failure == 'overflow': fields[3] = fields[4] = '1e308'
    elif failure == 'underflow': fields[3] = fields[4] = '1e-300'
    elif failure == 'empty_value': fields[3] = ''
    elif failure == 'field_count': fields.pop()
    elif failure == 'missing_revision': header = header.replace('object_revision', 'other_revision')
    elif failure == 'zero_image': fields[0] = '0'
    table = header+','.join(fields)+('' if failure == 'truncated' else '\n')
    with pytest.raises((ValueError, RuntimeError, OverflowError)):
        q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight')


@pytest.mark.parametrize('failure', ['empty', 'zero', 'null', 'count', 'payload'])
def test_cohort_capture_admission(failure):
    _, snapshots, _ = setup_cohort()
    kwargs = {}
    if failure == 'empty': snapshots = {}
    elif failure == 'zero': snapshots = {0: snapshots[11]}
    elif failure == 'null': snapshots = {11: None}
    elif failure == 'count': snapshots = {image: snapshots[11] for image in range(1, 4098)}
    else: kwargs['byte_limit'] = sum(grid.payload_bytes for grid in snapshots.values())-1
    with pytest.raises((ValueError, RuntimeError, TypeError)):
        q.tma_cohort_manifest_csv(snapshots, **kwargs)


def test_cohort_inventory_and_explicit_unversioned_rows():
    _, snapshots, _ = setup_cohort()
    result = rows(q.tma_cohort_case_summary_csv(snapshots))
    assert len(result) == 3 and all(row['measurement_revision_status'] == 'no_measurements' for row in result)
    core = snapshots[11].core(0, 0)
    table = f'image_id,object_id,value\n11,{core.id},2\n'
    with pytest.raises(ValueError): q.tma_cohort_case_summary_csv(snapshots, table, 'value')
    result = rows(q.tma_cohort_case_summary_csv(snapshots, table, 'value', allow_unversioned=True))
    pooled = next(row for row in result if row['case_id'] == 'P')
    assert pooled['mean'] == '2' and pooled['measurement_revision_status'] == 'unversioned'


def test_cohort_compensated_signed_sum_across_images():
    _, snapshots, _ = setup_cohort()
    a, b = snapshots[11].core(0, 0), snapshots[22].core(0, 0)
    table = (f'image_id,object_id,object_revision,value\n11,{a.id},{a.revision},1e16\n'
             f'22,{b.id},{b.revision},1\n11,{a.id},{a.revision},-1e16\n')
    pooled = next(row for row in rows(q.tma_cohort_case_summary_csv(snapshots, table, 'value')) if row['case_id'] == 'P')
    assert pooled['value_sum'] == '1' and float(pooled['mean']) == float(Fraction(1, 3))


@pytest.mark.parametrize('measurements_present', [False, True])
def test_cohort_file_export_and_held_snapshot(tmp_path, measurements_present):
    documents, snapshots, table = setup_cohort()
    source, output = tmp_path/'input-组织.csv', tmp_path/'cohort-组织.csv'
    source.write_bytes(table.encode('utf-8'))
    args = (str(source), 'value', 'weight') if measurements_present else ()
    expected = q.tma_cohort_case_summary_csv(snapshots, table if measurements_present else '', 'value' if measurements_present else '', 'weight' if measurements_present else '')
    assert q.save_tma_cohort_case_summary_csv(snapshots, str(output), *args) == len(expected.encode('utf-8'))
    assert output.read_bytes() == expected.encode('utf-8')
    documents[1].clear_tma_grid()
    captured = tmp_path/'held.csv'
    q.save_tma_cohort_case_summary_csv(snapshots, str(captured), *args)
    assert captured.read_bytes() == output.read_bytes() and source.read_bytes() == table.encode('utf-8')


@pytest.mark.parametrize('failure', ['existing', 'input_alias', 'budget', 'malformed', 'missing', 'empty_cohort'])
def test_cohort_file_errors_preserve_every_file(tmp_path, failure):
    _, snapshots, table = setup_cohort()
    source, output = tmp_path/'input.csv', tmp_path/'output.csv'
    source.write_bytes(table.encode('utf-8'))
    kwargs = {}
    if failure == 'existing': output.write_bytes(b'keep')
    elif failure == 'input_alias': output = source
    elif failure == 'budget': kwargs['byte_limit'] = 1
    elif failure == 'malformed': source.write_bytes(b'"unterminated')
    elif failure == 'missing': source = tmp_path/'absent.csv'
    elif failure == 'empty_cohort': snapshots = {}
    before = {path.name: path.read_bytes() for path in tmp_path.iterdir()}
    with pytest.raises((ValueError, RuntimeError, OverflowError)):
        q.save_tma_cohort_case_summary_csv(snapshots, str(output), str(source), 'value', 'weight', **kwargs)
    assert {path.name: path.read_bytes() for path in tmp_path.iterdir()} == before


@pytest.mark.skipif(not q.has_project, reason='standalone project CLI')
@pytest.mark.parametrize('selection', ['all', 'explicit'])
def test_cohort_standalone_cli_matches_captured_summary(tmp_path, selection):
    from test_tma_case_export import cli
    documents, snapshots, table = setup_cohort((1, 2))
    project = q.Project()
    assert [project.add_document(doc, 'grid') for doc in documents] == [1, 2]
    project.add_document(q.Document(q.Image(16, 16, [0]*16*16*3)), 'no grid')
    source, inputs, output = tmp_path/'source.qpxproj', tmp_path/'input.csv', tmp_path/'cohort.csv'
    project.save(str(source));inputs.write_bytes(table.encode('utf-8'))
    original = source.read_bytes()
    flags = ['--all-tma-images'] if selection == 'all' else ['--image', 2, '--image', 1]
    result = cli('--export-tma-cohort', source, output, *flags, '--measurements', inputs, '--value-column', 'value', '--weight-column', 'weight')
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report['schema'] == 'qpx.tma-cohort-export/1' and report['grids'] == 2
    assert output.read_bytes() == q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight').encode('utf-8')
    assert report['bytes'] == output.stat().st_size and source.read_bytes() == original
    assert inputs.read_bytes() == table.encode('utf-8')

@pytest.mark.skipif(not q.has_project, reason='standalone project CLI')
@pytest.mark.parametrize('failure,expected', [('selection', 2), ('conflict', 2), ('duplicate', 2),
                                           ('unknown', 1), ('no_grid', 1), ('subset', 1), ('existing', 1),
                                           ('project_alias', 1), ('input_alias', 1)])
def test_cohort_cli_selection_and_protected_files(tmp_path, failure, expected):
    from test_tma_case_export import cli
    documents, _, table = setup_cohort((1, 2))
    project = q.Project()
    assert [project.add_document(doc, 'grid') for doc in documents] == [1, 2]
    no_grid = project.add_document(q.Document(q.Image(16, 16, [0]*16*16*3)), 'plain')
    source, inputs, output = tmp_path/'source.qpxproj', tmp_path/'input.csv', tmp_path/'output.csv'
    project.save(str(source));inputs.write_bytes(table.encode('utf-8'))
    flags = ['--all-tma-images']
    if failure == 'selection': flags = []
    elif failure == 'conflict': flags += ['--image', 1]
    elif failure == 'duplicate': flags = ['--image', 1, '--image', 1]
    elif failure == 'unknown': flags = ['--image', 99999]
    elif failure == 'no_grid': flags = ['--image', no_grid]
    elif failure == 'subset': flags = ['--image', 1]
    elif failure == 'existing': output.write_bytes(b'keep')
    elif failure == 'project_alias': output = source
    elif failure == 'input_alias': output = inputs
    before = {path.name: path.read_bytes() for path in tmp_path.iterdir() if path.is_file()}
    result = cli('--export-tma-cohort', source, output, *flags, '--measurements', inputs, '--value-column', 'value', '--weight-column', 'weight')
    assert result.returncode == expected, result.stderr
    assert json.loads(result.stderr)['schema'] == 'qpx.batch-error/1'
    assert {path.name: path.read_bytes() for path in tmp_path.iterdir() if path.is_file()} == before
