import os

import numpy as np
import pytest
import qupathxx as q

from test_gui_tissue import event

pytestmark = pytest.mark.skipif(not q.has_gui or not q.has_geos or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in GEOS desktop replay")


@pytest.mark.parametrize("undo", [False, True])
def test_gui_traces_and_publishes_one_undoable_batch(tmp_path, undo):
    rgb = np.full((64, 64, 3), 255, dtype=np.uint8)
    rgb[8:56, 8:56] = 0
    rgb[20:44, 20:44] = 255
    rgb[28:36, 28:36] = 0
    doc = q.Document(q.Image(64, 64, rgb.ravel().tolist()))
    source = doc.add_rectangle(q.Rect(0, 0, 64, 64))
    doc.clear_history()
    options = q.GuiOptions()
    trace = event(10, q.GuiInputKind.TRACE_TISSUE_KEY)
    trace.wait_for = q.GuiInputWait.TISSUE_READY
    keep = event(12, q.GuiInputKind.KEEP_TISSUE_CONTOURS_KEY)
    keep.wait_for = q.GuiInputWait.TISSUE_CONTOURS_READY
    inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY), trace, keep]
    if undo:
        last = event(14, q.GuiInputKind.UNDO_KEY)
        last.wait_for = q.GuiInputWait.TISSUE_CONTOURS_PUBLISHED
        inputs.append(last)
    options.inputs = inputs
    report = q.show(doc, frame_limit=50, capture_path=str(tmp_path/"contours.ppm"), options=options)
    assert report.tissue_state == "ready", report.tissue_error
    assert report.tissue_contour_components == report.tissue_annotations_added == 2
    assert report.tissue_contour_edges == report.tissue_contour_vertices == 12
    assert report.tissue_contours_published and not report.tissue_error
    if undo:
        assert doc.object_count == 1 and doc.can_redo and not doc.can_undo
        assert doc.redo()
    assert doc.object_count == 3 and len(doc.children(source)) == 2
    outer, inner = [doc.annotation(i) for i in doc.children(source)]
    assert outer.geometry.area == 48*48-24*24 and inner.geometry.area == 64
    assert len(outer.geometry.polygon_parts[0].holes) == 1
    assert outer.name == "Tissue 1" and inner.name == "Tissue 2"
    assert doc.undo() and doc.object_count == 1
