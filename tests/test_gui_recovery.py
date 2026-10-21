"""Real ImGui autosave, workspace recovery and dirty-close decisions."""
import os
import sqlite3

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(
    not q.has_gui or not q.has_project or os.environ.get("QUPATHXX_TEST_GUI") != "1",
    reason="requires GUI/project build and QUPATHXX_TEST_GUI=1",
)


def event(frame, kind, target=q.GuiInputTarget.CANVAS):
    result = q.GuiInput()
    result.frame, result.kind, result.target = frame, kind, target
    return result


def click(frame, target):
    return [event(frame, q.GuiInputKind.LEFT_DOWN, target), event(frame+1, q.GuiInputKind.LEFT_UP, target)]


def workspace(tmp_path, name="primary", color=(30, 60, 90)):
    project = q.Project()
    doc = q.Document(q.Image(512, 512, list(color)*512*512))
    image = project.add_document(doc, name)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), "Original")
    primary = tmp_path / (name+".qpxproj")
    project.save(str(primary))
    doc.rename_annotation(obj, "Captured")
    options = q.GuiOptions()
    options.project, options.autosave_interval_seconds = project, 0
    options.autosave_path = str(tmp_path / (name+".qpxrecover"))
    return project, doc, image, obj, primary, options


def test_gui_manual_recovery_preserves_primary_and_snapshot_isolation(tmp_path):
    project, doc, image, obj, primary, options = workspace(tmp_path)
    before = primary.read_bytes()
    options.inputs = [event(4, q.GuiInputKind.AUTOSAVE_KEY), event(16, q.GuiInputKind.UNDO_KEY)]
    report = q.show(doc, frame_limit=50, options=options)
    assert report.autosaves_completed == 1 and not report.autosave_error
    assert project.dirty and primary.read_bytes() == before
    assert q.Project.recover(report.recovery_path).document(image).annotation(obj).name == "Captured"
    assert doc.annotation(obj).name == "Original"


def test_periodic_recovery_skips_unchanged_workspace(tmp_path):
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.autosave_interval_seconds = .1
    report = q.show(doc, frame_limit=45, options=options)
    assert report.autosaves_completed == 1 and not report.autosave_error and project.dirty
    assert q.Project.recover(report.recovery_path).document(image).annotation(obj).name == "Captured"


def test_explicit_save_clears_current_session_recovery(tmp_path):
    from pathlib import Path
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.inputs = [event(4, q.GuiInputKind.AUTOSAVE_KEY), event(20, q.GuiInputKind.SAVE_KEY)]
    report = q.show(doc, frame_limit=45, options=options)
    assert report.autosaves_completed == 1 and not report.autosave_error
    assert not Path(report.recovery_path).exists() and not project.dirty
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Captured"


def test_recover_replaces_workspace_after_discard_and_renders_new_pixels(tmp_path):
    from pathlib import Path
    from PIL import Image
    source, source_doc, image, obj, source_primary, source_options = workspace(tmp_path, "source", (40, 80, 160))
    autosave = q.ProjectAutosave()
    autosave.request(source, source_options.autosave_path)
    assert autosave.wait().state == "ready"
    project, doc, _, _, primary, options = workspace(tmp_path, "current", (210, 60, 30))
    current_id = project.identity
    options.recovery_path = source_options.autosave_path
    options.inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY), event(4, q.GuiInputKind.RECOVER_KEY),
                      *click(12, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    capture = tmp_path / "recovered.ppm"
    report = q.show(doc, frame_limit=40, capture_path=str(capture), options=options)
    assert not report.project_error and project.identity == source.identity != current_id
    assert project.document(image).annotation(obj).name == "Captured" and project.dirty
    assert doc.annotation(obj).name == "Captured" and not report.selected_ids
    assert Path(options.recovery_path).exists() and project.recovery_source == options.recovery_path
    assert q.Project.open(str(primary)).identity == current_id
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.65), frame.height//2)) == (40, 80, 160)


def test_failed_save_before_recovery_keeps_current_workspace_and_dialog(tmp_path):
    source, source_doc, image, obj, source_primary, source_options = workspace(tmp_path, "source")
    autosave = q.ProjectAutosave()
    autosave.request(source, source_options.autosave_path)
    assert autosave.wait().state == "ready"
    project, doc, image, obj, primary, options = workspace(tmp_path, "current")
    identity = project.identity
    with sqlite3.connect(primary) as db:
        db.execute("CREATE TRIGGER fail_save BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected close failure'); END")
    options.recovery_path = source_options.autosave_path
    options.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), *click(12, q.GuiInputTarget.SAVE_RECOVERY_REPLACEMENT),
                      *click(22, q.GuiInputTarget.CANCEL_RECOVERY_REPLACEMENT)]
    report = q.show(doc, frame_limit=40, options=options)
    assert "injected" in report.project_error and project.identity == identity and project.dirty
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"


