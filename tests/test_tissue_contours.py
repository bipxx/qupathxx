"""Independent raster, area and history checks for tissue contour conversion."""
import gc
import math

import numpy as np
import pytest
import qupathxx as q

from test_tissue import document, labels
from test_tissue_levels import pyramid_levels

pytestmark = pytest.mark.skipif(not q.has_geos, reason="contours require GEOS")


def job_for(mask, connectivity=8, tile=256):
    doc = document(np.where(mask[..., None], 0, 255).repeat(3, axis=2))
    obj = doc.add_rectangle(q.Rect(0, 0, mask.shape[1], mask.shape[0]))
    parameters = q.TissueParameters()
    parameters.connectivity, parameters.tile_size = connectivity, tile
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), parameters))
    assert job.wait(doc, obj).state == "ready"
    return doc, obj, job


def signed_area(path):
    xy = np.asarray([(p.x, p.y) for p in path])
    return .5*np.sum(xy[:, 0]*np.roll(xy[:, 1], -1)-xy[:, 1]*np.roll(xy[:, 0], -1))


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("connectivity", [4, 8])
@pytest.mark.parametrize("tile", [16, 31])
def test_contours_match_every_raster_label_area_and_perimeter(seed, connectivity, tile):
    mask = np.random.default_rng(seed).random((13, 17)) > .56
    mask[2:9, 3:13] = True
    mask[4:6, 6:9] = False
    doc, obj, job = job_for(mask, connectivity, tile)
    raster = labels(job.result)
    options = q.TissueContourOptions()
    options.clip_to_roi = False
    contours = q.trace_tissue(job.result, options)
    assert contours.component_count == job.result.component_count
    assert contours.input_sha256 == job.result.input_sha256 and contours.object_id == obj
    assert contours.algorithm == "qpx.pixel-contours/1"
    assert contours.retained_bytes <= contours.estimated_work_bytes <= options.work_bytes
    total = 0
    for contour in contours.components:
        expected = raster == contour.label
        assert contour.pixels == int(expected.sum()) and contour.geometry.area == contour.pixels
        for y, x in np.ndindex(raster.shape):
            assert contour.geometry.contains(q.Point(x+.5, y+.5)) == bool(expected[y, x])
        padded = np.pad(expected, 1)
        perimeter = int(np.count_nonzero(np.diff(padded, axis=0))+np.count_nonzero(np.diff(padded, axis=1)))
        assert contour.geometry.perimeter == perimeter
        area = 0
        for part in contour.geometry.polygon_parts:
            area += abs(signed_area(part.shell))-sum(abs(signed_area(hole)) for hole in part.holes)
        assert area == contour.pixels
        total += area
    assert total == job.result.foreground_pixels
    assert doc.object_count == 1  # Conversion is read-only until explicit publication.


def test_holes_nested_islands_and_diagonal_parts_are_real_geometry():
    mask = np.zeros((17, 17), dtype=bool)
    mask[1:16, 1:16] = True
    mask[4:13, 4:13] = False
    mask[7:10, 7:10] = True
    _, _, job = job_for(mask, 4)
    contours = q.trace_tissue(job.result)
    outer, inner = contours.components
    assert contours.component_count == 2
    assert len(outer.geometry.polygon_parts[0].holes) == 1 and outer.geometry.area == 144
    assert outer.geometry.perimeter == 96 and inner.geometry.area == 9
    assert not outer.geometry.contains(q.Point(8.5, 8.5)) and inner.geometry.contains(q.Point(8.5, 8.5))
    _, _, job = job_for(np.eye(3, dtype=bool), 8)
    diagonal = q.trace_tissue(job.result).components[0]
    assert diagonal.geometry.kind == q.RoiKind.MULTIPOLYGON and len(diagonal.geometry.polygon_parts) == 3
    assert diagonal.geometry.area == 3 and diagonal.geometry.perimeter == 12


def test_straight_runs_compress_without_approximating_the_mask():
    _, _, job = job_for(np.ones((65, 67), dtype=bool))
    options = q.TissueContourOptions()
    options.max_edges = options.max_vertices = 4
    result = q.trace_tissue(job.result, options)
    assert result.edge_count == result.vertex_count == 4 and result.components[0].geometry.area == 65*67
    options.max_vertices = 1
    result.options.max_vertices = 2
    result.components[0].geometry.bounds.x = 999
    assert result.options.max_vertices == 4 and result.components[0].geometry.bounds.x == 0


def test_fractional_roi_clipping_and_calibrated_child_publication():
    doc = document(np.zeros((14, 18, 3), dtype=np.uint8))
    source = doc.add_rectangle(q.Rect(2.2, 3.1, 9.5, 7.6))
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, source, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert job.wait(doc, source).state == "ready"
    raw = q.TissueContourOptions()
    raw.clip_to_roi = False
    assert q.trace_tissue(job.result, raw).components[0].geometry.area == 80
    assert job.request_contours() and not job.request_contours()
    assert job.result is not None and job.wait(doc, source).state == "ready"
    contour = job.contours.components[0]
    assert contour.pixels == 80 and contour.geometry.area == pytest.approx(9.5*7.6)
    doc.set_calibration(.25, .5)
    assert job.poll(doc, source).state == "ready"
    ids = job.keep_contours(doc, source, name_prefix="Region", class_name="Tissue")
    assert len(ids) == 1 and doc.annotation(ids[0]).parent_id == source
    assert doc.annotation(ids[0]).name == "Region 1" and doc.area_um2(ids[0]) == pytest.approx(9.5*7.6*.25*.5)


