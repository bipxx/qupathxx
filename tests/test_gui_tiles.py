"""Opt-in real OpenGL checks. Run with QUPATHXX_TEST_GUI=1 on a desktop."""
import os

import pytest

import qupathxx as q

pytestmark = pytest.mark.skipif(
    not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1",
    reason="requires GUI build and QUPATHXX_TEST_GUI=1",
)


def test_real_tiled_render_and_cleanup(tmp_path):
    from PIL import Image
    # Exact constant pixels test framebuffer orientation, channel order and
    # actual draw publication instead of accepting a merely nonempty window.
    doc = q.Document(q.Image(512, 512, [30, 120, 210] * (512 * 512)))
    capture = tmp_path / "tiles.ppm"
    report = q.show(doc, frame_limit=60, capture_path=str(capture))
    assert report.uploaded_tiles == 4
    assert report.pending_tiles == 0
    assert report.peak_gpu_bytes == 512 * 512 * 4
    assert report.rendered_frame_wall_samples == report.frames - 10
    assert report.rendered_frame_wall_p95_ms >= report.cpu_frame_p95_ms > 0
    assert report.max_rendered_frame_wall_ms >= report.max_cpu_frame_ms
    assert report.max_rendered_frame_wall_ms >= report.rendered_frame_wall_p95_ms

    with Image.open(capture) as frame:
        width, height = frame.size
        assert frame.getpixel((int(width * 0.65), height // 2)) == (30, 120, 210)
    doc.set_calibration(0.25, 0.5)
    doc.add_rectangle(q.Rect(10, 10, 50, 60))
    repeated = q.show(doc, frame_limit=30)
    assert repeated.uploaded_tiles == 4 and repeated.pending_tiles == 0


def test_real_aperio_view(tmp_path):
    path = os.environ.get("QUPATHXX_APERIO_FIXTURE")
    if not path or not q.has_openslide:
        pytest.skip("requires identified Aperio fixture and OpenSlide")
    doc = q.Document(q.open_slide(path))
    options = q.GuiOptions()
    options.settle_tiles = True
    report = q.show(doc, frame_limit=120, capture_path=str(tmp_path / "aperio.ppm"), options=options)
    assert report.uploaded_tiles > 0
    assert report.pending_tiles == 0
    assert report.tiles_settled
    assert report.peak_gpu_bytes <= 64 * 1024 * 1024
    assert report.cpu_frame_p95_ms > 0
    doc.server.close()


def replay(frame, kind, x=.5, y=.5, wheel=0, target=q.GuiInputTarget.CANVAS, ctrl=False, shift=False):
    event = q.GuiInput()
    event.frame, event.kind, event.x, event.y, event.wheel, event.target = frame, kind, x, y, wheel, target
    event.ctrl, event.shift = ctrl, shift
    return event


@pytest.mark.skipif(not q.has_interchange, reason="requires GeoJSON")
def test_interchange_keyboard_export_import_and_undo(tmp_path):
    source = q.Document(q.Image.demo())
    source.add_rectangle(q.Rect(10, 10, 20, 20), "Exported")
    options = q.GuiOptions()
    options.interchange_path = str(tmp_path / "annotations.geojson")
    options.inputs = [replay(4, q.GuiInputKind.EXPORT_KEY)]
    q.show(source, frame_limit=20, options=options)
    assert (tmp_path / "annotations.geojson").exists()
    target = q.Document(q.Image.demo())
    options.inputs = [replay(4, q.GuiInputKind.IMPORT_KEY)]
    report = q.show(target, frame_limit=20, options=options)
    assert target.object_count == 1 and len(report.selected_ids) == 1
    assert target.undo_label == "Import annotations"
    assert target.undo() and target.object_count == 0


def test_imgui_input_zoom_draw_pan_and_overview():
    doc = q.Document(q.Image.demo())
    options = q.GuiOptions()
    options.inputs = [
        replay(4, q.GuiInputKind.MOVE),
        replay(5, q.GuiInputKind.WHEEL, wheel=3),
        replay(8, q.GuiInputKind.RECTANGLE_KEY),
        replay(10, q.GuiInputKind.LEFT_DOWN, .45, .45),
        replay(11, q.GuiInputKind.MOVE, .55, .55),
        replay(12, q.GuiInputKind.LEFT_UP, .55, .55),
        replay(15, q.GuiInputKind.RIGHT_DOWN, .5, .5),
        replay(16, q.GuiInputKind.MOVE, .6, .6),
        replay(17, q.GuiInputKind.RIGHT_UP, .6, .6),
        replay(20, q.GuiInputKind.MOVE, .5, .25, target=q.GuiInputTarget.OVERVIEW),
        replay(21, q.GuiInputKind.LEFT_DOWN, .5, .25, target=q.GuiInputTarget.OVERVIEW),
        replay(22, q.GuiInputKind.LEFT_UP, .5, .25, target=q.GuiInputTarget.OVERVIEW),
    ]
    report = q.show(doc, frame_limit=90, options=options)
    assert len(doc.annotations) == 1
    assert doc.annotations[0].roi.area > 0
    # Dense overview already requests all source tiles. Navigating over that
    # same immutable set should reuse it rather than force generation churn.
    assert report.final_zoom > 1.3
    assert report.final_center.x == pytest.approx(512, abs=1)
    # ImGui snaps the overview pointer to whole logical pixels (5.12 source
    # pixels per overview pixel here), so a half-pixel rounding is expected.
    assert report.final_center.y == pytest.approx(192, abs=3)
    assert report.pending_tiles == 0


def test_imgui_pan_changes_origin_without_zoom():
    doc = q.Document(q.Image.demo())
    baseline = q.show(doc, frame_limit=20)
    options = q.GuiOptions()
    options.inputs = [
        replay(4, q.GuiInputKind.MOVE, .5, .5),
        replay(5, q.GuiInputKind.RIGHT_DOWN, .5, .5),
        replay(6, q.GuiInputKind.MOVE, .6, .6),
        replay(7, q.GuiInputKind.RIGHT_UP, .6, .6),
    ]
    moved = q.show(doc, frame_limit=30, options=options)
    assert moved.final_zoom == baseline.final_zoom
    assert moved.final_origin.x - baseline.final_origin.x > 80
    assert moved.final_origin.y - baseline.final_origin.y > 60


def test_display_transform_in_framebuffer(tmp_path):
    from PIL import Image
    doc = q.Document(q.Image(512, 512, [30, 120, 210] * (512 * 512)))
    options = q.GuiOptions()
    settings = q.DisplaySettings()
    settings.mode = q.DisplayMode.BLUE
    options.display = settings
    capture = tmp_path / "blue.ppm"
    report = q.show(doc, frame_limit=45, capture_path=str(capture), options=options)
    assert report.pending_tiles == 0
    with Image.open(capture) as frame:
        width, height = frame.size
        assert frame.getpixel((int(width * .65), height // 2)) == (210, 210, 210)
    assert doc.mean_intensity(doc.add_rectangle(q.Rect(0, 0, 1, 1))) == 120


def test_large_pyramid_navigation_and_payload():
    from pathlib import Path
    path = Path("build/fixtures/large-pyramid.tif")
    if not q.has_openslide or not path.is_file():
        pytest.skip("generate large fixture with tools/generate_large_fixture.py")
    server = q.open_slide(str(path))
    assert server.metadata.width == 100352 and len(server.metadata.levels) == 9
    # Independently specified pixels at both extremes and pyramid levels.
    for level in [0, 3, 8]:
        assert server.read_region(q.RegionRequest(0, 0, level, 1, 1)).data == bytes([40, 80, 160, 255])
    options = q.GuiOptions()
    options.width, options.height = 2560, 1440
    options.inputs = [replay(5, q.GuiInputKind.MOVE)]
    events = list(options.inputs)
    for frame in range(6, 36):
        events.append(replay(frame, q.GuiInputKind.WHEEL, .5, .5, wheel=1))
    events += [
        replay(40, q.GuiInputKind.MOVE, .7, .5, target=q.GuiInputTarget.OVERVIEW),
        replay(41, q.GuiInputKind.LEFT_DOWN, .7, .5, target=q.GuiInputTarget.OVERVIEW),
        replay(42, q.GuiInputKind.LEFT_UP, .7, .5, target=q.GuiInputTarget.OVERVIEW),
    ]
    options.inputs = events
    report = q.show(q.Document(server), frame_limit=150, options=options)
    assert report.uploaded_tiles > 10 and report.generations >= 10
    assert report.peak_gpu_bytes <= 64 * 1024 * 1024
    assert report.pending_tiles == 0
    assert report.framebuffer_width == 2560 and report.framebuffer_height == 1440
    server.close()


def test_keyboard_delete_undo_redo_restores_same_object():
    doc = q.Document(q.Image.demo())
    options = q.GuiOptions()
    options.inputs = [
        replay(4, q.GuiInputKind.RECTANGLE_KEY),
        replay(6, q.GuiInputKind.LEFT_DOWN, .4, .4),
        replay(7, q.GuiInputKind.MOVE, .6, .6),
        replay(8, q.GuiInputKind.LEFT_UP, .6, .6),
        replay(12, q.GuiInputKind.UNDO_KEY),
        replay(15, q.GuiInputKind.REDO_KEY),
        replay(18, q.GuiInputKind.LEFT_DOWN, .5, .5),
        replay(19, q.GuiInputKind.LEFT_UP, .5, .5),
    ]
    # Rectangle tool remains active, so select with R switched off first.
    events = list(options.inputs)
    events.insert(6, replay(16, q.GuiInputKind.RECTANGLE_KEY))
    events += [replay(22, q.GuiInputKind.DELETE_KEY), replay(25, q.GuiInputKind.UNDO_KEY)]
    options.inputs = events
    report = q.show(doc, frame_limit=60, options=options)
    assert doc.object_count == 1
    assert doc.annotation(1).name == "Region 1"
    assert doc.annotation(1).roi.width > 0
    assert doc.redo_label == "Delete subtree"
    assert doc.redo() and doc.object_count == 0
    assert doc.undo() and doc.annotation(1).id == 1
    assert report.pending_tiles == 0


@pytest.mark.parametrize("key,kind", [
    (q.GuiInputKind.ELLIPSE_KEY, q.RoiKind.ELLIPSE),
    (q.GuiInputKind.POLYGON_KEY, q.RoiKind.POLYGON),
    (q.GuiInputKind.POLYLINE_KEY, q.RoiKind.POLYLINE),
    (q.GuiInputKind.POINTS_KEY, q.RoiKind.POINTS),
])
def test_general_shape_drawing_and_history(key, kind):
    if kind == q.RoiKind.POLYGON and not q.has_geos:
        pytest.skip("requires GEOS")
    doc = q.Document(q.Image.demo())
    options = q.GuiOptions()
    if kind == q.RoiKind.ELLIPSE:
        inputs = [replay(4, key), replay(6, q.GuiInputKind.LEFT_DOWN, .4, .4),
                  replay(7, q.GuiInputKind.MOVE, .6, .6), replay(8, q.GuiInputKind.LEFT_UP, .6, .6)]
    else:
        inputs = [
            replay(4, key), replay(6, q.GuiInputKind.LEFT_DOWN, .4, .4), replay(7, q.GuiInputKind.LEFT_UP, .4, .4),
            replay(10, q.GuiInputKind.LEFT_DOWN, .6, .4), replay(11, q.GuiInputKind.LEFT_UP, .6, .4),
            replay(14, q.GuiInputKind.LEFT_DOWN, .5, .6), replay(15, q.GuiInputKind.LEFT_UP, .5, .6),
            replay(19, q.GuiInputKind.FINISH_KEY),
        ]
    options.inputs = inputs + [replay(25, q.GuiInputKind.UNDO_KEY), replay(28, q.GuiInputKind.REDO_KEY)]
    report = q.show(doc, frame_limit=60, options=options)
    assert doc.object_count == 1
    assert doc.annotation(1).geometry.kind == kind
    assert doc.undo() and not doc.object_count
    assert doc.redo() and doc.annotation(1).geometry.kind == kind
    assert report.visible_objects == 1


def test_cell_nucleus_renders_when_primary_boundary_is_offscreen(tmp_path):
    from PIL import Image
    import numpy as np

    doc = q.Document(q.Image.demo())
    cell = doc.add_cell(q.Roi.rectangle(q.Rect(900, 700, 10, 10)),
                        q.Roi.rectangle(q.Rect(100, 100, 50, 50)))
    view = q.Viewport()
    view.zoom = 2
    options = q.GuiOptions()
    options.start_view = view
    capture = tmp_path / "nucleus.ppm"
    report = q.show(doc, frame_limit=30, capture_path=str(capture), options=options)
    assert report.visible_objects == 1
    with Image.open(capture) as frame:
        pixels = np.asarray(frame)[:, 310:, :]
        # This color is reserved for nucleus outlines. Verify actual draw output,
        # with the cell body beyond the viewport's right and bottom edges.
        assert np.count_nonzero(np.all(pixels == (195, 145, 255), axis=2)) > 200
    assert doc.annotation(cell).nucleus.area == 2500


def test_registry_color_changes_and_undo_reach_framebuffer(tmp_path):
    from PIL import Image
    import numpy as np

    doc = q.Document(q.Image.demo())
    doc.add_detection(q.Roi.rectangle(q.Rect(100, 100, 50, 50)), class_name="Tumor")
    doc.set_class("Tumor", 0xe02030)
    view = q.Viewport()
    view.zoom = 2
    options = q.GuiOptions()
    options.start_view = view
    for step, rgb in enumerate(((224, 32, 48), (32, 64, 224), (224, 32, 48))):
        if step == 1:
            doc.set_class("Tumor", 0x2040e0)
        elif step == 2:
            assert doc.undo()
        capture = tmp_path / f"class-{step}.ppm"
        report = q.show(doc, frame_limit=20, capture_path=str(capture), options=options)
        assert report.visible_objects == 1
        with Image.open(capture) as frame:
            pixels = np.asarray(frame)[:, 310:, :]
            assert np.count_nonzero(np.all(pixels == rgb, axis=2)) > 200


@pytest.mark.parametrize("shift,expected", [(False, [1, 3]), (True, [1, 2, 3])])
def test_list_ctrl_toggle_and_shift_range_select(shift, expected, tmp_path):
    from PIL import Image
    import numpy as np

    doc = q.Document(q.Image.demo())
    doc.execute([q.ObjectEdit.create(q.Rect(100+i*100, 100, 30, 30)) for i in range(3)])
    options = q.GuiOptions()
    view = q.Viewport()
    view.zoom = 1
    options.start_view = view
    target = q.GuiInputTarget.OBJECT_LIST
    options.inputs = [
        replay(4, q.GuiInputKind.LEFT_DOWN, .3, .05, target=target),
        replay(5, q.GuiInputKind.LEFT_UP, .3, .05, target=target),
        replay(9, q.GuiInputKind.LEFT_DOWN, .3, .52, target=target, ctrl=not shift, shift=shift),
        replay(10, q.GuiInputKind.LEFT_UP, .3, .52, target=target, ctrl=not shift, shift=shift),
    ]
    capture = tmp_path / "selection.ppm"
    report = q.show(doc, frame_limit=30, options=options, capture_path=str(capture))
    assert report.selected_ids == expected and report.primary_selection == 3
    with Image.open(capture) as frame:
        pixels = np.asarray(frame)[:, 310:, :]
        assert np.count_nonzero(np.all(pixels == (255, 205, 85), axis=2)) > 200


@pytest.mark.parametrize("delete_undo", [False, True])
def test_canvas_window_selection_and_atomic_delete_undo(delete_undo):
    doc = q.Document(q.Image.demo())
    ids = doc.execute([q.ObjectEdit.create(q.Rect(x, y, 20, 20)) for x, y in [(100, 100), (200, 100), (700, 600)]])
    options = q.GuiOptions()
    view = q.Viewport()
    options.start_view = view
    options.inputs = [replay(4, q.GuiInputKind.LEFT_DOWN, .05, .05, shift=True),
                      replay(5, q.GuiInputKind.MOVE, .3, .3),
                      replay(6, q.GuiInputKind.LEFT_UP, .3, .3)]
    if delete_undo:
        options.inputs = list(options.inputs) + [replay(12, q.GuiInputKind.DELETE_KEY), replay(16, q.GuiInputKind.UNDO_KEY)]
    report = q.show(doc, frame_limit=30, options=options)
    assert doc.object_count == 3
    assert report.selected_ids == ([] if delete_undo else ids[:2])
    assert report.primary_selection == (0 if delete_undo else ids[1])
    if delete_undo:
        assert doc.redo_label == "Delete objects"
        assert doc.redo() and [a.id for a in doc.annotations] == ids[2:]


@pytest.mark.parametrize("clear", [False, True])
def test_select_all_then_escape_clears_presentation_only(clear):
    doc = q.Document(q.Image.demo())
    doc.execute([q.ObjectEdit.create(q.Rect(i, 0, 1, 1)) for i in range(10)])
    revision, history = doc.revision, doc.history_bytes
    options = q.GuiOptions()
    options.inputs = [replay(4, q.GuiInputKind.SELECT_ALL_KEY)]
    if clear:
        options.inputs = list(options.inputs) + [replay(8, q.GuiInputKind.ESCAPE_KEY)]
    report = q.show(doc, frame_limit=20, options=options)
    assert report.selected_ids == ([] if clear else list(range(1, 11)))
    assert report.primary_selection == (0 if clear else 10)
    assert (doc.revision, doc.history_bytes) == (revision, history)


def test_canvas_ctrl_click_toggles_membership():
    doc = q.Document(q.Image.demo())
    first, second = doc.execute([q.ObjectEdit.create(q.Rect(100, 100, 400, 300)),
                                q.ObjectEdit.create(q.Rect(600, 100, 300, 300))])
    options = q.GuiOptions()
    options.start_view = q.Viewport()
    options.inputs = [
        replay(4, q.GuiInputKind.LEFT_DOWN, .15, .2), replay(5, q.GuiInputKind.LEFT_UP, .15, .2),
        replay(9, q.GuiInputKind.LEFT_DOWN, .6, .2, ctrl=True), replay(10, q.GuiInputKind.LEFT_UP, .6, .2, ctrl=True),
        replay(14, q.GuiInputKind.LEFT_DOWN, .15, .2, ctrl=True), replay(15, q.GuiInputKind.LEFT_UP, .15, .2, ctrl=True),
    ]
    report = q.show(doc, frame_limit=25, options=options)
    assert report.selected_ids == [second] and report.primary_selection == second
    assert doc.object_count == 2


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS")
@pytest.mark.parametrize("cancel", [False, True])
def test_vertex_drag_commits_once_or_cancels(cancel):
    doc = q.Document(q.Image.demo())
    obj = doc.add_roi(q.Roi.polygon([q.Point(100, 100), q.Point(300, 100), q.Point(300, 300), q.Point(100, 300)]))
    options = q.GuiOptions()
    options.start_view = q.Viewport()
    image = q.GuiInputTarget.IMAGE
    options.inputs = [
        replay(4, q.GuiInputKind.LEFT_DOWN, 200, 200, target=image),
        replay(5, q.GuiInputKind.LEFT_UP, 200, 200, target=image),
        replay(8, q.GuiInputKind.VERTEX_KEY),
        replay(10, q.GuiInputKind.LEFT_DOWN, 100, 100, target=image),
        replay(11, q.GuiInputKind.MOVE, 80, 80, target=image),
    ]
    if cancel:
        options.inputs = list(options.inputs) + [replay(12, q.GuiInputKind.ESCAPE_KEY),
                        replay(14, q.GuiInputKind.LEFT_UP, 80, 80, target=image)]
    else:
        options.inputs = list(options.inputs) + [replay(12, q.GuiInputKind.LEFT_UP, 80, 80, target=image),
                        replay(16, q.GuiInputKind.UNDO_KEY), replay(20, q.GuiInputKind.REDO_KEY)]
    report = q.show(doc, frame_limit=30, options=options)
    assert report.selected_ids == [obj]
    assert doc.annotation(obj).geometry.area == (40000 if cancel else 44000)
    if cancel:
        assert doc.revision == 1
    else:
        assert doc.undo_label == "Move vertex"
        assert doc.undo() and doc.annotation(obj).geometry.area == 40000


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS")
def test_hole_drawing_and_boolean_gui_commands():
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(100, 100, 300, 300))
    options = q.GuiOptions()
    options.start_view = q.Viewport()
    image = q.GuiInputTarget.IMAGE
    options.inputs = [replay(4, q.GuiInputKind.LEFT_DOWN, 150, 150, target=image),
                      replay(5, q.GuiInputKind.LEFT_UP, 150, 150, target=image), replay(8, q.GuiInputKind.HOLE_KEY)]
    events = list(options.inputs)
    for frame, x, y in [(10, 200, 200), (14, 300, 200), (18, 250, 300)]:
        events += [replay(frame, q.GuiInputKind.LEFT_DOWN, x, y, target=image),
                   replay(frame+1, q.GuiInputKind.LEFT_UP, x, y, target=image)]
    options.inputs = events + [replay(22, q.GuiInputKind.FINISH_KEY), replay(26, q.GuiInputKind.UNDO_KEY), replay(30, q.GuiInputKind.REDO_KEY)]
    q.show(doc, frame_limit=40, options=options)
    assert doc.object_count == 1 and doc.annotation(obj).geometry.area == 85000
    assert len(doc.annotation(obj).geometry.polygon_parts[0].holes) == 1
    assert doc.undo() and doc.annotation(obj).geometry.kind == q.RoiKind.RECTANGLE

    other = doc.add_rectangle(q.Rect(300, 100, 300, 300))
    options.inputs = [replay(4, q.GuiInputKind.SELECT_ALL_KEY), replay(8, q.GuiInputKind.BOOLEAN_KEY)]
    report = q.show(doc, frame_limit=20, options=options)
    assert report.primary_selection == other and doc.object_count == 2
    assert doc.annotation(other).geometry.area == 150000
    assert doc.annotation(obj).geometry.area == 90000
    assert doc.undo_label == "Boolean ROI edit"
    assert doc.undo() and doc.annotation(other).geometry.area == 90000


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS")
def test_vertex_insert_and_delete_shortcuts_keep_object_identity():
    doc = q.Document(q.Image.demo())
    obj = doc.add_roi(q.Roi.polygon([q.Point(100, 100), q.Point(300, 100), q.Point(300, 300), q.Point(100, 300)]))
    options = q.GuiOptions()
    options.start_view = q.Viewport()
    image = q.GuiInputTarget.IMAGE
    select = [replay(4, q.GuiInputKind.LEFT_DOWN, 200, 200, target=image),
              replay(5, q.GuiInputKind.LEFT_UP, 200, 200, target=image), replay(8, q.GuiInputKind.VERTEX_KEY)]
    options.inputs = select + [replay(11, q.GuiInputKind.INSERT_KEY, 200, 100, target=image)]
    q.show(doc, frame_limit=20, options=options)
    assert len(doc.annotation(obj).geometry.paths[0]) == 5 and doc.undo_label == "Insert vertex"
    options.inputs = select + [replay(11, q.GuiInputKind.LEFT_DOWN, 200, 100, target=image),
                              replay(12, q.GuiInputKind.LEFT_UP, 200, 100, target=image),
                              replay(16, q.GuiInputKind.DELETE_KEY)]
    q.show(doc, frame_limit=25, options=options)
    assert doc.object_count == 1 and len(doc.annotation(obj).geometry.paths[0]) == 4
    assert doc.undo_label == "Remove vertex" and doc.undo()
    assert len(doc.annotation(obj).geometry.paths[0]) == 5


@pytest.mark.skipif(not q.has_geos, reason="requires GEOS")
def test_hole_draft_rejects_changed_primary_selection():
    doc = q.Document(q.Image.demo())
    doc.add_rectangle(q.Rect(100, 100, 300, 300))
    doc.add_rectangle(q.Rect(600, 100, 100, 100))
    revision = doc.revision
    options = q.GuiOptions()
    options.start_view = q.Viewport()
    image = q.GuiInputTarget.IMAGE
    events = [replay(4, q.GuiInputKind.LEFT_DOWN, 150, 150, target=image),
              replay(5, q.GuiInputKind.LEFT_UP, 150, 150, target=image), replay(8, q.GuiInputKind.HOLE_KEY)]
    for frame, x, y in [(10, 200, 200), (14, 300, 200), (18, 250, 300)]:
        events += [replay(frame, q.GuiInputKind.LEFT_DOWN, x, y, target=image),
                   replay(frame+1, q.GuiInputKind.LEFT_UP, x, y, target=image)]
    options.inputs = events + [replay(22, q.GuiInputKind.LEFT_DOWN, .3, .28, target=q.GuiInputTarget.OBJECT_LIST),
                              replay(23, q.GuiInputKind.LEFT_UP, .3, .28, target=q.GuiInputTarget.OBJECT_LIST),
                              replay(26, q.GuiInputKind.FINISH_KEY)]
    q.show(doc, frame_limit=35, options=options)
    assert doc.revision == revision
    assert [a.geometry.kind for a in doc.annotations] == [q.RoiKind.RECTANGLE]*2


@pytest.mark.skipif(not q.has_project, reason="requires project storage")
def test_gui_project_save_shortcut_publishes_current_document(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), "Saved")
    path = tmp_path / "viewer.qpxproj"
    project.save(str(path))
    doc.rename_annotation(obj, "From GUI")
    options = q.GuiOptions()
    options.project = project
    options.inputs = [replay(4, q.GuiInputKind.SAVE_KEY)]
    report = q.show(doc, frame_limit=20, options=options)
    assert report.uploaded_tiles > 0 and not project.dirty and project.generation == 2
    assert q.Project.open(str(path)).document(image).annotation(obj).name == "From GUI"
