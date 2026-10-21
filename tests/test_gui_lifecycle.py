"""Desktop empty-workspace and confirmed project-image removal contracts."""
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


def event(frame, kind, target=q.GuiInputTarget.CANVAS, x=.5, y=.5):
    result = q.GuiInput()
    result.frame, result.kind, result.target = frame, kind, target
    result.x, result.y = x, y
    return result


def click(frame, target):
    return [event(frame, q.GuiInputKind.LEFT_DOWN, target), event(frame+1, q.GuiInputKind.LEFT_UP, target)]


def options():
    result = q.GuiOptions()
    result.autosave_interval_seconds = 0
    return result


def workspace(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc, 'Original image')
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original object')
    view = q.ProjectView()
    view.center, view.zoom = q.Point(512, 384), 1
    project.set_view(image, view)
    path = tmp_path/'study.qpxproj'
    project.save(str(path))
    return project, doc, image, obj, path


def ppm(tmp_path):
    path = tmp_path/'source.ppm'
    path.write_bytes(b'P6\n32 32\n255\n'+bytes((40, 80, 160))*32*32)
    return path


def assert_empty(project, report):
    assert not report.project_error and not report.autosave_error
    assert project.active_image == report.active_image == 0 and project.images == []
    assert report.visible_objects == 0 and report.selected_ids == [] and report.primary_selection == 0
    assert report.final_zoom == 0 and report.pending_tiles == 0


def test_empty_project_has_no_uploaded_pixels_or_canonical_view_edits(tmp_path):
    project = q.Project()
    path = tmp_path/'empty.qpxproj'
    project.save(str(path))
    before = path.read_bytes()
    config = options()
    config.inputs = [event(3, q.GuiInputKind.RECTANGLE_KEY), event(4, q.GuiInputKind.LEFT_DOWN, x=.3, y=.3),
                     event(6, q.GuiInputKind.LEFT_UP, x=.6, y=.6), event(8, q.GuiInputKind.UNDO_KEY),
                     event(10, q.GuiInputKind.SELECT_ALL_KEY), event(12, q.GuiInputKind.DELETE_KEY),
                     event(14, q.GuiInputKind.IMPORT_KEY), event(16, q.GuiInputKind.REMOVE_IMAGE_KEY)]
    report = q.show(project, frame_limit=30, capture_path=str(tmp_path/'empty.ppm'), options=config)
    assert_empty(project, report)
    assert not project.dirty and report.uploaded_tiles == report.peak_gpu_bytes == 0
    assert path.read_bytes() == before and (tmp_path/'empty.ppm').exists()


def test_remove_last_image_save_and_reopen_empty_without_mutating_external_doc(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    config = options()
    config.inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY), event(5, q.GuiInputKind.REMOVE_IMAGE_KEY),
                     *click(14, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE), event(25, q.GuiInputKind.SAVE_KEY)]
    report = q.show(project, frame_limit=40, options=config)
    assert_empty(project, report)
    assert report.images_removed == 1 and not project.dirty
    assert doc.annotation(obj).name == 'Original object' and doc.can_undo
    loaded = q.Project.open(str(path))
    assert loaded.images == [] and loaded.active_image == 0 and not loaded.dirty
    with pytest.raises(IndexError):
        project.document(image)
    reopened = q.show(loaded, frame_limit=8, options=options())
    assert_empty(loaded, reopened)
    assert reopened.uploaded_tiles == 0 and not loaded.dirty


def test_cancel_removal_preserves_selection_and_blocks_underlying_undo(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    doc.rename_annotation(obj, 'Keep this edit')
    config = options()
    config.inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY), event(5, q.GuiInputKind.REMOVE_IMAGE_KEY),
                     event(10, q.GuiInputKind.UNDO_KEY), *click(16, q.GuiInputTarget.CANCEL_REMOVE_IMAGE)]
    report = q.show(project, frame_limit=28, options=config)
    assert not report.project_error and report.images_removed == 0
    assert report.selected_ids == [obj] and project.active_image == image
    assert doc.annotation(obj).name == 'Keep this edit' and project.dirty
    config.inputs = [event(3, q.GuiInputKind.UNDO_KEY)]
    q.show(project, frame_limit=10, options=config)
    assert doc.annotation(obj).name == 'Original object'


