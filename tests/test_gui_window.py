"""Actual GLFW resize/iconify/restore operations with native owner work continuing."""
import os
import time

import numpy as np
import pytest
import qupathxx as q

from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native window lifecycle')


def window(frame, kind, width=1440, height=900):
    return replay(frame, kind, width, height)


def workspace(tmp_path):
    doc = q.Document(q.Image(1024, 768, [40, 80, 160]*1024*768))
    doc.set_class('Focus', 0xfa0a0a)
    obj = doc.add_rectangle(q.Rect(380, 280, 40, 40), 'Focus')
    doc.classify_objects([obj], 'Focus')
    project = q.Project()
    image = project.add_document(doc)
    view = q.ProjectView()
    view.center, view.zoom = q.Point(400, 300), 2
    project.set_view(image, view)
    path = tmp_path/'source.qpxproj'
    project.save(str(path))
    return project, doc, obj, path


def test_resize_preserves_center_zoom_project_and_overlay_pixels(tmp_path):
    from PIL import Image
    project, _, _, _ = workspace(tmp_path)
    captures = []
    generation = project.generation
    for resized in [False, True]:
        options = q.GuiOptions()
        options.settle_tiles = True
        if resized:
            options.inputs = [window(4, q.GuiInputKind.RESIZE_WINDOW, 1040, 660)]
        capture = tmp_path/('resized.ppm' if resized else 'baseline.ppm')
        report = q.show(project, frame_limit=24, capture_path=str(capture), options=options)
        assert report.final_zoom == 2 and report.final_center.x == pytest.approx(400) and report.final_center.y == pytest.approx(300)
        assert not project.dirty and project.generation == generation and report.tiles_settled
        assert (report.framebuffer_width, report.framebuffer_height) == ((1040, 660) if resized else (1440, 900))
        with Image.open(capture) as image:
            pixels = np.asarray(image)
            mask = (pixels[:, :, 0] > 200) & (pixels[:, :, 1] < 40) & (pixels[:, :, 2] < 40)
            mask[:, :500] = False  # exclude the class swatch in the inspector
            captures.append(np.argwhere(mask))
    assert len(captures[0]) > 100
    np.testing.assert_array_equal(captures[1], captures[0] - [120, 200])


def test_resize_readiness_routes_image_coordinate_selection_after_new_layout(tmp_path):
    project, _, obj, _ = workspace(tmp_path)
    options = q.GuiOptions()
    down = replay(5, q.GuiInputKind.LEFT_DOWN, 400, 300, target=q.GuiInputTarget.IMAGE)
    down.wait_for = q.GuiInputWait.WINDOW_READY
    options.inputs = [window(4, q.GuiInputKind.RESIZE_WINDOW, 1040, 660), down,
                      replay(6, q.GuiInputKind.LEFT_UP, 400, 300, target=q.GuiInputTarget.IMAGE)]
    report = q.show(project, frame_limit=22, options=options)
    assert report.replay_complete and report.selected_ids == [obj] and report.final_zoom == 2
    assert not project.dirty


def test_minimize_restore_skips_rendering_and_keeps_view_state(tmp_path):
    project, _, _, _ = workspace(tmp_path)
    options = q.GuiOptions()
    ready = replay(19, q.GuiInputKind.MOVE)
    ready.wait_for = q.GuiInputWait.WINDOW_READY
    options.inputs = [window(4, q.GuiInputKind.MINIMIZE_WINDOW), window(18, q.GuiInputKind.RESTORE_WINDOW), ready]
    report = q.show(project, frame_limit=30, capture_path=str(tmp_path/'restored.ppm'), options=options)
    assert report.replay_complete and not report.window_iconified and report.suspended_iterations >= 10
    assert report.frames + report.suspended_iterations == report.replay_frame + report.replay_wait_frames
    assert report.suspended_work_ms >= 0 and report.suspended_wait_ms > 0
    assert report.final_zoom == 2 and report.final_center.x == pytest.approx(400) and report.final_center.y == pytest.approx(300)
    assert not project.dirty


def test_finite_minimized_view_returns_without_drawing_or_losing_autosave(tmp_path):
    project, doc, obj, primary = workspace(tmp_path)
    doc.rename_annotation(obj, 'Preserved while minimized')
    before = primary.read_bytes()
    options = q.GuiOptions()
    options.autosave_interval_seconds = .03
    options.autosave_path = str(tmp_path/'minimized.qpxrecover')
    options.inputs = [window(0, q.GuiInputKind.MINIMIZE_WINDOW)]
    report = q.show(project, frame_limit=16, options=options)
    assert report.frames == 0 and report.window_iconified and report.suspended_iterations == 16
    assert report.autosaves_completed >= 1 and not report.autosave_error
    recovered = q.Project.recover(report.recovery_path)
    assert recovered.document(1).annotation(obj).name == 'Preserved while minimized'
    assert primary.read_bytes() == before


