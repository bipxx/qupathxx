"""Independent window morphology and background-component references."""
import gc
import os

import numpy as np
import pytest
import qupathxx as q

from test_tissue import document, labels, reference
from test_tissue_levels import pyramid_levels, decoded_reference


def primitive(mask, domain, radius, dilate):
    padded = np.pad(mask, radius, constant_values=False)
    windows = np.lib.stride_tricks.sliding_window_view(padded, (2*radius+1, 2*radius+1))
    return (windows.any(axis=(-1, -2)) if dilate else windows.all(axis=(-1, -2))) & domain


def cleanup_reference(mask, domain, opening=0, closing=0, fill=False, maximum=0, connectivity=8):
    mask = mask & domain
    if opening:
        mask = primitive(primitive(mask, domain, opening, False), domain, opening, True)
    if closing:
        mask = primitive(primitive(mask, domain, closing, True), domain, closing, False)
    filled = 0
    if fill:
        background = 4 if connectivity == 8 else 8
        _, parts = reference((~mask) & domain, background)
        padded = np.pad(domain, 1, constant_values=False)
        safe = domain.copy()
        for dy in [-1, 0, 1]:
            for dx in [-1, 0, 1]:
                if dx == dy == 0 or (background == 4 and dx and dy):
                    continue
                safe &= padded[1+dy:1+dy+domain.shape[0], 1+dx:1+dx+domain.shape[1]]
        for points in parts:
            yy, xx = points.T
            if not safe[yy, xx].all() or (maximum and len(points) > maximum):
                continue
            mask[yy, xx] = True
            filled += len(points)
    return mask, filled


def run_mask(mask, parameters):
    doc = document(np.where(mask[..., None], 0, 255).repeat(3, axis=2))
    obj = doc.add_rectangle(q.Rect(0, 0, mask.shape[1], mask.shape[0]))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    request = q.TissueRequest(doc, obj, transform, parameters)
    return q.detect_tissue(request), request, doc, obj, transform


@pytest.mark.parametrize("connectivity", [4, 8])
@pytest.mark.parametrize("tile", [16, 31, 256])
@pytest.mark.parametrize("opening,closing,fill", [(1, 0, False), (0, 2, False), (1, 2, True), (0, 0, True)])
def test_cleanup_pipeline_against_independent_windows_and_flood_fill(connectivity, tile, opening, closing, fill):
    mask = np.random.default_rng(417).random((67, 71)) > .72
    mask[5:60, 6:66] = True
    mask[20:25, 15:20] = False
    mask[30:35, 30:35] = False
    mask[40:42, 48:50] = False
    mask[45:50, 60:] = False
    parameters = q.TissueParameters()
    parameters.connectivity, parameters.tile_size = connectivity, tile
    parameters.open_radius, parameters.close_radius, parameters.fill_holes = opening, closing, fill
    parameters.max_hole_pixels, parameters.min_pixels = 25, 3
    result, request, doc, obj, transform = run_mask(mask, parameters)
    cleaned, filled = cleanup_reference(mask, np.ones_like(mask), opening, closing, fill, 25, connectivity)
    expected, parts = reference(cleaned, connectivity, 3)
    np.testing.assert_array_equal(labels(result), expected)
    assert result.threshold_pixels == int(mask.sum()) and result.cleaned_pixels == int(cleaned.sum())
    assert result.filled_pixels == filled and result.component_count == len(parts)
    assert result.foreground_pixels == int((expected != 0).sum())
    assert result.algorithm == "qpx.threshold-components/3" and result.cleanup_ms >= 0
    assert result.retained_bytes <= request.estimated_work_bytes <= parameters.work_bytes
    plain = q.TissueParameters()
    plain.tile_size = tile
    unmodified = q.detect_tissue(q.TissueRequest(doc, obj, transform, plain))
    assert result.input_sha256 == unmodified.input_sha256
    assert request.estimated_work_bytes == q.TissueRequest(doc, obj, transform, plain).estimated_work_bytes + mask.size
    parameters.open_radius = 200
    request.parameters.open_radius = 201
    result.parameters.open_radius = 202
    assert request.parameters.open_radius == result.parameters.open_radius == opening


