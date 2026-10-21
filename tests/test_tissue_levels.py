"""Pyramid sampling: independent grids, pixels, hashes and component geometry."""
import hashlib
import math
import os
import struct

import numpy as np
import pytest
import qupathxx as q

from test_tissue import labels, reference


@pytest.fixture
def pyramid_levels(tmp_path):
    if not q.has_openslide:
        pytest.skip("requires OpenSlide")
    tifffile = pytest.importorskip("tifffile")
    path = tmp_path / "tissue-levels.tif"
    arrays = []
    with tifffile.TiffWriter(path) as writer:
        for index, (width, height) in enumerate([(768, 576), (512, 384), (192, 144)]):
            yy, xx = np.indices((height, width))
            # Each level has independent values: coarse pixels must be decoded,
            # never substituted with level-zero subsampling.
            rgb = np.stack(((xx*7+yy*3+index*29) % 256,
                            (xx*2+yy*11+index*13) % 256,
                            (xx*5+yy*7+index*19) % 256), axis=-1).astype(np.uint8)
            arrays.append(rgb)
            writer.write(rgb, tile=(64, 64), photometric="rgb", metadata=None,
                         subfiletype=0 if index == 0 else 1)
    server = q.open_slide(str(path))
    assert [level.downsample for level in server.metadata.levels] == [1, 1.5, 4]
    return server, arrays


def decoded_reference(server, roi, level, tile):
    metadata = server.metadata
    selected = metadata.levels[level]
    scale = selected.downsample
    b = roi.bounds
    x0, y0, x1, y1 = [math.ceil(edge/scale-.5) for edge in (b.x, b.y, b.x+b.width, b.y+b.height)]
    x0, x1 = max(0, x0), min(selected.width, x1)
    y0, y1 = max(0, y0), min(selected.height, y1)
    rgba = np.zeros((y1-y0, x1-x0, 4), dtype=np.uint8)
    digest = hashlib.sha256(b"qpx.rgba-level-tiles/1")

    def integers(*values):
        digest.update(struct.pack("<"+"Q"*len(values), *values))

    integers(metadata.width, metadata.height, level, selected.width, selected.height)
    digest.update(struct.pack("<d", scale))
    integers(x0, y0, x1, y1, tile)
    count = 0
    for y in range(y0, y1, tile):
        for x in range(x0, x1, tile):
            width, height = min(tile, x1-x), min(tile, y1-y)
            rx, ry = math.floor(x*scale+.5), math.floor(y*scale+.5)
            pixels = server.read_region(q.RegionRequest(rx, ry, level, width, height)).data
            integers(x, y, rx, ry, width, height)
            digest.update(pixels)
            rgba[y-y0:y-y0+height, x-x0:x-x0+width] = np.frombuffer(pixels, dtype=np.uint8).reshape(height, width, 4)
            count += 1
    return rgba, (x0, y0, x1, y1), digest.hexdigest(), count


