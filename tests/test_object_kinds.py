import pytest

import qupathxx as q


def rect(x, y, w, h):
    return q.Roi.rectangle(q.Rect(x, y, w, h))


def test_kinds_optional_nucleus_and_primary_measurements():
    doc = q.Document(q.Image(20, 20, [value for y in range(20) for x in range(20) for value in [x + 2*y]*3]))
    annotation = doc.add_roi(rect(0, 0, 20, 20))
    detection = doc.add_detection(rect(2, 2, 3, 4), parent_id=annotation)
    cell = doc.add_cell(rect(5, 5, 10, 10), rect(6, 6, 2, 3), parent_id=annotation)
    bare = doc.add_cell(rect(0, 0, 2, 2))
    assert doc.annotation(annotation).kind == q.ObjectKind.ANNOTATION
    assert not doc.annotation(annotation).is_detection
    assert doc.annotation(detection).kind == q.ObjectKind.DETECTION
    assert doc.annotation(cell).is_detection
    assert doc.annotation(bare).kind == q.ObjectKind.CELL
    assert doc.annotation(bare).nucleus is None
    assert doc.mean_intensity(cell) == 28.5
    assert doc.nucleus_mean_intensity(cell) == 20.5
    with pytest.raises(RuntimeError, match="calibration"):
        doc.nucleus_area_um2(cell)
    doc.set_calibration(.5, .25)
    assert doc.area_um2(cell) == 12.5
    assert doc.nucleus_area_um2(cell) == .75
    with pytest.raises(ValueError, match="no nucleus"):
        doc.nucleus_mean_intensity(bare)


def test_metadata_and_geometry_wrappers_preserve_cell_state():
    doc = q.Document(q.Image.demo())
    parent = doc.add_rectangle(q.Rect(0, 0, 100, 100))
    nucleus = rect(10, 10, 5, 5)
    cell = doc.add_cell(rect(0, 0, 20, 20), nucleus)
    snapshot = doc.annotation(cell)
    doc.rename_annotation(cell, "Renamed")
    doc.classify_annotation(cell, "Tumor")
    doc.reparent_annotation(cell, parent)
    doc.update_rectangle(cell, q.Rect(1, 1, 30, 30))
    doc.update_roi(cell, q.Roi.ellipse(q.Rect(1, 1, 30, 30)))
    assert doc.annotation(cell).kind == q.ObjectKind.CELL
    assert doc.annotation(cell).nucleus.equals(nucleus)
    for _ in range(5):
        assert doc.undo()
    assert doc.annotation(cell).name == snapshot.name
    assert doc.annotation(cell).geometry.equals(snapshot.geometry)
    assert snapshot.nucleus.equals(nucleus)
    for _ in range(5):
        assert doc.redo()
    assert doc.annotation(cell).class_name == "Tumor"
    assert doc.annotation(cell).parent_id == parent


def test_nucleus_index_deltas_and_history_without_body_change():
    doc = q.Document(q.Image.demo())
    parent = doc.add_roi(rect(10, 10, 10, 10))
    cell = doc.add_cell(rect(10, 10, 10, 10), rect(30, 30, 5, 5), parent_id=parent)
    assert doc.children(parent) == [cell]  # Parent containment uses the body.
    assert doc.annotation(cell).bounds.width == 25
    assert doc.pick(q.Point(32, 32)) == [cell]
    assert doc.pick(q.Point(25, 25)) == []
    assert doc.query_region(q.Rect(31, 31, 1, 1)) == [cell]
    builds = doc.spatial_stats.builds
    doc.update_nucleus(cell, rect(100, 100, 5, 5))
    assert doc.spatial_stats.dirty_entries == 1
    assert doc.pick(q.Point(32, 32)) == []
    assert doc.pick(q.Point(102, 102)) == [cell]
    assert doc.spatial_stats.builds == builds
    assert doc.undo() and doc.pick(q.Point(32, 32)) == [cell]
    assert doc.redo() and doc.pick(q.Point(102, 102)) == [cell]
    doc.update_nucleus(cell, None)
    assert doc.annotation(cell).nucleus is None
    assert doc.pick(q.Point(102, 102)) == []
    assert doc.undo() and doc.annotation(cell).nucleus is not None


