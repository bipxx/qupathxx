"""Independent raster/flood-fill references for native tissue masks."""
from collections import deque
import gc
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q


def labels(result):
    return np.frombuffer(result.labels, dtype=np.uint32).reshape(result.height, result.width)


def reference(mask, connectivity, minimum=1, maximum=0):
    mask = mask.copy()
    result = np.zeros(mask.shape, dtype=np.uint32)
    components = []
    for y, x in np.ndindex(mask.shape):
        if not mask[y, x]:
            continue
        queue = deque([(y, x)])
        mask[y, x] = False
        points = []
        while queue:
            yy, xx = queue.popleft()
            points.append((yy, xx))
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    if (dx == dy == 0) or (connectivity == 4 and dx and dy):
                        continue
                    ny, nx = yy+dy, xx+dx
                    if 0 <= ny < mask.shape[0] and 0 <= nx < mask.shape[1] and mask[ny, nx]:
                        mask[ny, nx] = False
                        queue.append((ny, nx))
        if len(points) < minimum or (maximum and len(points) > maximum):
            continue
        label = len(components)+1
        for yy, xx in points:
            result[yy, xx] = label
        components.append(np.asarray(points))
    return result, components


def document(rgb):
    rgb = np.asarray(rgb, dtype=np.uint8)
    return q.Document(q.Image(rgb.shape[1], rgb.shape[0], rgb.ravel().tolist()))


@pytest.mark.parametrize("connectivity", [4, 8])
@pytest.mark.parametrize("tile_size", [16, 31, 256])
@pytest.mark.parametrize("feature", [q.TissueFeature.OPTICAL_DENSITY_SUM, q.TissueFeature.STAIN_1, q.TissueFeature.STAIN_2, q.TissueFeature.STAIN_3])
def test_threshold_and_components_match_independent_reference(feature, tile_size, connectivity):
    rgb = np.random.default_rng(291).integers(0, 256, (37, 43, 3), dtype=np.uint8)
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, 43, 37))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]], [250, 245, 230])
    p = q.TissueParameters()
    p.feature, p.threshold, p.connectivity, p.tile_size = feature, .3, connectivity, tile_size
    p.min_pixels, p.max_pixels = 2, 1300
    request = q.TissueRequest(doc, obj, transform, p)
    result = q.detect_tissue(request)
    od = np.maximum(0, np.log10(np.asarray(transform.white)/np.maximum(rgb.astype(float), 1)))
    if feature == q.TissueFeature.OPTICAL_DENSITY_SUM:
        score = od.astype(np.float32).astype(float).sum(axis=2)
        kind = q.AnalysisKind.OPTICAL_DENSITY_MEANS
    else:
        concentrations = np.linalg.solve(np.asarray(transform.stains).T, od.reshape(-1, 3).T).T.reshape(rgb.shape).astype(np.float32)
        channel = [q.TissueFeature.STAIN_1, q.TissueFeature.STAIN_2, q.TissueFeature.STAIN_3].index(feature)
        score = concentrations[..., channel]
        kind = q.AnalysisKind.STAIN_MEANS
    mask = score >= p.threshold
    expected, parts = reference(mask, connectivity, p.min_pixels, p.max_pixels)
    np.testing.assert_array_equal(labels(result), expected)
    np.testing.assert_array_equal(np.frombuffer(result.mask, dtype=np.uint8).reshape(expected.shape), (expected != 0)*255)
    assert result.samples == 37*43 and result.threshold_pixels == int(mask.sum())
    assert result.foreground_pixels == int((expected != 0).sum()) and result.component_count == len(parts)
    assert result.algorithm == "qpx.threshold-components/1"
    assert result.retained_bytes <= request.estimated_work_bytes <= p.work_bytes
    assert result.input_sha256 == q.analyze(q.AnalysisRequest(doc, obj, transform, kind, tile_size=tile_size)).input_sha256
    for actual, points in zip(result.components, parts):
        assert actual.pixels == len(points)
        low, high = points.min(axis=0), points.max(axis=0)
        assert (actual.bounds.x, actual.bounds.y, actual.bounds.width, actual.bounds.height) == (low[1], low[0], high[1]-low[1]+1, high[0]-low[0]+1)
        assert actual.centroid.x == pytest.approx(points[:, 1].mean()+.5)
        assert actual.centroid.y == pytest.approx(points[:, 0].mean()+.5)
    job = q.TissueJob()
    assert job.request(request) and not job.request(request)
    assert job.wait(doc, obj).state == "ready" and not job.busy
    np.testing.assert_array_equal(labels(job.result), expected)


def test_components_reconcile_every_tile_boundary_and_diagonal():
    mask = np.zeros((65, 67), dtype=bool)
    mask[15:50, 15:20] = True
    mask[30:34, 2:66] = True
    mask[4, 4] = mask[5, 5] = True
    rgb = np.where(mask[..., None], 0, 255).repeat(3, axis=2).astype(np.uint8)
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, 67, 65))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    for connectivity in [4, 8]:
        expected, parts = reference(mask, connectivity)
        for tile in [16, 17, 31, 256]:
            p = q.TissueParameters()
            p.tile_size, p.connectivity = tile, connectivity
            result = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
            np.testing.assert_array_equal(labels(result), expected)
            assert result.component_count == len(parts)


