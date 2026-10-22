"""Native docking, view invariants and project-independent layout persistence."""
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest
import qupathxx as q
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native docking')


def config(path=None):
    options = q.GuiOptions()
    options.docked_workspace = True
    options.settle_tiles = True
    options.autosave_interval_seconds = 0
    if path is not None:
        options.layout_path = str(path)
    return options


def workspace(tmp_path):
    doc = q.Document(q.Image(1024, 768, [30, 120, 210]*1024*768))
    doc.add_rectangle(q.Rect(400, 300, 80, 60), 'Focus')
    project = q.Project()
    image = project.add_document(doc)
    view = q.ProjectView()
    view.center, view.zoom = q.Point(440, 330), 1.5
    project.set_view(image, view)
    project.save(str(tmp_path/'source.qpxproj'))
    return project, doc


def bounds(rect):
    return rect.x, rect.y, rect.width, rect.height


def test_docked_default_panel_geometry_and_canvas_pixels(tmp_path):
    from PIL import Image
    project, _ = workspace(tmp_path)
    original = (tmp_path/'source.qpxproj').read_bytes()
    generation = project.generation
    capture = tmp_path/'docked.ppm'
    report = q.show(project, frame_limit=12, capture_path=str(capture), options=config())
    assert report.docked_workspace and report.tiles_settled and not report.layout_error
    assert report.canvas_visible
    assert not report.layout_saved and not report.layout_loaded
    browser, display, measurement, canvas = report.browser_bounds, report.display_bounds, report.measurement_bounds, report.canvas_bounds
    assert browser.x+browser.width <= canvas.x+5 and canvas.x+canvas.width <= display.x+5
    assert canvas.y+canvas.height <= measurement.y+5 and measurement.x == pytest.approx(canvas.x, abs=5)
    assert all(rect.width > 200 and rect.height > 100 for rect in [browser, display, measurement, canvas])
    with Image.open(capture) as image:
        assert image.getpixel((int(canvas.x+canvas.width*.75), int(canvas.y+canvas.height*.6))) == (30, 120, 210)
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330) and report.final_zoom == 1.5
    assert project.generation == generation and not project.dirty
    assert (tmp_path/'source.qpxproj').read_bytes() == original


def test_drag_dock_splitter_preserves_center_and_persists_separately(tmp_path):
    project, _ = workspace(tmp_path)
    baseline = q.show(project, frame_limit=10, options=config())
    path = tmp_path/'preferences'/'\u5e03\u5c40.ini'
    options = config(path)
    x = (baseline.browser_bounds.x+baseline.browser_bounds.width+baseline.canvas_bounds.x)/2
    y = baseline.canvas_bounds.y+baseline.canvas_bounds.height/2
    def pointer(frame, kind, px):
        return replay(frame, kind, px/options.width, y/options.height, target=q.GuiInputTarget.WINDOW)
    options.inputs = [pointer(frame, q.GuiInputKind.MOVE, x) for frame in range(2, 8)]
    options.inputs += [pointer(8, q.GuiInputKind.LEFT_DOWN, x)]
    options.inputs += [pointer(frame, q.GuiInputKind.MOVE, x+50) for frame in range(9, 16)]
    options.inputs += [pointer(16, q.GuiInputKind.LEFT_UP, x+50)]
    generation = project.generation
    resized = q.show(project, frame_limit=24, options=options)
    assert resized.browser_bounds.width > baseline.browser_bounds.width+30
    assert resized.final_center.x == pytest.approx(440) and resized.final_center.y == pytest.approx(330) and resized.final_zoom == 1.5
    assert project.generation == generation and not project.dirty
    assert resized.layout_saved and not resized.layout_loaded and not resized.layout_error
    assert path.read_bytes().startswith(b'; qupathxx workspace layout v1\n')
    assert b'[Docking][Data]' in path.read_bytes() and b'source.qpxproj' not in path.read_bytes()
    restored = q.show(project, frame_limit=10, options=config(path))
    assert restored.layout_loaded and restored.layout_saved and not restored.layout_error
    assert bounds(restored.browser_bounds) == pytest.approx(bounds(resized.browser_bounds), abs=1)
    assert bounds(restored.canvas_bounds) == pytest.approx(bounds(resized.canvas_bounds), abs=1)
    reset = config(path)
    reset.reset_layout = True
    default = q.show(project, frame_limit=10, options=reset)
    assert not default.layout_loaded and default.layout_saved
    assert bounds(default.browser_bounds) == pytest.approx(bounds(baseline.browser_bounds), abs=1)


def test_docked_cached_canvas_matches_immediate_pixels(tmp_path):
    captures, reports = [], []
    project, _ = workspace(tmp_path)
    for budget in [0, 64*1024*1024]:
        options = config()
        options.overlay_cache_budget = budget
        options.inputs = [replay(3, q.GuiInputKind.SELECT_ALL_KEY)]
        capture = tmp_path/f'cache-{budget}.ppm'
        reports.append(q.show(project, frame_limit=15, capture_path=str(capture), options=options))
        captures.append(capture.read_bytes())
    assert captures[0] == captures[1]
    assert reports[0].overlay_cache_hits == 0 and reports[1].overlay_cache_hits > 0