def test_remove_active_image_restores_surviving_session_selection_and_view(tmp_path):
    project, first_doc, first, first_obj, path = workspace(tmp_path)
    second_doc = q.Document(q.Image(32, 32, [40, 80, 160]*32*32))
    second = project.add_document(second_doc, 'Second')
    second_obj = second_doc.add_rectangle(q.Rect(1, 1, 5, 5), 'Second object')
    project.save(str(path))
    before = path.read_bytes()
    config = options()
    config.inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY), event(6, q.GuiInputKind.NEXT_IMAGE_KEY),
                     event(9, q.GuiInputKind.SELECT_ALL_KEY), event(12, q.GuiInputKind.REMOVE_IMAGE_KEY),
                     *click(21, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE)]
    report = q.show(project, frame_limit=38, options=config)
    assert not report.project_error and report.images_removed == 1 and report.active_image == first
    assert report.selected_ids == [first_obj] and report.final_zoom == 1
    assert report.final_center.x == pytest.approx(512) and report.final_center.y == pytest.approx(384)
    assert [item.id for item in project.images] == [first] and project.dirty and path.read_bytes() == before
    assert second_doc.annotation(second_obj).name == 'Second object'
    with pytest.raises(IndexError):
        project.view(second)


@pytest.mark.parametrize('remove_first', [False, True])
def test_add_from_empty_or_after_last_removal_uses_new_entry_and_saves(tmp_path, remove_first):
    source = ppm(tmp_path)
    if remove_first:
        project, doc, old_image, obj, path = workspace(tmp_path)
    else:
        project, old_image, path = q.Project(), 0, tmp_path/'empty.qpxproj'
        project.save(str(path))
    config = options()
    config.project_image_path = str(source)
    config.inputs = ([event(3, q.GuiInputKind.REMOVE_IMAGE_KEY), *click(12, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE)] if remove_first else [])
    config.inputs += [event(22, q.GuiInputKind.ADD_IMAGE_KEY), event(32, q.GuiInputKind.SAVE_KEY)]
    report = q.show(project, frame_limit=45, options=config)
    assert not report.project_error and report.active_image > old_image and report.uploaded_tiles > 0
    assert len(project.images) == 1 and project.images[0].source_path == str(source)
    assert not project.dirty and q.Project.open(str(path)).active_image == project.active_image
    assert source.read_bytes().endswith(bytes((40, 80, 160))*32*32)


@pytest.mark.parametrize('decision', ['save', 'discard', 'cancel'])
def test_new_project_routes_dirty_work_through_replacement_decision(tmp_path, decision):
    project, doc, image, obj, path = workspace(tmp_path)
    identity = project.identity
    doc.rename_annotation(obj, 'Unsaved edit')
    config = options()
    target = {'save': q.GuiInputTarget.SAVE_RECOVERY_REPLACEMENT,
              'discard': q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT,
              'cancel': q.GuiInputTarget.CANCEL_RECOVERY_REPLACEMENT}[decision]
    config.inputs = [event(4, q.GuiInputKind.NEW_PROJECT_KEY), *click(15, target)]
    report = q.show(project, frame_limit=32, options=config)
    assert not report.project_error and doc.annotation(obj).name == 'Unsaved edit'
    if decision == 'cancel':
        assert project.identity == identity and project.active_image == image and report.project_replacements == 0
    else:
        assert_empty(project, report)
        assert project.identity != identity and project.path == '' and project.dirty and report.project_replacements == 1
    assert q.Project.open(str(path)).document(image).annotation(obj).name == ('Unsaved edit' if decision == 'save' else 'Original object')


def test_remove_linked_last_image_keep_empty_recovery_without_deleting_source(tmp_path):
    source = ppm(tmp_path)
    project = q.Project()
    image = project.add_image(str(source))
    primary = tmp_path/'linked.qpxproj'
    project.save(str(primary))
    before, source_before = primary.read_bytes(), source.read_bytes()
    config = options()
    config.autosave_path = str(tmp_path/'empty.qpxrecover')
    config.inputs = [event(3, q.GuiInputKind.REMOVE_IMAGE_KEY), *click(12, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE),
                     event(22, q.GuiInputKind.CLOSE_WINDOW), *click(30, q.GuiInputTarget.KEEP_RECOVERY_CLOSE)]
    report = q.show(project, frame_limit=80, options=config)
    assert_empty(project, report)
    assert report.images_removed == 1 and report.autosaves_completed == 1
    recovered = q.Project.recover(report.recovery_path)
    assert recovered.images == [] and recovered.active_image == 0 and recovered.dirty
    assert source.read_bytes() == source_before and primary.read_bytes() == before
    assert q.Project.open(str(primary)).active_image == image