@pytest.mark.parametrize("maximum", [0, 1, 3, 4, 8, 9])
def test_hole_area_limits_are_inclusive(maximum):
    mask = np.ones((12, 18), dtype=bool)
    mask[2, 2] = False
    mask[2:4, 6:8] = False
    mask[6:9, 11:14] = False
    mask[:4, 16] = False  # Open background channel must remain background.
    p = q.TissueParameters()
    p.fill_holes, p.max_hole_pixels = True, maximum
    result, *_ = run_mask(mask, p)
    expected, filled = cleanup_reference(mask, np.ones_like(mask), fill=True, maximum=maximum)
    np.testing.assert_array_equal(labels(result) != 0, expected)
    assert result.filled_pixels == filled


@pytest.mark.parametrize("connectivity,filled", [(4, 0), (8, 3)])
def test_background_uses_complementary_connectivity(connectivity, filled):
    mask = np.ones((7, 7), dtype=bool)
    mask[np.arange(4), np.arange(4)] = False
    p = q.TissueParameters()
    p.connectivity, p.fill_holes = connectivity, True
    result, *_ = run_mask(mask, p)
    assert result.filled_pixels == filled and result.cleaned_pixels == 45+filled
    assert labels(result)[0, 0] == 0


def test_cleanup_precedes_component_filtering_and_zero_border_is_explicit():
    mask = np.zeros((9, 9), dtype=bool)
    mask[2:7, 2:7] = True
    mask[3:6, 3:6] = False
    p = q.TissueParameters()
    p.min_pixels = 20
    assert run_mask(mask, p)[0].foreground_pixels == 0
    p.fill_holes = True
    result, *_ = run_mask(mask, p)
    assert result.threshold_pixels == 16 and result.cleaned_pixels == result.foreground_pixels == 25
    p = q.TissueParameters()
    p.close_radius = 1
    result, *_ = run_mask(np.ones((5, 5), dtype=bool), p)
    expected = np.zeros((5, 5), dtype=bool)
    expected[1:4, 1:4] = True
    np.testing.assert_array_equal(labels(result) != 0, expected)


@pytest.mark.parametrize("shape", [(1, 1), (1, 13), (13, 1), (3, 7)])
def test_maximum_radius_and_slender_masks(shape):
    p = q.TissueParameters()
    p.open_radius, p.close_radius, p.fill_holes = 256, 256, True
    result, *_ = run_mask(np.ones(shape, dtype=bool), p)
    assert result.samples == np.prod(shape) and result.cleaned_pixels == result.foreground_pixels == 0


@pytest.mark.parametrize("name,value", [("open_radius", -1), ("open_radius", 257), ("close_radius", -1), ("close_radius", 257)])
def test_invalid_radii(name, value):
    p = q.TissueParameters()
    setattr(p, name, value)
    with pytest.raises(ValueError, match="radii"):
        p.validate()


@pytest.mark.parametrize("connectivity", [4, 8])
def test_roi_holes_remain_excluded_from_every_cleanup_stage(connectivity):
    if not q.has_geos:
        pytest.skip("requires GEOS")
    rng = np.random.default_rng(96)
    mask = rng.random((70, 72)) > .12
    doc = document(np.where(mask[..., None], 0, 255).repeat(3, axis=2))
    roi = q.Roi.polygon([q.Point(1, 2), q.Point(66, 2), q.Point(66, 62), q.Point(1, 62)],
                        [[q.Point(24, 23), q.Point(39, 23), q.Point(39, 38), q.Point(24, 38)]])
    obj = doc.add_roi(roi)
    domain = np.ones((60, 65), dtype=bool)
    domain[21:36, 23:38] = False
    p = q.TissueParameters()
    p.open_radius, p.close_radius, p.fill_holes, p.connectivity = 1, 2, True, connectivity
    result = q.detect_tissue(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p))
    expected, filled = cleanup_reference(mask[2:62, 1:66], domain, 1, 2, True, connectivity=connectivity)
    np.testing.assert_array_equal(labels(result) != 0, expected)
    assert result.samples == int(domain.sum()) and result.filled_pixels == filled
    assert not labels(result)[~domain].any()


