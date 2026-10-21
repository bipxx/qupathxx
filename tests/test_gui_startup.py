"""Initial file I/O begins after a painted native workspace; adoption is atomic."""
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


def source(tmp_path):
    project = q.Project()
    a = project.add_document(q.Document(q.Image.demo()), 'First')
    b = project.add_document(q.Document(q.Image.demo()), 'Second')
    obj = project.document(b).add_rectangle(q.Rect(1, 2, 3, 4), 'Saved object')
    project.active_image = a
    path = tmp_path/'study.qpxproj'
    project.save(str(path))
    return project, path, b, obj


def config(kind, path):
    options = q.GuiOptions()
    options.startup_kind, options.startup_path = kind, str(path)
    options.autosave_interval_seconds = 0
    return options


@pytest.mark.parametrize('kind', ['project', 'recovery'])
def test_startup_validates_then_adopts_and_selects_image(tmp_path, kind):
    original, path, image, obj = source(tmp_path)
    primary_bytes = path.read_bytes()
    if kind == 'recovery':
        original.document(image).rename_annotation(obj, 'Recovered object')
        recovery = tmp_path/'checkpoint.qpxrecover'
        writer = q.ProjectAutosave()
        assert writer.request(original, str(recovery)) and writer.wait().state == 'ready'
        path = recovery
    target = q.Project()
    options = config(kind, path)
    options.startup_image_id = image
    report = q.show(target, frame_limit=30, options=options)
    assert report.startup_succeeded and not report.startup_cancelled and not report.project_error
    assert report.startup_frames >= 1 and report.frames >= 30+report.startup_frames
    assert report.startup_images == 2 and report.startup_objects == 1 and report.startup_ms > 0
    assert report.project_replacements == 1 and report.active_image == target.active_image == image
    assert target.identity == original.identity and report.uploaded_tiles > 0
    assert target.document(image).annotation(obj).name == ('Recovered object' if kind == 'recovery' else 'Saved object')
    assert target.dirty  # Selecting a different persisted active image is an edit.
    assert (tmp_path/'study.qpxproj').read_bytes() == primary_bytes


def test_startup_image_opens_native_source_after_initial_frame(tmp_path):
    pixels = tmp_path/'source.PPM'
    pixels.write_bytes(b'P6\n32 32\n255\n'+bytes([40, 80, 160])*32*32)
    target = q.Project()
    report = q.show(target, frame_limit=20, options=config('image', pixels))
    assert report.startup_succeeded and report.startup_frames >= 1 and report.startup_images == 1
    assert target.dirty and target.images[0].source_path == str(pixels)
    assert target.document(target.active_image).metadata.width == 32 and report.uploaded_tiles > 0


@pytest.mark.parametrize('kind', ['project', 'recovery', 'image'])
def test_missing_startup_file_keeps_empty_authority_and_reports_error(tmp_path, kind):
    target = q.Project()
    identity = target.identity
    report = q.show(target, frame_limit=8, options=config(kind, tmp_path/'missing'))
    assert not report.startup_succeeded and not report.startup_cancelled and report.project_error
    assert report.startup_frames >= 1 and report.project_replacements == report.uploaded_tiles == 0
    assert target.identity == identity and target.images == [] and target.dirty


def test_bad_stored_image_id_cannot_publish_partially_loaded_project(tmp_path):
    _, path, _, _ = source(tmp_path)
    target = q.Project()
    identity = target.identity
    options = config('project', path)
    options.startup_image_id = 999
    report = q.show(target, frame_limit=8, options=options)
    assert not report.startup_succeeded and report.project_error and report.project_replacements == 0
    assert target.identity == identity and target.images == [] and target.dirty


