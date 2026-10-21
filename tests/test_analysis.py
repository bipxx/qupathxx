"""Native request capture, tiled summaries and owner-thread publication."""
import gc
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q


def setup():
    rgb = np.random.default_rng(125).integers(0, 256, (67, 81, 3), dtype=np.uint8)
    doc = q.Document(q.Image(81, 67, rgb.ravel().tolist()))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]], [250, 240, 230])
    return doc, rgb, transform


@pytest.mark.parametrize("kind", [q.AnalysisKind.OPTICAL_DENSITY_MEANS, q.AnalysisKind.STAIN_MEANS])
@pytest.mark.parametrize("tile_size", [16, 31, 256])
def test_tiled_means_match_independent_roi_formula(kind, tile_size):
    doc, rgb, transform = setup()
    roi = q.Roi.ellipse(q.Rect(3.2, 2.7, 70.1, 59.9))
    obj = doc.add_roi(roi)
    request = q.AnalysisRequest(doc, obj, transform, kind, tile_size=tile_size)
    result = q.analyze(request)
    y, x = np.mgrid[:67, :81]
    mask = ((x+.5-38.25)/35.05)**2 + ((y+.5-32.65)/29.95)**2 < 1
    od = np.maximum(0, np.log10(np.array(transform.white)/np.maximum(rgb.astype(float), 1)))
    reference = od if kind == q.AnalysisKind.OPTICAL_DENSITY_MEANS else np.linalg.solve(np.asarray(transform.stains).T, od.reshape(-1, 3).T).T.reshape(rgb.shape)
    np.testing.assert_allclose(result.means, reference[mask].mean(axis=0), atol=2e-7, rtol=1e-6)
    assert result.samples == int(mask.sum())
    assert result.tiles == request.total_tiles
    assert result.object_id == obj and result.object_revision == doc.annotation(obj).revision
    assert result.document_revision == request.document_revision
    assert result.kind == kind and not result.nucleus
    assert result.algorithm == "qpx.roi-channel-means/1"
    np.testing.assert_array_equal(result.white, transform.white)
    np.testing.assert_array_equal(result.stains, transform.stains)
    job = q.AnalysisJob()
    assert job.request(request) and job.busy
    assert not job.request(request)  # Completed-but-unpolled work also has no backlog.
    assert job.wait(doc, obj).state == "ready" and not job.busy
    assert job.result.samples == result.samples
    np.testing.assert_array_equal(job.result.means, result.means)
    assert job.poll(doc, obj).completed_tiles == request.total_tiles


def test_nucleus_and_hole_masks():
    if not q.has_geos:
        pytest.skip("polygon topology requires GEOS")
    doc, rgb, transform = setup()
    roi = q.Roi.polygon([q.Point(1, 1), q.Point(79, 1), q.Point(79, 66), q.Point(1, 66)],
                        [[q.Point(20, 20), q.Point(30, 20), q.Point(30, 40), q.Point(20, 40)]])
    obj = doc.add_cell(roi, q.Roi.rectangle(q.Rect(40, 40, 10, 12)))
    for nucleus in [False, True]:
        request = q.AnalysisRequest(doc, obj, transform, nucleus=nucleus, tile_size=16)
        result = q.analyze(request)
        mask = np.zeros((67, 81), dtype=bool)
        if nucleus:
            mask[40:52, 40:50] = True
        else:
            mask[1:66, 1:79] = True
            mask[20:40, 20:30] = False
        od = np.maximum(0, np.log10(np.asarray(transform.white)/np.maximum(rgb.astype(float), 1)))
        expected = np.linalg.solve(np.asarray(transform.stains).T, od[mask].T).T.mean(axis=0)
        assert result.nucleus == nucleus and result.samples == int(mask.sum())
        np.testing.assert_allclose(result.means, expected, atol=2e-7)


@pytest.mark.parametrize("change", ["rename", "geometry", "remove", "selection", "close", "other_document"])
def test_stale_results_never_publish(change):
    doc, _, transform = setup()
    obj = doc.add_rectangle(q.Rect(0, 0, 81, 67))
    request = q.AnalysisRequest(doc, obj, transform)
    job = q.AnalysisJob()
    assert job.request(request)
    selected = obj
    if change == "rename":
        doc.rename_annotation(obj, "Changed")
        doc.undo()  # Undo does not revive the captured object revision.
    elif change == "geometry":
        doc.update_rectangle(obj, q.Rect(0, 0, 80, 66))
    elif change == "remove":
        doc.remove_annotation(obj)
    elif change == "selection":
        selected = 0
    elif change == "close":
        doc.server.close()
    else:
        doc = q.Document(doc.server)
        assert doc.add_rectangle(q.Rect(0, 0, 81, 67)) == obj
    assert not request.matches(doc, selected)
    assert job.wait(doc, selected).state == "stale"
    assert job.result is None


