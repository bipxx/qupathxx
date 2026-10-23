"""Moved/renamed recovery association preserves identity and captured generations."""
from pathlib import Path
import shutil

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage disabled')


def study(tmp_path, name='original'):
    root = tmp_path/name
    (root/'assets').mkdir(parents=True)
    source = root/'assets'/'sample.ppm'
    source.write_bytes(b'P6\n16 12\n255\n'+bytes((40, 80, 160))*16*12)
    project = q.Project()
    image = project.add_image(str(source), name='Linked image')
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(1, 1, 6, 6), 'Original')
    doc.set_calibration(.25, .5)
    doc.set_class('Tumor', 0x123456)
    doc.classify_annotation(obj, 'Tumor')
    view = q.ProjectView()
    view.center, view.zoom = q.Point(8, 6), 2
    project.set_view(image, view)
    primary = root/'study.qpxproj'
    project.save(str(primary))
    doc.rename_annotation(obj, 'Captured')
    writer = q.ProjectAutosave()
    path = Path(writer.default_path(project))
    assert writer.request(project) and writer.wait().state == 'ready'
    return project, doc, image, obj, primary, path, writer


def moved_study(tmp_path, copy=False):
    project, doc, image, obj, primary, path, writer = study(tmp_path)
    destination = tmp_path/'relocated'
    if copy:
        shutil.copytree(primary.parent, destination)
    else:
        primary.parent.rename(destination)
    new_primary = destination/'renamed-study.qpxproj'
    (destination/primary.name).rename(new_primary)
    return project, doc, image, obj, primary, destination/path.name, new_primary, writer


@pytest.mark.parametrize('copy', [False, True])
def test_identity_discovery_and_explicit_association_after_move_or_copy(tmp_path, copy):
    project, doc, image, obj, original, recovery, primary, writer = moved_study(tmp_path, copy)
    checkpoint_before, primary_before = recovery.read_bytes(), primary.read_bytes()
    original_before = original.read_bytes() if copy else None
    info = q.Project.find_recoveries(str(primary))
    assert len(info) == 1 and info[0].path == str(recovery)
    assert info[0].base_path == str(original) and info[0].associated_base_path == str(primary)
    assert info[0].base_status == 'relocated' and info[0].base_generation == 1
    assert q.Project.recover(str(recovery)).path == str(original)
    associated = q.Project.recover(str(recovery), base_path=str(primary))
    assert associated.identity == project.identity and associated.path == str(primary)
    assert associated.generation == 1 and associated.dirty and associated.recovery_source == str(recovery)
    captured = associated.document(image)
    assert captured.annotation(obj).name == 'Captured' and captured.annotation(obj).class_name == 'Tumor'
    assert captured.object_class('Tumor').color == 0x123456 and captured.pixel_width_um == .25
    assert associated.view(image).zoom == 2
    assert captured.server.read_region(q.RegionRequest(0, 0, 0, 1, 1)).data == bytes((40, 80, 160, 255))
    assert associated.images[0].source_path == str(primary.parent/'assets'/'sample.ppm')
    assert recovery.read_bytes() == checkpoint_before and primary.read_bytes() == primary_before
    associated.save(str(primary))
    assert associated.generation == 2 and not associated.dirty
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == 'Captured'
    assert recovery.read_bytes() == checkpoint_before
    if copy:
        assert original.read_bytes() == original_before
    else:
        assert not original.exists()


def test_newer_relocated_primary_keeps_checkpoint_generation_and_rejects_save(tmp_path):
    project, doc, image, obj, original, recovery, primary, writer = moved_study(tmp_path)
    current = q.Project.open(str(primary))
    current.document(image).rename_annotation(obj, 'Newer primary')
    current.save(str(primary))
    before = primary.read_bytes()
    info = q.Project.find_recoveries(str(primary))[0]
    assert info.base_status == 'changed' and info.associated_base_path == str(primary)
    recovered = q.Project.recover(str(recovery), str(primary))
    assert recovered.generation == 1 and recovered.path == str(primary) and recovered.dirty
    with pytest.raises(RuntimeError, match='changed on disk'):
        recovered.save(str(primary))
    assert primary.read_bytes() == before
    destination = tmp_path/'recovered-copy.qpxproj'
    recovered.save(str(destination))
    assert recovered.generation == 1 and not recovered.dirty
    assert q.Project.open(str(destination)).document(image).annotation(obj).name == 'Captured'
    assert primary.read_bytes() == before


def test_wrong_identity_and_invalid_association_leave_current_authority_unchanged(tmp_path):
    project, doc, image, obj, original, recovery, primary, writer = moved_study(tmp_path)
    foreign = q.Project()
    foreign.add_document(q.Document(q.Image.demo()))
    foreign_path = tmp_path/'foreign.qpxproj'
    foreign.save(str(foreign_path))
    before = foreign_path.read_bytes()
    for target in [foreign_path, recovery, tmp_path/'missing.qpxproj']:
        with pytest.raises((RuntimeError, ValueError)):
            project.restore_recovery(str(recovery), base_path=str(target))
        assert project.path == str(original) and project.identity != foreign.identity
        assert doc.annotation(obj).name == 'Captured' and foreign_path.read_bytes() == before
    assert q.Project.find_recoveries(str(foreign_path)) == []


