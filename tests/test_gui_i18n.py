"""Language preferences, widget routing and project invariants on real GL."""
import os
import re

import pytest
import qupathxx as q
from test_gui_docking import config, workspace
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1',
                                reason='opt-in localized desktop GUI')
LANGUAGES = ['en', 'es', 'fr', 'de', 'pt-BR', 'it', 'zh-CN', 'zh-TW',
             'ja', 'ko', 'ru', 'tr', 'nl', 'pl', 'id', 'vi']


def chooser_click(options, frame, x, y):
    return [replay(frame, q.GuiInputKind.LEFT_DOWN, x/options.width, y/options.height, target=q.GuiInputTarget.WINDOW),
            replay(frame+1, q.GuiInputKind.LEFT_UP, x/options.width, y/options.height, target=q.GuiInputTarget.WINDOW)]


@pytest.mark.parametrize('language', ['es', 'zh-CN'])
def test_language_chooser_switches_live_and_blocks_background_edits(tmp_path, monkeypatch, language):
    monkeypatch.setenv('QUPATHXX_UI_LANGUAGE', 'en')
    project, doc = workspace(tmp_path)
    path = tmp_path/'chooser.ini'
    options = config(path)
    opening = [replay(5, q.GuiInputKind.POLYLINE_KEY, ctrl=True, shift=True)]
    options.inputs = opening
    q.show(project, frame_limit=24, options=options)
    text = path.read_text(encoding='utf8')
    modal = re.search(r'\[Window\]\[Choose language\]\s+Pos=(\d+),(\d+)\s+Size=(\d+),(\d+)', text)
    assert modal, 'Language menu did not open the chooser'
    x, y, width, height = map(int, modal.groups())
    options.inputs = [replay(3, q.GuiInputKind.SELECT_ALL_KEY)]+opening
    options.inputs += [replay(13, q.GuiInputKind.DELETE_KEY)]
    row = LANGUAGES.index(language)+1  # Automatic appears before explicit languages.
    options.inputs += chooser_click(options, 16, x+80, y+91+22*row)
    options.inputs += chooser_click(options, 22, x+35, y+height-20)
    report = q.show(project, frame_limit=32, options=options)
    assert report.replay_complete and doc.object_count == 1
    assert f'; qupathxx language={language}\n' in path.read_text(encoding='utf8')
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5 and not project.dirty


@pytest.mark.parametrize('language', LANGUAGES)
def test_language_persists_without_changing_user_data_or_camera(tmp_path, monkeypatch, language):
    monkeypatch.setenv('QUPATHXX_UI_LANGUAGE', language)
    project, doc = workspace(tmp_path)
    # User strings that resemble catalogue keys must stay literal data.
    doc.rename_annotation(1, 'Project')
    project.save(str(tmp_path/'source.qpxproj'))
    generation = project.generation
    original = (tmp_path/'source.qpxproj').read_bytes()
    path = tmp_path/'workspace.ini'
    report = q.show(project, frame_limit=12, options=config(path))
    assert report.canvas_visible and report.tiles_settled and not report.layout_error
    assert report.ui_overflow_items == 0, (report.ui_overflow_label, report.ui_max_overflow)
    assert f'; qupathxx language={language}\n' in path.read_text(encoding='utf8')
    monkeypatch.delenv('QUPATHXX_UI_LANGUAGE')
    restored = q.show(project, frame_limit=10, options=config(path))
    assert restored.layout_loaded and restored.layout_saved
    assert f'; qupathxx language={language}\n' in path.read_text(encoding='utf8')
    assert restored.final_center.x == pytest.approx(440) and restored.final_center.y == pytest.approx(330)
    assert restored.final_zoom == 1.5 and doc.annotation(1).name == 'Project'
    assert project.generation == generation and not project.dirty
    assert (tmp_path/'source.qpxproj').read_bytes() == original


@pytest.mark.parametrize('language', ['es', 'zh-CN', 'ja', 'ru'])
def test_localized_save_button_routes_after_dpi_resize(tmp_path, monkeypatch, language):
    monkeypatch.setenv('QUPATHXX_UI_LANGUAGE', language)
    project, _ = workspace(tmp_path)
    project.rename_image(1, '中文 / Project / Français')
    options = config(tmp_path/'workspace.ini')
    options.width, options.height, options.ui_scale = 900, 700, 2
    down = replay(4, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_PROJECT)
    down.wait_for = q.GuiInputWait.TARGET_READY
    options.inputs = [down, replay(5, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SAVE_PROJECT)]
    report = q.show(project, frame_limit=40, options=options)
    assert report.replay_complete and report.saves_completed == 1 and not project.dirty
    assert report.ui_overflow_items == 0, (report.ui_overflow_label, report.ui_max_overflow)
    assert q.Project.open(str(tmp_path/'source.qpxproj')).images[0].name == '中文 / Project / Français'


def test_reset_layout_retains_language_and_unknown_preference_is_safe(tmp_path, monkeypatch):
    project, _ = workspace(tmp_path)
    path = tmp_path/'workspace.ini'
    monkeypatch.setenv('QUPATHXX_UI_LANGUAGE', 'zh-TW')
    q.show(project, frame_limit=10, options=config(path))
    monkeypatch.delenv('QUPATHXX_UI_LANGUAGE')
    options = config(path)
    options.reset_layout = True
    report = q.show(project, frame_limit=10, options=options)
    assert not report.layout_loaded and report.layout_saved and not report.layout_error
    text = path.read_text(encoding='utf8')
    assert '; qupathxx language=zh-TW\n' in text
    path.write_text(text.replace('language=zh-TW', 'language=unknown'), encoding='utf8')
    report = q.show(project, frame_limit=10, options=config(path))
    assert report.layout_loaded and report.layout_saved and not report.layout_error
    assert '; qupathxx language=auto\n' in path.read_text(encoding='utf8')


def test_sessions_restore_english_and_language_changes_draw_real_text(tmp_path, monkeypatch):
    project, _ = workspace(tmp_path)
    captures = []
    for language in ['en', 'zh-CN', 'en']:
        monkeypatch.setenv('QUPATHXX_UI_LANGUAGE', language)
        path = tmp_path/f'capture-{len(captures)}.ppm'
        report = q.show(project, frame_limit=16, capture_path=str(path), options=config())
        assert report.tiles_settled and report.canvas_visible
        captures.append(path.read_bytes())
    assert captures[0] != captures[1]
    assert captures[0] == captures[2]