@pytest.mark.parametrize("level", [1, 2])
def test_coarse_nucleus_cleanup_uses_sampled_radii_and_scaled_components(pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    roi = q.Roi.ellipse(q.Rect(13.2, 17.1, 231.5, 189.6))
    obj = doc.add_cell(q.Roi.rectangle(q.Rect(0, 0, 768, 576)), roi)
    p = q.TissueParameters()
    p.level, p.open_radius, p.close_radius, p.fill_holes, p.threshold = level, 1, 2, True, 1.4
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p, nucleus=True)
    result = q.detect_tissue(request)
    rgba, (x0, y0, x1, y1), digest, _ = decoded_reference(server, roi, level, 256)
    scale = request.downsample
    y, x = np.mgrid[y0:y1, x0:x1]
    b = roi.bounds
    domain = ((((x+.5)*scale-b.x-b.width/2)/(b.width/2))**2 + (((y+.5)*scale-b.y-b.height/2)/(b.height/2))**2 <= 1) & (rgba[..., 3] == 255)
    od = np.maximum(0, np.log10(255/np.maximum(rgba[..., :3].astype(float), 1))).astype(np.float32)
    mask = od.astype(float).sum(axis=2) >= 1.4
    cleaned, filled = cleanup_reference(mask, domain, 1, 2, True)
    expected, parts = reference(cleaned, 8)
    np.testing.assert_array_equal(labels(result), expected)
    assert result.input_sha256 == digest and result.filled_pixels == filled
    for actual, points in zip(result.components, parts):
        assert actual.centroid.x == pytest.approx((x0+points[:, 1].mean()+.5)*scale)
        assert actual.centroid.y == pytest.approx((y0+points[:, 0].mean()+.5)*scale)


def test_cleanup_budget_and_cancelled_stale_results_do_not_publish():
    p = q.TissueParameters()
    p.open_radius, p.fill_holes = 1, True
    _, request, doc, obj, transform = run_mask(np.ones((65, 67), dtype=bool), p)
    p.work_bytes = request.estimated_work_bytes-1
    with pytest.raises((ValueError, RuntimeError), match="work budget"):
        q.TissueRequest(doc, obj, transform, p)
    job = q.TissueJob()
    assert job.request(request)
    job.cancel()
    assert job.wait(doc, obj).state == "cancelled" and job.result is None
    assert job.request(request) and job.wait(doc, obj).state == "ready"
    saved = job.result
    doc.rename_annotation(obj, "Edited")
    assert job.poll(doc, obj).state == "stale" and job.result is None
    del request, job, doc
    gc.collect()
    assert saved.cleaned_pixels == 65*67


def test_auto_resolution_accounts_for_cleanup_domain_memory(pyramid_levels):
    server, _ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_rectangle(q.Rect(0, 0, 768, 576))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    p = q.TissueParameters()
    plain = q.TissueRequest(doc, obj, transform, p)
    assert plain.level == 0
    p.work_bytes, p.close_radius = plain.estimated_work_bytes, 1
    cleaned = q.TissueRequest(doc, obj, transform, p)
    assert cleaned.level == 1 and cleaned.estimated_work_bytes <= p.work_bytes


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in GUI")
def test_gui_cleanup_matches_native_and_renders_filled_hole(tmp_path):
    from PIL import Image
    mask = np.zeros((64, 64), dtype=bool)
    mask[12:52, 12:52] = True
    mask[28:36, 28:36] = False
    p = q.TissueParameters()
    p.open_radius, p.close_radius, p.fill_holes = 1, 1, True
    result, _, doc, _, _ = run_mask(mask, p)
    options = q.GuiOptions()
    options.tissue_open_radius, options.tissue_close_radius, options.tissue_fill_holes = 1, 1, True
    events = []
    for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (8, q.GuiInputKind.DETECT_TISSUE_KEY)]:
        event = q.GuiInput()
        event.frame, event.kind = frame, kind
        events.append(event)
    options.inputs = events
    capture = tmp_path/"cleanup.ppm"
    report = q.show(doc, frame_limit=60, capture_path=str(capture), options=options)
    assert report.tissue_state == "ready" and report.tissue_preview_ready, report.tissue_error
    assert report.tissue_input_sha256 == result.input_sha256
    assert report.tissue_components == result.component_count == 1
    assert report.tissue_foreground_pixels == report.tissue_cleaned_pixels == result.cleaned_pixels == 1600
    assert report.tissue_filled_pixels == result.filled_pixels == 64
    image = np.asarray(Image.open(capture)).astype(int)
    expected = np.asarray([255, 205, 162])  # White hole under the orange overlay.
    assert int((np.abs(image-expected) <= 2).all(axis=2).sum()) > 3000
