"""Native TMA ownership, editing, persistence and bounded table contracts."""
import csv
import gc
import io
import sqlite3

import pytest
import qupathxx as q


def scene():
    doc = q.Document(q.Image(256, 192, [32] * 256 * 192 * 3))
    options = q.TmaGridOptions()
    options.rows, options.columns = 2, 3
    options.origin = q.Point(30, 30)
    options.row_step, options.column_step = q.Point(0, 60), q.Point(60, 0)
    options.width, options.height = 30, 24
    return doc, doc.create_tma_grid(options)


def rows(text):
    return list(csv.DictReader(io.StringIO(text)))


def test_immutable_snapshot_history_and_independent_tma_revision():
    doc, ids = scene()
    held = doc.tma_grid()
    epoch = doc.tma_revision
    ordinary = doc.add_rectangle(q.Rect(200, 140, 10, 10))
    assert doc.tma_revision == epoch
    doc.set_tma_missing([ids[1], ids[1]], True)
    assert doc.tma_revision > epoch and doc.tma_core_ids() == [ids[0], *ids[2:]]
    assert not held.core(0, 1).tma_core.missing
    revision = doc.revision
    doc.set_tma_missing([ids[1]], True)
    assert doc.revision == revision
    doc.set_tma_case_ids({ids[0]: 'case,"组织"\n1', ids[1]: 'case,"组织"\n1'})
    assert doc.undo() and doc.annotation(ids[0]).tma_core.case_id == ''
    assert doc.redo() and doc.annotation(ids[0]).tma_core.case_id.startswith('case,')
    before = doc.revision
    with pytest.raises((ValueError, RuntimeError)):
        doc.set_tma_missing([ids[0], ordinary], True)
    assert doc.revision == before and not doc.annotation(ids[0]).tma_core.missing
    with pytest.raises(AttributeError):
        held.info.name = 'mutated'
    doc.clear_tma_grid()
    assert doc.tma_grid() is None and doc.object_count == 7
    assert doc.undo() and doc.tma_core_count == 6
    del doc
    gc.collect()
    assert held.core(1, 2).name == 'B-3'


def test_vacancy_metadata_and_context_preserve_csv_and_revision():
    doc, ids = scene()
    doc.set_tma_case_ids({ids[0]: 'case,"组织"\n1'})
    doc.set_tma_missing([ids[1]], True)
    doc.remove_annotation(ids[2])
    snapshot = doc.tma_grid()
    metadata = rows(q.tma_metadata_csv(snapshot))
    assert len(metadata) == 6 and all(len(row) == 18 for row in metadata)
    assert [r['slot_state'] for r in metadata[:3]] == ['present', 'missing', 'vacant']
    assert metadata[0]['case_id'] == 'case,"组织"\n1'
    assert metadata[2]['object_id'] == ''
    core = snapshot.core(0, 0)
    table = f'source_object_id,source_object_revision,value\r\n{core.id},{core.revision},"a,b\n""q"""\r\n'
    joined = rows(q.tma_context_csv(snapshot, table))
    assert joined[0]['value'] == 'a,b\n"q"'
    assert joined[0]['tma_case_id'] == metadata[0]['case_id']
    with pytest.raises((ValueError, RuntimeError)):
        q.tma_context_csv(snapshot, table.replace(f'{core.id},{core.revision},', f'{core.id},{core.revision+1},'))
    with pytest.raises((ValueError, RuntimeError)):
        q.tma_context_csv(snapshot, 'object_id\n999\n', require_revision=False)
    for operation in [lambda: doc.tma_grid(1), lambda: q.tma_metadata_csv(snapshot, 1), lambda: q.tma_context_csv(snapshot, table, 1)]:
        with pytest.raises((ValueError, RuntimeError)):
            operation()
    replacement = doc.add_tma_core(0, 2, q.Rect(135, 18, 30, 24), 'replacement')
    assert replacement != ids[2] and snapshot.core(0, 2) is None
    assert doc.tma_grid().core(0, 2).id == replacement