def test_capture_requires_restoration_before_finite_end(tmp_path):
    options = q.GuiOptions()
    options.inputs = [window(0, q.GuiInputKind.MINIMIZE_WINDOW)]
    capture = tmp_path/'not-written.ppm'
    with pytest.raises(RuntimeError, match='restore the window before capture'):
        q.show(q.Document(q.Image.demo()), frame_limit=5, capture_path=str(capture), options=options)
    assert not capture.exists()


def test_dirty_close_restores_window_for_confirmation_without_rewriting_primary(tmp_path):
    project, doc, obj, source = workspace(tmp_path)
    doc.rename_annotation(obj, 'Unsaved')
    original = source.read_bytes()
    options = q.GuiOptions()
    options.autosave_interval_seconds = 0
    confirm = replay(12, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.DISCARD_CLOSE)
    confirm.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [window(3, q.GuiInputKind.MINIMIZE_WINDOW), replay(10, q.GuiInputKind.CLOSE_WINDOW),
                      confirm, replay(13, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.DISCARD_CLOSE)]
    report = q.show(project, frame_limit=30, options=options)
    assert report.replay_complete and not report.window_iconified and report.suspended_iterations > 0
    assert report.frames < 20 and source.read_bytes() == original and doc.annotation(obj).name == 'Unsaved'


def test_minimized_startup_deadline_cancels_waiting_commands(tmp_path):
    _, _, _, source = workspace(tmp_path)
    session = q.ViewerSession(str(source))
    ticket = session.catalog()
    options = q.GuiOptions()
    options.replay_timeout_seconds = .1
    options.inputs = [window(0, q.GuiInputKind.MINIMIZE_WINDOW)]
    with pytest.raises(RuntimeError, match='minimized startup'):
        q.show(session, frame_limit=3, options=options)
    assert session.stats.state == 'failed' and session.take(ticket).state == 'cancelled'


def test_managed_commands_save_and_force_close_continue_without_rendering(tmp_path):
    from test_viewer_session import completed, run
    project, _, obj, source = workspace(tmp_path)
    second = project.add_document(q.Document(q.Image(16, 16, [32]*16*16*3)), 'Second')
    project.save(str(source))
    session = q.ViewerSession(str(source))
    output = tmp_path/'managed.qpxproj'
    options = q.GuiOptions()
    options.autosave_interval_seconds = 0
    options.inputs = [window(30, q.GuiInputKind.MINIMIZE_WINDOW)]
    def producer():
        completed(session, session.catalog())
        end = time.monotonic() + 8
        while session.stats.frame < 30:
            assert time.monotonic() < end
            time.sleep(.01)
        time.sleep(.06)
        assert session.stats.frame == 30
        completed(session, session.execute(1, [q.ObjectEdit.replace(obj, q.Rect(380, 280, 40, 40),
                                                                  'From minimized owner', class_name='Focus')]))
        assert completed(session, session.inspect(1, obj)).object.name == 'From minimized owner'
        completed(session, session.activate(second))
        assert session.stats.active_image == second
        completed(session, session.save(str(output)))
        assert session.stats.frame == 30
        completed(session, session.close(True))
    report = run(session, producer, frames=600, options=options)
    assert report.window_iconified and report.suspended_iterations > 0 and report.frames == 30
    assert session.stats.state == 'closed'
    restored = q.Project.open(str(output))
    assert restored.active_image == second and restored.document(1).annotation(obj).name == 'From minimized owner'


@pytest.mark.parametrize('width', [0, 319, 16385, 1000.5, float('inf'), float('nan')])
def test_invalid_resize_is_rejected_before_opening_window(width):
    options = q.GuiOptions()
    options.inputs = [window(0, q.GuiInputKind.RESIZE_WINDOW, width, 600)]
    with pytest.raises(ValueError, match='integer window dimensions'):
        q.show(q.Document(q.Image.demo()), frame_limit=2, options=options)


def test_replay_requires_restore_before_ui_input():
    options = q.GuiOptions()
    options.inputs = [window(0, q.GuiInputKind.MINIMIZE_WINDOW), replay(2, q.GuiInputKind.SELECT_ALL_KEY)]
    with pytest.raises(ValueError, match='restore the window before replaying UI input'):
        q.show(q.Document(q.Image.demo()), frame_limit=4, options=options)
