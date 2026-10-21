import math

import pytest

import qupathxx as q

geos = pytest.mark.skipif(not q.has_geos, reason="requires GEOS")


def ring(x, y, w, h):
    return [q.Point(x, y), q.Point(x+w, y), q.Point(x+w, y+h), q.Point(x, y+h)]


def rect(x, y, w, h):
    return q.Roi.rectangle(q.Rect(x, y, w, h))


@geos
@pytest.mark.parametrize("operation,area", [(q.BooleanOp.UNION, 150), (q.BooleanOp.INTERSECTION, 50),
                                          (q.BooleanOp.DIFFERENCE, 50), (q.BooleanOp.SYMMETRIC_DIFFERENCE, 100)])
def test_boolean_area_against_independent_rectangles(operation, area):
    a, b = rect(0, 0, 10, 10), rect(5, 0, 10, 10)
    result = a.boolean_operation(b, operation)
    assert result.area == area
    assert a.area == b.area == 100
    assert result.kind in (q.RoiKind.POLYGON, q.RoiKind.MULTIPOLYGON)
    # Independently evaluate each unit-pixel center as a Boolean mask.
    for y in range(10):
        for x in range(15):
            left, right = x < 10, x >= 5
            expected = {q.BooleanOp.UNION: left or right, q.BooleanOp.INTERSECTION: left and right,
                        q.BooleanOp.DIFFERENCE: left and not right, q.BooleanOp.SYMMETRIC_DIFFERENCE: left != right}[operation]
            assert result.contains(q.Point(x+.5, y+.5)) == expected


@geos
def test_boolean_holes_multipart_and_empty_dimensional_results():
    a = rect(0, 0, 10, 10)
    hole = a.boolean_operation(rect(2, 2, 6, 6), q.BooleanOp.DIFFERENCE)
    assert hole.area == 64 and len(hole.polygon_parts[0].holes) == 1
    split = a.boolean_operation(rect(4, 0, 2, 10), q.BooleanOp.DIFFERENCE)
    assert split.kind == q.RoiKind.MULTIPOLYGON and len(split.polygon_parts) == 2
    assert split.area == 80
    assert a.boolean_operation(rect(10, 0, 10, 10), q.BooleanOp.INTERSECTION) is None
    assert a.boolean_operation(rect(20, 20, 1, 1), q.BooleanOp.INTERSECTION) is None
    assert a.boolean_operation(a, q.BooleanOp.SYMMETRIC_DIFFERENCE) is None
    with pytest.raises(ValueError, match="area"):
        a.boolean_operation(q.Roi.polyline([q.Point(0, 0), q.Point(1, 1)]), q.BooleanOp.UNION)


@geos
def test_ellipse_polygon_conversion_is_explicit_and_bounded():
    ellipse = q.Roi.ellipse(q.Rect(0, 0, 20, 10))
    polygon = ellipse.as_polygon()
    assert len(polygon.paths[0]) == 512
    assert polygon.area == pytest.approx(ellipse.area, rel=3e-5)
    result = ellipse.boolean_operation(rect(0, 0, 20, 10), q.BooleanOp.INTERSECTION)
    assert result.area == pytest.approx(polygon.area, rel=1e-13)
    assert ellipse.area == 50*math.pi


@geos
def test_vertex_hole_and_multipart_edits_preserve_grouping_and_snapshots():
    original = rect(0, 0, 10, 10)
    hole = original.with_hole(0, ring(2, 2, 6, 6))
    multipart = q.Roi.multipolygon([hole.polygon_parts[0], q.PolygonPart(ring(20, 0, 10, 10))])
    moved = multipart.with_vertex(2, 0, q.Point(19, 0))
    assert moved.area == 169 and multipart.area == 164
    assert len(moved.polygon_parts) == 2 and len(moved.polygon_parts[0].holes) == 1
    inserted = moved.inserted_vertex(1, 1, q.Point(5, 2))
    assert len(inserted.paths[1]) == 5 and inserted.area == moved.area
    assert inserted.without_vertex(1, 1).equals(moved)
    assert hole.without_hole(0, 0).area == 100 and original.area == 100
    with pytest.raises(ValueError, match="invalid"):
        original.as_polygon().with_vertex(0, 0, q.Point(15, 5))
    with pytest.raises(ValueError, match="invalid"):
        hole.with_hole(0, ring(3, 3, 1, 1))
    with pytest.raises(IndexError):
        moved.with_vertex(100, 0, q.Point(0, 0))
    with pytest.raises(ValueError, match="three"):
        q.Roi.polygon([q.Point(0, 0), q.Point(10, 0), q.Point(0, 10)]).without_vertex(0, 0)


