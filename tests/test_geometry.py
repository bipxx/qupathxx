import math
from concurrent.futures import ThreadPoolExecutor

import pytest

import qupathxx as q


def ring(x0, y0, x1, y1):
    return [q.Point(x0, y0), q.Point(x1, y0), q.Point(x1, y1), q.Point(x0, y1)]


def test_ellipse_analytic_area_mask_and_translation():
    shape = q.Roi.ellipse(q.Rect(0, 0, 4, 4))
    assert shape.kind == q.RoiKind.ELLIPSE
    assert shape.area == pytest.approx(4 * math.pi)
    assert shape.perimeter == pytest.approx(4 * math.pi, rel=1e-4)
    assert shape.contains(q.Point(2, 2)) and not shape.contains(q.Point(.1, .1))
    moved = shape.translated(10, 20)
    assert moved.bounds.x == 10 and moved.bounds.y == 20
    assert moved.area == shape.area
    assert moved.contains(q.Point(12, 22)) and not moved.contains(q.Point(2, 2))
    image = q.Image(4, 4, [v for i in range(16) for v in [i, i, i]])
    doc = q.Document(image)
    annotation = doc.add_roi(shape)
    selected = [y * 4 + x for y in range(4) for x in range(4) if ((x + .5 - 2) / 2) ** 2 + ((y + .5 - 2) / 2) ** 2 <= 1]
    assert doc.mean_intensity(annotation) == pytest.approx(sum(selected) / len(selected))
    doc.set_calibration(.25, .5)
    assert doc.area_um2(annotation) == pytest.approx(math.pi / 2)


def test_ellipse_distance_uses_analytic_boundary_not_draw_tessellation():
    shape = q.Roi.ellipse(q.Rect(0, 0, 10, 4))
    for angle in [0, .2, .7, 1.2, math.pi / 2]:
        x, y = 5 + 5 * math.cos(angle), 2 + 2 * math.sin(angle)
        nx, ny = math.cos(angle) / 5, math.sin(angle) / 2
        norm = math.hypot(nx, ny)
        for distance in [.001, 2, 100]:
            point = q.Point(x + distance * nx / norm, y + distance * ny / norm)
            assert shape.distance(point) == pytest.approx(distance, abs=1e-10)
    assert shape.perimeter == pytest.approx(23.01311259566484, rel=1e-10)
    assert not shape.covers(q.Roi.ellipse(q.Rect(8, 1, 2, 2)))  # BBox fits; ellipse crosses at curved edge.
    assert shape.covers(q.Roi.ellipse(q.Rect(7.9, 1, 2, 2)))


def test_lines_and_points_bounds_length_and_owning_paths():
    line = q.Roi.polyline([q.Point(0, 0), q.Point(3, 0), q.Point(3, 4)])
    assert line.kind == q.RoiKind.POLYLINE and line.area == 0
    assert line.perimeter == 7
    assert line.distance(q.Point(1, 2)) == 2
    assert line.contains(q.Point(3, 2))
    points = q.Roi.points([q.Point(1, 2), q.Point(4, 6)])
    assert points.bounds.width == 3 and points.bounds.height == 4
    paths = points.paths
    paths[0][0].x = 999
    assert points.contains(q.Point(1, 2))
    assert points.distance(q.Point(1, 3)) == 1
    doc = q.Document(q.Image.demo())
    line_id = doc.add_roi(line)
    with pytest.raises(ValueError, match="area ROI"):
        doc.mean_intensity(line_id)


