"""Theme persistence and scientific/view invariants of the shared UI system."""
import os

import pytest
import qupathxx as q
from test_gui_docking import config, workspace
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native design system')


def test_compact_spatial_action_routes_to_the_existing_panel(tmp_path):
    project, doc = workspace(tmp_path)
    options = config(tmp_path/'workspace.ini')
    down = replay(4, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SPATIAL_OPEN)
    down.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [down, replay(5, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SPATIAL_OPEN)]
    generation = project.generation
    report = q.show(project, frame_limit=16, options=options)
    assert report.replay_complete and doc.object_count == 1 and project.generation == generation
    assert '[Window][Spatial analysis]' in (tmp_path/'workspace.ini').read_text(encoding='utf8')


@pytest.mark.parametrize('theme', ['dark', 'light', 'system'])
def test_appearance_preference_persists_through_reset_without_project_changes(tmp_path, monkeypatch, theme):
    project, doc = workspace(tmp_path)
    generation = project.generation
    original = (tmp_path/'source.qpxproj').read_bytes()
    path = tmp_path/'workspace.ini'
    monkeypatch.setenv('QUPATHXX_UI_THEME', theme)
    first = q.show(project, frame_limit=12, options=config(path))
    assert first.canvas_visible and first.tiles_settled and not first.layout_error
    assert f'; qupathxx theme={theme}\n' in path.read_text(encoding='utf8')
    monkeypatch.delenv('QUPATHXX_UI_THEME')
    options = config(path)
    options.reset_layout = True
    report = q.show(project, frame_limit=12, options=options)
    assert report.layout_saved and not report.layout_error
    assert f'; qupathxx theme={theme}\n' in path.read_text(encoding='utf8')
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5 and project.generation == generation and not project.dirty
    assert (tmp_path/'source.qpxproj').read_bytes() == original and doc.object_count == 1


def test_both_palettes_preserve_actual_image_pixels_and_overlay_geometry(tmp_path, monkeypatch):
    from PIL import Image
    project, _ = workspace(tmp_path)
    chrome = []
    for theme in ['dark', 'light']:
        monkeypatch.setenv('QUPATHXX_UI_THEME', theme)
        capture = tmp_path/f'{theme}.ppm'
        report = q.show(project, frame_limit=16, capture_path=str(capture), options=config())
        canvas = report.canvas_bounds
        with Image.open(capture) as frame:
            assert frame.getpixel((int(canvas.x+canvas.width*.75), int(canvas.y+canvas.height*.6))) == (30, 120, 210)
            chrome.append(frame.getpixel((int(report.browser_bounds.x+20), int(report.browser_bounds.y+report.browser_bounds.height-20))))
        assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
        assert report.final_zoom == 1.5 and report.ui_overflow_items == 0
    assert chrome == [(22, 22, 22), (250, 250, 249)]


@pytest.mark.parametrize('theme,language', [('dark', 'zh-CN'), ('light', 'fr')])
def test_themed_localized_save_stays_usable_at_200_percent(tmp_path, monkeypatch, theme, language):
    monkeypatch.setenv('QUPATHXX_UI_THEME', theme)
    monkeypatch.setenv('QUPATHXX_UI_LANGUAGE', language)
    project, _ = workspace(tmp_path)
    project.rename_image(1, 'Scientific data / 科学数据')
    options = config(tmp_path/'workspace.ini')
    options.width, options.height, options.ui_scale = 900, 700, 2
    down = replay(4, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_PROJECT)
    down.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [down, replay(5, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SAVE_PROJECT)]
    report = q.show(project, frame_limit=40, options=options)
    assert report.replay_complete and report.saves_completed == 1 and not project.dirty
    assert report.ui_overflow_items == 0, (report.ui_overflow_label, report.ui_max_overflow)
    assert q.Project.open(str(tmp_path/'source.qpxproj')).images[0].name == 'Scientific data / 科学数据'
