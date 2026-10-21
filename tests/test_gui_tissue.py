import os

import numpy as np
import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in native desktop replay")


def event(frame, kind):
    value = q.GuiInput()
    value.frame, value.kind = frame, kind
    return value


@pytest.mark.parametrize("feature", [0, 1])
def test_gui_tissue_matches_native_mask_and_draws_overlay(tmp_path, feature):
    rgb = np.full((64, 64, 3), 255, dtype=np.uint8)
    rgb[10:30, 15:40] = 0
    doc = q.Document(q.Image(64, 64, rgb.ravel().tolist()))
    obj = doc.add_rectangle(q.Rect(0, 0, 64, 64))
    revision = doc.revision
    options = q.GuiOptions()
    options.tissue_feature = feature
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY)]
    p = q.TissueParameters()
    p.feature = q.TissueFeature.STAIN_1 if feature else q.TissueFeature.OPTICAL_DENSITY_SUM
    transform = q.StainTransform([options.analysis_vectors[:3], options.analysis_vectors[3:]])
    expected = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    capture = tmp_path/"tissue.ppm"
    report = q.show(doc, frame_limit=60, capture_path=str(capture), options=options)
    assert report.tissue_state == "ready", report.tissue_error
    assert report.tissue_preview_ready and not report.tissue_error
    assert report.tissue_components == expected.component_count == 1
    assert report.tissue_samples == expected.samples and report.tissue_foreground_pixels == expected.foreground_pixels == 500
    assert report.tissue_input_sha256 == expected.input_sha256
    assert 0 < report.tissue_gpu_bytes <= 32*1024*1024
    assert doc.revision == revision and doc.object_count == 1
    header, dimensions, maximum, data = capture.read_bytes().split(b"\n", 3)
    assert header == b"P6" and maximum == b"255"
    width, height = map(int, dimensions.split())
    image = np.frombuffer(data, dtype=np.uint8).reshape(height, width, 3).astype(int)
    # Black source pixels plus the known orange alpha-mask color. Interior
    # pixels are independent of bilinear source sampling at the mask boundary.
    orange = (np.abs(image-np.array([110, 60, 17])) <= 2).all(axis=2)
    assert int(orange.sum()) > 1000


def test_gui_drops_mask_after_selected_object_deletion():
    doc = q.Document(q.Image.demo())
    doc.add_rectangle(q.Rect(100, 100, 300, 250))
    options = q.GuiOptions()
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY), event(30, q.GuiInputKind.DELETE_KEY)]
    report = q.show(doc, frame_limit=45, options=options)
    assert report.tissue_state == "stale" and not report.tissue_preview_ready
    assert report.tissue_components == report.tissue_foreground_pixels == report.tissue_samples == 0
    assert doc.object_count == 0
