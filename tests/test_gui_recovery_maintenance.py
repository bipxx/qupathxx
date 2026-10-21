"""Real ImGui asynchronous recovery loading and reviewed checkpoint maintenance."""
from contextlib import contextmanager
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(
    not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='requires GUI/project build and QUPATHXX_TEST_GUI=1',
)


def event(frame, kind, target=q.GuiInputTarget.CANVAS):
    result = q.GuiInput()
    result.frame, result.kind, result.target = frame, kind, target
    return result


def click(frame, target):
    down = event(frame, q.GuiInputKind.LEFT_DOWN, target)
    down.wait_for = q.GuiInputWait.TARGET_READY
    return [down, event(frame+1, q.GuiInputKind.LEFT_UP, target)]


def workspace(tmp_path, name):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc, name)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original')
    primary = tmp_path/(name+'.qpxproj')
    project.save(str(primary))
    doc.rename_annotation(obj, 'Captured')
    return project, doc, image, obj, primary


def setup(tmp_path):
    source, source_doc, image, obj, source_primary = workspace(tmp_path, 'source')
    writer = q.ProjectAutosave()
    path = tmp_path/'source.qpxrecover'
    assert writer.request(source, str(path)) and writer.wait().state == 'ready'
    project, doc, current_image, current_obj, primary = workspace(tmp_path, 'current')
    config = q.GuiOptions()
    config.autosave_interval_seconds, config.recovery_path = 0, str(path)
    return project, doc, current_obj, primary, config, source, writer, path


@contextmanager
def locked(path, tmp_path):
    ready = tmp_path/'lock-ready'
    script = "import sqlite3,sys,time; from pathlib import Path; db=sqlite3.connect(sys.argv[1]); db.execute('BEGIN EXCLUSIVE'); Path(sys.argv[2]).write_text('ready'); time.sleep(0.9); db.rollback(); db.close()"
    child = subprocess.Popen([sys.executable, '-c', script, str(path), str(ready)])
    try:
        deadline = time.monotonic()+5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        assert ready.exists()
        yield
    finally:
        child.wait(timeout=5)


def test_delete_review_confirmation_removes_selected_file_without_changing_workspace(tmp_path):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    identity, before = project.identity, primary.read_bytes()
    config.inputs = [event(4, q.GuiInputKind.REVIEW_RECOVERY_KEY), *click(18, q.GuiInputTarget.CONFIRM_DISCARD_CHECKPOINT)]
    report = q.show(project, frame_limit=38, options=config)
    assert not report.recovery_maintenance_error and report.checkpoints_deleted == 1 and not path.exists()
    assert project.identity == identity and project.dirty and doc.annotation(obj).name == 'Captured'
    assert primary.read_bytes() == before and report.project_replacements == 0
    assert writer.request(source, str(path)) and writer.wait().state == 'ready' and path.exists()


def test_cancel_review_preserves_file_and_blocks_underlying_undo(tmp_path):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    before = path.read_bytes()
    # Hold Cancel after its dialog is ready; exercise Undo before releasing it.
    down, up = click(12, q.GuiInputTarget.CANCEL_DISCARD_CHECKPOINT)
    up.frame = 18
    config.inputs = [event(4, q.GuiInputKind.REVIEW_RECOVERY_KEY), down,
                     event(13, q.GuiInputKind.UNDO_KEY), up]
    report = q.show(project, frame_limit=32, options=config)
    assert not report.recovery_maintenance_error and report.checkpoints_deleted == 0 and path.read_bytes() == before
    assert doc.annotation(obj).name == 'Captured'
    config.inputs = [event(4, q.GuiInputKind.UNDO_KEY)]
    q.show(project, frame_limit=12, options=config)
    assert doc.annotation(obj).name == 'Original'


@pytest.mark.parametrize('invalid', ['primary', 'missing', 'corrupt'])
def test_invalid_checkpoint_review_reports_error_without_blocking_edits(tmp_path, invalid):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    target = primary if invalid == 'primary' else tmp_path/(invalid+'.qpxrecover')
    if invalid == 'corrupt':
        target.write_bytes(b'not a database')
    config.recovery_path = str(target)
    before = target.read_bytes() if target.exists() else None
    config.inputs = [event(4, q.GuiInputKind.REVIEW_RECOVERY_KEY), event(16, q.GuiInputKind.UNDO_KEY)]
    report = q.show(project, frame_limit=30, options=config)
    assert report.recovery_maintenance_error and report.checkpoints_deleted == 0
    assert doc.annotation(obj).name == 'Original'
    assert (target.read_bytes() if target.exists() else None) == before and path.exists()


def test_locked_recovery_load_can_be_cancelled_while_current_undo_runs(tmp_path):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    identity = project.identity
    config.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), *click(8, q.GuiInputTarget.CANCEL_PROJECT_LOAD),
                     event(12, q.GuiInputKind.UNDO_KEY)]
    with locked(path, tmp_path):
        report = q.show(project, frame_limit=85, options=config)
    assert not report.project_error and report.project_replacements == 0 and report.recovery_load_ms > 0
    assert project.identity == identity and doc.annotation(obj).name == 'Original' and path.exists()


def test_current_edits_remain_live_until_background_recovery_is_staged(tmp_path):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    config.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), event(12, q.GuiInputKind.UNDO_KEY),
                     *click(70, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    with locked(path, tmp_path):
        report = q.show(project, frame_limit=95, options=config)
    assert not report.project_error and report.project_replacements == 1 and report.recovery_load_ms > 0
    assert doc.annotation(obj).name == 'Original' and project.identity == source.identity and project.dirty
    assert Path(project.recovery_source) == path and path.exists()


def test_checkpoint_review_does_not_block_current_undo_while_file_is_locked(tmp_path):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    before = path.read_bytes()
    config.inputs = [event(4, q.GuiInputKind.REVIEW_RECOVERY_KEY), event(12, q.GuiInputKind.UNDO_KEY),
                     *click(70, q.GuiInputTarget.CANCEL_DISCARD_CHECKPOINT)]
    with locked(path, tmp_path):
        report = q.show(project, frame_limit=90, options=config)
    assert not report.recovery_maintenance_error and report.checkpoints_deleted == 0
    assert doc.annotation(obj).name == 'Original' and path.read_bytes() == before


def test_finite_run_waits_for_confirmed_checkpoint_deletion(tmp_path):
    project, doc, obj, primary, config, source, writer, path = setup(tmp_path)
    config.inputs = [event(4, q.GuiInputKind.REVIEW_RECOVERY_KEY), *click(18, q.GuiInputTarget.CONFIRM_DISCARD_CHECKPOINT)]
    report = q.show(project, frame_limit=20, options=config)
    assert not report.recovery_maintenance_error and report.checkpoints_deleted == 1 and not path.exists()
