"""Real desktop project replacement, failures, and concurrent loading."""
import os
from pathlib import Path
import sqlite3
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
    return [event(frame, q.GuiInputKind.LEFT_DOWN, target), event(frame+1, q.GuiInputKind.LEFT_UP, target)]


def workspace(tmp_path, name, color):
    project = q.Project()
    doc = q.Document(q.Image(512, 512, list(color)*512*512))
    image = project.add_document(doc, name)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original')
    view = q.ProjectView()
    view.center, view.zoom = q.Point(256, 256), 1
    project.set_view(image, view)
    path = tmp_path / (name+'.qpxproj')
    project.save(str(path))
    return project, doc, image, obj, path


def options(project, path):
    result = q.GuiOptions()
    result.project, result.project_open_path, result.autosave_interval_seconds = project, str(path), 0
    return result


def test_open_after_discard_renders_saved_pixels_and_preserves_old_document(tmp_path):
    from PIL import Image
    source, source_doc, image, obj, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    current, doc, _, old_obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    doc.rename_annotation(old_obj, 'Live old work')
    before, source_before = primary.read_bytes(), source_path.read_bytes()
    config = options(current, source_path)
    config.inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY), event(4, q.GuiInputKind.OPEN_PROJECT_KEY),
                     *click(18, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    capture = tmp_path/'opened.ppm'
    report = q.show(doc, frame_limit=45, capture_path=str(capture), options=config)
    assert report.project_replacements == 1 and report.project_load_ms > 0 and not report.project_error
    assert current.identity == source.identity and current.path == str(source_path)
    assert current.recovery_source == '' and not current.dirty and not report.selected_ids
    assert current.document(image).annotation(obj).name == 'Original'
    assert doc.annotation(old_obj).name == 'Live old work' and doc.can_undo
    assert primary.read_bytes() == before and source_path.read_bytes() == source_before
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.65), frame.height//2)) == (40, 80, 160)


@pytest.mark.parametrize('decision', ['save', 'cancel'])
def test_open_dirty_save_or_cancel_keeps_authority_and_primary_consistent(tmp_path, decision):
    source, _, _, _, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    current, doc, image, obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    identity = current.identity
    doc.rename_annotation(obj, 'Edited')
    config = options(current, source_path)
    target = q.GuiInputTarget.SAVE_RECOVERY_REPLACEMENT if decision == 'save' else q.GuiInputTarget.CANCEL_RECOVERY_REPLACEMENT
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY), *click(18, target)]
    report = q.show(doc, frame_limit=45, options=config)
    assert not report.project_error
    assert report.project_replacements == (1 if decision == 'save' else 0)
    if decision == 'save':
        assert current.identity == source.identity and not current.dirty
        assert q.Project.open(str(primary)).document(image).annotation(obj).name == 'Edited'
    else:
        assert current.identity == identity and current.dirty
        assert q.Project.open(str(primary)).document(image).annotation(obj).name == 'Original'


def test_clean_workspace_opens_without_prompt_and_save_writes_new_authority(tmp_path):
    source, _, image, obj, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    current, doc, _, _, primary = workspace(tmp_path, 'current', (210, 60, 30))
    before = primary.read_bytes()
    config = options(current, source_path)
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY), event(24, q.GuiInputKind.SAVE_KEY)]
    report = q.show(doc, frame_limit=45, options=config)
    assert not report.project_error and report.project_replacements == 1
    assert current.identity == source.identity and current.generation == 2 and not current.dirty
    assert primary.read_bytes() == before
    assert q.Project.open(str(source_path)).generation == 2


def test_save_before_opening_same_file_reloads_saved_generation(tmp_path):
    current, doc, image, obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    doc.rename_annotation(obj, 'Latest edit')
    config = options(current, primary)
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY), *click(18, q.GuiInputTarget.SAVE_RECOVERY_REPLACEMENT),
                     event(35, q.GuiInputKind.SAVE_KEY)]
    report = q.show(doc, frame_limit=50, options=config)
    assert not report.project_error and report.project_replacements == 1
    assert current.generation == 3 and not current.dirty
    assert current.document(image).annotation(obj).name == 'Latest edit'
    assert q.Project.open(str(primary)).generation == 3


def test_failed_save_before_open_keeps_current_workspace_and_dialog(tmp_path):
    source, _, _, _, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    current, doc, image, obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    doc.rename_annotation(obj, 'Keep me')
    identity = current.identity
    db = sqlite3.connect(primary)
    db.execute("CREATE TRIGGER fail_save BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected open save failure'); END")
    db.commit()
    db.close()
    config = options(current, source_path)
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY), *click(18, q.GuiInputTarget.SAVE_RECOVERY_REPLACEMENT),
                     event(27, q.GuiInputKind.UNDO_KEY)]
    report = q.show(doc, frame_limit=40, options=config)
    assert 'injected open save failure' in report.project_error and report.project_replacements == 0
    assert current.identity == identity and doc.annotation(obj).name == 'Keep me' and current.dirty


