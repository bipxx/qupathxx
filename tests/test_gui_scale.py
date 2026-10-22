"""UI scaling exercises real font textures, cached overlays and routed input."""
import os
from pathlib import Path
import subprocess
import sys

import pytest
import qupathxx as q
from test_gui_docking import config, workspace
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native DPI scaling')


@pytest.mark.parametrize('multiplier', [.75, 1., 1.5, 2.])
def test_scaled_workspace_keeps_camera_and_routes_image_input(tmp_path, multiplier):
    project, doc = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.ui_scale = multiplier
    options.inputs = [replay(4, q.GuiInputKind.MOVE, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(5, q.GuiInputKind.LEFT_DOWN, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(6, q.GuiInputKind.LEFT_UP, 440, 330, target=q.GuiInputTarget.IMAGE)]
    report = q.show(project, frame_limit=14, options=options)
    assert report.tiles_settled and report.replay_complete and report.canvas_visible
    assert report.ui_scale == pytest.approx(report.monitor_scale/report.framebuffer_scale*multiplier)
    assert report.font_rebuilds == 1
    assert report.primary_selection == 1 and report.selected_ids == [1]
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5
    assert project.generation == generation and not project.dirty and doc.object_count == 1


def test_live_scale_changes_invalidate_cache_without_camera_drift(tmp_path):
    project, _ = workspace(tmp_path)
    generation = project.generation
    captures, reports = [], []
    for budget in [0, 64*1024*1024]:
        options = config()
        options.overlay_cache_budget = budget
        options.inputs = [replay(3, q.GuiInputKind.SELECT_ALL_KEY),
                          replay(6, q.GuiInputKind.SET_UI_SCALE, 1.5),
                          replay(10, q.GuiInputKind.SET_UI_SCALE, 1.5),
                          replay(13, q.GuiInputKind.SET_UI_SCALE, 1),
                          replay(17, q.GuiInputKind.SET_UI_SCALE, 1.25)]
        capture = tmp_path/f'cache-{budget}.ppm'
        report = q.show(project, frame_limit=27, capture_path=str(capture), options=options)
        assert report.font_rebuilds == 4  # Repeating the current scale does no GL work.
        assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
        assert report.final_zoom == 1.5 and report.tiles_settled and report.replay_complete
        reports.append(report)
        captures.append(capture.read_bytes())
    assert captures[0] == captures[1]
    assert reports[1].overlay_cache_hits > 0 and reports[1].overlay_cache_builds >= 4
    assert project.generation == generation and not project.dirty


def test_small_successive_changes_accumulate_against_built_font_scale(tmp_path):
    project, _ = workspace(tmp_path)
    baseline = q.show(project, frame_limit=4, options=config()).ui_scale
    options = config()
    options.inputs = [replay(frame, q.GuiInputKind.SET_UI_SCALE, 1+delta/baseline)
                      for frame, delta in [(3, .0006), (5, .0012), (7, .0018), (9, .0024)]]
    report = q.show(project, frame_limit=14, options=options)
    assert report.font_rebuilds == 3
    assert report.ui_scale == pytest.approx(baseline+.0024, abs=1e-6)


def test_compact_scaled_resize_and_live_scale_preserve_camera(tmp_path):
    project, _ = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.docked_workspace = False
    options.ui_scale = 2
    options.inputs = [replay(3, q.GuiInputKind.RESIZE_WINDOW, 1100, 700),
                      replay(7, q.GuiInputKind.SET_UI_SCALE, 1.25),
                      replay(11, q.GuiInputKind.MOVE, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(12, q.GuiInputKind.LEFT_DOWN, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(13, q.GuiInputKind.LEFT_UP, 440, 330, target=q.GuiInputTarget.IMAGE)]
    report = q.show(project, frame_limit=20, options=options)
    assert not report.docked_workspace and report.font_rebuilds == 2
    assert report.tiles_settled and report.replay_complete and report.primary_selection == 1
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5 and project.generation == generation and not project.dirty


@pytest.mark.parametrize('value', [0, .49, 3.01, float('inf'), float('nan')])
def test_invalid_scale_rejected_before_opening_window(value):
    options = q.GuiOptions()
    options.ui_scale = value
    with pytest.raises(ValueError, match='UI scale'):
        q.show(q.Document(q.Image.demo()), frame_limit=2, options=options)
    options.ui_scale = 1
    options.inputs = [replay(0, q.GuiInputKind.SET_UI_SCALE, value)]
    with pytest.raises(ValueError, match='UI scale'):
        q.show(q.Document(q.Image.demo()), frame_limit=2, options=options)


@pytest.mark.parametrize('native', [False, True])
def test_launchers_accept_scaled_empty_workspace(native):
    if native and sys.platform != 'win32':
        pytest.skip('Windows standalone GUI')
    command = [str(Path(q.__file__).parent/'qupathxx_app.exe')] if native else [sys.executable, '-I', '-m', 'qupathxx']
    subprocess.run(command+['--empty', '--smoke', '--ui-scale', '1.5'], check=True, capture_output=True, timeout=45)
