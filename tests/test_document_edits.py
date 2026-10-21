import pytest

import qupathxx as q


def document():
    return q.Document(q.Image.demo())


def snapshot(doc):
    return [(a.id, a.name, a.class_name, a.parent_id, a.roi.x, a.roi.y, a.roi.width, a.roi.height) for a in doc.annotations]


def test_edit_identity_branching_and_revision():
    doc = document()
    assert not doc.can_undo and not doc.can_redo
    first = doc.add_rectangle(q.Rect(10, 20, 30, 40), "Original")
    original = doc.annotation(first)
    doc.update_rectangle(first, q.Rect(100, 200, 10, 20))
    assert doc.annotation(first).revision > original.revision
    assert doc.undo_label == "Edit rectangle"
    revision = doc.revision
    assert doc.undo()
    assert doc.annotation(first).roi.x == 10
    assert doc.annotation(first).revision > original.revision
    assert doc.revision > revision
    assert original.roi.x == 10
    assert doc.redo()
    assert doc.annotation(first).roi.x == 100
    assert doc.undo()
    doc.rename_annotation(first, "Branch")
    assert not doc.redo()
    assert doc.annotation(first).name == "Branch"
    assert doc.undo()
    assert doc.undo()
    assert doc.object_count == 0
    second = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    assert second > first


def test_compound_changes_and_invalid_batch_are_atomic():
    doc = document()
    ids = doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 200, 200), "A"),
                       q.ObjectEdit.create(q.Rect(300, 0, 200, 200), "B")], "Create pair")
    before = snapshot(doc)
    revision, history_bytes = doc.revision, doc.history_bytes
    with pytest.raises(ValueError, match="inside"):
        doc.execute([q.ObjectEdit.replace(ids[0], q.Rect(5, 5, 50, 50), "Changed"),
                     q.ObjectEdit.create(q.Rect(-1, 0, 1, 1))], "Invalid batch")
    assert snapshot(doc) == before
    assert doc.revision == revision and doc.history_bytes == history_bytes
    assert doc.undo_label == "Create pair"
    doc.execute([q.ObjectEdit.replace(ids[0], q.Rect(5, 5, 50, 50), "Tumor", class_name="Tumor"),
                 q.ObjectEdit.replace(ids[1], q.Rect(305, 5, 50, 50), "Stroma", class_name="Stroma")], "Classify pair")
    assert [doc.annotation(i).class_name for i in ids] == ["Tumor", "Stroma"]
    assert doc.undo() and snapshot(doc) == before
    assert doc.redo() and doc.annotation(ids[0]).name == "Tumor"
    assert doc.undo() and doc.undo() and not doc.object_count
    assert doc.redo() and snapshot(doc) == before


def test_hierarchy_containment_cycle_and_subtree_restore():
    doc = document()
    parent = doc.add_rectangle(q.Rect(0, 0, 300, 300), "Parent")
    child = doc.add_rectangle(q.Rect(10, 10, 100, 100), "Child")
    grandchild = doc.add_rectangle(q.Rect(20, 20, 10, 10), "Grandchild")
    doc.reparent_annotation(child, parent)
    doc.reparent_annotation(grandchild, child)
    doc.classify_annotation(child, "Tumor")
    before = snapshot(doc)
    assert doc.children(0) == [parent]
    assert doc.children(parent) == [child]
    with pytest.raises(ValueError, match="contain"):
        doc.update_rectangle(parent, q.Rect(0, 0, 30, 30))
    with pytest.raises(ValueError):
        doc.reparent_annotation(parent, grandchild)
    with pytest.raises(ValueError, match="does not exist"):
        doc.reparent_annotation(child, 999)
    assert snapshot(doc) == before
    assert doc.remove_annotation(parent)
    assert not doc.object_count and not doc.children(0)
    assert doc.undo() and snapshot(doc) == before
    assert doc.children(child) == [grandchild]
    assert doc.redo() and not doc.object_count