def test_line_point_edits_and_minimum_sizes():
    line = q.Roi.polyline([q.Point(0, 0), q.Point(3, 0), q.Point(3, 4)])
    assert line.with_vertex(0, 2, q.Point(3, 8)).perimeter == 11
    assert line.inserted_vertex(0, 1, q.Point(1, 0)).without_vertex(0, 1).equals(line)
    with pytest.raises(ValueError):
        line.without_vertex(0, 0).without_vertex(0, 0)
    point = q.Roi.points([q.Point(1, 1)])
    assert point.inserted_vertex(0, 1, q.Point(2, 2)).without_vertex(0, 0).contains(q.Point(2, 2))
    with pytest.raises(ValueError):
        point.without_vertex(0, 0)
    with pytest.raises(ValueError):
        line.with_vertex(0, 0, q.Point(float("nan"), 0))


@geos
def test_document_edit_history_parent_containment_index_and_failed_budget():
    doc = q.Document(q.Image.demo())
    parent = doc.add_roi(rect(0, 0, 20, 20).as_polygon())
    child = doc.add_cell(rect(3, 3, 2, 2), parent_id=parent, class_name="Tumor")
    snapshot = doc.annotation(parent).geometry
    assert doc.pick(q.Point(4, 4)) == [parent, child]
    builds, revision, history = doc.spatial_stats.builds, doc.revision, doc.history_bytes
    with pytest.raises(ValueError, match="contain"):
        doc.add_hole(parent, 0, ring(2, 2, 6, 6))
    assert (doc.revision, doc.history_bytes) == (revision, history)
    assert doc.class_count("Tumor") == 1 and doc.spatial_stats.builds == builds
    cutter = doc.add_roi(rect(2, 2, 6, 6))
    revision, history = doc.revision, doc.history_bytes
    with pytest.raises(ValueError, match="contain"):
        doc.apply_boolean(parent, [cutter], q.BooleanOp.DIFFERENCE)
    assert (doc.revision, doc.history_bytes) == (revision, history)
    assert doc.annotation(parent).geometry.equals(snapshot)
    doc.add_hole(parent, 0, ring(10, 10, 3, 3))
    assert doc.pick(q.Point(11, 11)) == [] and doc.spatial_stats.builds == builds
    assert doc.undo() and doc.annotation(parent).geometry.equals(snapshot)
    assert doc.redo() and doc.annotation(parent).geometry.area == 391
    doc.remove_hole(parent, 0, 0)
    doc.insert_vertex(parent, 0, 1, q.Point(10, 0))
    assert doc.undo_label == "Insert vertex"
    doc.remove_vertex(parent, 0, 1)
    assert doc.annotation(parent).geometry.equals(snapshot)
    doc.clear_history()
    revision = doc.revision
    doc.edit_vertex(parent, 0, 0, q.Point(0, 0))
    assert doc.revision == revision and not doc.can_undo
    doc.set_history_limits(128, 1)
    with pytest.raises(ValueError, match="budget"):
        doc.edit_vertex(parent, 0, 0, q.Point(1, 0))
    assert doc.revision == revision and doc.annotation(parent).geometry.equals(snapshot)


@geos
def test_document_boolean_preserves_inputs_metadata_and_empty_xor_fold():
    doc = q.Document(q.Image.demo())
    first = doc.add_cell(rect(0, 0, 10, 10), rect(1, 1, 2, 2), class_name="Tumor")
    second = doc.add_roi(rect(5, 0, 10, 10))
    doc.apply_boolean(first, [second], q.BooleanOp.UNION)
    assert doc.annotation(first).geometry.area == 150 and doc.object_count == 2
    assert doc.annotation(first).kind == q.ObjectKind.CELL and doc.annotation(first).nucleus.area == 4
    assert doc.annotation(second).geometry.area == 100 and doc.class_count("Tumor") == 1
    assert doc.undo() and doc.annotation(first).geometry.area == 100
    same = doc.add_roi(rect(0, 0, 10, 10))
    third = doc.add_roi(rect(20, 20, 1, 1))
    doc.apply_boolean(first, [same, third], q.BooleanOp.SYMMETRIC_DIFFERENCE)
    assert doc.annotation(first).geometry.area == 1
    assert doc.undo() and doc.annotation(first).geometry.area == 100
    revision, history, redo = doc.revision, doc.history_bytes, doc.redo_label
    for operands in ([same], [first], [], [second, second], [999]):
        with pytest.raises((ValueError, IndexError)):
            doc.apply_boolean(first, operands, q.BooleanOp.SYMMETRIC_DIFFERENCE)
        assert (doc.revision, doc.history_bytes, doc.redo_label) == (revision, history, redo)