@pytest.mark.parametrize("level", [1, 2])
def test_pyramid_contours_use_level_zero_edges_and_optional_roi_clip(pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    rectangle = q.Rect(13.2, 17.1, 231.5, 189.6)
    obj = doc.add_rectangle(rectangle)
    p = q.TissueParameters()
    p.level, p.threshold = level, 0
    mask = q.detect_tissue(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p))
    assert mask.foreground_pixels == mask.width*mask.height
    options = q.TissueContourOptions()
    options.clip_to_roi = False
    raw = q.trace_tissue(mask, options).components[0]
    assert raw.geometry.area == pytest.approx(mask.foreground_pixels*mask.downsample**2)
    assert raw.geometry.bounds.x == mask.bounds.x and raw.geometry.bounds.y == mask.bounds.y
    clipped = q.trace_tissue(mask).components[0]
    b = mask.bounds
    width = min(b.x+b.width, rectangle.x+rectangle.width)-max(b.x, rectangle.x)
    height = min(b.y+b.height, rectangle.y+rectangle.height)-max(b.y, rectangle.y)
    assert clipped.geometry.area == pytest.approx(width*height)
    assert q.Roi.rectangle(rectangle).covers(clipped.geometry)


def test_ellipse_clip_has_documented_polygon_error_and_parent_containment():
    doc = document(np.zeros((40, 40, 3), dtype=np.uint8))
    b = q.Rect(5.2, 5.3, 18.1, 12.4)
    roi = q.Roi.ellipse(b)
    obj = doc.add_roi(roi)
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert job.wait(doc, obj).state == "ready" and job.request_contours()
    assert job.wait(doc, obj).state == "ready"
    result = job.result
    nodes, weights = np.polynomial.legendre.leggauss(256)
    cx, cy, rx, ry = b.x+b.width/2, b.y+b.height/2, b.width/2, b.height/2
    exact = 0.
    for yy, xx in np.argwhere(labels(result) != 0):
        left, top = result.bounds.x+xx, result.bounds.y+yy
        xs = left+(nodes+1)/2
        half = ry*np.sqrt(np.maximum(0, 1-((xs-cx)/rx)**2))
        heights = np.maximum(0, np.minimum(top+1, cy+half)-np.maximum(top, cy-half))
        exact += .5*np.dot(weights, heights)
    geometry = job.contours.components[0].geometry
    assert exact-math.pi*rx*ry*3e-5-.001 <= geometry.area <= exact+.001
    assert roi.covers(roi.as_polygon()) and roi.covers(geometry)
    assert not roi.covers(q.Roi.rectangle(q.Rect(b.x-1e-5, cy-.001, .002, .002)))
    assert job.keep_contours(doc, obj)


def test_fractional_source_hole_clips_pixel_footprints_before_child_creation():
    def ring(a, b, c, d):
        return [q.Point(a, b), q.Point(c, b), q.Point(c, d), q.Point(a, d)]

    source_roi = q.Roi.polygon(ring(.2, .3, 15.7, 15.8), [ring(4.2, 4.3, 10.7, 10.8)])
    doc = document(np.zeros((17, 17, 3), dtype=np.uint8))
    obj = doc.add_roi(source_roi)
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert job.wait(doc, obj).state == "ready" and job.request_contours()
    assert job.wait(doc, obj).state == "ready"
    expected = 0
    for y, x in np.argwhere(labels(job.result) != 0):
        left, top = job.result.bounds.x+x, job.result.bounds.y+y
        def overlap(a, b, c, d):
            return max(0, min(left+1, c)-max(left, a))*max(0, min(top+1, d)-max(top, b))
        expected += overlap(.2, .3, 15.7, 15.8)-overlap(4.2, 4.3, 10.7, 10.8)
    geometry = job.contours.components[0].geometry
    assert geometry.area == pytest.approx(expected) and source_roi.covers(geometry)
    assert not geometry.contains(q.Point(5, 5))
    assert doc.annotation(job.keep_contours(doc, obj)[0]).parent_id == obj


@pytest.mark.parametrize("name,value", [("max_components", 0), ("max_components", 1000001), ("max_edges", 0),
    ("max_edges", 1000001), ("max_vertices", 0), ("max_vertices", 1000001), ("work_bytes", 0), ("work_bytes", 512*1024*1024+1)])
def test_invalid_contour_limits(name, value):
    options = q.TissueContourOptions()
    setattr(options, name, value)
    with pytest.raises(ValueError):
        options.validate()


