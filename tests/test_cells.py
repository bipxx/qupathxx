import gc

import numpy as np
import pytest
import qupathxx as q

from test_tissue import document, labels
from test_tissue_levels import pyramid_levels, decoded_reference


def transform():
    return q.StainTransform([[.65, .70, .29], [.07, .99, .11]], [250, 245, 230])


def phantom(size=48):
    yy, xx = np.indices((size, size))
    mask = (xx-size*.35)**2+(yy-size*.5)**2 < (size*.2)**2
    mask |= (xx-size*.65)**2+(yy-size*.5)**2 < (size*.2)**2
    return np.where(mask[..., None], [30, 40, 60], [240, 245, 250]).astype(np.uint8)


def reference():
    pytest.importorskip("scipy")
    pytest.importorskip("skimage")
    from cell_reference import run
    return run


@pytest.mark.parametrize("feature", [q.TissueFeature.OPTICAL_DENSITY_SUM, q.TissueFeature.STAIN_1, q.TissueFeature.STAIN_2, q.TissueFeature.STAIN_3])
@pytest.mark.parametrize("sigma", [0, 1.2])
@pytest.mark.parametrize("connectivity", [4, 8])
def test_cells_match_independent_watershed_expansion_and_compartment_means(feature, sigma, connectivity):
    rgb = phantom(40)
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, 40, 40))
    p = q.CellParameters()
    p.feature, p.sigma, p.connectivity = feature, sigma, connectivity
    p.threshold = .8 if feature == q.TissueFeature.OPTICAL_DENSITY_SUM else .05
    p.min_nucleus_pixels, p.cell_expansion, p.tile_size = 2, 3.5, 16
    t = transform()
    expected, bodies, markers, remap, channels, peaks = reference()(rgb, t, p)
    request = q.CellRequest(doc, obj, t, p)
    result = q.detect_cells(request)
    assert result.algorithm == "qpx.distance-watershed/1"
    np.testing.assert_array_equal(labels(result.nuclei), expected)
    np.testing.assert_array_equal(labels(result.cells), bodies)
    assert [m.pixel_index for m in result.markers] == markers and result.raw_peaks == peaks
    assert [m.result_label for m in result.markers] == remap[1:].tolist()
    assert result.retained_bytes <= request.estimated_work_bytes <= p.work_bytes
    for f in result.features:
        nucleus, body = expected == f.label, bodies == f.label
        cyto = body & ~nucleus
        assert f.nucleus_pixels == int(nucleus.sum()) and f.cell_pixels == int(body.sum())
        np.testing.assert_allclose(f.nucleus_rgb, rgb[nucleus].mean(axis=0), atol=1e-12)
        np.testing.assert_allclose(f.cell_rgb, rgb[body].mean(axis=0), atol=1e-12)
        np.testing.assert_allclose(f.nucleus_channels, channels[nucleus].mean(axis=0), atol=1e-6)
        np.testing.assert_allclose(f.cell_channels, channels[body].mean(axis=0), atol=1e-6)
        if cyto.any():
            np.testing.assert_allclose(f.cytoplasm_rgb, rgb[cyto].mean(axis=0), atol=1e-12)
            np.testing.assert_allclose(f.cytoplasm_channels, channels[cyto].mean(axis=0), atol=1e-6)
        else:
            assert f.cytoplasm_rgb is f.cytoplasm_channels is None
    assert doc.object_count == 1


@pytest.mark.parametrize("tile", [16, 17, 31, 256])
def test_touching_nuclei_have_two_cells_without_decode_seams(tile):
    rgb = phantom(65)
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, 65, 65))
    p = q.CellParameters()
    p.tile_size, p.sigma, p.cell_expansion = tile, 0, 0
    t = transform()
    result = q.detect_cells(q.CellRequest(doc, obj, t, p))
    assert result.cell_count == 2 and len(result.markers) == 2
    tissue = q.TissueParameters()
    tissue.tile_size = tile
    assert result.nuclei.input_sha256 == q.detect_tissue(q.TissueRequest(doc, obj, t, tissue)).input_sha256
    assert result.nuclei.labels == result.cells.labels
    assert all(f.cytoplasm_rgb is None for f in result.features)
    expected = reference()(rgb, t, p)[0]
    np.testing.assert_array_equal(labels(result.nuclei), expected)


