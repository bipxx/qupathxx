import math
import random

import pytest

import qupathxx as q


def test_index_updates_undo_and_metadata_reuse():
    doc = q.Document(q.Image.demo())
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i * 10, 0, 5, 5), str(i)) for i in range(80)])
    assert doc.query_region(q.Rect(0, 0, 15, 5)) == ids[:2]
    builds = doc.spatial_stats.builds
    doc.rename_annotation(ids[0], "Metadata only")
    assert doc.query_region(q.Rect(0, 0, 15, 5)) == ids[:2]
    assert doc.spatial_stats.builds == builds and doc.spatial_stats.dirty_entries == 0
    doc.update_rectangle(ids[0], q.Rect(900, 200, 5, 5))
    assert doc.query_region(q.Rect(0, 0, 15, 5)) == ids[1:2]
    assert doc.pick(q.Point(902, 202)) == [ids[0]]
    assert doc.spatial_stats.builds == builds and doc.spatial_stats.dirty_entries == 1
    assert doc.undo() and doc.pick(q.Point(2, 2)) == [ids[0]]
    assert doc.redo() and doc.pick(q.Point(902, 202)) == [ids[0]]
    assert doc.remove_annotation(ids[0]) and not doc.pick(q.Point(902, 202))
    assert doc.undo() and doc.pick(q.Point(902, 202)) == [ids[0]]


def test_spatial_rebuild_threshold_and_failed_batch():
    doc = q.Document(q.Image.demo())
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, 0, 1, 1), str(i)) for i in range(400)])
    assert len(doc.query_region(q.Rect(0, 0, 500, 10))) == 400
    builds = doc.spatial_stats.builds
    doc.execute([q.ObjectEdit.replace(i, q.Rect((i - 1) % 1000, 200, 1, 1), str(i)) for i in ids[:300]])
    assert doc.spatial_stats.base_entries == 0
    assert len(doc.query_region(q.Rect(0, 199, 500, 3))) == 300
    assert doc.spatial_stats.builds == builds + 1
    with pytest.raises(ValueError):
        doc.execute([q.ObjectEdit.replace(ids[0], q.Rect(0, 400, 1, 1), "Moved"),
                     q.ObjectEdit.create(q.Rect(-1, 0, 1, 1))])
    assert doc.pick(q.Point(.5, 200.5)) == [ids[0]]
    assert not doc.query_region(q.Rect(0, 400, 1, 1))
    assert doc.undo() and len(doc.query_region(q.Rect(0, 0, 500, 10))) == 400


def test_queries_match_independent_brute_force():
    rng = random.Random(731)
    boxes = [(rng.uniform(0, 980), rng.uniform(0, 720), rng.uniform(.1, 30), rng.uniform(.1, 30)) for _ in range(2000)]
    doc = q.Document(q.Image.demo())
    ids = doc.execute([q.ObjectEdit.create(q.Rect(*box)) for box in boxes])
    for _ in range(100):
        query = (rng.uniform(-100, 1000), rng.uniform(-100, 750), 80, 60)
        x, y, width, height = query
        expected = [i for i, (bx, by, bw, bh) in zip(ids, boxes) if bx <= x + width and x <= bx + bw and by <= y + height and y <= by + bh]
        assert doc.query_region(q.Rect(*query)) == expected
    with pytest.raises(ValueError):
        doc.query_region(q.Rect(math.nan, 0, 1, 1))
    with pytest.raises(ValueError):
        doc.pick(q.Point(0, 0), -1)


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS topology")
def test_picking_checks_geometry_not_just_bounding_boxes():
    p = q.Point
    outer = [p(0, 0), p(10, 0), p(10, 10), p(0, 10)]
    hole = [p(2, 2), p(8, 2), p(8, 8), p(2, 8)]
    doc = q.Document(q.Image.demo())
    polygon = doc.add_roi(q.Roi.polygon(outer, [hole]))
    assert doc.pick(p(1, 1)) == [polygon]
    assert not doc.pick(p(5, 5))
    assert doc.query_region(q.Rect(5, 5, 0, 0)) == [polygon]  # Broad phase is deliberately bounds-only.
    point = doc.add_roi(q.Roi.points([p(20, 20)]))
    assert doc.pick(p(20.5, 20), .6) == [point]
    assert not doc.pick(p(20.5, 20), .4)
    doc.update_roi(polygon, q.Roi.rectangle(q.Rect(0, 0, 10, 10)))
    assert doc.pick(p(5, 5)) == [polygon]  # Same bounds, new exact shape.