def test_unsaved_checkpoint_cannot_acquire_a_primary_baseline_after_first_save(tmp_path):
    project = q.Project()
    project.add_document(q.Document(q.Image.demo()))
    writer = q.ProjectAutosave()
    recovery = tmp_path/'unsaved.qpxrecover'
    assert writer.request(project, str(recovery)) and writer.wait().state == 'ready'
    primary = tmp_path/'first-save.qpxproj'
    project.save(str(primary))
    with pytest.raises(ValueError, match='saved-project recovery'):
        q.Project.recover(str(recovery), str(primary))
    restored = q.Project.recover(str(recovery))
    assert restored.path == '' and restored.generation == 0 and restored.dirty
    assert q.Project.find_recoveries(str(primary)) == []


def test_missing_primary_and_replaced_identity_remain_discoverable_by_recorded_path(tmp_path):
    project, doc, image, obj, primary, recovery, writer = study(tmp_path)
    primary.unlink()
    info = q.Project.find_recoveries(str(primary))[0]
    assert info.base_status == 'missing' and info.associated_base_path == ''
    foreign = q.Project()
    foreign.add_document(q.Document(q.Image.demo()))
    foreign.save(str(primary))
    info = q.Project.find_recoveries(str(primary))[0]
    assert info.identity == project.identity and info.base_status == 'changed' and info.associated_base_path == ''
    before = primary.read_bytes()
    restored = q.Project.recover(str(recovery))
    with pytest.raises(ValueError, match='different project'):
        restored.save(str(primary))
    assert primary.read_bytes() == before


def test_checkpoint_moved_alone_preserves_literal_links_for_explicit_relink(tmp_path):
    project, doc, image, obj, primary, recovery, writer = study(tmp_path)
    elsewhere = tmp_path/'checkpoint-only'
    elsewhere.mkdir()
    relocated = elsewhere/'custom.qpxrecover'
    recovery.rename(relocated)
    restored = q.Project.recover(str(relocated), str(primary))
    assert not restored.images[0].available and restored.document(image).annotation(obj).name == 'Captured'
    restored.relink(image, str(primary.parent/'assets'/'sample.ppm'))
    assert restored.document(image).server.read_region(q.RegionRequest(0, 0, 0, 1, 1)).data == bytes((40, 80, 160, 255))
    assert restored.path == str(primary) and restored.generation == 1 and restored.dirty


def test_custom_checkpoint_name_matches_identity_and_corrupt_prefix_errors_stay_visible(tmp_path):
    project, doc, image, obj, primary, recovery, writer = study(tmp_path)
    custom = recovery.with_name('arbitrary-name.qpxrecover')
    recovery.rename(custom)
    corrupt = Path(str(primary)+'.recovery-corrupt.qpxrecover')
    corrupt.write_bytes(b'not a database')
    unrelated = recovery.with_name('unrelated-corrupt.qpxrecover')
    unrelated.write_bytes(b'not a database')
    info = q.Project.find_recoveries(str(primary))
    assert len(info) == 2 and info[0].path == str(custom) and not info[0].error
    assert info[1].path == str(corrupt) and info[1].error
    assert q.Project.find_recoveries(str(primary), limit=1)[0].path == str(custom)


def test_legacy_primary_association_is_readonly_until_explicit_migrating_save(tmp_path):
    import sqlite3
    old = tmp_path/'legacy-old'
    old.mkdir()
    primary = old/'legacy.qpxproj'
    primary.write_bytes((Path(__file__).parent/'fixtures'/'project-v1.qpxproj').read_bytes())
    project = q.Project.open(str(primary))
    image = project.images[0].id
    obj = project.document(image).add_rectangle(q.Rect(0, 0, 1, 1), 'Captured on schema 1')
    writer = q.ProjectAutosave()
    recovery = Path(writer.default_path(project))
    assert writer.request(project) and writer.wait().state == 'ready'
    before = primary.read_bytes()
    new = tmp_path/'legacy-new'
    old.rename(new)
    target, checkpoint = new/primary.name, new/recovery.name
    assert q.Project.find_recoveries(str(target))[0].base_status == 'relocated'
    restored = q.Project.recover(str(checkpoint), str(target))
    assert target.read_bytes() == before and restored.generation == 1 and restored.dirty
    restored.save(str(target))
    assert restored.generation == 2 and not restored.dirty
    db = sqlite3.connect(target)
    assert db.execute('PRAGMA user_version').fetchone()[0] == 11
    db.close()
    assert q.Project.open(str(target)).document(image).annotation(obj).name == 'Captured on schema 1'
