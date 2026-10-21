import pytest

import qupathxx as q


def classes(doc):
    return [(c.name, c.color) for c in doc.classes]


def test_class_auto_registration_is_atomic_and_undoable():
    doc = q.Document(q.Image.demo())
    cell = doc.add_cell(q.Roi.rectangle(q.Rect(0, 0, 20, 20)),
                        q.Roi.ellipse(q.Rect(3, 3, 5, 5)), class_name="Tumor")
    snapshot = doc.object_class("Tumor")
    assert classes(doc) == [("Tumor", 0x41e1be)]
    assert doc.class_count("Tumor") == 1
    assert doc.undo() and not doc.classes and not doc.object_count
    assert doc.redo() and doc.class_count("Tumor") == 1
    assert doc.annotation(cell).nucleus is not None
    assert snapshot.name == "Tumor" and snapshot.color == 0x41e1be
    with pytest.raises(ValueError):
        doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 1, 1), class_name="Stroma"),
                     q.ObjectEdit.create(q.Rect(-1, 0, 1, 1))])
    assert classes(doc) == [("Tumor", 0x41e1be)] and doc.class_count("Stroma") == 0
    assert doc.add_rectangle(q.Rect(0, 0, 1, 1)) == cell + 1


def test_color_edit_keeps_object_revision_and_spatial_index():
    doc = q.Document(q.Image.demo())
    obj = doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 10, 10)), class_name="Tumor")
    assert doc.pick(q.Point(1, 1)) == [obj]
    builds = doc.spatial_stats.builds
    object_revision = doc.annotation(obj).revision
    before = doc.object_class("Tumor")
    doc.set_class("Tumor", 0xff0022)
    payload = doc.history_bytes
    revision = doc.revision
    doc.set_class("Tumor", 0xff0022)
    assert (doc.revision, doc.history_bytes) == (revision, payload)
    assert doc.annotation(obj).revision == object_revision
    assert doc.pick(q.Point(1, 1)) == [obj] and doc.spatial_stats.builds == builds
    assert doc.spatial_stats.dirty_entries == 0
    assert before.color == 0x41e1be
    assert doc.undo() and doc.object_class("Tumor").color == before.color
    assert doc.redo() and doc.object_class("Tumor").color == 0xff0022


def test_rename_reclassifies_preserving_cell_geometry_and_history():
    doc = q.Document(q.Image.demo())
    parent = doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 100, 100)), class_name="Tumor")
    child = doc.add_cell(q.Roi.rectangle(q.Rect(10, 10, 20, 20)),
                         q.Roi.rectangle(q.Rect(12, 12, 3, 3)), parent_id=parent, class_name="Tumor")
    other = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    doc.set_class("Tumor", 0x123456)
    doc.query_region(q.Rect(0, 0, 100, 100))
    builds = doc.spatial_stats.builds
    doc.rename_class("Tumor", "Positive")
    assert classes(doc) == [("Positive", 0x123456)]
    assert doc.class_count("Positive") == 2 and doc.class_count("Tumor") == 0
    assert [doc.annotation(i).class_name for i in (parent, child, other)] == ["Positive", "Positive", ""]
    assert doc.annotation(child).kind == q.ObjectKind.CELL
    assert doc.annotation(child).nucleus.area == 9 and doc.annotation(child).parent_id == parent
    assert doc.undo_label == "Rename class"
    assert doc.undo() and doc.annotation(child).class_name == "Tumor"
    assert doc.redo() and doc.annotation(parent).class_name == "Positive"
    doc.query_region(q.Rect(0, 0, 100, 100))
    assert doc.spatial_stats.builds == builds and doc.spatial_stats.dirty_entries == 0
    assert doc.remove_annotation(parent)
    assert doc.class_count("Positive") == 0
    doc.remove_class("Positive")
    assert not doc.classes
    assert doc.undo() and doc.object_class("Positive").color == 0x123456
    assert doc.undo() and doc.class_count("Positive") == 2