def test_docked_native_resize_keeps_project_view(tmp_path):
    project, doc = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.inputs = [replay(3, q.GuiInputKind.RESIZE_WINDOW, 1100, 700)]
    report = q.show(project, frame_limit=16, options=options)
    assert report.active_image == 1 and report.replay_complete
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330) and report.final_zoom == 1.5
    assert (report.framebuffer_width, report.framebuffer_height) == (1100, 700)
    assert project.generation == generation and not project.dirty and doc.object_count == 1


def test_docked_image_switch_restores_source_camera(tmp_path):
    project, doc = workspace(tmp_path)
    project.add_document(q.Document(q.Image(128, 128, [100, 50, 20]*128*128)), 'Second')
    options = config()
    options.inputs = [replay(4, q.GuiInputKind.NEXT_IMAGE_KEY), replay(8, q.GuiInputKind.PREVIOUS_IMAGE_KEY)]
    report = q.show(project, frame_limit=16, options=options)
    assert report.active_image == 1 and report.image_switches == 2 and report.replay_complete
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330) and report.final_zoom == 1.5
    assert doc.object_count == 1


def test_tabbed_away_canvas_skips_uploads_without_overwriting_stored_view(tmp_path):
    project, _ = workspace(tmp_path)
    path = tmp_path/'workspace.ini'
    q.show(project, frame_limit=10, options=config(path))
    text = path.read_text(encoding='utf8')
    # A legitimate saved layout can tab the slide with the measurements pane.
    # Keep the measurements node selected to start with its slide tab hidden.
    node = re.search(r'\[Window\]\[Measurements and jobs\][\s\S]*?DockId=(0x[0-9A-F]+),', text).group(1)
    text = re.sub(r'(\[Window\]\[Slide\][\s\S]*?DockId=)0x[0-9A-F]+,0', lambda m: m.group(1)+node+',1', text)
    path.write_text(text, encoding='utf8')
    generation = project.generation
    report = q.show(project, frame_limit=12, options=config(path))
    assert report.layout_loaded and not report.canvas_visible and report.tiles_settled
    assert report.uploaded_tiles == 0 and report.visible_objects == 0
    assert report.overview_state == 'succeeded' and report.overview_gpu_bytes > 0
    view = project.view(1)
    assert view.center.x == pytest.approx(440) and view.center.y == pytest.approx(330) and view.zoom == 1.5
    assert project.generation == generation and not project.dirty


@pytest.mark.parametrize('contents', [b'not qupathxx settings', b'x'*(1024*1024+1)], ids=['unrecognized', 'oversized'])
def test_unrecognized_or_oversized_preferences_are_preserved_until_reset(tmp_path, contents):
    path = tmp_path/'workspace.ini'
    path.write_bytes(contents)
    report = q.show(q.Document(q.Image.demo()), frame_limit=5, options=config(path))
    assert report.layout_error and not report.layout_saved and not report.layout_loaded
    assert path.read_bytes() == contents
    options = config(path)
    options.inputs = [replay(3, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.RESET_LAYOUT),
                      replay(4, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.RESET_LAYOUT)]
    report = q.show(q.Document(q.Image.demo()), frame_limit=8, options=options)
    assert report.layout_saved and not report.layout_error
    assert path.read_bytes().startswith(b'; qupathxx workspace layout v1\n')


def test_preference_write_failure_keeps_project_usable(tmp_path):
    project, _ = workspace(tmp_path)
    parent = tmp_path/'blocked'
    parent.write_text('file, not directory', encoding='utf8')
    report = q.show(project, frame_limit=5, options=config(parent/'workspace.ini'))
    assert report.layout_error and not report.layout_saved and report.tiles_settled
    assert not project.dirty and parent.read_text(encoding='utf8') == 'file, not directory'


def test_reset_cannot_replace_a_project_named_with_ini_extension(tmp_path):
    project, _ = workspace(tmp_path)
    path = tmp_path/'workspace.ini'
    project.save(str(path))
    original = path.read_bytes()
    options = config(path)
    options.reset_layout = True
    report = q.show(project, frame_limit=5, options=options)
    assert not report.layout_saved and 'cannot replace' in report.layout_error
    assert path.read_bytes() == original and not project.dirty


def test_empty_docked_project_and_annotation_history(tmp_path):
    empty = q.show(q.Project(), frame_limit=5, options=config())
    assert empty.tiles_settled and empty.canvas_bounds.width > 200 and empty.measurement_bounds.height > 100
    project, doc = workspace(tmp_path)
    options = config()
    options.inputs = [replay(3, q.GuiInputKind.SELECT_ALL_KEY), replay(5, q.GuiInputKind.DELETE_KEY), replay(7, q.GuiInputKind.UNDO_KEY)]
    report = q.show(project, frame_limit=12, options=options)
    assert report.replay_complete and doc.object_count == 1 and doc.annotation(1).name == 'Focus'


def test_docked_project_launcher_does_not_write_global_preferences_in_smoke_mode(tmp_path):
    project, _ = workspace(tmp_path)
    executable = Path(q.__file__).parent/'qupathxx_app.exe'
    if sys.platform != 'win32':
        pytest.skip('Windows standalone GUI')
    path = tmp_path/'cli.ini'
    subprocess.run([str(executable), '--project', str(tmp_path/'source.qpxproj'), '--layout', str(path), '--smoke'],
                   check=True, capture_output=True, timeout=45)
    assert b'[Docking][Data]' in path.read_bytes()