def test_roi_holes_and_fractional_pixel_grid():
    if not q.has_geos:
        pytest.skip("polygon topology requires GEOS")
    doc = document(np.zeros((35, 40, 3), dtype=np.uint8))
    roi = q.Roi.polygon([q.Point(2, 3), q.Point(32, 3), q.Point(32, 25), q.Point(2, 25)],
                       [[q.Point(10, 10), q.Point(20, 10), q.Point(20, 20), q.Point(10, 20)]])
    obj = doc.add_roi(roi)
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    result = q.detect_tissue(q.TissueRequest(doc, obj, transform))
    expected = np.ones((22, 30), dtype=np.uint32)
    expected[7:17, 8:18] = 0
    np.testing.assert_array_equal(labels(result), expected)
    assert result.samples == 560 and result.component_count == 1
    fractional = doc.add_rectangle(q.Rect(2.2, 3.1, 29.5, 21.6))
    cropped = q.detect_tissue(q.TissueRequest(doc, fractional, transform))
    assert (cropped.bounds.x, cropped.bounds.y, cropped.width, cropped.height) == (2, 3, 30, 22)
    assert cropped.samples == 660 and cropped.foreground_pixels == 660


def test_nucleus_ellipse_and_unknown_calibration():
    doc = document(np.zeros((40, 40, 3), dtype=np.uint8))
    obj = doc.add_cell(q.Roi.rectangle(q.Rect(0, 0, 40, 40)), q.Roi.ellipse(q.Rect(10, 12, 16, 12)))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    request = q.TissueRequest(doc, obj, transform, nucleus=True)
    result = q.detect_tissue(request)
    y, x = np.mgrid[12:24, 10:26]
    expected = ((x+.5-18)/8)**2+((y+.5-18)/6)**2 <= 1
    np.testing.assert_array_equal(labels(result) != 0, expected)
    assert result.nucleus and result.samples == int(expected.sum())
    doc.set_calibration(.2, .7)
    assert request.matches(doc, obj)
    assert q.detect_tissue(request).labels == result.labels


def test_empty_mask_and_inclusive_threshold():
    doc = document(np.full((20, 25, 3), 255, dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 25, 20))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    result = q.detect_tissue(q.TissueRequest(doc, obj, transform))
    assert result.component_count == result.foreground_pixels == result.threshold_pixels == 0
    assert not labels(result).any() and result.samples == 500
    p = q.TissueParameters()
    p.threshold = 0
    result = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    assert result.component_count == 1 and result.foreground_pixels == 500


@pytest.mark.parametrize("name,value", [("threshold", float("nan")), ("threshold", float("inf")), ("connectivity", 6),
    ("min_pixels", 0), ("tile_size", 15), ("tile_size", 1025), ("max_components", 0),
    ("max_components", 1000001), ("work_bytes", 0), ("work_bytes", 512*1024*1024+1)])
def test_invalid_parameters(name, value):
    p = q.TissueParameters()
    setattr(p, name, value)
    with pytest.raises(ValueError):
        p.validate()


def test_work_and_component_limits_fail_without_publishing():
    rgb = np.full((20, 20, 3), 255, dtype=np.uint8)
    rgb[::2, ::2] = 0
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, 20, 20))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    p = q.TissueParameters()
    request = q.TissueRequest(doc, obj, transform, p)
    p.work_bytes = request.estimated_work_bytes-1
    with pytest.raises((ValueError, RuntimeError, IndexError), match="work budget"):
        q.TissueRequest(doc, obj, transform, p)
    p.work_bytes, p.max_components, p.connectivity = 64*1024*1024, 1, 4
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, transform, p))
    status = job.wait(doc, obj)
    assert status.state == "failed" and "component limit" in status.error and job.result is None
    p.max_components = 100
    assert job.request(q.TissueRequest(doc, obj, transform, p))
    assert job.wait(doc, obj).state == "ready" and job.result.component_count == 100


def test_parameter_and_result_snapshots_are_independent():
    doc = document(np.zeros((20, 20, 3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 20, 20))
    p = q.TissueParameters()
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p)
    p.threshold = 100
    request.parameters.threshold = 200
    result = q.detect_tissue(request)
    assert result.parameters.threshold == .3 and result.foreground_pixels == 400
    result.parameters.threshold = 300
    result.components[0].bounds.x = 500
    assert result.parameters.threshold == .3 and result.components[0].bounds.x == 0


@pytest.mark.parametrize("change", ["object", "selection", "close", "other_document"])
def test_stale_cancel_reuse_and_result_lifetime(change):
    doc = document(np.zeros((40, 50, 3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 50, 40))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, transform))
    job.cancel()
    assert job.wait(doc, obj).state == "cancelled" and job.result is None
    request = q.TissueRequest(doc, obj, transform)
    assert job.request(request) and job.wait(doc, obj).state == "ready"
    saved = job.result
    before = saved.labels
    selected = obj
    if change == "object":
        doc.rename_annotation(obj, "Changed")
        doc.undo()
    elif change == "selection":
        selected = 0
    elif change == "close":
        doc.server.close()
    else:
        doc = q.Document(doc.server)
        assert doc.add_rectangle(q.Rect(0, 0, 50, 40)) == obj
    assert not request.matches(doc, selected)
    assert job.poll(doc, selected).state == "stale" and job.result is None
    del job, request, doc, transform
    gc.collect()
    assert saved.labels == before and saved.foreground_pixels == 2000


def test_native_worker_teardown_with_external_source():
    code = '''
import gc
import qupathxx as q
server=q.dense_image_server(q.Image.demo())
doc=q.Document(server)
obj=doc.add_rectangle(q.Rect(0,0,1024,768))
request=q.TissueRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
job=q.TissueJob();assert job.request(request)
del request, server, doc
gc.collect()
del job
gc.collect()
print("retired")
'''
    completed = subprocess.run([sys.executable, "-I", "-c", code], text=True, capture_output=True, timeout=20)
    assert completed.returncode == 0, completed.stderr
    assert "retired" in completed.stdout
