"""Responsive control bounds, actual wrapped-button input and camera invariants."""
import os
import re

import pytest
import qupathxx as q
from test_gui_docking import config, workspace
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in responsive GUI')


def no_overflow(report):
    assert report.ui_overflow_items == 0, (report.ui_overflow_label, report.ui_max_overflow)
    assert report.ui_max_overflow <= .75


@pytest.mark.parametrize('width,height,scale', [(1440, 900, 1), (1440, 900, 2), (1100, 700, 2), (900, 700, 1.5)])
def test_responsive_docked_controls_and_camera(tmp_path, width, height, scale):
    project, _ = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.width, options.height, options.ui_scale = width, height, scale
    report = q.show(project, frame_limit=14, capture_path=str(tmp_path/'layout.ppm'), options=options)
    no_overflow(report)
    if scale > 1:
        assert report.ui_stacked_fields > 0
    if scale == 2:
        assert report.ui_wrapped_buttons > 0
    assert report.canvas_visible and report.tiles_settled and report.canvas_bounds.height > 100
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5 and project.generation == generation and not project.dirty


def test_wrapped_save_project_button_performs_real_save(tmp_path):
    project, _ = workspace(tmp_path)
    project.rename_image(1, 'Saved through wrapped button')
    options = config()
    options.ui_scale = 2
    down = replay(3, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_PROJECT)
    down.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [down, replay(4, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SAVE_PROJECT)]
    report = q.show(project, frame_limit=40, options=options)
    assert report.replay_complete and report.saves_completed == 1 and not project.dirty
    assert q.Project.open(str(tmp_path/'source.qpxproj')).images[0].name == 'Saved through wrapped button'
    assert report.ui_wrapped_buttons > 0
    no_overflow(report)


def test_compact_resize_keeps_canvas_available_and_input_aligned(tmp_path):
    project, _ = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.docked_workspace = False
    options.ui_scale = 2
    options.inputs = [replay(3, q.GuiInputKind.RESIZE_WINDOW, 900, 700),
                      replay(7, q.GuiInputKind.MOVE, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(8, q.GuiInputKind.LEFT_DOWN, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(9, q.GuiInputKind.LEFT_UP, 440, 330, target=q.GuiInputTarget.IMAGE)]
    report = q.show(project, frame_limit=18, options=options)
    no_overflow(report)
    assert report.primary_selection == 1 and report.tiles_settled and report.replay_complete
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5 and project.generation == generation and not project.dirty


def test_scaled_cached_canvas_matches_immediate_after_reflow(tmp_path):
    project, _ = workspace(tmp_path)
    frames = []
    for budget in [0, 64*1024*1024]:
        options = config()
        options.ui_scale = 2
        options.overlay_cache_budget = budget
        options.inputs = [replay(3, q.GuiInputKind.SELECT_ALL_KEY),
                          replay(6, q.GuiInputKind.RESIZE_WINDOW, 1100, 700),
                          replay(10, q.GuiInputKind.SET_UI_SCALE, 1.5)]
        capture = tmp_path/f'cache-{budget}.ppm'
        report = q.show(project, frame_limit=22, capture_path=str(capture), options=options)
        no_overflow(report)
        assert report.tiles_settled and report.replay_complete
        frames.append(capture.read_bytes())
    assert frames[0] == frames[1]


def test_scaled_save_close_modal_fits_and_publishes_saved_project(tmp_path):
    project, _ = workspace(tmp_path)
    project.rename_image(1, 'Saved from scaled modal')
    options = config()
    options.width, options.height, options.ui_scale = 900, 700, 2
    down = replay(8, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_CLOSE)
    down.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [replay(3, q.GuiInputKind.CLOSE_WINDOW), down,
                      replay(9, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SAVE_CLOSE)]
    report = q.show(project, frame_limit=80, options=options)
    assert report.frames < 80 and report.saves_completed == 1 and not project.dirty
    assert q.Project.open(str(tmp_path/'source.qpxproj')).images[0].name == 'Saved from scaled modal'
    no_overflow(report)


def test_hidden_project_pane_does_not_publish_a_save_button_target(tmp_path):
    project, _ = workspace(tmp_path)
    path = tmp_path/'workspace.ini'
    q.show(project, frame_limit=10, options=config(path))
    text = path.read_text(encoding='utf8')
    node = re.search(r'\[Window\]\[Measurements and jobs\][\s\S]*?DockId=(0x[0-9A-F]+),', text).group(1)
    text = re.sub(r'(\[Window\]\[Project and objects\][\s\S]*?DockId=)0x[0-9A-F]+,0', lambda m: m.group(1)+node+',1', text)
    path.write_text(text, encoding='utf8')
    original = (tmp_path/'source.qpxproj').read_bytes()
    project.rename_image(1, 'Unsaved hidden edit')
    options = config(path)
    options.ui_scale, options.replay_timeout_seconds = 2, .2
    down = replay(3, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_PROJECT)
    down.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [down]
    with pytest.raises(RuntimeError, match='timed out'):
        q.show(project, frame_limit=15, options=options)
    assert project.dirty and (tmp_path/'source.qpxproj').read_bytes() == original


def test_unpainted_minimized_viewer_does_not_inherit_previous_layout_counters(tmp_path):
    project, _ = workspace(tmp_path)
    options = config()
    options.ui_scale = 2
    painted = q.show(project, frame_limit=10, options=options)
    assert painted.ui_wrapped_buttons > 0
    options.settle_tiles = False
    options.inputs = [replay(0, q.GuiInputKind.MINIMIZE_WINDOW)]
    report = q.show(project, frame_limit=5, options=options)
    assert report.frames == 0 and report.font_rebuilds == 0 and report.window_iconified
    assert report.ui_stacked_fields == report.ui_wrapped_buttons == report.ui_overflow_items == 0
    assert not report.ui_overflow_label and report.ui_max_overflow == 0
