import os

import pytest
import qupathxx as q

from test_gui_replay import event

pytestmark = pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in project desktop replay")


def test_gui_keep_reopen_reverify_and_publish_saved_mask(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(100, 100, 120, 80))
    options = q.GuiOptions()
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY),
                      event(10, q.GuiInputKind.KEEP_TISSUE_KEY, wait=q.GuiInputWait.TISSUE_READY)]
    first = q.show(doc, frame_limit=40, options=options)
    assert first.saved_tissue_id and first.saved_tissue_status == "current"
    record = doc.latest_tissue(obj)
    assert doc.tissue_record_count == 1 and record.result.input_sha256 == first.tissue_input_sha256
    path = tmp_path/"mask.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert restored.tissue_status(record.id) == "unverified"
    revision = restored.revision
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.RECHECK_TISSUE_KEY),
                      event(10, q.GuiInputKind.MOVE, wait=q.GuiInputWait.TISSUE_VERIFIED)]
    options.inputs[-1].x, options.inputs[-1].y = .9, .9
    replayed = q.show(restored, frame_limit=40, options=options)
    assert replayed.saved_tissue_id == record.id and replayed.saved_tissue_status == "current"
    assert replayed.tissue_state == "ready" and replayed.tissue_input_sha256 == record.result.input_sha256
    assert restored.revision == revision and restored.tissue_record_count == 1
    assert replayed.tissue_foreground_pixels == record.result.foreground_pixels
    if q.has_geos:
        # Use the rechecked native job through the normal inspector shortcuts.
        options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.RECHECK_TISSUE_KEY),
                          event(10, q.GuiInputKind.TRACE_TISSUE_KEY, wait=q.GuiInputWait.TISSUE_VERIFIED),
                          event(12, q.GuiInputKind.KEEP_TISSUE_CONTOURS_KEY, wait=q.GuiInputWait.TISSUE_CONTOURS_READY)]
        contours = q.show(restored, frame_limit=45, options=options)
        assert contours.tissue_contours_published and len(restored.children(obj)) == record.result.component_count
        assert restored.tissue_record_count == 1 and restored.tissue_status(record.id) == "current"


def test_archived_wait_gates_actual_keep_application():
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(100, 100, 120, 80))
    options = q.GuiOptions()
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY),
                      event(10, q.GuiInputKind.KEEP_TISSUE_KEY, wait=q.GuiInputWait.TISSUE_READY),
                      event(12, q.GuiInputKind.RECHECK_TISSUE_KEY, wait=q.GuiInputWait.TISSUE_ARCHIVED)]
    report = q.show(doc, frame_limit=45, options=options)
    assert report.saved_tissue_id == doc.latest_tissue(obj).id and report.saved_tissue_status == "current"
    assert doc.tissue_record_count == 1 and report.tissue_state == "ready"


def test_tissue_readiness_timeout_rejects_missing_archive():
    doc = q.Document(q.Image.demo())
    doc.add_rectangle(q.Rect(100, 100, 120, 80))
    options = q.GuiOptions()
    options.replay_timeout_seconds = .1
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY),
                      event(8, q.GuiInputKind.RECHECK_TISSUE_KEY, wait=q.GuiInputWait.TISSUE_ARCHIVED)]
    with pytest.raises(RuntimeError, match="GUI replay timed out"):
        q.show(doc, frame_limit=10, options=options)
    assert doc.tissue_record_count == 0