@pytest.mark.parametrize("level", [1, 2])
@pytest.mark.parametrize("tile", [16, 31, 256])
@pytest.mark.parametrize("stain", [False, True])
def test_pyramid_pixels_geometry_and_digest(pyramid_levels, level, tile, stain):
    server, arrays = pyramid_levels
    doc = q.Document(server)
    roi = q.Roi.ellipse(q.Rect(13.2, 17.1, 231.5, 189.6))
    obj = doc.add_roi(roi)
    vectors = np.asarray([[.65, .70, .29], [.07, .99, .11], [.3, .1, .9]], dtype=float)
    transform = q.StainTransform(vectors.tolist())
    parameters = q.TissueParameters()
    parameters.level, parameters.tile_size, parameters.min_pixels = level, tile, 3
    parameters.feature = q.TissueFeature.STAIN_2 if stain else q.TissueFeature.OPTICAL_DENSITY_SUM
    parameters.threshold = .2 if stain else 1.4
    request = q.TissueRequest(doc, obj, transform, parameters)
    result = q.detect_tissue(request)
    rgba, (x0, y0, x1, y1), digest, tiles = decoded_reference(server, roi, level, tile)
    scale = server.metadata.levels[level].downsample
    yy, xx = np.mgrid[y0:y1, x0:x1]
    b = roi.bounds
    inside = (((xx+.5)*scale-(b.x+b.width/2))/(b.width/2))**2 + (((yy+.5)*scale-(b.y+b.height/2))/(b.height/2))**2 <= 1
    valid = inside & (rgba[..., 3] == 255)
    od = np.maximum(0, np.log10(255/np.maximum(rgba[..., :3].astype(float), 1)))
    if stain:
        normalized = vectors/np.linalg.norm(vectors, axis=1)[:, None]
        score = np.linalg.solve(normalized.T, od.reshape(-1, 3).T).T.astype(np.float32).reshape(od.shape)[..., 1]
    else:
        score = od.astype(np.float32).astype(float).sum(axis=2)
    expected, parts = reference(valid & (score >= parameters.threshold), 8, 3)
    np.testing.assert_array_equal(labels(result), expected)
    assert result.samples == int(valid.sum())
    assert result.component_count == len(parts) and result.input_sha256 == digest
    assert result.input_format == "qpx.rgba-level-tiles/1" and result.algorithm == "qpx.threshold-components/2"
    assert request.total_tiles == tiles
    assert result.level == request.level == level and result.downsample == request.downsample == scale
    assert (result.width, result.height) == (x1-x0, y1-y0)
    assert (result.bounds.x, result.bounds.y, result.bounds.width, result.bounds.height) == (x0*scale, y0*scale, (x1-x0)*scale, (y1-y0)*scale)
    assert result.level_bounds.x == x0 and result.level_bounds.y == y0
    assert result.retained_bytes <= request.estimated_work_bytes <= parameters.work_bytes
    for actual, points in zip(result.components, parts):
        low, high = points.min(axis=0), points.max(axis=0)
        assert (actual.bounds.x, actual.bounds.y, actual.bounds.width, actual.bounds.height) == (
            (x0+low[1])*scale, (y0+low[0])*scale, (high[1]-low[1]+1)*scale, (high[0]-low[0]+1)*scale)
        assert actual.centroid.x == pytest.approx((x0+points[:, 1].mean()+.5)*scale)
        assert actual.centroid.y == pytest.approx((y0+points[:, 0].mean()+.5)*scale)
    if level == 2:
        # Integer-scale decoding also agrees with independently stored TIFF pixels.
        np.testing.assert_array_equal(rgba[..., :3], arrays[level][y0:y1, x0:x1])


def test_auto_selects_finest_budget_fit_and_explicit_level_is_strict(pyramid_levels):
    server, _ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_rectangle(q.Rect(0, 0, 768, 576))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    estimates = []
    parameters = q.TissueParameters()
    for level in range(3):
        parameters.level = level
        estimates.append(q.TissueRequest(doc, obj, transform, parameters).estimated_work_bytes)
    assert estimates[0] > estimates[1] > estimates[2]
    for chosen in range(3):
        parameters.level, parameters.work_bytes = -1, estimates[chosen]
        request = q.TissueRequest(doc, obj, transform, parameters)
        assert request.level == chosen and request.parameters.level == -1
        assert request.estimated_work_bytes == estimates[chosen]
        job = q.TissueJob()
        assert job.request(request) and job.wait(doc, obj).state == "ready"
        assert job.result.level == chosen
    parameters.level, parameters.work_bytes = 0, estimates[2]
    with pytest.raises((RuntimeError, ValueError), match="work budget"):
        q.TissueRequest(doc, obj, transform, parameters)
    parameters.level, parameters.work_bytes = -1, estimates[2]-1
    with pytest.raises((RuntimeError, ValueError), match="work budget"):
        q.TissueRequest(doc, obj, transform, parameters)


