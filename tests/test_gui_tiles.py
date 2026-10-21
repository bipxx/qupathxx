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
    report = q.show(doc, frame_limit=120, capture_path=str(tmp_path / "aperio.ppm"))
    assert report.uploaded_tiles > 0
    assert report.pending_tiles == 0
    assert report.peak_gpu_bytes <= 64 * 1024 * 1024
    assert report.cpu_frame_p95_ms > 0
    doc.server.close()


def replay(frame, kind, x=.5, y=.5, wheel=0, target=q.GuiInputTarget.CANVAS):
    event = q.GuiInput()
    event.frame, event.kind, event.x, event.y, event.wheel, event.target = frame, kind, x, y, wheel, target
    return event


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
