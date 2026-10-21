import os
import pytest
import qupathxx as q
from test_cells import phantom
from test_tissue import document
from test_gui_replay import event
from test_cell_records import snapshot

pytestmark = pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in project desktop replay")


def test_gui_archive_reopen_recheck_and_publish_cells(tmp_path):
    project = q.Project()
    doc = document(phantom(64))
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(0,0,64,64))
    options = q.GuiOptions()
    options.inputs = [event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10,q.GuiInputKind.KEEP_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_READY)]
    report = q.show(doc,frame_limit=40,options=options)
    assert report.saved_cell_id and report.saved_cell_status == "current" and doc.cell_record_count == 1
    record = doc.cell_record(report.saved_cell_id)
    path = tmp_path/"cells.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert snapshot(restored.cell_record(record.id).result) == snapshot(record.result)
    assert restored.cell_status(record.id) == "unverified"
    revision = restored.revision
    options.inputs = [event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.RECHECK_CELL_RUN_KEY),
                      event(10,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELLS_VERIFIED)]
    checked = q.show(restored,frame_limit=40,options=options)
    assert checked.saved_cell_id == record.id and checked.saved_cell_status == "current" and checked.cell_preview_ready
    assert restored.revision == revision and restored.cell_record_count == 1
    assert checked.cell_result.nuclei.labels == record.result.nuclei.labels
    if q.has_geos:
        options.inputs = [event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.RECHECK_CELL_RUN_KEY),
                          event(10,q.GuiInputKind.KEEP_CELLS_KEY,wait=q.GuiInputWait.CELLS_VERIFIED)]
        published = q.show(restored,frame_limit=40,options=options)
        assert published.cells_published and len(restored.children(obj)) == record.result.cell_count
        assert restored.cell_record_count == 1 and restored.cell_status(record.id) == "current"


def test_archive_wait_gates_keep_before_recheck_and_does_not_duplicate():
    doc = document(phantom(64))
    obj = doc.add_rectangle(q.Rect(0,0,64,64))
    options = q.GuiOptions()
    options.inputs = [event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10,q.GuiInputKind.KEEP_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_READY),
                      event(12,q.GuiInputKind.KEEP_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED),
                      event(14,q.GuiInputKind.RECHECK_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED)]
    report = q.show(doc,frame_limit=40,options=options)
    assert doc.cell_record_count == 1 and report.saved_cell_id == doc.latest_cells(obj).id
    assert report.saved_cell_status == "current" and report.cell_state == "ready"


def test_missing_cell_archive_readiness_times_out_without_mutation():
    doc = document(phantom(32))
    doc.add_rectangle(q.Rect(0,0,32,32))
    options = q.GuiOptions()
    options.replay_timeout_seconds = .1
    options.inputs = [event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.RECHECK_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED)]
    with pytest.raises(RuntimeError,match="GUI replay timed out"):
        q.show(doc,frame_limit=10,options=options)
    assert doc.cell_record_count == 0