def test_cycle_with_identical_bounds_and_final_batch_validation():
    doc = document()
    a, b = doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 100, 100), "A"),
                        q.ObjectEdit.create(q.Rect(0, 0, 100, 100), "B")])
    before = snapshot(doc)
    with pytest.raises(ValueError, match="cycle"):
        doc.execute([q.ObjectEdit.replace(a, q.Rect(0, 0, 100, 100), "A", b),
                     q.ObjectEdit.replace(b, q.Rect(0, 0, 100, 100), "B", a)])
    assert snapshot(doc) == before
    doc.reparent_annotation(b, a)
    doc.execute([q.ObjectEdit.replace(a, q.Rect(200, 200, 100, 100), "A"),
                 q.ObjectEdit.replace(b, q.Rect(210, 210, 10, 10), "B", a)], "Move subtree")
    assert doc.annotation(b).roi.x == 210
    assert doc.undo() and doc.annotation(b).roi.x == 0


def test_calibration_history_and_noop_edits():
    doc = document()
    a = doc.add_rectangle(q.Rect(0, 0, 10, 20), "A")
    revision, size = doc.revision, doc.history_bytes
    doc.rename_annotation(a, "A")
    doc.update_rectangle(a, q.Rect(0, 0, 10, 20))
    doc.execute([])
    doc.clear_calibration()
    assert doc.revision == revision and doc.history_bytes == size
    doc.set_calibration(.25, .5)
    assert doc.area_um2(a) == 25
    assert doc.undo() and doc.pixel_width_um is None
    assert doc.redo() and doc.area_um2(a) == 25
    doc.clear_calibration()
    assert doc.undo() and doc.pixel_height_um == .5


def test_budget_eviction_and_recording_disabled():
    doc = document()
    doc.set_history_limits(2, 1024 * 1024)
    a = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    doc.rename_annotation(a, "A")
    doc.classify_annotation(a, "Tumor")
    assert doc.undo() and doc.undo() and not doc.undo()
    assert doc.object_count == 1
    before, revision = snapshot(doc), doc.revision
    doc.set_history_limits(10, 1)
    assert not doc.history_bytes
    with pytest.raises((ValueError, RuntimeError), match="budget"):
        doc.rename_annotation(a, "Too large")
    assert snapshot(doc) == before and doc.revision == revision
    doc.set_history_limits(0, 0)
    doc.rename_annotation(a, "No history")
    assert not doc.can_undo and doc.history_bytes == 0
    assert not doc.remove_annotation(999)
    assert doc.annotation(a).name == "No history"


def test_child_index_stays_consistent_through_undo_and_branch():
    doc = document()
    p1, p2 = doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 500, 500), "P1"),
                          q.ObjectEdit.create(q.Rect(0, 0, 500, 500), "P2")])
    child = doc.execute([q.ObjectEdit.create(q.Rect(10, 10, 20, 20), "Child", p1)])[0]
    doc.reparent_annotation(child, p2)
    assert doc.children(p1) == [] and doc.children(p2) == [child]
    assert doc.undo()
    assert doc.children(p1) == [child] and doc.children(p2) == []
    doc.classify_annotation(child, "Branch")
    assert not doc.can_redo
    assert doc.remove_annotation(p2) and doc.object_count == 2
    assert doc.remove_annotation(p1) and doc.object_count == 0


def test_depth_limit_checks_unchanged_descendants():
    doc = document()
    doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 10, 10), str(i), i) for i in range(256)])
    root = doc.add_rectangle(q.Rect(0, 0, 10, 10))
    child = doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 10, 10), "Nested", root)])[0]
    before, revision = snapshot(doc), doc.revision
    with pytest.raises(ValueError, match="256 levels"):
        doc.reparent_annotation(root, 255)
    assert snapshot(doc) == before and doc.revision == revision
    assert doc.children(root) == [child]


def test_rejected_batch_does_not_consume_ids_or_drop_redo():
    doc = document()
    a = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    doc.rename_annotation(a, "Renamed")
    assert doc.undo()
    with pytest.raises(ValueError):
        doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 1, 1)), q.ObjectEdit.create(q.Rect(-1, 0, 1, 1))])
    assert doc.can_redo and doc.redo_label == "Rename annotation"
    assert doc.redo() and doc.annotation(a).name == "Renamed"
    assert doc.add_rectangle(q.Rect(1, 1, 1, 1)) == a + 1
