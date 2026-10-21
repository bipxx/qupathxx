"""Real ImGui project navigation and native document isolation."""
import os

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(
    not q.has_gui or not q.has_project or os.environ.get("QUPATHXX_TEST_GUI") != "1",
    reason="requires GUI/project build and QUPATHXX_TEST_GUI=1",
)


def event(frame, kind, x=.5, y=.5, target=q.GuiInputTarget.CANVAS):
    result = q.GuiInput()
    result.frame, result.kind, result.x, result.y, result.target = frame, kind, x, y, target
    return result


def workspace():
    project = q.Project()
    first = q.Document(q.Image(512, 512, [30, 120, 210]*512*512))
    second = q.Document(q.Image(256, 128, [210, 60, 30]*256*128))
    a = project.add_document(first, "First")
    b = project.add_document(second, "Second")
    options = q.GuiOptions()
    options.project = project
    return project, first, second, a, b, options


def test_switch_restores_view_selection_and_keeps_history_isolated(tmp_path):
    from PIL import Image
    project, first, second, a, b, options = workspace()
    first.add_rectangle(q.Rect(10, 10, 20, 20), "First object")
    second.add_rectangle(q.Rect(10, 10, 20, 20), "Second object")
    view = q.Viewport()
    view.zoom, view.origin = 1.25, q.Point(-20, -30)
    options.start_view = view
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(7, q.GuiInputKind.NEXT_IMAGE_KEY),
                      event(11, q.GuiInputKind.UNDO_KEY), event(14, q.GuiInputKind.PREVIOUS_IMAGE_KEY)]
    capture = tmp_path / "restored.ppm"
    report = q.show(first, frame_limit=35, capture_path=str(capture), options=options)
    assert report.active_image == project.active_image == a and report.image_switches == 2
    assert report.selected_ids == [1] and report.primary_selection == 1
    assert report.final_zoom == view.zoom
    assert (report.final_origin.x, report.final_origin.y) == (view.origin.x, view.origin.y)
    assert first.object_count == 1 and second.object_count == 0 and second.can_redo
    assert report.peak_gpu_bytes <= 64*1024*1024 and not report.project_error
    with Image.open(capture) as frame:
        assert frame.getpixel((frame.width//2, frame.height//2)) == (30, 120, 210)


def test_project_list_switch_renders_other_image_pixels(tmp_path):
    from PIL import Image
    project, first, second, a, b, options = workspace()
    options.inputs = [event(4, q.GuiInputKind.LEFT_DOWN, .4, .3, q.GuiInputTarget.PROJECT_LIST),
                      event(5, q.GuiInputKind.LEFT_UP, .4, .3, q.GuiInputTarget.PROJECT_LIST)]
    capture = tmp_path / "second.ppm"
    report = q.show(first, frame_limit=35, capture_path=str(capture), options=options)
    assert report.active_image == project.active_image == b and report.image_switches == 1
    assert report.pending_tiles == 0 and not report.project_error
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.65), frame.height//2)) == (210, 60, 30)


def test_switch_cancels_unfinished_rectangle():
    project, first, second, a, b, options = workspace()
    options.inputs = [event(3, q.GuiInputKind.RECTANGLE_KEY), event(4, q.GuiInputKind.LEFT_DOWN, .45, .45),
                      event(5, q.GuiInputKind.MOVE, .55, .55), event(6, q.GuiInputKind.NEXT_IMAGE_KEY),
                      event(9, q.GuiInputKind.LEFT_UP, .55, .55)]
    report = q.show(first, frame_limit=25, options=options)
    assert report.active_image == b and not report.selected_ids
    assert first.object_count == second.object_count == 0


@pytest.mark.parametrize("finish_on_second", [False, True])
def test_each_image_retains_independent_display_settings(tmp_path, finish_on_second):
    from PIL import Image
    project, first, second, a, b, options = workspace()
    display = q.DisplaySettings()
    display.mode = q.DisplayMode.RED
    options.display = display
    inputs = [event(4, q.GuiInputKind.NEXT_IMAGE_KEY),
                      event(10, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.DISPLAY_RESET),
                      event(11, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.DISPLAY_RESET),
                      event(17, q.GuiInputKind.PREVIOUS_IMAGE_KEY)]
    if finish_on_second:
        inputs.append(event(23, q.GuiInputKind.NEXT_IMAGE_KEY))
    options.inputs = inputs
    capture = tmp_path / "display.ppm"
    report = q.show(first, frame_limit=35, capture_path=str(capture), options=options)
    assert (report.active_image, report.image_switches, report.project_error) == ((b if finish_on_second else a), (3 if finish_on_second else 2), "")
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.65), frame.height//2)) == ((210, 60, 30) if finish_on_second else (30, 30, 30))


