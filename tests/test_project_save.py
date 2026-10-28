"""Native explicit-save snapshots, reconciliation, rollback and cancellation."""
import gc
from pathlib import Path
import sqlite3

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage disabled')


def workspace(tmp_path, name='study'):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc, name)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original')
    path = tmp_path/(name+'.qpxproj')
    project.save(str(path))
    return project, doc, image, obj, path


def test_async_save_keeps_newer_edits_dirty_and_preserves_history(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    doc.rename_annotation(obj, 'Captured')
    job = q.ProjectSaveJob()
    assert job.request(project, str(path)) and job.busy
    assert not job.request(project, str(path))
    doc.rename_annotation(obj, 'Later live edit')
    project.rename_image(image, 'Later image name')
    stats = job.wait()
    assert stats.state == 'saved' and stats.applied and stats.completed == 1 and stats.generation == 2
    assert stats.capture_ms >= 0 and stats.write_ms > 0 and stats.snapshot_bytes > 0
    assert project.dirty and project.generation == 2 and project.path == str(path)
    captured = q.Project.open(str(path))
    assert captured.document(image).annotation(obj).name == 'Captured' and captured.images[0].name == 'study'
    assert doc.annotation(obj).name == 'Later live edit' and doc.can_undo
    assert job.request(project, str(path)) and job.wait().state == 'saved'
    assert not project.dirty and project.generation == 3 and job.stats.completed == 2
    doc.undo()
    assert doc.annotation(obj).name == 'Captured' and project.dirty


def test_snapshot_contains_removed_image_and_captured_view_without_reverting_live_graph(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    view = q.ProjectView()
    view.center, view.zoom = q.Point(100, 200), 3
    project.set_view(image, view)
    doc.set_class('Tumor', 0x123456)
    doc.classify_annotation(obj, 'Tumor')
    doc.set_calibration(.2, .4)
    job = q.ProjectSaveJob()
    assert job.request(project, str(path))
    project.remove_image(image)
    new_image = project.add_document(q.Document(q.Image.demo()), 'New live image')
    assert job.wait().applied and project.dirty and project.active_image == new_image
    saved = q.Project.open(str(path))
    assert saved.active_image == image and saved.view(image).zoom == 3
    assert saved.document(image).annotation(obj).class_name == 'Tumor'
    assert saved.document(image).object_class('Tumor').color == 0x123456
    assert saved.document(image).pixel_width_um == .2 and saved.document(image).pixel_height_um == .4
    assert [item.id for item in project.images] == [new_image]


def test_save_as_preserves_original_and_relative_links(tmp_path):
    source = tmp_path/'source.ppm'
    source.write_bytes(b'P6\n4 4\n255\n'+bytes((40, 80, 160))*16)
    project = q.Project()
    image = project.add_image(str(source))
    original = tmp_path/'original.qpxproj'
    project.save(str(original))
    before = original.read_bytes()
    destination = tmp_path/'other'/'new.qpxproj'
    destination.parent.mkdir()
    job = q.ProjectSaveJob()
    assert job.request(project, str(destination)) and job.wait().applied
    assert not project.dirty and project.path == str(destination) and project.generation == 1
    assert original.read_bytes() == before
    loaded = q.Project.open(str(destination))
    assert loaded.images[0].source_path == str(source)
    with sqlite3.connect(destination) as db:
        assert db.execute('SELECT source FROM images').fetchone()[0] == '../source.ppm'
    assert loaded.document(image).server.read_region(q.RegionRequest(0, 0, 0, 4, 4)).data == bytes((40, 80, 160, 255))*16


def test_failed_transaction_preserves_disk_and_live_baseline_then_retries(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    db = sqlite3.connect(path)
    db.execute("CREATE TRIGGER fail_save BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected async failure'); END")
    db.commit()
    db.close()
    before = path.read_bytes()
    doc.rename_annotation(obj, 'Unsaved')
    job = q.ProjectSaveJob()
    assert job.request(project, str(path))
    stats = job.wait()
    assert stats.state == 'error' and 'injected async failure' in stats.error and not stats.applied
    assert project.dirty and project.generation == 1 and path.read_bytes() == before
    db = sqlite3.connect(path)
    db.execute('DROP TRIGGER fail_save')
    db.commit()
    db.close()
    assert job.request(project, str(path)) and job.wait().applied and not project.dirty


def test_stale_writer_and_foreign_project_reject_without_changing_live_baseline(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    stale = q.Project.open(str(path))
    project.rename_image(image, 'Winner')
    project.save(str(path))
    job = q.ProjectSaveJob()
    assert job.request(stale, str(path))
    stats = job.wait()
    assert stats.state == 'error' and 'changed on disk' in stats.error
    assert stale.generation == 1 and stale.path == str(path)
    foreign, _, _, _, foreign_path = workspace(tmp_path, 'foreign')
    before = foreign_path.read_bytes()
    assert job.request(stale, str(foreign_path))
    assert job.wait().state == 'error' and foreign_path.read_bytes() == before
    new_path = tmp_path/'stale-copy.qpxproj'
    assert job.request(stale, str(new_path)) and job.wait().applied
    assert stale.generation == 1 and not stale.dirty


def test_request_does_not_query_locked_destination_on_owner_and_cancel_rolls_back(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    before = path.read_bytes()
    doc.rename_annotation(obj, 'Cancelled snapshot')
    db = sqlite3.connect(path)
    db.execute('BEGIN EXCLUSIVE')
    job = q.ProjectSaveJob()
    try:
        assert job.request(project, str(path)) and job.busy
        assert job.poll().state == 'saving' and project.generation == 1
        job.cancel()
        db.rollback()
    finally:
        db.close()
    stats = job.wait()
    assert stats.state == 'cancelled' and not stats.applied and stats.completed == 0
    assert path.read_bytes() == before and project.generation == 1 and project.dirty


def test_competing_workers_allow_exactly_one_generation_commit(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    doc.rename_annotation(obj, 'Contended snapshot')
    jobs = [q.ProjectSaveJob(), q.ProjectSaveJob()]
    db = sqlite3.connect(path)
    db.execute('BEGIN EXCLUSIVE')
    try:
        for job in jobs:
            assert job.request(project, str(path))
        db.rollback()
    finally:
        db.close()
    stats = [job.wait() for job in jobs]
    assert sorted(result.state for result in stats) == ['error', 'saved']
    assert sum(result.completed for result in stats) == 1 and project.generation == 2 and not project.dirty
    assert q.Project.open(str(path)).generation == 2


def test_replaced_workspace_is_not_acknowledged_by_old_save_result(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    other, other_doc, other_image, other_obj, other_path = workspace(tmp_path, 'other')
    other_doc.rename_annotation(other_obj, 'Recovered other')
    recovery = tmp_path/'other.qpxrecover'
    autosave = q.ProjectAutosave()
    assert autosave.request(other, str(recovery)) and autosave.wait().state == 'ready'
    doc.rename_annotation(obj, 'Old captured workspace')
    db = sqlite3.connect(path)
    db.execute('BEGIN EXCLUSIVE')
    job = q.ProjectSaveJob()
    try:
        assert job.request(project, str(path))
        project.restore_recovery(str(recovery))
        db.rollback()
    finally:
        db.close()
    stats = job.wait()
    assert stats.state == 'detached' and stats.completed == 1 and not stats.applied
    assert project.identity == other.identity and project.path == str(other_path) and project.generation == 1 and project.dirty
    assert project.document(other_image).annotation(other_obj).name == 'Recovered other'
    assert q.Project.open(str(path)).document(image).annotation(obj).name == 'Old captured workspace'


def test_separate_save_path_change_during_job_does_not_get_overwritten(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    job = q.ProjectSaveJob()
    db = sqlite3.connect(path)
    db.execute('BEGIN EXCLUSIVE')
    destination = tmp_path/'separate.qpxproj'
    try:
        assert job.request(project, str(path))
        project.save(str(destination))
        db.rollback()
    finally:
        db.close()
    assert job.wait().state == 'detached'
    assert project.path == str(destination) and project.generation == 1 and not project.dirty


def test_owner_thread_destructor_finishes_and_reconciles_authorized_save(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    doc.rename_annotation(obj, 'Destructor save')
    job = q.ProjectSaveJob()
    assert job.request(project, str(path))
    del job
    gc.collect()
    assert project.generation == 2 and not project.dirty
    assert q.Project.open(str(path)).document(image).annotation(obj).name == 'Destructor save'


def test_budget_invalid_paths_and_missing_directory_do_not_mutate_live_state(tmp_path):
    project, doc, image, obj, path = workspace(tmp_path)
    before = path.read_bytes()
    with pytest.raises(ValueError):
        q.ProjectSaveJob(0)
    with pytest.raises(ValueError):
        q.ProjectSaveJob(4*1024**3+1)
    with pytest.raises((ValueError, RuntimeError), match='budget'):
        q.ProjectSaveJob(1).request(project, str(path))
    job = q.ProjectSaveJob()
    for invalid in ['', 'bad\0path']:
        with pytest.raises(ValueError):
            job.request(project, invalid)
    assert job.request(project, str(tmp_path/'missing'/'file.qpxproj'))
    assert job.wait().state == 'error'
    assert project.generation == 1 and not project.dirty and path.read_bytes() == before


def test_empty_project_and_legacy_schema_save_use_same_worker(tmp_path):
    project = q.Project()
    job = q.ProjectSaveJob()
    path = tmp_path/'empty.qpxproj'
    assert job.request(project, str(path)) and job.wait().applied
    assert not project.dirty and q.Project.open(str(path)).images == []
    legacy = tmp_path/'legacy.qpxproj'
    legacy.write_bytes((Path(__file__).parent/'fixtures'/'project-v1.qpxproj').read_bytes())
    loaded = q.Project.open(str(legacy))
    assert job.request(loaded, str(legacy)) and job.wait().applied
    db = sqlite3.connect(legacy)
    assert db.execute('PRAGMA user_version').fetchone()[0] == 14
    db.close()
    assert not loaded.dirty and loaded.generation == 2


@pytest.mark.parametrize('read_only', [False, True])
def test_killed_writer_hot_journal_recovers_natively_when_writable(tmp_path, read_only):
    import os
    import stat
    import subprocess
    import sys
    import time
    if read_only and os.name != 'nt':
        pytest.skip('Windows read-only file attribute check')
    project, doc, image, obj, path = workspace(tmp_path)
    db = sqlite3.connect(path)
    # Keep a real native transaction in flight after its image pages spill.
    # This needs no Python callbacks on the native writer connection.
    db.execute('''CREATE TRIGGER slow_write AFTER INSERT ON images BEGIN
        SELECT sum(x) FROM (WITH RECURSIVE c(x) AS
        (VALUES(1) UNION ALL SELECT x+1 FROM c WHERE x<4000000) SELECT x FROM c);
        END''')
    db.commit()
    db.close()
    script = r'''
import sys,time,os
sys.path.insert(0,sys.argv[2])
import qupathxx as q
print(os.getpid(),flush=True)
project=q.Project.open(sys.argv[1]); doc=project.document(project.active_image)
doc.rename_annotation(1,'Interrupted snapshot')
doc.execute([q.ObjectEdit.create(q.Rect(i%100,i//100,1,1),str(i)) for i in range(1000)])
job=q.ProjectSaveJob(); job.request(project,sys.argv[1])
while job.busy:
    job.poll(); time.sleep(.001)
'''
    # A Windows virtualenv launcher has a different PID from its interpreter.
    child = subprocess.Popen([sys._base_executable, '-I', '-c', script, str(path),
                              str(Path(q.__file__).parent.parent)], stdout=subprocess.PIPE, text=True)
    journal = Path(str(path)+'-journal')
    magic = b'\xd9\xd5\x05\xf9\x20\xa1\x63\xd7'
    hot = False
    try:
        assert int(child.stdout.readline()) == child.pid
        deadline = time.monotonic()+10
        while child.poll() is None and time.monotonic() < deadline:
            try:
                with journal.open('rb') as stream:
                    hot = stream.read(8) == magic
            except OSError:
                pass
            if hot:
                break
            time.sleep(.001)
        assert hot, 'native transaction did not expose a hot journal before completion'
        child.kill()
        child.wait(timeout=5)
        assert journal.exists() and journal.read_bytes()[:8] == magic
        if read_only:
            path.chmod(stat.S_IREAD)
            with pytest.raises(RuntimeError):
                q.Project.open(str(path))
            assert journal.exists()
            path.chmod(stat.S_IREAD | stat.S_IWRITE)
        restored = q.Project.open(str(path))
        assert restored.generation == 1 and restored.document(image).object_count == 1
        assert restored.document(image).annotation(obj).name == 'Original'
        assert not journal.exists()
        db = sqlite3.connect(path)
        assert db.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
        db.close()
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=5)
        path.chmod(stat.S_IREAD | stat.S_IWRITE)


@pytest.mark.parametrize('kind', ['save', 'autosave'])
def test_factory_server_roundtrip_does_not_require_gil_during_snapshot_release(tmp_path, kind):
    import subprocess
    import sys
    script = r'''
import gc,sys
from pathlib import Path
import qupathxx as q
project=q.Project()
server=q.dense_image_server(q.Image.demo())
doc=q.Document(server)
image=project.add_document(doc)
del server
path=Path(sys.argv[1])
if sys.argv[2]=='save':
    job=q.ProjectSaveJob()
else:
    job=q.ProjectAutosave()
assert job.request(project,str(path))
project.remove_image(image)
del doc; gc.collect()
status=job.wait()
assert status.state==('saved' if sys.argv[2]=='save' else 'ready')
loaded=q.Project.open(str(path)) if sys.argv[2]=='save' else q.Project.recover(str(path))
assert len(loaded.images)==1
assert loaded.document(loaded.active_image).server.read_region(q.RegionRequest(0,0,0,1,1)).data
assert project.images==[] and project.dirty
'''
    extension = '.qpxproj' if kind == 'save' else '.qpxrecover'
    subprocess.run([sys.executable, '-I', '-c', script, str(tmp_path/('server-roundtrip'+extension)), kind], check=True, timeout=15)