def test_equal_layouts_from_independent_owners_are_charged_separately():
    doc, ids = scene()
    other, _ = scene()
    before = doc.tma_grid()
    foreign = other.tma_grid().core(0, 0).tma_core.with_missing(True)
    assert foreign.grid_info.same_layout(before.info)
    doc.execute([q.ObjectEdit.tma_metadata(ids[0], foreign)])
    snapshot = doc.tma_grid()
    assert snapshot.payload_bytes == before.payload_bytes + foreign.grid_info.payload_bytes
    with pytest.raises((ValueError, RuntimeError)):
        doc.tma_grid(before.payload_bytes)


@pytest.mark.parametrize('field,value', [('rows', 0), ('columns', 513), ('width', -1), ('name', ''), ('row_labels', ['A', 'A'])])
def test_invalid_options_are_atomic(field, value):
    doc = q.Document(q.Image.demo())
    options = q.TmaGridOptions()
    options.rows = 2
    setattr(options, field, value)
    with pytest.raises((ValueError, RuntimeError)):
        doc.create_tma_grid(options)
    assert doc.revision == 0 and doc.tma_grid() is None


@pytest.mark.skipif(not q.has_project, reason='project storage')
def test_project_roundtrip_and_schema10_upgrade(tmp_path):
    doc, ids = scene()
    doc.set_tma_missing([ids[1]], True)
    doc.set_tma_case_ids({ids[0]: 'patient'})
    project = q.Project()
    image = project.add_document(doc, 'TMA')
    path = tmp_path/'tma.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert restored.tma_core_ids() == doc.tma_core_ids()
    assert restored.annotation(ids[0]).tma_core.case_id == 'patient'
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0] == 12
    # A real schema-10 file has neither TMA table nor TMA annotations.
    doc.clear_tma_grid()
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute('DROP TABLE tiff_views');db.execute('DROP TABLE tma_cores')
        db.execute('DROP TABLE tma_grids')
        db.execute('PRAGMA user_version=10')
    legacy = q.Project.open(str(path))
    assert legacy.document(image).tma_grid() is None
    legacy.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0] == 12


@pytest.mark.skipif(not q.has_project, reason='project storage')
@pytest.mark.parametrize('mutation', ['checksum', 'row', 'missing', 'orphan', 'duplicate-slot', 'case-limit'])
def test_corrupt_project_metadata_rejected(tmp_path, mutation):
    doc, ids = scene()
    project = q.Project()
    project.add_document(doc, 'TMA')
    path = tmp_path/'corrupt.qpxproj'
    project.save(str(path))
    with sqlite3.connect(path) as db:
        if mutation == 'checksum':
            blob = bytearray(db.execute('SELECT payload FROM tma_grids').fetchone()[0])
            blob[-1] ^= 1
            db.execute('UPDATE tma_grids SET payload=?', (blob,))
        elif mutation == 'row':
            db.execute('UPDATE tma_cores SET row_index=512 WHERE object_id=?', (str(ids[0]),))
        elif mutation == 'missing':
            db.execute('UPDATE tma_cores SET missing=2')
        elif mutation == 'orphan':
            db.execute("UPDATE tma_cores SET object_id='999999' WHERE object_id=?", (str(ids[0]),))
        elif mutation == 'duplicate-slot':
            db.execute('UPDATE tma_cores SET row_index=0,column_index=0')
        else:
            db.execute('UPDATE tma_cores SET case_id=?', ('x'*257,))
    with pytest.raises((ValueError, RuntimeError)):
        q.Project.open(str(path))


