"""Cached canvas pixels must preserve the immediate renderer's framebuffer."""
import os

import numpy as np
import pytest
import qupathxx as q

from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in desktop replay")


def document():
    doc = q.Document(q.Image(128, 128, [35, 60, 90]*(128*128)))
    parent = doc.add_rectangle(q.Rect(8, 7, 105, 107), "Parent")
    doc.add_cell(q.Roi.ellipse(q.Rect(20, 22, 35, 28)), q.Roi.ellipse(q.Rect(30, 29, 9, 8)),
                 "Cell", parent_id=parent, class_name="Tumor")
    doc.add_roi(q.Roi.polyline([q.Point(14, 18), q.Point(70, 68), q.Point(100, 38)]), "Line")
    doc.add_roi(q.Roi.points([q.Point(85, 82), q.Point(72, 94)]), "Points")
    if q.has_geos:
        ring = lambda x,y,w: [q.Point(x,y),q.Point(x+w,y),q.Point(x+w,y+w),q.Point(x,y+w)]
        doc.add_roi(q.Roi.polygon(ring(62, 14, 30), [ring(70, 22, 10)]), "Hole")
        doc.add_roi(q.Roi.multipolygon([q.PolygonPart(ring(15, 85, 8)), q.PolygonPart(ring(32, 85, 9))]), "Multipart")
    doc.set_class("Tumor", 0xe03080)
    doc.clear_history()
    return doc


def inputs(scenario):
    result = [replay(4, q.GuiInputKind.SELECT_ALL_KEY)]
    if scenario == "selection":
        result += [replay(10, q.GuiInputKind.ESCAPE_KEY), replay(16, q.GuiInputKind.LEFT_DOWN, .45, .45),
                   replay(17, q.GuiInputKind.LEFT_UP, .45, .45)]
    elif scenario == "history":
        result += [replay(10, q.GuiInputKind.DELETE_KEY), replay(14, q.GuiInputKind.UNDO_KEY), replay(18, q.GuiInputKind.REDO_KEY),
                   replay(22, q.GuiInputKind.UNDO_KEY)]
    elif scenario == "camera":
        result += [replay(10, q.GuiInputKind.WHEEL, .6, .4, wheel=1),
                   replay(14, q.GuiInputKind.RIGHT_DOWN, .5, .5), replay(15, q.GuiInputKind.MOVE, .65, .6),
                   replay(16, q.GuiInputKind.RIGHT_UP, .65, .6)]
    elif scenario == "draw":
        result += [replay(10, q.GuiInputKind.RECTANGLE_KEY), replay(12, q.GuiInputKind.LEFT_DOWN, .6, .6),
                   replay(14, q.GuiInputKind.MOVE, .7, .7), replay(16, q.GuiInputKind.LEFT_UP, .7, .7)]
    return result


@pytest.mark.parametrize("scenario", ["static", "selection", "history", "camera", "draw"])
@pytest.mark.parametrize("budget", [1, 64*1024*1024])
def test_overlay_cache_and_budget_fallback_match_immediate_pixels(tmp_path, scenario, budget):
    captures, reports = [], []
    for cache_budget in [0, budget]:
        doc = document()
        options = q.GuiOptions()
        options.overlay_cache_budget, options.settle_tiles = cache_budget, True
        options.inputs = inputs(scenario)
        capture = tmp_path/f"{cache_budget}.ppm"
        report = q.show(doc, frame_limit=40, capture_path=str(capture), options=options)
        captures.append(capture.read_bytes())
        reports.append(report)
    assert captures[0] == captures[1]
    reference, cached = reports
    assert cached.selected_ids == reference.selected_ids and cached.visible_objects == reference.visible_objects
    assert cached.overlay_cache_bytes <= budget and cached.overlay_cache_peak_bytes <= budget
    assert reference.overlay_cache_bytes == reference.overlay_cache_builds == reference.overlay_cache_hits == 0
    if budget == 1:
        assert cached.overlay_cache_bytes == 0 and cached.overlay_cache_fallbacks == cached.frames
        assert cached.overlay_cache_hits == cached.overlay_cache_builds == 0
    else:
        assert 0 < cached.overlay_cache_builds < cached.frames
        assert cached.overlay_cache_hits > 0 and cached.overlay_cache_fallbacks == 0
        assert cached.overlay_cache_bytes > 0
    if scenario != "static" and budget != 1:
        assert cached.overlay_cache_builds > 2


@pytest.mark.skipif(not q.has_project, reason="project workspace")
def test_overlay_cache_isolated_across_image_switches(tmp_path):
    captures = []
    for budget in [0, 64*1024*1024]:
        project = q.Project()
        first = project.add_document(document(), "One")
        other = document()
        other.add_rectangle(q.Rect(70, 70, 30, 30), "Only in second image")
        project.add_document(other, "Two")
        project.active_image = first
        options = q.GuiOptions()
        options.overlay_cache_budget, options.settle_tiles = budget, True
        options.inputs = [replay(4, q.GuiInputKind.SELECT_ALL_KEY), replay(10, q.GuiInputKind.NEXT_IMAGE_KEY),
                          replay(16, q.GuiInputKind.PREVIOUS_IMAGE_KEY), replay(22, q.GuiInputKind.NEXT_IMAGE_KEY)]
        capture = tmp_path/f"project-{budget}.ppm"
        report = q.show(project, frame_limit=40, capture_path=str(capture), options=options)
        assert report.image_switches == 3
        if budget:
            assert report.overlay_cache_builds >= 4 and report.overlay_cache_hits > 0
        captures.append(capture.read_bytes())
    assert captures[0] == captures[1]


