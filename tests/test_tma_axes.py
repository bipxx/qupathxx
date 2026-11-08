"""Structural TMA corrections share native geometry, history and persistence."""
import pytest
import qupathxx as q
from test_tma import scene


def rect(annotation):
    b = annotation.geometry.bounds
    return b.x, b.y, b.width, b.height


@pytest.mark.parametrize('axis', ['row', 'column'])
def test_insert_preserves_old_cores_and_atomic_history(axis):
    doc, ids = scene()
    doc.set_tma_case_ids({ids[0]: 'patient'})
    doc.set_tma_missing([ids[0]], True)
    original = {i: rect(doc.annotation(i)) for i in ids}
    held = doc.tma_grid()
    added = getattr(doc, f'insert_tma_{axis}')(1, 'new', missing=True)
    assert len(added) == (3 if axis == 'row' else 2)
    assert {i: rect(doc.annotation(i)) for i in ids} == original
    assert doc.annotation(ids[0]).tma_core.case_id == 'patient'
    assert doc.annotation(ids[0]).tma_core.missing
    for i in added:
        assert doc.annotation(i).tma_core.case_id == ''
        assert doc.annotation(i).tma_core.missing
    assert held.info.rows == 2 and held.info.columns == 3
    assert doc.undo() and doc.tma_core_count == 6
    assert doc.redo() and doc.tma_core_count == 6 + len(added)
    getattr(doc, f'remove_tma_{axis}')(1)
    assert doc.tma_core_count == 6
    assert doc.undo() and doc.tma_grid().core(1 if axis == 'row' else 0, 1 if axis == 'column' else 0).id == added[0]


@pytest.mark.parametrize('axis', ['row', 'column'])
def test_explicit_and_vacant_insertions(axis):
    doc, ids = scene()
    cross = 3 if axis == 'row' else 2
    explicit = [q.Rect(10 + i * 40, 120, 12, 14) for i in range(cross)]
    added = getattr(doc, f'insert_tma_{axis}')(0, bounds=explicit)
    assert [rect(doc.annotation(i)) for i in added] == [(10 + i * 40, 120, 12, 14) for i in range(cross)]
    assert doc.undo()
    assert getattr(doc, f'insert_tma_{axis}')(0, bounds=[]) == []
    assert doc.tma_core_count == 6
    assert doc.tma_grid().core(0, 0) is None
    getattr(doc, f'remove_tma_{axis}')(0)
    assert doc.tma_core_ids(True) == ids


@pytest.mark.parametrize('operation', [
    lambda d: d.insert_tma_row(3),
    lambda d: d.remove_tma_column(3),
    lambda d: d.insert_tma_column(1, '1'),
    lambda d: d.insert_tma_row(1, bounds=[q.Rect(1, 1, 2, 2)]),
    lambda d: d.insert_tma_row(1, 'x' * 129),
    lambda d: d.insert_tma_row(1, bounds=[q.Rect(1, 1, 2, 2), q.Rect(2, 2, 2, 2), q.Rect(999, 999, 2, 2)]),
])
def test_rejection_is_atomic(operation):
    doc, ids = scene()
    revision = doc.revision
    with pytest.raises((ValueError, IndexError, RuntimeError)):
        operation(doc)
    assert doc.revision == revision and doc.tma_core_ids(True) == ids
    # Rejection does not consume object IDs.
    assert doc.add_rectangle(q.Rect(0, 0, 2, 2)) == max(ids) + 1


def test_singleton_requires_explicit_spacing():
    doc = q.Document(q.Image.demo())
    o = q.TmaGridOptions()
    o.rows = o.columns = 1
    doc.create_tma_grid(o)
    revision = doc.revision
    with pytest.raises(ValueError, match='ambiguous'):
        doc.insert_tma_row(0)
    assert doc.revision == revision
    assert len(doc.insert_tma_row(0, bounds=[q.Rect(10, 10, 10, 10)])) == 1


@pytest.mark.skipif(not q.has_project, reason='project storage')
def test_edited_grid_project_and_qptma_roundtrip(tmp_path):
    doc, ids = scene()
    new = doc.insert_tma_row(1, 'Inserted')
    doc.set_tma_case_ids({new[0]: 'new case'})
    doc.remove_tma_column(2)
    p = q.Project()
    image = p.add_document(doc, 'axes')
    path = tmp_path / 'axes.qpxproj'
    p.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert (restored.tma_grid().info.rows, restored.tma_grid().info.columns) == (3, 2)
    assert restored.tma_grid().core(1, 0).id == new[0]
    assert restored.annotation(new[0]).tma_core.case_id == 'new case'
    wire = q.parse_qptma(q.qptma_text(q.qptma_from_grid(restored.tma_grid()).data))
    other = q.Document(q.Image(256, 192, [32] * 256 * 192 * 3))
    assert len(q.import_qptma(other, wire)) == 6
    assert other.tma_grid().core(1, 0).tma_core.case_id == 'new case'


def test_sparse_cross_axis_uses_observed_spacing():
    doc, ids = scene()
    doc.remove_annotation(ids[1])
    added = doc.insert_tma_row(1)
    assert rect(doc.annotation(added[1])) == (75, 48, 30, 24)
    assert doc.tma_grid().core(0, 1) is None


def test_dimensions_and_total_slot_limits_reject_before_mutation():
    doc = q.Document(q.Image(128, 128, [32] * 128 * 128 * 3))
    options = q.TmaGridOptions()
    options.rows, options.columns = 512, 1
    options.origin = q.Point(1, 1)
    options.row_step, options.column_step = q.Point(0, .1), q.Point(.1, 0)
    options.width = options.height = .05
    doc.create_tma_grid(options)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError)):
        doc.insert_tma_row(1, bounds=[])
    assert doc.revision == revision and doc.tma_core_count == 512
    doc.clear_tma_grid(True)
    options.rows = options.columns = 100
    doc.create_tma_grid(options)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError)):
        doc.insert_tma_column(1, bounds=[])
    assert doc.revision == revision and doc.tma_core_count == 10000


def test_retained_nested_core_survives_axis_removal_and_undo():
    doc = q.Document(q.Image(512, 512, [255] * 512 * 512 * 3))
    o = q.TmaGridOptions()
    o.rows, o.columns = 2, 3
    o.origin = q.Point(100, 100)
    o.row_step, o.column_step = q.Point(10, 60), q.Point(50, 5)
    o.width, o.height = 20, 16
    ids = doc.create_tma_grid(o)
    doc.update_roi(ids[0], q.Roi.ellipse(q.Rect(50, 50, 200, 200)))
    doc.update_roi(ids[3], q.Roi.ellipse(q.Rect(100, 100, 60, 60)))
    doc.update_roi(ids[1], q.Roi.ellipse(q.Rect(120, 120, 10, 10)))
    doc.reparent_annotation(ids[3], ids[0])
    kept = doc.add_cell(q.Roi.ellipse(q.Rect(110, 110, 40, 40)), None, 'kept', ids[3])
    removed = doc.add_cell(q.Roi.ellipse(q.Rect(145, 145, 2, 2)), None, 'removed', ids[0])
    doc.reparent_annotation(ids[1], kept)
    doc.remove_tma_row(0)
    assert doc.annotation(ids[3]).parent_id == 0
    assert doc.annotation(kept).parent_id == ids[3]
    for i in [ids[0], ids[1], removed]:
        with pytest.raises(IndexError):
            doc.annotation(i)
    assert doc.undo()
    assert doc.annotation(ids[3]).parent_id == ids[0]
    assert doc.annotation(ids[1]).parent_id == kept
    assert doc.annotation(removed).parent_id == ids[0]