@pytest.mark.parametrize("name,value", [("max_components", 1), ("max_edges", 3), ("max_vertices", 3), ("work_bytes", 1)])
def test_resource_failure_is_atomic_and_can_retry(name, value):
    doc, obj, job = job_for(np.eye(3, dtype=bool), 4)
    revision = doc.revision
    options = q.TissueContourOptions()
    setattr(options, name, value)
    assert job.request_contours(options)
    status = job.wait(doc, obj)
    assert status.state == "failed" and status.error and job.contours is None and job.result is not None
    assert doc.revision == revision and doc.object_count == 1
    assert job.request_contours() and job.wait(doc, obj).state == "ready"
    assert len(job.keep_contours(doc, obj)) == 3


def test_publication_is_one_undoable_batch_and_invalid_metadata_changes_nothing():
    doc, obj, job = job_for(np.eye(3, dtype=bool), 4)
    assert job.request_contours() and job.wait(doc, obj).state == "ready"
    doc.clear_history()
    revision = doc.revision
    with pytest.raises(ValueError, match="4096"):
        job.keep_contours(doc, obj, name_prefix="x"*4096, class_name="Tissue")
    assert doc.revision == revision and doc.object_count == 1 and not doc.classes and not job.contours_published
    ids = job.keep_contours(doc, obj, class_name="Tissue")
    assert len(ids) == 3 and doc.children(obj) == ids and doc.class_count("Tissue") == 3
    assert doc.undo_label == "Add tissue annotations" and job.contours_published
    with pytest.raises(RuntimeError, match="already published"):
        job.keep_contours(doc, obj)
    assert doc.undo() and doc.object_count == 1 and not doc.classes and not doc.can_undo
    assert doc.redo() and doc.object_count == 4 and doc.children(obj) == ids


def test_unclipped_footprints_publish_at_root_and_empty_masks_are_noops():
    doc, obj, job = job_for(np.eye(3, dtype=bool), 4)
    options = q.TissueContourOptions()
    options.clip_to_roi = False
    assert job.request_contours(options) and job.wait(doc, obj).state == "ready"
    assert all(doc.annotation(i).parent_id == 0 for i in job.keep_contours(doc, obj))
    doc, obj, job = job_for(np.zeros((8, 9), dtype=bool))
    assert job.request_contours() and job.wait(doc, obj).state == "ready"
    assert job.contours.component_count == job.contours.edge_count == job.contours.vertex_count == 0
    revision = doc.revision
    doc.clear_history()
    assert job.keep_contours(doc, obj) == [] and doc.revision == revision and not doc.can_undo


@pytest.mark.parametrize("change", ["object", "selection", "close", "authority"])
def test_stale_work_never_publishes_and_captured_geometry_outlives_owner(change):
    doc, obj, job = job_for(np.ones((65, 67), dtype=bool))
    assert job.request_contours() and job.wait(doc, obj).state == "ready"
    saved = job.contours
    assert job.request_contours()
    selected = obj
    if change == "object":
        doc.rename_annotation(obj, "Edited")
        doc.undo()
    elif change == "selection":
        selected = 0
    elif change == "close":
        doc.server.close()
    else:
        doc = q.Document(doc.server)
        assert doc.add_rectangle(q.Rect(0, 0, 67, 65)) == obj
    assert job.wait(doc, selected).state == "stale" and job.contours is None
    with pytest.raises(RuntimeError, match="no current"):
        job.keep_contours(doc, selected)
    del job, doc
    gc.collect()
    assert saved.components[0].geometry.area == 65*67


def test_contour_cancel_reuse_and_independent_workers():
    doc, obj, first = job_for(np.ones((65, 67), dtype=bool))
    assert first.request_contours()
    first.cancel()
    assert first.wait(doc, obj).state == "cancelled" and first.contours is None and first.result is None
    second = q.TissueJob()
    assert first.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert second.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert first.wait(doc, obj).state == second.wait(doc, obj).state == "ready"
    assert first.request_contours() and second.request_contours()
    first.cancel()
    assert first.wait(doc, obj).state == "cancelled" and second.wait(doc, obj).state == "ready"
    assert second.contours.components[0].geometry.area == 65*67


@pytest.mark.skipif(not q.has_project or not q.has_interchange, reason="requires project/interchange")
def test_created_geometry_roundtrips_project_and_geojson(tmp_path):
    doc, obj, job = job_for(np.eye(3, dtype=bool), 8)
    assert job.request_contours() and job.wait(doc, obj).state == "ready"
    ids = job.keep_contours(doc, obj, class_name="Tissue")
    original = doc.annotation(ids[0]).geometry.wkt
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"contours.qpxproj"
    project.save(str(path))
    loaded = q.Project.open(str(path)).document(image)
    assert loaded.annotation(ids[0]).geometry.wkt == original and loaded.annotation(ids[0]).parent_id == obj
    exported = q.export_geojson(doc)
    target = q.Document(q.Image(3, 3, [255]*27))
    report = q.import_geojson(target, exported.text)
    assert len(report.imported_ids) == 2
    child = target.annotation(report.imported_ids[1])
    assert child.geometry.area == 3 and len(child.geometry.polygon_parts) == 3
    assert child.parent_id == report.imported_ids[0] and child.class_name == "Tissue"
