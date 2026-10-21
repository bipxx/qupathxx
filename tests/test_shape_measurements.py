"""Native shape features: independent metrics, calibration keys and bounded reuse."""
import math
import pytest
import qupathxx as q


def document():
    return q.Document(q.Image(128, 128, [40, 80, 160]*128*128))


def ellipse_length(a, b):
    # Independent composite Gauss-Legendre reference, not native adaptive Simpson.
    import numpy as np
    nodes, weights = np.polynomial.legendre.leggauss(64)
    total = 0.0
    for segment in range(32):
        left, right = segment*math.pi/64, (segment+1)*math.pi/64
        theta = (right-left)*nodes/2+(left+right)/2
        total += (right-left)/2*float(np.sum(weights*np.hypot(a*np.sin(theta), b*np.cos(theta))))
    return 4*total


def test_rectangle_unknown_and_anisotropic_calibration_and_cache_reuse():
    doc = document()
    obj = doc.add_rectangle(q.Rect(2, 3, 10, 8))
    raw = doc.shape_measurements(obj)
    assert (raw.roi.area_px2, raw.roi.length_px) == (80, 36)
    assert raw.roi.area_um2 is raw.roi.length_um is raw.nucleus is None
    stats = doc.feature_cache_stats()
    assert (stats.entries, stats.hits, stats.misses) == (1, 0, 1)
    doc.shape_measurements(obj)
    assert doc.feature_cache_stats().hits == 1
    previous = doc.calibration_revision
    doc.set_calibration(.5, 2)
    assert doc.calibration_revision > previous
    result = doc.shape_measurements(obj)
    assert result.roi.area_um2 == 80 and result.roi.length_um == 42
    assert doc.feature_cache_stats().misses == 2
    doc.set_calibration(.5, 2)
    doc.shape_measurements(obj)
    assert doc.feature_cache_stats().hits == 2


@pytest.mark.parametrize('width,height,sx,sy', [(10, 6, .25, 2), (2, 80, 3, .1), (100, 1, .1, 10), (8, 8, 1, 1)])
def test_ellipse_lengths_match_independent_quadrature(width, height, sx, sy):
    doc = document()
    obj = doc.add_roi(q.Roi.ellipse(q.Rect(0, 0, width, height)))
    doc.set_calibration(sx, sy)
    value = doc.shape_measurements(obj).roi
    assert value.area_px2 == pytest.approx(math.pi*width*height/4, rel=2e-14)
    assert value.area_um2 == pytest.approx(math.pi*width*height*sx*sy/4, rel=2e-14)
    assert value.length_px == pytest.approx(ellipse_length(width/2, height/2), rel=2e-11)
    assert value.length_um == pytest.approx(ellipse_length(width*sx/2, height*sy/2), rel=2e-11)


@pytest.mark.skipif(not q.has_geos, reason='requires polygon topology')
def test_holes_multipart_and_open_line_lengths_use_scaled_edges():
    doc = document()
    shell = [q.Point(0,0),q.Point(10,0),q.Point(10,8),q.Point(0,8)]
    hole = [q.Point(1,1),q.Point(3,1),q.Point(3,4),q.Point(1,4)]
    obj = doc.add_roi(q.Roi.polygon(shell, [hole]))
    doc.set_calibration(.5, 2)
    value = doc.shape_measurements(obj).roi
    assert (value.area_px2, value.length_px, value.area_um2, value.length_um) == (74,46,74,56)
    triangle = [q.Point(20,0),q.Point(23,0),q.Point(20,4)]
    multi = doc.add_roi(q.Roi.multipolygon([q.PolygonPart(shell, [hole]), q.PolygonPart(triangle)]))
    measured = doc.shape_measurements(multi).roi
    assert measured.area_px2 == 80 and measured.length_px == 58
    assert measured.length_um == pytest.approx(56+1.5+8+math.hypot(1.5,8))
    line = doc.add_roi(q.Roi.polyline([q.Point(0,0),q.Point(3,0),q.Point(3,4)]))
    assert doc.shape_measurements(line).roi.length_um == 9.5
    points = doc.add_roi(q.Roi.points([q.Point(1,1),q.Point(2,2)]))
    assert doc.shape_measurements(points).roi.length_um == 0