@pytest.mark.parametrize("factory", [
    lambda: q.Roi.polyline([q.Point(0, 0)]),
    lambda: q.Roi.polyline([q.Point(0, 0), q.Point(0, 0)]),
    lambda: q.Roi.points([]),
    lambda: q.Roi.points([q.Point(math.nan, 0)]),
    lambda: q.Roi.ellipse(q.Rect(0, 0, -1, 1)),
])
def test_invalid_native_geometry(factory):
    with pytest.raises(ValueError):
        factory()


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS topology")
def test_polygon_holes_metrics_masks_and_parent_coverage():
    polygon = q.Roi.polygon(ring(0, 0, 10, 10), [ring(1, 1, 4, 4)])
    assert polygon.area == 91
    assert polygon.perimeter == 52
    assert len(polygon.polygon_parts) == 1 and len(polygon.polygon_parts[0].holes) == 1
    assert polygon.contains(q.Point(.5, .5)) and not polygon.contains(q.Point(2, 2))
    assert polygon.covers(q.Roi.rectangle(q.Rect(5, 5, 1, 1)))
    assert not polygon.covers(q.Roi.rectangle(q.Rect(2, 2, 1, 1)))
    doc = q.Document(q.Image(10, 10, [v for i in range(100) for v in [i, i, i]]))
    parent = doc.add_roi(polygon, "With hole")
    expected = [y * 10 + x for y in range(10) for x in range(10) if not (1 <= x < 4 and 1 <= y < 4)]
    assert doc.mean_intensity(parent) == pytest.approx(sum(expected) / len(expected))
    before = doc.revision
    with pytest.raises(ValueError, match="contain"):
        doc.execute([q.ObjectEdit.create(q.Rect(2, 2, 1, 1), "Inside hole", parent)])
    assert doc.revision == before and doc.object_count == 1
    child = doc.execute([q.ObjectEdit.create(q.Rect(5, 5, 1, 1), "Inside tissue", parent)])[0]
    doc.execute([
        q.ObjectEdit.replace_geometry(parent, polygon.translated(0, 0), "Renamed"),
        q.ObjectEdit.replace_geometry(child, q.Roi.ellipse(q.Rect(5, 5, 1, 1)), "Cell", parent, "Tumor"),
    ], "Compound shape edit")
    assert doc.undo() and doc.annotation(child).geometry.kind == q.RoiKind.RECTANGLE
    assert doc.redo() and doc.annotation(child).geometry.kind == q.RoiKind.ELLIPSE
    assert doc.annotation(parent).geometry.wkt == polygon.wkt
    triangle = q.Roi.polygon([q.Point(0, 0), q.Point(10, 0), q.Point(0, 10)])
    assert triangle.covers(q.Roi.ellipse(q.Rect(3.2, 3.2, 2, 2)))
    assert not triangle.covers(q.Roi.ellipse(q.Rect(3.3, 3.3, 2, 2)))


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS topology")
def test_multipart_and_exact_hierarchy_with_equal_bounds():
    multi = q.Roi.multipolygon([q.PolygonPart(ring(0, 0, 2, 2)), q.PolygonPart(ring(5, 0, 7, 2))])
    assert multi.kind == q.RoiKind.MULTIPOLYGON and multi.area == 8 and multi.perimeter == 16
    assert len(multi.polygon_parts) == 2
    parts = multi.polygon_parts
    parts[0].shell[0].x = 500
    assert multi.polygon_parts[0].shell[0].x == 0
    assert not multi.contains(q.Point(3, 1)) and multi.contains(q.Point(6, 1))
    doc = q.Document(q.Image.demo())
    parent = doc.add_rectangle(q.Rect(0, 0, 10, 10))
    child = doc.execute([q.ObjectEdit.create(q.Rect(2, 2, 1, 1), "Child", parent)])[0]
    revision = doc.revision
    with pytest.raises(ValueError, match="contain"):
        doc.update_roi(parent, q.Roi.polygon(ring(0, 0, 10, 10), [ring(1, 1, 4, 4)]))
    assert doc.revision == revision and doc.children(parent) == [child]
    a = doc.add_roi(multi)
    old = doc.annotation(a).geometry
    doc.rename_annotation(a, "Multipart")
    assert doc.annotation(a).geometry.equals(old)
    doc.update_roi(a, multi.translated(20, 20))
    assert doc.undo() and doc.annotation(a).geometry.equals(old)
    assert old.contains(q.Point(6, 1))


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS topology")
@pytest.mark.parametrize("factory", [
    lambda: q.Roi.polygon([q.Point(0, 0), q.Point(4, 4), q.Point(0, 4), q.Point(4, 0)]),
    lambda: q.Roi.polygon(ring(0, 0, 2, 2), [ring(3, 3, 4, 4)]),
    lambda: q.Roi.multipolygon([q.PolygonPart(ring(0, 0, 3, 3)), q.PolygonPart(ring(1, 1, 4, 4))]),
    lambda: q.Roi.polygon([q.Point(0, 0), q.Point(1, 1)]),
])
def test_invalid_topology_is_rejected(factory):
    with pytest.raises(ValueError):
        factory()


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS topology")
def test_shared_polygon_queries_and_snapshot_lifetime():
    polygon = q.Roi.polygon(ring(0, 0, 100, 100), [ring(25, 25, 75, 75)])
    with ThreadPoolExecutor(max_workers=4) as pool:
        actual = list(pool.map(lambda i: polygon.contains(q.Point(i % 100 + .5, i // 100 + .5)), range(10000)))
    assert sum(actual) == 7500
    doc = q.Document(q.Image.demo())
    a = doc.add_roi(polygon)
    snapshot = doc.annotation(a).geometry
    doc.remove_annotation(a)
    del doc, polygon
    assert snapshot.area == 7500 and not snapshot.contains(q.Point(50, 50))