@pytest.mark.parametrize('recovery', [False, True])
def test_open_or_recover_empty_workspace_replaces_previous_authority(tmp_path, recovery):
    empty = q.Project()
    empty_path = tmp_path/'empty.qpxproj'
    empty.save(str(empty_path))
    if recovery:
        added = empty.add_document(q.Document(q.Image.demo()))
        empty.remove_image(added)
        writer = q.ProjectAutosave()
        empty_path = tmp_path/'empty.qpxrecover'
        assert writer.request(empty, str(empty_path)) and writer.wait().state == 'ready'
    project, doc, image, obj, path = workspace(tmp_path)
    doc.rename_annotation(obj, 'Retain externally')
    config = options()
    config.recovery_path = str(empty_path) if recovery else ''
    config.project_open_path = str(empty_path) if not recovery else ''
    config.inputs = [event(4, q.GuiInputKind.RECOVER_KEY if recovery else q.GuiInputKind.OPEN_PROJECT_KEY),
                     *click(16, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    report = q.show(project, frame_limit=36, options=config)
    assert_empty(project, report)
    assert project.identity == empty.identity and report.project_replacements == 1
    assert project.dirty == recovery and doc.annotation(obj).name == 'Retain externally'


def test_project_with_no_active_image_can_select_existing_catalog_entry(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    project.add_document(q.Document(q.Image.demo()), 'Second')
    project.active_image = 0
    config = options()
    config.inputs = [event(4, q.GuiInputKind.NEXT_IMAGE_KEY)]
    report = q.show(project, frame_limit=18, options=config)
    assert not report.project_error and report.active_image == image and report.uploaded_tiles > 0


def test_project_show_rejects_conflicting_options_owner():
    config = options()
    config.project = q.Project()
    with pytest.raises(ValueError, match='different project'):
        q.show(q.Project(), frame_limit=3, options=config)


def test_empty_project_and_recovery_launchers_smoke(tmp_path):
    import qupathxx._native as native
    project, doc, image, obj, primary = workspace(tmp_path)
    project.remove_image(image)
    writer = q.ProjectAutosave()
    recovery = tmp_path/'empty.qpxrecover'
    assert writer.request(project, str(recovery)) and writer.wait().state == 'ready'
    project.save(str(primary))
    executable = Path(native.__file__).parent / ('qupathxx_app.exe' if os.name == 'nt' else 'qupathxx_app')
    for arguments in [('--empty',), ('--project', str(primary)), ('--recover', str(recovery))]:
        subprocess.run([sys.executable, '-I', '-m', 'qupathxx', *arguments, '--smoke'], check=True, timeout=30)
        subprocess.run([str(executable), *arguments, '--smoke'], check=True, timeout=30)


def test_close_immediately_after_confirmation_observes_pending_removal(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    config = options()
    config.autosave_path = str(tmp_path/'pending-close.qpxrecover')
    config.inputs = [event(3, q.GuiInputKind.REMOVE_IMAGE_KEY), *click(12, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE),
                     event(13, q.GuiInputKind.CLOSE_WINDOW, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE), *click(24, q.GuiInputTarget.KEEP_RECOVERY_CLOSE)]
    report = q.show(project, frame_limit=70, options=config)
    assert_empty(project, report)
    assert report.images_removed == 1 and report.autosaves_completed == 1
    assert q.Project.recover(report.recovery_path).images == []
    assert q.Project.open(str(path)).active_image == image


def test_project_only_viewer_releases_removed_python_owned_documents(tmp_path):
    script = r'''
import gc, weakref
import qupathxx as q
class Tracked(q.Document):
    pass
project = q.Project()
doc = Tracked(q.Image.demo())
reference = weakref.ref(doc)
project.add_document(doc)
del doc
gc.collect()
assert reference() is not None
config = q.GuiOptions()
config.autosave_interval_seconds = 0
inputs = []
for frame, kind, target in [(3, q.GuiInputKind.REMOVE_IMAGE_KEY, q.GuiInputTarget.CANVAS),
                            (12, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE),
                            (13, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE)]:
    item = q.GuiInput()
    item.frame, item.kind, item.target = frame, kind, target
    inputs.append(item)
config.inputs = inputs
report = q.show(project, frame_limit=30, options=config)
gc.collect()
assert report.images_removed == 1 and not report.project_error
assert reference() is None and project.images == []
'''
    subprocess.run([sys.executable, '-I', '-c', script], check=True, timeout=20)