@pytest.mark.skipif(not q.has_project, reason='project storage')
def test_batch_present_missing_and_snapshot_capture(tmp_path):
    doc, ids = scene()
    doc.set_tma_missing([ids[1]], True)
    doc.set_tma_case_ids({ids[0]: 'patient'})
    project = q.Project()
    image = project.add_document(doc, 'TMA')
    project.add_document(q.Document(q.Image.demo()), 'Without grid')
    options = q.ProjectBatchOptions()
    options.tma_only = True
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    request = q.ProjectBatchRequest(project, transform, [], options)
    assert request.object_count == 5
    doc.set_tma_missing([ids[0]], True)
    result = q.run_project_batch(request, str(tmp_path/'out'))
    assert result.state == 'succeeded' and result.saved == 1
    assert result.images[0].records == 5
    table = rows(q.batch_checkpoint_csv(result.images[0].path))
    assert len(table) == 5 and {int(r['source_object_id']) for r in table} == set(ids)-{ids[1]}
    assert next(r for r in table if int(r['source_object_id']) == ids[0])['tma_case_id'] == 'patient'
    assert q.run_project_batch(request, str(tmp_path/'out')).reused == 1
    saved = q.Project.open(result.images[0].path).document(image)
    assert not saved.annotation(ids[0]).tma_core.missing
    options.include_missing_tma = True
    assert q.ProjectBatchRequest(project, transform, [], options).object_count == 6


@pytest.mark.skipif(not q.has_project, reason='project storage')
@pytest.mark.parametrize('kind', [q.ProjectBatchKind.ROI, q.ProjectBatchKind.TISSUE, q.ProjectBatchKind.CELLS, q.ProjectBatchKind.PIXELS])
def test_per_core_workflows_join_metadata(tmp_path, kind):
    from pathlib import Path
    doc, ids = scene()
    doc.set_tma_case_ids({ids[0]: 'patient'})
    doc.set_tma_missing(ids[1:], True)
    project = q.Project()
    project.add_document(doc, 'TMA')
    options = q.ProjectBatchOptions()
    options.kind, options.tma_only = kind, True
    if kind == q.ProjectBatchKind.PIXELS:
        options.pixel_model = q.PixelStreamFile(str(Path(__file__).parent/'data/pixel-streams/rectangle-v1-windows.qps')).read_block(0).model
    request = q.ProjectBatchRequest(project, q.StainTransform([[1, 0, 0], [0, 1, 0]]), [], options)
    result = q.run_project_batch(request, str(tmp_path/'output'))
    assert result.state == 'succeeded', (result.error, [(i.state, i.error) for i in result.images])
    assert result.images[0].objects == 1
    text = q.batch_checkpoint_csv(result.images[0].path)
    assert 'tma_case_id' in text
    for row in rows(text):
        assert row['tma_case_id'] == 'patient' and row['tma_row_label'] == 'A'


@pytest.mark.skipif(not q.has_project, reason='project storage')
def test_all_missing_grid_has_empty_analysis_and_keeps_inventory(tmp_path):
    doc, ids = scene()
    doc.set_tma_missing(ids, True)
    project = q.Project()
    project.add_document(doc, 'TMA')
    options = q.ProjectBatchOptions()
    options.tma_only = True
    request = q.ProjectBatchRequest(project, q.StainTransform([[1, 0, 0], [0, 1, 0]]), [], options)
    assert request.object_count == 0
    result = q.run_project_batch(request, str(tmp_path/'out'))
    assert result.state == 'succeeded' and result.images[0].records == 0
    assert not rows(q.batch_checkpoint_csv(result.images[0].path))


@pytest.mark.skipif(not q.has_project, reason='project storage')
@pytest.mark.parametrize('include_missing', [False, True])
def test_native_cli_tma_scope_and_csv(tmp_path, include_missing):
    import json
    import os
    from pathlib import Path
    import subprocess
    from qupathxx import _native
    doc, ids = scene()
    doc.set_tma_missing([ids[1]], True)
    doc.set_tma_case_ids({ids[0]: 'patient'})
    project = q.Project()
    project.add_document(doc, 'TMA')
    source = tmp_path/'source.qpxproj'
    project.save(str(source))
    before = source.read_bytes()
    executable = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(executable), str(source), str(tmp_path/'out'), '--tma-only']
    if include_missing:
        command.append('--include-missing-tma')
    result = subprocess.run(command, capture_output=True, text=True, encoding='utf8', timeout=30)
    assert result.returncode == 0, result.stderr+result.stdout
    image = json.loads(result.stdout.splitlines()[0])
    table = rows(q.batch_checkpoint_csv(image['path']))
    assert len(table) == (6 if include_missing else 5)
    assert sum(row['tma_missing'] == '1' for row in table) == int(include_missing)
    assert source.read_bytes() == before