def test_keep_recovery_and_close_waits_for_publication(tmp_path):
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.inputs = [event(5, q.GuiInputKind.CLOSE_WINDOW), *click(12, q.GuiInputTarget.KEEP_RECOVERY_CLOSE)]
    report = q.show(doc, frame_limit=80, options=options)
    assert report.frames < 80 and report.autosaves_completed == 1 and not report.autosave_error
    assert project.dirty and project.generation == 1
    assert q.Project.recover(report.recovery_path).document(image).annotation(obj).name == "Captured"
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"


def test_cancel_close_keeps_edits_and_blocks_underlying_undo(tmp_path):
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.inputs = [event(5, q.GuiInputKind.CLOSE_WINDOW), event(10, q.GuiInputKind.UNDO_KEY),
                      *click(14, q.GuiInputTarget.CANCEL_CLOSE)]
    report = q.show(doc, frame_limit=35, options=options)
    assert report.frames == 35 and report.autosaves_completed == 0
    assert doc.annotation(obj).name == "Captured" and project.dirty


def test_discard_close_removes_owned_recovery_without_saving_primary(tmp_path):
    from pathlib import Path
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.inputs = [event(3, q.GuiInputKind.AUTOSAVE_KEY), event(10, q.GuiInputKind.CLOSE_WINDOW),
                      *click(16, q.GuiInputTarget.DISCARD_CLOSE)]
    report = q.show(doc, frame_limit=80, options=options)
    assert report.frames < 80 and report.autosaves_completed == 1
    assert not Path(report.recovery_path).exists()
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"


def test_save_and_close_saves_latest_state(tmp_path):
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.inputs = [event(5, q.GuiInputKind.CLOSE_WINDOW), *click(12, q.GuiInputTarget.SAVE_CLOSE)]
    report = q.show(doc, frame_limit=80, options=options)
    assert report.frames < 80 and not report.project_error and not project.dirty
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Captured"


def test_failed_close_save_keeps_workspace_available_for_cancel(tmp_path):
    project, doc, image, obj, primary, options = workspace(tmp_path)
    with sqlite3.connect(primary) as db:
        db.execute("CREATE TRIGGER fail_save BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected save failure'); END")
    options.inputs = [event(5, q.GuiInputKind.CLOSE_WINDOW), *click(12, q.GuiInputTarget.SAVE_CLOSE),
                      *click(22, q.GuiInputTarget.CANCEL_CLOSE)]
    report = q.show(doc, frame_limit=40, options=options)
    assert report.frames == 40 and project.dirty and "injected" in report.project_error
    assert doc.annotation(obj).name == "Captured"
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"


def test_recovery_budget_error_does_not_block_editing_or_change_primary(tmp_path):
    from pathlib import Path
    project, doc, image, obj, primary, options = workspace(tmp_path)
    options.autosave_snapshot_budget = 1
    options.inputs = [event(4, q.GuiInputKind.AUTOSAVE_KEY), event(10, q.GuiInputKind.UNDO_KEY)]
    report = q.show(doc, frame_limit=30, options=options)
    assert "budget" in report.autosave_error and not report.project_error
    assert not Path(options.autosave_path).exists() and project.dirty
    assert doc.annotation(obj).name == "Original"
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"


def test_saving_recovered_work_retains_other_session_checkpoint(tmp_path):
    from pathlib import Path
    source, source_doc, image, obj, source_primary, source_options = workspace(tmp_path, "source")
    autosave = q.ProjectAutosave()
    autosave.request(source, source_options.autosave_path)
    assert autosave.wait().state == "ready"
    checkpoint = Path(source_options.autosave_path)
    before = checkpoint.read_bytes()
    project, doc, _, _, primary, options = workspace(tmp_path, "current")
    options.recovery_path = str(checkpoint)
    options.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), *click(12, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                      event(24, q.GuiInputKind.SAVE_KEY)]
    report = q.show(doc, frame_limit=45, options=options)
    assert not report.project_error and not report.autosave_error and not project.dirty
    assert checkpoint.read_bytes() == before
    assert q.Project.open(str(source_primary)).document(image).annotation(obj).name == "Captured"