@pytest.mark.parametrize("shape", ["ellipse", "hole"])
def test_normalized_smoothing_respects_source_domain_and_clipped_geometry(shape):
    rgb = phantom(40)
    yy, xx = np.indices((40, 40))
    if shape == "ellipse":
        b = q.Rect(4.2, 5.3, 31.1, 29.2)
        roi = q.Roi.ellipse(b)
        domain = ((xx+.5-b.x-b.width/2)/(b.width/2))**2+((yy+.5-b.y-b.height/2)/(b.height/2))**2 <= 1
    else:
        if not q.has_geos:
            pytest.skip("polygon source requires GEOS")
        def ring(a, b, c, d):
            return [q.Point(a, b), q.Point(c, b), q.Point(c, d), q.Point(a, d)]
        roi = q.Roi.polygon(ring(4.2, 5.3, 35.3, 34.5), [ring(18.2, 17.3, 21.3, 23.5)])
        domain = (xx+.5 >= 4.2) & (xx+.5 <= 35.3) & (yy+.5 >= 5.3) & (yy+.5 <= 34.5)
        domain &= ~((xx+.5 > 18.2) & (xx+.5 < 21.3) & (yy+.5 > 17.3) & (yy+.5 < 23.5))
    doc = document(rgb)
    obj = doc.add_roi(roi)
    p = q.CellParameters()
    p.sigma, p.cell_expansion = 1.3, 5
    request = q.CellRequest(doc, obj, transform(), p)
    b = request.level_bounds
    y0, x0, y1, x1 = int(b.y), int(b.x), int(b.y+b.height), int(b.x+b.width)
    expected = reference()(rgb[y0:y1, x0:x1], transform(), p, domain[y0:y1, x0:x1])
    result = q.detect_cells(request)
    np.testing.assert_array_equal(labels(result.nuclei), expected[0])
    np.testing.assert_array_equal(labels(result.cells), expected[1])
    if q.has_geos:
        for body, nucleus in zip(result.cell_contours.components, result.nucleus_contours.components):
            assert roi.covers(body.geometry) and body.geometry.covers(nucleus.geometry)


@pytest.mark.parametrize("field,value", [("sigma", -1), ("sigma", 17), ("sigma", float("nan")), ("threshold", float("inf")),
    ("min_seed_distance", -1), ("min_peak_radius", -1), ("cell_expansion", -1), ("min_nucleus_pixels", 0),
    ("max_cells", 0), ("max_cells", 1000001), ("level", -2), ("connectivity", 6), ("tile_size", 1), ("work_bytes", 0)])
def test_invalid_cell_parameters(field, value):
    p = q.CellParameters()
    setattr(p, field, value)
    with pytest.raises(ValueError):
        p.validate()


def job_for(rgb=None, parameters=None):
    rgb = phantom() if rgb is None else rgb
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, rgb.shape[1], rgb.shape[0]))
    job = q.CellJob()
    assert job.request(q.CellRequest(doc, obj, transform(), parameters or q.CellParameters()))
    return doc, obj, job


def test_empty_result_and_resource_failure_are_atomic_and_reusable():
    doc, obj, job = job_for(np.full((32, 32, 3), 255, dtype=np.uint8))
    assert job.wait(doc, obj).state == "ready" and job.result.cell_count == 0
    assert not any(job.result.nuclei.labels) and not any(job.result.cells.labels)
    if q.has_geos:
        doc.clear_history()
        revision = doc.revision
        assert job.keep_result(doc, obj) == [] and doc.revision == revision and not doc.can_undo
    p = q.CellParameters()
    p.max_cells, p.sigma = 1, 0
    doc, obj, job = job_for(parameters=p)
    revision = doc.revision
    assert job.wait(doc, obj).state == "failed" and "candidate limit" in job.poll(doc, obj).error
    assert job.result is None and doc.revision == revision
    assert job.request(q.CellRequest(doc, obj, transform())) and job.wait(doc, obj).state == "ready"


def test_publication_is_one_batch_and_captured_results_outlive_job():
    doc, obj, job = job_for()
    assert job.wait(doc, obj).state == "ready"
    result = job.result
    result.parameters.sigma = 100
    result.features[0].nucleus_rgb[0] = 999
    assert result.parameters.sigma == 1 and result.features[0].nucleus_rgb[0] != 999
    if not q.has_geos:
        with pytest.raises(RuntimeError, match="GEOS"):
            job.keep_result(doc, obj)
        return
    doc.clear_history()
    revision = doc.revision
    with pytest.raises(ValueError):
        job.keep_result(doc, obj, name_prefix="x"*4096, class_name="Cells")
    assert doc.revision == revision and not doc.classes and not job.published
    ids = job.keep_result(doc, obj, class_name="Cells")
    assert len(ids) == result.cell_count and doc.children(obj) == ids and job.published
    assert all(doc.annotation(i).kind == q.ObjectKind.CELL and doc.annotation(i).nucleus for i in ids)
    with pytest.raises(RuntimeError, match="already published"):
        job.keep_result(doc, obj)
    assert doc.undo() and doc.object_count == 1 and not doc.can_undo
    assert doc.redo() and doc.children(obj) == ids
    del job, doc
    gc.collect()
    assert result.nuclei.foreground_pixels > 0 and result.cell_contours.components


