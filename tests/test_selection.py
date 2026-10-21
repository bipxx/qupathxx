import gc

import pytest

import qupathxx as q


def setup(count=6):
    doc = q.Document(q.Image.demo())
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i*10, 0, 5, 5)) for i in range(count)])
    return doc, ids, q.ObjectSelection(doc)


def test_selection_primary_toggle_range_and_snapshot():
    doc, ids, selection = setup()
    revision, history = doc.revision, doc.history_bytes
    selection.replace([ids[2], ids[0], ids[2]])
    assert selection.ids == [ids[0], ids[2]] and selection.primary == ids[2]
    snapshot = selection.ids
    selection.toggle(ids[2])
    assert selection.primary == ids[0] and selection.size == 1
    selection.toggle(ids[4])
    assert selection.contains(ids[4]) and selection.primary == ids[4]
    selection.select_range(ids[3], ids[1])
    assert selection.ids == ids[1:4] and selection.primary == ids[1]
    selection.select_range(ids[4], ids[5], additive=True)
    assert selection.ids == ids[1:] and selection.primary == ids[5]
    selection.select_all()
    assert selection.ids == ids and selection.primary == ids[5]
    assert snapshot == [ids[0], ids[2]]
    selection.replace(ids, primary=ids[1])
    assert selection.primary == ids[1]
    selection.clear()
    assert selection.ids == [] and selection.primary == 0 and selection.size == 0
    assert (doc.revision, doc.history_bytes) == (revision, history)


def test_invalid_selection_preserves_live_state():
    doc, ids, selection = setup()
    selection.replace(ids[:2])
    for action in (lambda: selection.replace([ids[0], 999]), lambda: selection.toggle(0),
                   lambda: selection.select_range(ids[0], 999),
                   lambda: selection.replace(ids[:2], primary=ids[-1]),
                   lambda: selection.select_region(q.Rect(0, 0, -1, 1))):
        with pytest.raises((IndexError, ValueError)):
            action()
        assert selection.ids == ids[:2] and selection.primary == ids[1]


def test_selection_prunes_deleted_ids_without_reselecting_after_undo():
    doc, ids, selection = setup()
    selection.replace(ids[:3])
    doc.remove_annotation(ids[2])
    assert selection.primary == ids[1] and selection.ids == ids[:2]
    assert doc.undo() and selection.ids == ids[:2]
    doc.rename_annotation(ids[0], "Renamed")
    doc.set_class("Tumor", 0xff0000)
    doc.classify_objects(ids[:2], "Tumor")
    assert selection.ids == ids[:2] and selection.primary == ids[1]
    doc.remove_objects(ids[:2])
    assert selection.ids == [] and selection.primary == 0


def test_region_selection_encloses_both_cell_parts_and_supports_zero_extents():
    doc, ids, selection = setup(3)
    cell = doc.add_cell(q.Roi.rectangle(q.Rect(2, 2, 1, 1)),
                        q.Roi.rectangle(q.Rect(50, 50, 1, 1)))
    point = doc.add_roi(q.Roi.points([q.Point(15, 0)]))
    selection.select_region(q.Rect(0, 0, 16, 5))
    assert selection.ids == [ids[0], ids[1], point]
    assert not selection.contains(cell)
    selection.select_region(q.Rect(0, 0, 60, 60), additive=True)
    assert selection.ids == ids + [cell, point]
    selection.select_region(q.Rect(15, 0, 0, 0))
    assert selection.ids == [point]
    builds = doc.spatial_stats.builds
    selection.select_region(q.Rect(100, 100, 1, 1), additive=True)
    assert selection.ids == [point] and doc.spatial_stats.builds == builds


def test_atomic_batch_delete_handles_selected_ancestors_and_duplicates():
    doc = q.Document(q.Image.demo())
    # Parent created after child ensures deletion works in either ID order.
    child = doc.add_cell(q.Roi.rectangle(q.Rect(1, 1, 2, 2)), class_name="Tumor")
    parent = doc.add_roi(q.Roi.rectangle(q.Rect(0, 0, 20, 20)))
    doc.reparent_annotation(child, parent)
    other = doc.add_rectangle(q.Rect(100, 100, 1, 1))
    revision, history = doc.revision, doc.history_bytes
    with pytest.raises(IndexError):
        doc.remove_objects([child, 999, parent])
    assert (doc.revision, doc.history_bytes) == (revision, history)
    assert doc.class_count("Tumor") == 1
    selection = q.ObjectSelection(doc)
    selection.replace([parent, child])
    doc.set_history_limits(128, 1)
    with pytest.raises(ValueError, match="budget"):
        doc.remove_objects(selection.ids)
    assert selection.ids == [child, parent] and doc.object_count == 3
    assert doc.revision == revision and doc.class_count("Tumor") == 1
    doc.set_history_limits(128, 64*1024*1024)
    doc.remove_objects([child, parent, child])
    assert [a.id for a in doc.annotations] == [other]
    assert doc.undo_label == "Delete objects" and doc.class_count("Tumor") == 0
    assert doc.undo() and doc.children(parent) == [child]
    assert doc.annotation(child).kind == q.ObjectKind.CELL and doc.class_count("Tumor") == 1
    assert doc.redo() and doc.object_count == 1
    revision = doc.revision
    doc.remove_objects([])
    assert doc.revision == revision


def test_selection_retains_document_python_lifetime():
    doc, ids, selection = setup(3)
    del doc
    gc.collect()
    selection.select_all()
    assert selection.ids == ids