def test_missing_levels_empty_grids_and_tiny_coarse_sample(pyramid_levels):
    server, _ = pyramid_levels
    doc = q.Document(server)
    tiny = doc.add_rectangle(q.Rect(.7, .7, .1, .1))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    parameters = q.TissueParameters()
    parameters.level = 1
    result = q.detect_tissue(q.TissueRequest(doc, tiny, transform, parameters))
    assert result.width == result.height == result.samples == 1 and result.downsample == 1.5
    parameters.level = -1
    assert q.TissueRequest(doc, tiny, transform, parameters).level == 1
    parameters.level = 0
    with pytest.raises(ValueError, match="no pixel centers"):
        q.TissueRequest(doc, tiny, transform, parameters)
    parameters.level = 3
    with pytest.raises(IndexError, match="pyramid level"):
        q.TissueRequest(doc, tiny, transform, parameters)
    parameters.level = -2
    with pytest.raises(ValueError, match="level"):
        parameters.validate()
    with pytest.raises(ValueError, match="no pixel centers"):
        q.AnalysisRequest(doc, tiny, transform)


def test_coarse_nucleus_holes_and_stale_result(pyramid_levels):
    if not q.has_geos:
        pytest.skip("requires GEOS")
    server, _ = pyramid_levels
    doc = q.Document(server)
    polygon = q.Roi.polygon([q.Point(12, 16), q.Point(252, 16), q.Point(252, 208), q.Point(12, 208)],
                            [[q.Point(52, 56), q.Point(108, 56), q.Point(108, 96), q.Point(52, 96)]])
    obj = doc.add_cell(q.Roi.rectangle(q.Rect(0, 0, 768, 576)), polygon)
    parameters = q.TissueParameters()
    parameters.level, parameters.threshold = 2, 0
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), parameters, nucleus=True)
    job = q.TissueJob()
    assert job.request(request) and job.wait(doc, obj).state == "ready"
    result = job.result
    expected = np.ones((48, 60), dtype=np.uint32)
    expected[10:20, 10:24] = 0
    np.testing.assert_array_equal(labels(result), expected)
    doc.set_calibration(.25, .5)
    assert job.poll(doc, obj).state == "ready"
    doc.rename_annotation(obj, "Changed")
    assert job.poll(doc, obj).state == "stale" and job.result is None
    assert result.nucleus and result.foreground_pixels == int((expected != 0).sum())


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in GUI")
def test_gui_coarse_mask_uses_level_zero_geometry(pyramid_levels, tmp_path):
    from PIL import Image
    server, _ = pyramid_levels
    doc = q.Document(server)
    # Mask all ROI pixels, so the colored rectangle checks position and scale.
    obj = doc.add_rectangle(q.Rect(64, 80, 192, 160))
    options = q.GuiOptions()
    options.tissue_level, options.tissue_threshold = 2, 0
    view = q.Viewport()
    view.zoom, view.origin = 2, q.Point(0, 0)
    options.start_view = view
    events = []
    for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (8, q.GuiInputKind.DETECT_TISSUE_KEY)]:
        event = q.GuiInput()
        event.frame, event.kind = frame, kind
        events.append(event)
    options.inputs = events
    baseline, overlay = tmp_path/"base.ppm", tmp_path/"mask.ppm"
    # Same camera and selected outline in both frames.
    before = q.GuiOptions()
    before.start_view = options.start_view
    before.inputs = events[:1]
    q.show(doc, frame_limit=60, capture_path=str(baseline), options=before)
    report = q.show(doc, frame_limit=60, capture_path=str(overlay), options=options)
    assert report.tissue_state == "ready" and report.tissue_preview_ready, report.tissue_error
    assert (report.tissue_level, report.tissue_downsample, report.tissue_width, report.tissue_height) == (2, 4, 48, 40)
    assert report.tissue_foreground_pixels == 48*40 and doc.object_count == 1
    a, b = np.asarray(Image.open(baseline)).astype(float), np.asarray(Image.open(overlay)).astype(float)
    # Canvas starts after inspector; find the image's origin from unaffected
    # vertical edge and compare a large interior patch, well away from outlines.
    delta = np.max(np.abs(a-b), axis=2)
    ys, xs = np.where(delta[:, 310:] > 3)
    assert len(xs) > 50000
    assert xs.max()-xs.min() == pytest.approx(192*2, abs=8)
    assert ys.max()-ys.min() == pytest.approx(160*2, abs=8)