def test_add_image_then_save_and_restart(tmp_path):
    project, first, second, a, b, options = workspace()
    path = tmp_path / "\u6807\u6ce8.ppm"
    path.write_bytes(b"P6\n16 8\n255\n" + bytes([40, 80, 160])*16*8)
    destination = tmp_path / "workspace.qpxproj"
    project.save(str(destination))
    options.project_image_path = str(path)
    options.inputs = [event(4, q.GuiInputKind.ADD_IMAGE_KEY), event(12, q.GuiInputKind.SAVE_KEY)]
    report = q.show(first, frame_limit=30, options=options)
    assert len(project.images) == 3 and report.active_image == project.images[-1].id
    assert not project.dirty and not report.project_error
    loaded = q.Project.open(str(destination))
    assert loaded.active_image == report.active_image and len(loaded.images) == 3
    assert loaded.document(report.active_image).metadata.width == 16


def test_relink_missing_image_rebinds_tile_reader(tmp_path):
    from PIL import Image
    original, replacement = tmp_path / "old.ppm", tmp_path / "new.ppm"
    pixels = b"P6\n64 64\n255\n" + bytes([40, 80, 160])*64*64
    original.write_bytes(pixels)
    project = q.Project()
    image = project.add_image(str(original))
    destination = tmp_path / "missing.qpxproj"
    project.save(str(destination))
    original.rename(replacement)
    project = q.Project.open(str(destination))
    assert not project.images[0].available
    doc = project.document(image)
    options = q.GuiOptions()
    options.project, options.project_image_path = project, str(replacement)
    options.inputs = [event(4, q.GuiInputKind.RELINK_IMAGE_KEY)]
    capture = tmp_path / "relinked.ppm"
    report = q.show(doc, frame_limit=35, capture_path=str(capture), options=options)
    assert not report.project_error and project.images[0].available
    assert report.uploaded_tiles > 0 and report.pending_tiles == 0
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.65), frame.height//2)) == (40, 80, 160)


def test_failed_add_keeps_current_document_and_reports_error(tmp_path):
    project, first, second, a, b, options = workspace()
    options.project_image_path = str(tmp_path / "absent.ppm")
    options.inputs = [event(4, q.GuiInputKind.ADD_IMAGE_KEY)]
    report = q.show(first, frame_limit=20, options=options)
    assert report.project_error and report.active_image == a and report.image_switches == 0
    assert len(project.images) == 2


def test_failed_relink_keeps_source_and_view(tmp_path):
    original, incompatible = tmp_path / "source.ppm", tmp_path / "wrong.ppm"
    original.write_bytes(b"P6\n64 64\n255\n" + bytes([30, 120, 210])*64*64)
    incompatible.write_bytes(b"P6\n32 32\n255\n" + bytes([210, 60, 30])*32*32)
    project = q.Project()
    image = project.add_image(str(original))
    options = q.GuiOptions()
    options.project, options.project_image_path = project, str(incompatible)
    view = q.Viewport()
    view.zoom, view.origin = 4, q.Point(10, 20)
    options.start_view = view
    options.inputs = [event(4, q.GuiInputKind.RELINK_IMAGE_KEY)]
    report = q.show(project.document(image), frame_limit=20, options=options)
    assert report.project_error and report.active_image == image and report.image_switches == 0
    assert project.images[0].source_path == str(original)
    assert report.final_zoom == 4 and (report.final_origin.x, report.final_origin.y) == (10, 20)
