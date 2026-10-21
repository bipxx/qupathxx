"""Desktop discovery, association decisions and launchers after project relocation."""
import os
from pathlib import Path
import subprocess
import sys

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


def moved(tmp_path):
    old = tmp_path/'old'
    old.mkdir()
    project = q.Project()
    doc = q.Document(q.Image(128, 128, [40, 80, 160]*128*128))
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original')
    view = q.ProjectView()
    view.center, view.zoom = q.Point(64, 64), 2
    project.set_view(image, view)
    original = old/'study.qpxproj'
    project.save(str(original))
    doc.rename_annotation(obj, 'Captured')
    writer = q.ProjectAutosave()
    recovery = Path(writer.default_path(project))
    assert writer.request(project) and writer.wait().state == 'ready'
    new = tmp_path/'new'
    old.rename(new)
    primary = new/'renamed.qpxproj'
    (new/original.name).rename(primary)
    recovery = new/recovery.name
    return q.Project.open(str(primary)), image, obj, original, primary, recovery, writer


def options(recovery='', primary=''):
    result = q.GuiOptions()
    result.autosave_interval_seconds = 0
    result.recovery_path, result.recovery_base_path = str(recovery), str(primary)
    return result


def test_explicit_recovery_primary_choice_survives_replace_and_save(tmp_path):
    from PIL import Image
    project, image, obj, original, primary, recovery, writer = moved(tmp_path)
    doc = project.document(image)
    doc.rename_annotation(obj, 'Unsaved current work')
    before = recovery.read_bytes()
    config = options(recovery, primary)
    config.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), *click(18, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                     event(32, q.GuiInputKind.SAVE_KEY)]
    capture = tmp_path/'associated.ppm'
    report = q.show(project, frame_limit=50, capture_path=str(capture), options=config)
    assert not report.project_error and report.project_replacements == report.saves_completed == 1
    assert project.path == str(primary) and project.generation == 2 and not project.dirty
    assert doc.annotation(obj).name == 'Unsaved current work'
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == 'Captured'
    assert recovery.read_bytes() == before and not original.exists() and not original.parent.exists()
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.65), frame.height//2)) == (40, 80, 160)


def test_find_key_discovers_old_filename_and_prefills_verified_primary_location(tmp_path):
    project, image, obj, original, primary, recovery, writer = moved(tmp_path)
    config = options()
    config.inputs = [event(4, q.GuiInputKind.FIND_RECOVERIES_KEY), event(20, q.GuiInputKind.RECOVER_KEY),
                     event(36, q.GuiInputKind.SAVE_KEY)]
    report = q.show(project, frame_limit=50, options=config)
    assert not report.project_error and report.project_replacements == report.saves_completed == 1
    assert project.path == str(primary) and project.generation == 2 and not project.dirty
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == 'Captured'
    assert recovery.exists() and not original.exists()


def test_wrong_primary_choice_rejects_before_staging_and_leaves_live_undo_usable(tmp_path):
    project, image, obj, original, primary, recovery, writer = moved(tmp_path)
    doc = project.document(image)
    doc.rename_annotation(obj, 'Current edit')
    foreign = q.Project()
    foreign.add_document(q.Document(q.Image.demo()))
    foreign_path = tmp_path/'foreign.qpxproj'
    foreign.save(str(foreign_path))
    before, foreign_before = primary.read_bytes(), foreign_path.read_bytes()
    config = options(recovery, foreign_path)
    config.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), event(18, q.GuiInputKind.UNDO_KEY)]
    report = q.show(project, frame_limit=30, options=config)
    assert 'same primary project identity' in report.project_error and report.project_replacements == 0
    assert doc.annotation(obj).name == 'Original' and project.path == str(primary) and project.generation == 1
    assert primary.read_bytes() == before and foreign_path.read_bytes() == foreign_before


def test_associated_stale_recovery_cannot_overwrite_newer_primary_from_gui(tmp_path):
    project, image, obj, original, primary, recovery, writer = moved(tmp_path)
    project.document(image).rename_annotation(obj, 'Newer saved data')
    project.save(str(primary))
    before = primary.read_bytes()
    config = options(recovery, primary)
    config.inputs = [event(4, q.GuiInputKind.RECOVER_KEY), event(26, q.GuiInputKind.SAVE_KEY)]
    report = q.show(project, frame_limit=42, options=config)
    assert 'changed on disk' in report.project_error and report.project_replacements == 1 and report.saves_completed == 0
    assert project.path == str(primary) and project.generation == 1 and project.dirty
    assert project.document(image).annotation(obj).name == 'Captured' and primary.read_bytes() == before


def test_both_launchers_associate_moved_recovery_and_validate_option_pairing(tmp_path):
    import qupathxx._native as native
    project, image, obj, original, primary, recovery, writer = moved(tmp_path)
    before, checkpoint_before = primary.read_bytes(), recovery.read_bytes()
    executable = Path(native.__file__).parent/('qupathxx_app.exe' if os.name == 'nt' else 'qupathxx_app')
    for command in [[sys.executable, '-I', '-m', 'qupathxx'], [str(executable)]]:
        subprocess.run([*command, '--recover', str(recovery), '--recovery-base', str(primary), '--smoke'], check=True, timeout=30)
        invalid = subprocess.run([*command, '--recovery-base', str(primary), '--smoke'], capture_output=True, text=True, timeout=10)
        assert invalid.returncode != 0 and 'requires --recover' in invalid.stderr
    assert primary.read_bytes() == before and recovery.read_bytes() == checkpoint_before and not original.exists()