def test_cell_nucleus_geometry_and_calibration_undo_invalidate_keys():
    doc = document()
    cell = doc.add_cell(q.Roi.rectangle(q.Rect(0,0,20,20)), q.Roi.ellipse(q.Rect(2,2,4,4)))
    doc.set_calibration(.5, 2)
    result = doc.shape_measurements(cell)
    assert result.nucleus.area_px2 == pytest.approx(4*math.pi)
    assert result.nucleus.length_um == pytest.approx(ellipse_length(1,4), rel=2e-11)
    doc.update_nucleus(cell, q.Roi.rectangle(q.Rect(1,1,2,3)))
    assert doc.shape_measurements(cell).nucleus.area_px2 == 6
    assert doc.undo()
    assert doc.shape_measurements(cell).nucleus.area_px2 == pytest.approx(4*math.pi)
    doc.update_nucleus(cell, None)
    assert doc.shape_measurements(cell).nucleus is None
    assert doc.undo() and doc.shape_measurements(cell).nucleus is not None
    old = doc.calibration_revision
    doc.clear_calibration()
    assert doc.calibration_revision > old and doc.shape_measurements(cell).roi.area_um2 is None
    old = doc.calibration_revision
    assert doc.undo() and doc.calibration_revision > old
    assert doc.shape_measurements(cell).roi.area_um2 == 400
    doc.remove_annotation(cell)
    with pytest.raises(IndexError):
        doc.shape_measurements(cell)
    assert doc.undo() and doc.shape_measurements(cell).nucleus is not None


def test_unrelated_edits_and_rejected_calibration_preserve_valid_keys():
    doc = document()
    first = doc.add_rectangle(q.Rect(0,0,10,10))
    other = doc.add_rectangle(q.Rect(20,20,2,2))
    doc.shape_measurements(first)
    doc.rename_annotation(other, 'Other edit')
    doc.shape_measurements(first)
    assert (doc.feature_cache_stats().hits, doc.feature_cache_stats().misses) == (1,1)
    doc.rename_annotation(first, 'Renamed')
    doc.shape_measurements(first)
    assert doc.feature_cache_stats().misses == 2
    old = doc.calibration_revision
    with pytest.raises(ValueError):
        doc.set_calibration(0, 1)
    assert doc.calibration_revision == old
    doc.shape_measurements(first)
    assert doc.feature_cache_stats().hits == 2


def test_lru_eviction_disable_clear_and_limits():
    doc = document()
    ids = [doc.add_rectangle(q.Rect(i,0,1,1)) for i in range(4)]
    doc.set_feature_cache_limit(2)
    for obj in ids[:2]:
        doc.shape_measurements(obj)
    doc.shape_measurements(ids[0])
    doc.shape_measurements(ids[2])
    doc.shape_measurements(ids[0])
    assert doc.feature_cache_stats().hits == 2
    doc.shape_measurements(ids[1])
    assert doc.feature_cache_stats().misses == 4 and doc.feature_cache_stats().entries == 2
    with pytest.raises(ValueError):
        doc.set_feature_cache_limit(65537)
    doc.set_feature_cache_limit(0)
    doc.shape_measurements(ids[0]); doc.shape_measurements(ids[0])
    assert doc.feature_cache_stats().entries == 0 and doc.feature_cache_stats().capacity == 0
    doc.clear_feature_cache()
    assert doc.feature_cache_stats().hits == doc.feature_cache_stats().misses == 0