def test_overlay_cache_budget_rejects_unbounded_allocation():
    options = q.GuiOptions()
    options.overlay_cache_budget = 256*1024*1024+1
    with pytest.raises(ValueError, match="overlay cache budget"):
        q.show(document(), frame_limit=2, options=options)


@pytest.mark.parametrize("cells", [False, True])
def test_finished_native_preview_invalidates_cached_canvas(tmp_path, cells):
    captures = []
    for budget in [0, 64*1024*1024]:
        doc = document()
        doc.add_rectangle(q.Rect(12, 12, 85, 85), "Preview source")
        options = q.GuiOptions()
        options.overlay_cache_budget, options.settle_tiles = budget, True
        start = q.GuiInputKind.DETECT_CELLS_KEY if cells else q.GuiInputKind.DETECT_TISSUE_KEY
        wait = q.GuiInputWait.CELLS_READY if cells else q.GuiInputWait.TISSUE_READY
        ready = replay(12, q.GuiInputKind.SELECT_ALL_KEY)
        ready.wait_for = wait
        options.inputs = [replay(4, q.GuiInputKind.SELECT_ALL_KEY), replay(8, start), ready]
        capture = tmp_path/f"mask-{budget}.ppm"
        report = q.show(doc, frame_limit=45, capture_path=str(capture), options=options)
        assert (report.cell_preview_ready if cells else report.tissue_preview_ready)
        if budget:
            assert report.overlay_cache_hits > 0 and report.overlay_cache_builds > 2
        _, dimensions, _, pixels = capture.read_bytes().split(b"\n",3)
        width, height = map(int, dimensions.split())
        # Inspector displays nondeterministic worker timing; compare canvas bytes.
        captures.append(np.frombuffer(pixels,dtype=np.uint8).reshape(height,width,3)[60:,310:].copy())
    np.testing.assert_array_equal(*captures)


def test_opt_in_frame_profile_reports_bounded_phase_timings(tmp_path, monkeypatch):
    import json

    path = tmp_path/'frames.json'
    monkeypatch.setenv('QUPATHXX_GUI_PROFILE', str(path))
    report = q.show(document(), frame_limit=15)
    trace = json.loads(path.read_text())
    assert trace['schema'] == 'qpx.gui-profile/1'
    assert trace['source_tree_sha256'] == q.build_environment().source_tree_sha256
    assert len(trace['frames']) == min(report.frames, 4096)
    phases = ['model_ms', 'widgets_ms', 'tiles_ms', 'cache_ms', 'query_ms',
              'geometry_ms', 'other_ms', 'render_ms']
    for frame in trace['frames']:
        assert all(frame[key] >= 0 for key in phases)
        assert sum(frame[key] for key in phases) <= frame['total_ms'] + .01
    assert any(frame['cache_hit'] for frame in trace['frames'])


@pytest.mark.skipif(not q.has_project, reason='project workspace')
def test_dense_and_sparse_selection_paths_preserve_canvas_pixels(tmp_path):
    from test_viewer_session import completed, run

    doc = q.Document(q.Image(1024, 1024, [35, 60, 90]*1024*1024))
    doc.set_history_limits(0, 0)
    edits = [q.ObjectEdit.create_geometry(
        q.Roi.rectangle(q.Rect(6+(i%16)*7, 8+(i//16)*12, 5, 7)),
        kind=q.ObjectKind.DETECTION, class_name='Tumor') for i in range(128)]
    edits += [q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(900+(i%32)*2, 900+(i//32)*2, 1, 1)),
              kind=q.ObjectKind.DETECTION) for i in range(512)]
    doc.execute(edits)
    doc.set_class('Tumor', 0xe03080)
    project = q.Project()
    project.add_document(doc)
    view = q.ProjectView()
    view.center, view.zoom = q.Point(64, 64), 4
    project.set_view(1, view)
    source = tmp_path/'selection.qpxproj'
    project.save(str(source))
    captures = []
    visible_selection = list(range(1, 97))
    for extra in [[], list(range(129, 641))]:
        session = q.ViewerSession(str(source))
        # Keep the primary visible object identical in both presentations.
        selected = extra + visible_selection
        def producer():
            completed(session, session.catalog())
            completed(session, session.select(1, selected))
        capture = tmp_path/f'selection-{len(extra)}.ppm'
        options = q.GuiOptions()
        options.settle_tiles = True
        report = run(session, producer, frames=40, options=options, capture=str(capture))
        assert report.selected_ids == sorted(selected)
        assert report.visible_objects == 128
        _, dimensions, _, pixels = capture.read_bytes().split(b'\n', 3)
        width, height = map(int, dimensions.split())
        captures.append(np.frombuffer(pixels, dtype=np.uint8).reshape(height, width, 3)[60:,310:].copy())
    np.testing.assert_array_equal(*captures)
