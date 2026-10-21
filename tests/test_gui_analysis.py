import os

import numpy as np
import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in native desktop replay")


def event(frame, kind):
    e = q.GuiInput()
    e.frame, e.kind = frame, kind
    return e


@pytest.mark.parametrize("kind,nucleus", [(0, False), (1, False), (1, True)])
def test_gui_publishes_same_native_summary(kind, nucleus):
    doc = q.Document(q.Image.demo())
    obj = doc.add_cell(q.Roi.ellipse(q.Rect(100, 100, 120, 80)), q.Roi.rectangle(q.Rect(130, 120, 20, 10)))
    options = q.GuiOptions()
    options.analysis_kind, options.analysis_nucleus = kind, nucleus
    options.analysis_white = [250, 245, 230]
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.ANALYZE_KEY)]
    transform = q.StainTransform([options.analysis_vectors[:3], options.analysis_vectors[3:]], options.analysis_white)
    expected = q.analyze(q.AnalysisRequest(doc, obj, transform,
        q.AnalysisKind.STAIN_MEANS if kind else q.AnalysisKind.OPTICAL_DENSITY_MEANS, nucleus))
    report = q.show(doc, frame_limit=50, options=options)
    assert report.analysis_state == "ready", report.analysis_error
    assert report.analysis_samples == expected.samples and report.analysis_object == obj
    np.testing.assert_array_equal(report.analysis_means, expected.means)
    assert report.analysis_ms > 0 and report.analysis_tiles == expected.tiles


def test_gui_discards_results_after_selected_object_deletion():
    doc = q.Document(q.Image.demo())
    doc.add_rectangle(q.Rect(0, 0, 1024, 768))
    options = q.GuiOptions()
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.ANALYZE_KEY), event(25, q.GuiInputKind.DELETE_KEY)]
    report = q.show(doc, frame_limit=40, options=options)
    assert report.analysis_state == "stale"
    assert not report.analysis_means and report.analysis_samples == 0
    assert doc.object_count == 0