@pytest.mark.parametrize("kind", [q.ObjectKind.ANNOTATION, q.ObjectKind.DETECTION])
def test_nucleus_rejected_for_other_kinds_atomically(kind):
    doc = q.Document(q.Image.demo())
    existing = doc.add_cell(rect(0, 0, 20, 20))
    revision, history = doc.revision, doc.history_bytes
    with pytest.raises(ValueError, match="only cells"):
        doc.execute([
            q.ObjectEdit.replace_geometry(existing, rect(1, 1, 10, 10), "Changed", kind=q.ObjectKind.CELL),
            q.ObjectEdit.create_geometry(rect(0, 0, 10, 10), kind=kind, nucleus=rect(1, 1, 2, 2)),
        ])
    assert doc.object_count == 1 and doc.annotation(existing).name == "Cell"
    assert (doc.revision, doc.history_bytes) == (revision, history)
    with pytest.raises(ValueError, match="only cells"):
        doc.update_nucleus(doc.add_detection(rect(0, 0, 1, 1)), None)


def test_invalid_nucleus_and_cell_area_preserve_redo_and_identity():
    doc = q.Document(q.Image.demo())
    cell = doc.add_cell(rect(0, 0, 20, 20), rect(1, 1, 2, 2))
    doc.rename_annotation(cell, "Changed")
    assert doc.undo()
    revision, history, label = doc.revision, doc.history_bytes, doc.redo_label
    for invalid in (rect(-1, 0, 1, 1), rect(100000, 0, 1, 1), q.Roi.points([q.Point(1, 1)])):
        with pytest.raises(ValueError):
            doc.update_nucleus(cell, invalid)
        assert (doc.revision, doc.history_bytes, doc.redo_label) == (revision, history, label)
    with pytest.raises(ValueError, match="area"):
        doc.add_cell(q.Roi.polyline([q.Point(0, 0), q.Point(1, 1)]))
    assert doc.redo() and doc.annotation(cell).name == "Changed"
    assert doc.add_cell(rect(0, 0, 2, 2)) == cell + 1


def test_compound_conversion_subtree_delete_and_nucleus_budget():
    doc = q.Document(q.Image.demo())
    parent = doc.add_rectangle(q.Rect(0, 0, 100, 100))
    cell = doc.add_detection(rect(10, 10, 20, 20), parent_id=parent)
    doc.execute([q.ObjectEdit.replace_geometry(cell, rect(10, 10, 20, 20), "Cell",
                parent, "Tumor", q.ObjectKind.CELL, rect(12, 12, 5, 5))], "Convert detection")
    assert doc.annotation(cell).kind == q.ObjectKind.CELL
    assert doc.undo() and doc.annotation(cell).kind == q.ObjectKind.DETECTION
    assert doc.redo() and doc.annotation(cell).nucleus is not None
    assert doc.remove_annotation(parent) and doc.object_count == 0
    assert doc.undo() and doc.annotation(cell).nucleus.area == 25
    doc.clear_history()
    doc.update_nucleus(cell, None)
    clear_payload = doc.history_bytes
    doc.clear_history()
    doc.update_nucleus(cell, rect(12, 12, 5, 5))
    assert doc.history_bytes == clear_payload
    doc.clear_history()
    revision = doc.revision
    doc.update_nucleus(cell, rect(12, 12, 5, 5))
    assert doc.history_bytes == 0 and doc.revision == revision
    doc.set_history_limits(128, 1)
    with pytest.raises(ValueError, match="budget"):
        doc.update_nucleus(cell, None)
    assert doc.annotation(cell).nucleus.area == 25
