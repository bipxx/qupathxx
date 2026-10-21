"""Readiness waits preserve actions and bound unavailable-target failures."""
import os

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in desktop replay")


def event(frame, kind, target=q.GuiInputTarget.CANVAS, wait=q.GuiInputWait.IMMEDIATE):
    result = q.GuiInput()
    result.frame, result.kind, result.target, result.wait_for = frame, kind, target, wait
    return result


def test_legacy_finite_run_does_not_wait_for_uploads():
    doc = q.Document(q.Image(512, 512, [30, 120, 210]*(512*512)))
    report = q.show(doc, frame_limit=1)
    assert report.frames == report.replay_frame == 1
    assert report.replay_complete and report.replay_wait_frames == report.settle_frames == 0
    assert report.uploaded_tiles == 0 and report.pending_tiles == 4 and not report.tiles_settled


def test_settlement_renders_and_captures_the_complete_current_image(tmp_path):
    from PIL import Image
    doc = q.Document(q.Image(512, 512, [30, 120, 210]*(512*512)))
    options = q.GuiOptions()
    options.settle_tiles = True
    capture = tmp_path/"settled.ppm"
    report = q.show(doc, frame_limit=1, capture_path=str(capture), options=options)
    assert report.replay_frame == 1 and report.settle_frames >= 1
    assert report.frames == report.replay_frame+report.settle_frames
    assert report.pending_tiles == 0 and report.uploaded_tiles == 4 and report.tiles_settled
    assert report.peak_gpu_bytes == 512*512*4
    with Image.open(capture) as image:
        assert image.getpixel((int(image.width*.65), image.height//2)) == (30, 120, 210)


def test_missing_button_target_times_out_without_injecting_a_click():
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20))
    revision = doc.revision
    options = q.GuiOptions()
    options.replay_timeout_seconds = .05
    options.inputs = [event(2, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_CLOSE, q.GuiInputWait.TARGET_READY)]
    with pytest.raises(RuntimeError, match="GUI replay timed out.*logical frame 2"):
        q.show(doc, frame_limit=5, options=options)
    assert doc.revision == revision and doc.annotation(obj).bounds.width == 20
    # A failed replay must retire its context and permit another viewer session.
    assert q.show(doc, frame_limit=3).frames == 3


@pytest.mark.skipif(not q.has_project, reason="requires projects")
def test_startup_capture_follows_adoption_and_completed_upload(tmp_path):
    from PIL import Image
    source = tmp_path/"source.ppm"
    source.write_bytes(b"P6\n32 32\n255\n"+bytes([40, 80, 160])*32*32)
    options = q.GuiOptions()
    options.startup_kind, options.startup_path, options.settle_tiles = "image", str(source), True
    capture = tmp_path/"startup.ppm"
    report = q.show(q.Project(), frame_limit=1, capture_path=str(capture), options=options)
    assert report.startup_succeeded and report.startup_frames >= 1
    assert report.tiles_settled and report.uploaded_tiles == 1 and report.pending_tiles == 0
    with Image.open(capture) as image:
        assert image.getpixel((int(image.width*.65), image.height//2)) == (40, 80, 160)


@pytest.mark.skipif(not q.has_project, reason="requires projects")
def test_closed_modal_coordinates_are_not_ready_for_a_later_action():
    project = q.Project()
    doc = q.Document(q.Image.demo())
    project.add_document(doc)
    doc.add_rectangle(q.Rect(10, 10, 20, 20))
    options = q.GuiOptions()
    options.autosave_interval_seconds = 0
    options.replay_timeout_seconds = .15
    options.inputs = [event(3, q.GuiInputKind.CLOSE_WINDOW),
                      event(4, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.CANCEL_CLOSE, q.GuiInputWait.TARGET_READY),
                      event(5, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.CANCEL_CLOSE),
                      event(6, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_CLOSE, q.GuiInputWait.TARGET_READY)]
    with pytest.raises(RuntimeError, match="GUI replay timed out.*logical frame 6"):
        q.show(project, frame_limit=10, options=options)
    assert project.dirty and doc.object_count == 1


@pytest.mark.parametrize("seconds", [0, -1, float("nan"), float("inf"), 61])
def test_invalid_replay_timeout_is_rejected_before_window_creation(seconds):
    options = q.GuiOptions()
    options.replay_timeout_seconds = seconds
    with pytest.raises(ValueError, match="replay timeout"):
        q.show(q.Document(q.Image.demo()), frame_limit=5, options=options)


def test_settlement_requires_a_finite_run():
    options = q.GuiOptions()
    options.settle_tiles = True
    with pytest.raises(ValueError, match="settlement.*positive frame_limit"):
        q.show(q.Document(q.Image.demo()), options=options)


@pytest.mark.parametrize("kind,target", [(q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_CLOSE),
                                       (q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.CANVAS),
                                       (q.GuiInputKind.CLOSE_WINDOW, q.GuiInputTarget.DISCARD_CLOSE)])
def test_target_readiness_requires_a_named_button_press(kind, target):
    options = q.GuiOptions()
    options.inputs = [event(2, kind, target, q.GuiInputWait.TARGET_READY)]
    with pytest.raises(ValueError, match="target readiness"):
        q.show(q.Document(q.Image.demo()), frame_limit=5, options=options)