@pytest.mark.skipif(not q.has_project, reason='requires project storage')
def test_features_are_derived_cold_after_storage_roundtrip(tmp_path):
    project = q.Project()
    doc = document()
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(0,0,5,3))
    doc.set_calibration(.5, .25)
    expected = doc.shape_measurements(obj).roi.length_um
    path = tmp_path/'features.qpxproj'
    project.save(str(path))
    reopened = q.Project.open(str(path)).document(image)
    assert reopened.feature_cache_stats().entries == 0 and reopened.calibration_revision == 0
    assert reopened.shape_measurements(obj).roi.length_um == expected


def test_unrepresentable_physical_results_are_not_cached():
    doc = document()
    obj = doc.add_rectangle(q.Rect(0,0,10,10))
    doc.set_calibration(1e308, 1e308)
    with pytest.raises(OverflowError):
        doc.shape_measurements(obj)
    assert doc.feature_cache_stats().entries == 0
    with pytest.raises(OverflowError):
        doc.area_um2(obj)


def test_returned_measurements_survive_replacement_eviction_and_document_release():
    doc = document()
    obj = doc.add_rectangle(q.Rect(0,0,10,8))
    previous = doc.shape_measurements(obj).roi
    doc.update_rectangle(obj, q.Rect(0,0,2,3))
    assert doc.shape_measurements(obj).roi.area_px2 == 6
    doc.clear_feature_cache()
    del doc
    assert previous.area_px2 == 80 and previous.length_px == 36


def test_scaled_ellipse_normalization_handles_extreme_finite_scales():
    doc = document()
    obj = doc.add_roi(q.Roi.ellipse(q.Rect(0,0,1,1)))
    doc.set_calibration(1e-308,1e-308)
    value = doc.shape_measurements(obj).roi
    assert value.length_um > 0 and math.isclose(value.length_um,math.pi*1e-308,rel_tol=2e-11,abs_tol=0)
    doc.set_calibration(1e308,1e-308)
    with pytest.raises(OverflowError):
        doc.shape_measurements(obj)
    # The old resident entry remains inaccessible under the new calibration key.
    doc.set_calibration(1,1)
    assert doc.shape_measurements(obj).roi.length_um == pytest.approx(math.pi,rel=2e-12)


def test_shape_features_remain_available_without_reading_closed_pixels():
    doc = document()
    obj = doc.add_rectangle(q.Rect(0,0,10,8))
    doc.server.close()
    value = doc.shape_measurements(obj)
    assert value.roi.area_px2 == 80 and value.roi.length_px == 36
    with pytest.raises(RuntimeError):
        doc.mean_intensity(obj)


def test_physical_area_avoids_intermediate_overflow_with_reciprocal_axes():
    doc = document()
    obj = doc.add_rectangle(q.Rect(0,0,10,10))
    doc.set_calibration(1e308,1e-308)
    assert doc.area_um2(obj) == pytest.approx(100,rel=2e-14)
    # Length is genuinely outside binary64, even though area is representable.
    with pytest.raises(OverflowError):
        doc.shape_measurements(obj)


def test_physical_area_avoids_intermediate_underflow_in_cached_and_scalar_api():
    doc = document()
    obj = doc.add_rectangle(q.Rect(0,0,1e-150,1e-150))
    doc.set_calibration(1e-100,1e100)
    value = doc.shape_measurements(obj).roi
    assert math.isclose(value.area_um2,1e-300,rel_tol=2e-14,abs_tol=0)
    assert value.area_um2 == doc.area_um2(obj)
    assert math.isclose(value.length_um,2e-50,rel_tol=2e-14,abs_tol=0)


@pytest.mark.skipif(not q.has_geos,reason='requires polygon topology')
def test_isotropic_lengths_preserve_exact_scaling_of_pixel_measurement():
    doc = document()
    polygon = q.Roi.polygon([q.Point(0,0),q.Point(3.1,.7),q.Point(2.4,6.3)])
    obj = doc.add_roi(polygon)
    for scale in [1., .3, 1.25]:
        doc.set_calibration(scale,scale)
        value = doc.shape_measurements(obj).roi
        assert value.length_um == value.length_px*scale