def test_unrelated_edits_and_calibration_keep_dimensionless_result():
    doc, _, transform = setup()
    obj = doc.add_rectangle(q.Rect(0, 0, 40, 30))
    other = doc.add_rectangle(q.Rect(40, 30, 20, 20))
    request = q.AnalysisRequest(doc, obj, transform)
    job = q.AnalysisJob()
    job.request(request)
    doc.rename_annotation(other, "Unrelated")
    doc.set_calibration(.2, .7)
    assert request.matches(doc, obj)
    assert job.wait(doc, obj).state == "ready"
    saved = job.result
    doc.rename_annotation(obj, "Now stale")
    assert job.poll(doc, obj).state == "stale" and job.result is None
    del request, job, doc, transform
    gc.collect()
    assert saved.samples == 1200 and np.isfinite(saved.means).all()


def test_cancel_and_reuse_with_new_parameters():
    doc, _, transform = setup()
    obj = doc.add_rectangle(q.Rect(0, 0, 81, 67))
    job = q.AnalysisJob()
    first = q.AnalysisRequest(doc, obj, transform)
    job.request(first)
    job.cancel()  # Cancellation wins even if calculation has just finished.
    assert job.wait(doc, obj).state == "cancelled" and job.result is None
    second = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]], [220]*3))
    assert job.request(second)
    status = job.wait(doc, obj)
    assert status.state == "ready" and status.generation == 2
    np.testing.assert_array_equal(job.result.white, [220]*3)
    job.cancel()
    assert job.result is None and job.poll(doc, obj).state == "cancelled"


@pytest.mark.parametrize("tile_size", [0, 15, 1025])
def test_invalid_tile_size(tile_size):
    doc, _, transform = setup()
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    with pytest.raises(ValueError, match="tile size"):
        q.AnalysisRequest(doc, obj, transform, tile_size=tile_size)


def test_invalid_roi_and_missing_nucleus():
    doc, _, transform = setup()
    obj = doc.add_roi(q.Roi.points([q.Point(1, 1)]))
    with pytest.raises(ValueError, match="area ROI"):
        q.AnalysisRequest(doc, obj, transform)
    with pytest.raises(ValueError, match="no nucleus"):
        q.AnalysisRequest(doc, obj, transform, nucleus=True)
    tiny = doc.add_rectangle(q.Rect(.1, .1, .1, .1))
    with pytest.raises(ValueError, match="pixel centers"):
        q.AnalysisRequest(doc, tiny, transform)
    with pytest.raises(IndexError):
        q.AnalysisRequest(doc, 999, transform)
    doc.server.close()
    with pytest.raises(ValueError, match="open source"):
        q.AnalysisRequest(doc, tiny, transform)


def test_relink_and_reload_invalidate_runtime_source(tmp_path):
    if not q.has_project:
        pytest.skip("project storage disabled")
    path = tmp_path/"image.ppm"
    path.write_bytes(b"P6\n2 1\n255\n"+bytes([10, 20, 30]*2))
    project = q.Project()
    image_id = project.add_image(str(path))
    doc = project.document(image_id)
    obj = doc.add_rectangle(q.Rect(0, 0, 2, 1))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    request = q.AnalysisRequest(doc, obj, transform)
    job = q.AnalysisJob()
    job.request(request)
    project.relink(image_id, str(path))
    assert not request.matches(doc, obj)
    assert job.wait(doc, obj).state == "stale"
    current = q.AnalysisRequest(doc, obj, transform)
    project.save(str(tmp_path/"project.qpxproj"))
    restored = q.Project.open(str(tmp_path/"project.qpxproj")).document(image_id)
    assert not current.matches(restored, obj)


def test_worker_teardown_with_external_source_ownership():
    code = '''
import gc
import qupathxx as q
server = q.dense_image_server(q.Image.demo())
doc = q.Document(server)
obj = doc.add_rectangle(q.Rect(0,0,1024,768))
request = q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
job = q.AnalysisJob()
assert job.request(request)
del server, doc, request
gc.collect()
del job
gc.collect()
print("retired")
'''
    completed = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True, timeout=20)
    assert completed.returncode == 0, completed.stderr
    assert "retired" in completed.stdout