@pytest.mark.parametrize('cancel', [False, True])
def test_locked_startup_remains_drawable_and_can_cancel_before_adoption(tmp_path, cancel):
    _, path, _, _ = source(tmp_path)
    before = path.read_bytes()
    ready = tmp_path/'locked'
    script = "import sqlite3,sys,time; from pathlib import Path; db=sqlite3.connect(sys.argv[1]); db.execute('BEGIN EXCLUSIVE'); Path(sys.argv[2]).touch(); time.sleep(.7); db.rollback(); db.close()"
    child = subprocess.Popen([sys.executable, '-I', '-c', script, str(path), str(ready)])
    try:
        deadline = time.monotonic()+5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        assert ready.exists()
        target = q.Project()
        identity = target.identity
        options = config('project', path)
        if cancel:
            inputs = []
            for frame, kind in [(4, q.GuiInputKind.LEFT_DOWN), (5, q.GuiInputKind.LEFT_UP)]:
                event = q.GuiInput()
                event.frame, event.kind, event.target = frame, kind, q.GuiInputTarget.CANCEL_PROJECT_LOAD
                inputs.append(event)
            options.inputs = inputs
        report = q.show(target, frame_limit=20, options=options)
        assert report.startup_frames > 5 and not report.project_error
        assert report.startup_cancelled == cancel and report.startup_succeeded != cancel
        if cancel:
            assert target.identity == identity and target.images == [] and target.dirty
            assert report.project_replacements == 0
        else:
            assert target.images and report.project_replacements == 1 and report.uploaded_tiles > 0
        assert path.read_bytes() == before
    finally:
        child.wait(timeout=5)



def test_window_close_during_locked_startup_cancels_without_dirty_placeholder_prompt(tmp_path):
    _, path, _, _ = source(tmp_path)
    target = q.Project()
    options = config('project', path)
    event = q.GuiInput()
    event.frame, event.kind = 4, q.GuiInputKind.CLOSE_WINDOW
    options.inputs = [event]
    with sqlite3.connect(path) as lock:
        lock.execute('BEGIN EXCLUSIVE')
        try:
            report = q.show(target, frame_limit=12, options=options)
        finally:
            lock.rollback()
    lock.close()
    assert report.startup_cancelled and not report.startup_succeeded
    assert report.frames < 12 and report.project_replacements == 0 and target.images == []

def test_startup_rejects_nonempty_authority_before_creating_window(tmp_path):
    project, path, _, _ = source(tmp_path)
    with pytest.raises(ValueError, match='empty project'):
        q.show(project, frame_limit=8, options=config('project', path))


@pytest.mark.parametrize('kind', ['project', 'recovery', 'image'])
def test_launchers_report_failed_startup_in_smoke_mode(tmp_path, kind):
    executable = Path(q.__file__).with_name('qupathxx_app.exe')
    args = [str(tmp_path/'missing.ppm')] if kind == 'image' else ['--project' if kind == 'project' else '--recover', str(tmp_path/'missing')]
    for command in [[sys.executable, '-I', '-m', 'qupathxx'], [str(executable)]]:
        result = subprocess.run([*command, *args, '--smoke'], capture_output=True, text=True, timeout=15)
        assert result.returncode != 0 and result.stderr


@pytest.mark.parametrize('mode', ['image', 'project-image-id'])
def test_launchers_load_pixels_and_selected_stored_image(tmp_path, mode):
    if mode == 'image':
        pixels = tmp_path/'source.PPM'
        pixels.write_bytes(b'P6\n32 32\n255\n'+bytes([40, 80, 160])*32*32)
        args = [str(pixels)]
    else:
        _, path, image, _ = source(tmp_path)
        args = ['--project', str(path), '--image-id', str(image)]
    executable = Path(q.__file__).with_name('qupathxx_app.exe')
    for command in [[sys.executable, '-I', '-m', 'qupathxx'], [str(executable)]]:
        subprocess.run([*command, *args, '--smoke'], check=True, timeout=20)


def test_startup_opens_aperio_source_on_worker_when_fixture_available():
    fixture = os.environ.get('QUPATHXX_APERIO_FIXTURE')
    if not fixture or not q.has_openslide:
        pytest.skip('requires OpenSlide Aperio fixture')
    target = q.Project()
    report = q.show(target, frame_limit=60, options=config('image', Path(fixture)))
    assert report.startup_succeeded and report.startup_frames >= 1 and report.uploaded_tiles > 0
    assert target.document(target.active_image).metadata.levels


def test_failed_startup_can_open_another_project_in_same_window(tmp_path):
    source_project, path, _, _ = source(tmp_path)
    target = q.Project()
    options = config('project', tmp_path/'missing.qpxproj')
    options.project_open_path = str(path)
    inputs = []
    for frame, kind, action in [(12, q.GuiInputKind.OPEN_PROJECT_KEY, q.GuiInputTarget.CANVAS),
                                (25, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                                (26, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]:
        event = q.GuiInput()
        event.frame, event.kind, event.target = frame, kind, action
        inputs.append(event)
    options.inputs = inputs
    report = q.show(target, frame_limit=45, options=options)
    assert not report.startup_succeeded and not report.project_error
    assert report.project_replacements == 1 and target.identity == source_project.identity and report.uploaded_tiles > 0
