"""Desktop explicit-save progress, live editing, close ordering and budgets."""
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


def click(frame, target, wait=False):
    down = event(frame, q.GuiInputKind.LEFT_DOWN, target)
    if wait:
        down.wait_for = q.GuiInputWait.TARGET_READY
    return [down, event(frame+1, q.GuiInputKind.LEFT_UP, target)]


def workspace(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original')
    view = q.ProjectView()
    view.center, view.zoom = q.Point(512, 384), 1
    project.set_view(image, view)
    path = tmp_path/'primary.qpxproj'
    project.save(str(path))
    doc.rename_annotation(obj, 'Captured')
    config = q.GuiOptions()
    config.autosave_interval_seconds = 0
    return project, doc, image, obj, path, config


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


def test_live_undo_during_locked_save_remains_dirty_and_keeps_recovery(tmp_path):
    project, doc, image, obj, path, config = workspace(tmp_path)
    identity = project.identity
    config.autosave_path = str(tmp_path/'current.qpxrecover')
    config.inputs = [event(3, q.GuiInputKind.AUTOSAVE_KEY), event(7, q.GuiInputKind.SAVE_KEY),
                     event(11, q.GuiInputKind.UNDO_KEY), event(13, q.GuiInputKind.SAVE_KEY),
                     event(15, q.GuiInputKind.NEW_PROJECT_KEY)]
    with locked(path, tmp_path):
        report = q.show(project, frame_limit=85, options=config)
    assert not report.project_error and not report.autosave_error and report.saves_completed == 1
    assert report.save_capture_ms >= 0 and report.save_write_ms > 0
    assert project.identity == identity and project.generation == 2 and project.dirty
    assert doc.annotation(obj).name == 'Original'
    assert q.Project.open(str(path)).document(image).annotation(obj).name == 'Captured'
    assert Path(report.recovery_path).exists() and report.autosaves_completed == 1


def test_close_waits_for_inflight_save_then_prompts_for_newer_edits(tmp_path):
    project, doc, image, obj, path, config = workspace(tmp_path)
    config.inputs = [event(3, q.GuiInputKind.SAVE_KEY), event(7, q.GuiInputKind.UNDO_KEY),
                     event(10, q.GuiInputKind.CLOSE_WINDOW), *click(11, q.GuiInputTarget.DISCARD_CLOSE, wait=True)]
    with locked(path, tmp_path):
        report = q.show(project, frame_limit=100, options=config)
    assert not report.project_error and report.saves_completed == 1 and report.replay_frame < 100
    assert report.replay_complete and report.replay_wait_frames > 0
    assert report.frames == report.replay_frame+report.replay_wait_frames
    assert project.dirty and doc.annotation(obj).name == 'Original'
    assert q.Project.open(str(path)).document(image).annotation(obj).name == 'Captured'


def test_finite_run_finishes_pending_save_and_reconciles_before_return(tmp_path):
    project, doc, image, obj, path, config = workspace(tmp_path)
    config.inputs = [event(3, q.GuiInputKind.SAVE_KEY)]
    with locked(path, tmp_path):
        report = q.show(project, frame_limit=8, options=config)
    assert not report.project_error and report.saves_completed == 1 and report.frames == 8
    assert project.generation == 2 and not project.dirty
    assert q.Project.open(str(path)).document(image).annotation(obj).name == 'Captured'


def test_explicit_save_budget_failure_is_visible_and_preserves_primary(tmp_path):
    project, doc, image, obj, path, config = workspace(tmp_path)
    before = path.read_bytes()
    config.save_snapshot_budget = 1
    config.inputs = [event(3, q.GuiInputKind.SAVE_KEY), event(8, q.GuiInputKind.UNDO_KEY)]
    report = q.show(project, frame_limit=18, options=config)
    assert 'budget' in report.project_error and report.saves_completed == 0
    assert path.read_bytes() == before and project.generation == 1
    assert doc.annotation(obj).name == 'Original'