@pytest.mark.parametrize("change", ["selection", "rename", "close", "authority", "cancel"])
def test_cell_jobs_reject_stale_results_and_cancel(change):
    doc, obj, job = job_for()
    selected = obj
    if change == "selection":
        selected = 0
    elif change == "rename":
        doc.rename_annotation(obj, "Changed")
        doc.undo()
    elif change == "close":
        doc.server.close()
    elif change == "authority":
        doc = q.Document(doc.server)
        assert doc.add_rectangle(q.Rect(0, 0, 48, 48)) == obj
    else:
        job.cancel()
    assert job.wait(doc, selected).state == ("cancelled" if change == "cancel" else "stale")
    assert job.result is None
    with pytest.raises(RuntimeError, match="no current"):
        job.keep_result(doc, selected)


@pytest.mark.parametrize("level", [1, 2])
def test_cells_decode_native_pyramid_pixels_and_world_coordinates(pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    roi = q.Roi.rectangle(q.Rect(13.2, 17.1, 91.5, 89.6))
    obj = doc.add_roi(roi)
    p = q.CellParameters()
    p.level, p.tile_size, p.sigma, p.min_nucleus_pixels = level, 31, .7, 2
    request = q.CellRequest(doc, obj, transform(), p)
    rgba, (x0, y0, x1, y1), digest, tiles = decoded_reference(server, roi, level, p.tile_size)
    expected = reference()(rgba[..., :3], transform(), p, rgba[..., 3] == 255)
    result = q.detect_cells(request)
    np.testing.assert_array_equal(labels(result.nuclei), expected[0])
    np.testing.assert_array_equal(labels(result.cells), expected[1])
    assert result.nuclei.input_sha256 == digest and request.total_tiles == tiles
    scale = server.metadata.levels[level].downsample
    assert request.downsample == scale and request.level == level
    for marker in result.markers:
        assert marker.center.x == (x0+marker.pixel_index % (x1-x0)+.5)*scale
        assert marker.center.y == (y0+marker.pixel_index // (x1-x0)+.5)*scale
    auto = q.CellParameters()
    auto.tile_size = p.tile_size
    auto.contours.work_bytes = p.contours.work_bytes
    auto.work_bytes = request.estimated_work_bytes
    assert q.CellRequest(doc, obj, transform(), auto).level == level


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_geos, reason="native persistence and geometry")
def test_published_cell_geometry_survives_project_and_geojson_roundtrip(tmp_path):
    from test_project import state
    doc, obj, job = job_for()
    assert job.wait(doc, obj).state == "ready"
    ids = job.keep_result(doc, obj, class_name="Detected")
    before = state(doc)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"cells.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert state(restored) == before and restored.children(obj) == ids
    target = document(phantom())
    imported = q.import_geojson(target, q.export_geojson(doc).text)
    assert imported.imported_ids == [obj, *ids]
    for a, b in zip(doc.annotations, target.annotations):
        assert a.geometry.covers(b.geometry) and b.geometry.covers(a.geometry) and a.kind == b.kind
        assert a.parent_id == b.parent_id and a.class_name == b.class_name
        assert bool(a.nucleus) == bool(b.nucleus)
        if a.nucleus:
            assert a.nucleus.covers(b.nucleus) and b.nucleus.covers(a.nucleus)


def test_contour_cap_failure_is_atomic_and_allows_retry():
    if not q.has_geos:
        pytest.skip("GEOS contour budget")
    p = q.CellParameters()
    options = p.contours
    options.max_vertices = 4
    p.contours = options
    doc, obj, job = job_for(parameters=p)
    revision = doc.revision
    status = job.wait(doc, obj)
    assert status.state == "failed" and status.error
    assert job.result is None and doc.revision == revision and doc.object_count == 1
    assert job.request(q.CellRequest(doc, obj, transform()))
    assert job.wait(doc, obj).state == "ready"


def test_subnormal_gaussian_sigma_preserves_identity_limit():
    doc = document(phantom())
    obj = doc.add_rectangle(q.Rect(0, 0, 48, 48))
    p = q.CellParameters()
    p.sigma = 0
    expected = q.detect_cells(q.CellRequest(doc, obj, transform(), p))
    p.sigma = 1e-300
    result = q.detect_cells(q.CellRequest(doc, obj, transform(), p))
    assert result.nuclei.labels == expected.nuclei.labels
    assert result.cells.labels == expected.cells.labels