@pytest.mark.parametrize('invalid', ['missing', 'corrupt', 'empty', 'recovery'])
def test_invalid_project_open_keeps_live_document_and_history(tmp_path, invalid):
    current, doc, image, obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    target = tmp_path/'invalid.qpxproj'
    if invalid == 'corrupt':
        target.write_bytes(b'not a database')
    elif invalid == 'empty':
        q.Project().save(str(target))
    elif invalid == 'recovery':
        doc.rename_annotation(obj, 'Recovered')
        writer = q.ProjectAutosave()
        target = tmp_path/'invalid.qpxrecover'
        assert writer.request(current, str(target)) and writer.wait().state == 'ready'
    identity, before = current.identity, primary.read_bytes()
    doc.rename_annotation(obj, 'Live')
    config = options(current, target)
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY), event(24, q.GuiInputKind.UNDO_KEY)]
    report = q.show(doc, frame_limit=40, options=config)
    assert report.project_error and report.project_replacements == 0 and current.identity == identity
    assert doc.annotation(obj).name == ('Recovered' if invalid == 'recovery' else 'Original')
    assert primary.read_bytes() == before


def test_locked_project_load_can_be_cancelled_while_live_undo_runs(tmp_path):
    source, _, _, _, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    current, doc, _, obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    identity = current.identity
    doc.rename_annotation(obj, 'Undo while loading')
    ready = tmp_path/'locked'
    script = "import sqlite3,sys,time; from pathlib import Path; db=sqlite3.connect(sys.argv[1]); db.execute('BEGIN EXCLUSIVE'); Path(sys.argv[2]).write_text('ready'); time.sleep(0.9); db.rollback(); db.close()"
    child = subprocess.Popen([sys.executable, '-c', script, str(source_path), str(ready)])
    try:
        deadline = time.monotonic()+5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        assert ready.exists()
        config = options(current, source_path)
        config.inputs = [event(3, q.GuiInputKind.OPEN_PROJECT_KEY), *click(7, q.GuiInputTarget.CANCEL_PROJECT_LOAD),
                         event(12, q.GuiInputKind.UNDO_KEY)]
        report = q.show(doc, frame_limit=85, options=config)
        assert not report.project_error and report.project_replacements == 0
        assert current.identity == identity and doc.annotation(obj).name == 'Original'
    finally:
        child.wait(timeout=5)


def test_python_owned_inactive_documents_retire_without_gil_deadlock(tmp_path):
    script = r'''
from pathlib import Path
import sys
import qupathxx as q
root = Path(sys.argv[1])
source = q.Project()
source.add_document(q.Document(q.Image.demo()))
path = root / 'source.qpxproj'
source.save(str(path))
current = q.Project()
doc = q.Document(q.Image.demo())
current.add_document(doc)
current.add_document(q.Document(q.Image.demo()), 'Python-owned inactive document')
options = q.GuiOptions()
options.project, options.project_open_path, options.autosave_interval_seconds = current, str(path), 0
inputs = []
for frame, kind, target in [(3, q.GuiInputKind.OPEN_PROJECT_KEY, q.GuiInputTarget.CANVAS),
                            (18, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                            (19, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]:
    item = q.GuiInput()
    item.frame, item.kind, item.target = frame, kind, target
    inputs.append(item)
options.inputs = inputs
report = q.show(doc, frame_limit=40, options=options)
assert report.project_replacements == 1 and not report.project_error
assert current.identity == source.identity
'''
    subprocess.run([sys.executable, '-I', '-c', script, str(tmp_path)], check=True, timeout=20)


def test_open_restores_active_image_view_and_browses_new_catalog(tmp_path):
    source, _, first, obj, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    second_doc = q.Document(q.Image.demo())
    second = source.add_document(second_doc, 'Second image')
    source.active_image = second
    view = q.ProjectView()
    view.center, view.zoom = q.Point(400, 240), 2
    source.set_view(second, view)
    source.save(str(source_path))
    current, doc, _, _, _ = workspace(tmp_path, 'current', (210, 60, 30))
    config = options(current, source_path)
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY)]
    report = q.show(doc, frame_limit=40, options=config)
    assert not report.project_error and report.active_image == second and not current.dirty
    assert report.final_center.x == pytest.approx(400) and report.final_center.y == pytest.approx(240)
    assert report.final_zoom == 2
    config.inputs = [event(4, q.GuiInputKind.NEXT_IMAGE_KEY)]
    report = q.show(current.document(second), frame_limit=25, options=config)
    assert not report.project_error and report.active_image == first
    assert current.document(first).annotation(obj).name == 'Original'
    assert len(current.images) == 2 and current.view(second).zoom == 2


def test_open_finishes_and_clears_only_old_session_autosave(tmp_path):
    source, _, _, _, source_path = workspace(tmp_path, 'source', (40, 80, 160))
    current, doc, _, obj, primary = workspace(tmp_path, 'current', (210, 60, 30))
    doc.rename_annotation(obj, 'Captured old work')
    before = primary.read_bytes()
    config = options(current, source_path)
    config.autosave_path = str(tmp_path/'old-session.qpxrecover')
    config.inputs = [event(3, q.GuiInputKind.AUTOSAVE_KEY), event(5, q.GuiInputKind.OPEN_PROJECT_KEY),
                     *click(22, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    report = q.show(doc, frame_limit=45, options=config)
    assert not report.project_error and not report.autosave_error
    assert report.project_replacements == 1 and report.autosaves_completed == 1
    assert current.identity == source.identity and not Path(config.autosave_path).exists()
    assert primary.read_bytes() == before and doc.annotation(obj).name == 'Captured old work'
