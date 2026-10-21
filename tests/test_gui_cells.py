import os

import numpy as np
import pytest
import qupathxx as q

from test_cells import phantom
from test_tissue import document
from test_gui_replay import event

pytestmark = pytest.mark.skipif(not q.has_gui or not q.has_geos or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in geometry desktop replay")


@pytest.mark.parametrize("undo", [False, True])
def test_gui_cells_match_native_masks_and_one_step_history(tmp_path, undo):
    rgb = phantom(64)
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, 64, 64))
    doc.clear_history()
    p = q.CellParameters()
    p.sigma, p.cell_expansion = 0, 3
    expected = q.detect_cells(q.CellRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p))
    options = q.GuiOptions()
    options.cell_parameters = p
    p.sigma = 10
    assert options.cell_parameters.sigma == 0
    inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_CELLS_KEY),
              event(10, q.GuiInputKind.KEEP_CELLS_KEY, wait=q.GuiInputWait.CELLS_READY)]
    if undo:
        inputs.append(event(12, q.GuiInputKind.UNDO_KEY, wait=q.GuiInputWait.CELLS_PUBLISHED))
    options.inputs = inputs
    capture = tmp_path/"cells.ppm"
    report = q.show(doc, frame_limit=50, capture_path=str(capture), options=options)
    assert report.cell_state == "ready" and not report.cell_error
    assert report.cell_result.nuclei.labels == expected.nuclei.labels and report.cell_result.cells.labels == expected.cells.labels
    assert report.cells_published and report.cell_annotations_added == expected.cell_count == 2
    assert report.cell_preview_ready and 0 < report.cell_gpu_bytes <= 32*1024*1024
    assert report.selected_ids == [obj]
    if undo:
        assert doc.object_count == 1 and doc.can_redo and not doc.can_undo
        assert doc.redo()
    assert doc.object_count == 3 and len(doc.children(obj)) == 2
    for i, feature in zip(doc.children(obj), expected.features):
        cell = doc.annotation(i)
        assert cell.kind == q.ObjectKind.CELL and cell.nucleus
        assert cell.cell_feature_link.label == feature.label and doc.cell_object_status(i) == "current"
        assert doc.cell_object_feature(i).nucleus_rgb == feature.nucleus_rgb
        assert cell.geometry.area == feature.cell_pixels and cell.nucleus.area == feature.nucleus_pixels
    header, dimensions, maximum, data = capture.read_bytes().split(b"\n", 3)
    assert header == b"P6" and maximum == b"255"
    width, height = map(int, dimensions.split())
    pixels = np.frombuffer(data, dtype=np.uint8).reshape(height, width, 3).astype(int)
    orange = (np.abs(pixels-np.array([127, 83, 51])) <= 2).all(axis=2)
    assert int(orange.sum()) > 1000


def test_gui_cell_result_is_discarded_after_source_deletion():
    doc = document(phantom(64))
    doc.add_rectangle(q.Rect(0, 0, 64, 64))
    options = q.GuiOptions()
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10, q.GuiInputKind.DELETE_KEY, wait=q.GuiInputWait.CELLS_READY)]
    report = q.show(doc, frame_limit=35, options=options)
    assert report.cell_state == "stale" and report.cell_result is None
    assert not report.cell_preview_ready and not report.cells_published and doc.object_count == 0


@pytest.mark.parametrize("state",["current","geometry_changed","archive_missing"])
def test_gui_inspector_shows_historical_cell_without_mutating_features(tmp_path,state):
    from test_cell_feature_links import setup,signature
    doc,_,_,result,record=setup()
    ids=doc.publish_cell_record(record)
    cell=ids[-1]
    if state=="geometry_changed":doc.update_nucleus(cell,None)
    if state=="archive_missing":doc.remove_cells(record)
    before=doc.revision
    options=q.GuiOptions();options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY)]
    report=q.show(doc,frame_limit=20,capture_path=str(tmp_path/"inspector.ppm"),options=options)
    assert report.primary_selection==cell and doc.revision==before and doc.cell_object_status(cell)==state
    if state!="archive_missing":assert signature(doc.cell_object_feature(cell))==signature(result.features[-1])