def test_batch_classification_counts_duplicates_and_failure():
    doc = q.Document(q.Image.demo())
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i, 0, 1, 1)) for i in range(10)])
    doc.classify_objects(ids + ids, "Tumor")
    assert doc.class_count("Tumor") == 10
    assert doc.undo() and not doc.classes
    assert doc.redo() and doc.class_count("Tumor") == 10
    revision, history = doc.revision, doc.history_bytes
    with pytest.raises(IndexError):
        doc.classify_objects([ids[0], 1000], "Other")
    assert (doc.revision, doc.history_bytes) == (revision, history)
    assert doc.class_count("Tumor") == 10 and doc.class_count("Other") == 0
    doc.classify_objects(ids[:3], "")
    assert doc.class_count("Tumor") == 7 and doc.class_count("") == 0
    assert doc.undo() and doc.class_count("Tumor") == 10
    doc.classify_objects([], "Unused")
    assert "Unused" not in [c.name for c in doc.classes]
    assert doc.redo() and doc.class_count("Tumor") == 7


@pytest.mark.parametrize("name,color", [("", 0), ("bad\0name", 1), ("x"*4097, 1), ("Invalid", 0x1000000)])
def test_invalid_class_edits_preserve_redo(name, color):
    doc = q.Document(q.Image.demo())
    doc.set_class("Tumor", 0x123456)
    assert doc.undo()
    revision, history = doc.revision, doc.history_bytes
    with pytest.raises(ValueError):
        doc.set_class(name, color)
    assert not doc.classes and (doc.revision, doc.history_bytes) == (revision, history)
    assert doc.redo() and classes(doc) == [("Tumor", 0x123456)]


def test_in_use_removal_rename_collision_budget_and_branching():
    doc = q.Document(q.Image.demo())
    obj = doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 5, 5)), class_name="Tumor")
    doc.set_class("Other", 0x123456)
    revision, history = doc.revision, doc.history_bytes
    with pytest.raises(ValueError, match="assigned"):
        doc.remove_class("Tumor")
    with pytest.raises(ValueError, match="exists"):
        doc.rename_class("Tumor", "Other")
    with pytest.raises(ValueError, match="empty"):
        doc.rename_class("Tumor", "")
    assert (doc.revision, doc.history_bytes) == (revision, history)
    assert doc.class_count("Tumor") == 1
    doc.set_history_limits(128, 1)
    with pytest.raises(ValueError, match="budget"):
        doc.set_class("Tumor", 0)
    assert doc.object_class("Tumor").color == 0x41e1be
    doc.set_history_limits(128, 64*1024*1024)
    doc.classify_annotation(obj, "Other")
    assert doc.undo() and doc.class_count("Tumor") == 1
    doc.set_class("Other", 0x999999)
    assert not doc.can_redo
    doc.set_history_limits(0, 0)
    doc.classify_annotation(obj, "New")
    assert doc.class_count("New") == 1 and not doc.can_undo


def test_repeated_replacements_count_only_final_class_and_preserve_noop():
    doc = q.Document(q.Image.demo())
    obj = doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 1, 1), "Object", class_name="A")])[0]
    doc.rename_annotation(obj, "Renamed")
    assert doc.undo()
    revision, history = doc.revision, doc.history_bytes
    doc.execute([q.ObjectEdit.replace(obj, q.Rect(0, 0, 1, 1), "Object", class_name="B"),
                 q.ObjectEdit.replace(obj, q.Rect(0, 0, 1, 1), "Object", class_name="A")])
    assert (doc.revision, doc.history_bytes) == (revision, history)
    assert classes(doc) == [("A", 0x41e1be)] and doc.class_count("A") == 1
    assert doc.class_count("B") == 0 and doc.redo()
    doc.execute([q.ObjectEdit.replace(obj, q.Rect(0, 0, 1, 1), "Renamed", class_name="B"),
                 q.ObjectEdit.replace(obj, q.Rect(0, 0, 1, 1), "Renamed", class_name="C")])
    assert [c.name for c in doc.classes] == ["A", "C"]
    assert doc.class_count("C") == 1 and doc.class_count("B") == 0
    assert doc.undo() and doc.class_count("A") == 1 and doc.class_count("C") == 0
