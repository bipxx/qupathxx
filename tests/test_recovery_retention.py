"""Retention preserves live owners and untrusted files while bounding collection."""
from contextlib import contextmanager
import gc
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage disabled')


def workspace(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1), 'Original')
    image = project.add_document(doc)
    primary = tmp_path/'primary.qpxproj'
    project.save(str(primary))
    return project, doc, obj, image, primary


def age(path, days):
    timestamp = time.time()-days*86400
    with sqlite3.connect(path) as db:
        db.execute('UPDATE recovery SET created_ms=?', (int(timestamp*1000),))
    db.close()
    os.utime(path, (timestamp, timestamp))


def checkpoint(project, doc, obj, path, days=0, active=False):
    doc.rename_annotation(obj, path.stem)
    writer = q.ProjectAutosave()
    assert writer.request(project, str(path)) and writer.wait().state == 'ready'
    if days:
        age(path, days)
    return writer if active else None


def policy(keep=2, limit=32):
    result = q.RecoveryRetention()
    result.enabled, result.keep_latest, result.max_age_days, result.max_deletions = True, keep, 7, limit
    return result


def test_default_disabled_and_newest_retention_preserve_primary(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    before = primary.read_bytes()
    paths = [tmp_path/f'old-{i}.qpxrecover' for i in range(4)]
    for path, days in zip(paths, [100, 90, 80, 70]):
        checkpoint(project, doc, obj, path, days)
    current = tmp_path/'current.qpxrecover'
    active = checkpoint(project, doc, obj, current, active=True)
    assert not active.retention.enabled
    assert q.Project.prune_recoveries(str(current), q.RecoveryRetention()).removed == 0
    assert all(path.exists() for path in paths)
    result = q.Project.prune_recoveries(str(current), policy())
    assert result.removed == 3 and result.failed == 0 and not result.error
    assert result.inspected == 5 and result.elapsed_ms > 0
    assert [path.exists() for path in paths] == [False, False, False, True]
    assert current.exists() and primary.read_bytes() == before
    assert q.Project.recover(str(current)).document(image).annotation(obj).name == 'current'
    assert doc.annotation(obj).name == 'current' and project.dirty
    assert q.Project.prune_recoveries(str(current), policy()).removed == 0


def test_idle_session_is_protected_until_its_controller_dies(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, current = tmp_path/'active.qpxrecover', tmp_path/'current.qpxrecover'
    owner = checkpoint(project, doc, obj, old, 100, active=True)
    checkpoint(project, doc, obj, current)
    assert not owner.busy
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 0 and old.exists()
    del owner
    gc.collect()
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 1 and not old.exists()


def test_one_session_can_own_multiple_destinations_and_closed_copies_are_collected(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    writer = q.ProjectAutosave()
    paths = [tmp_path/f'session-{i}.qpxrecover' for i in range(3)]
    for path in paths:
        doc.rename_annotation(obj, path.stem)
        assert writer.request(project, str(path)) and writer.wait().state == 'ready'
        age(path, 100)
    current = tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, current)
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 0
    del writer
    gc.collect()
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 3


def test_missing_local_lease_state_preserves_foreign_checkpoints(tmp_path, monkeypatch):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, current = tmp_path/'old.qpxrecover', tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    checkpoint(project, doc, obj, current)
    state = tmp_path/'other-user-state'
    monkeypatch.setenv('LOCALAPPDATA' if os.name == 'nt' else 'XDG_STATE_HOME', str(state))
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 0 and old.exists()
    assert not state.exists()


def test_unavailable_lease_storage_preserves_publication_as_unmanaged(tmp_path, monkeypatch):
    project, doc, obj, image, primary = workspace(tmp_path)
    blocked = tmp_path/'blocked-state'
    blocked.write_bytes(b'not a directory')
    monkeypatch.setenv('LOCALAPPDATA' if os.name == 'nt' else 'XDG_STATE_HOME', str(blocked))
    doc.rename_annotation(obj, 'Capture without lease storage')
    path = tmp_path/'unmanaged.qpxrecover'
    writer = q.ProjectAutosave()
    writer.retention = policy(1)
    assert writer.request(project, str(path))
    stats = writer.wait()
    assert stats.state == 'ready' and stats.retention_error and not stats.error
    assert not Path(str(path)+'.session').exists()
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == 'Capture without lease storage'


@pytest.mark.parametrize('kind', ['file', 'directory'])
def test_unrecognized_marker_path_is_preserved_without_losing_publication(tmp_path, kind):
    project, doc, obj, image, primary = workspace(tmp_path)
    path = tmp_path/'new.qpxrecover'
    marker = Path(str(path)+'.session')
    if kind == 'file':
        marker.write_bytes(b'keep this data')
    else:
        marker.mkdir()
    doc.rename_annotation(obj, 'Still publish')
    writer = q.ProjectAutosave()
    writer.retention = policy(1)
    assert writer.request(project, str(path))
    stats = writer.wait()
    assert stats.state == 'ready' and stats.retention_error and not stats.error
    assert marker.read_bytes() == b'keep this data' if kind == 'file' else marker.is_dir()
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == 'Still publish'


@pytest.mark.skipif(os.name != 'nt', reason='Windows file delete sharing')
def test_automatic_removal_error_preserves_publication_and_retries(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, current = tmp_path/'old.qpxrecover', tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    writer = q.ProjectAutosave()
    writer.retention = policy(1)
    doc.rename_annotation(obj, 'New publication')
    with old.open('rb'):
        assert writer.request(project, str(current))
        stats = writer.wait()
        assert stats.state == 'ready' and stats.retention_error and not stats.error
        assert stats.checkpoints_pruned == 0 and old.exists()
        assert q.Project.recover(str(current)).document(image).annotation(obj).name == 'New publication'
    doc.rename_annotation(obj, 'Retry collection')
    assert writer.request(project, str(current))
    stats = writer.wait()
    assert stats.state == 'ready' and not stats.retention_error and stats.checkpoints_pruned == 1
    assert not old.exists()


@pytest.mark.parametrize('gate', ['fresh-mtime', 'fresh-header', 'future-header', 'unmanaged', 'wrong-lease', 'oversized-marker', 'corrupt'])
def test_age_and_managed_ownership_gates_preserve_files(tmp_path, gate):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, current = tmp_path/'candidate.qpxrecover', tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    checkpoint(project, doc, obj, current)
    if gate == 'fresh-mtime':
        os.utime(old, None)
    elif gate in ['fresh-header', 'future-header']:
        stamp = old.stat()
        with sqlite3.connect(old) as db:
            db.execute('UPDATE recovery SET created_ms=?', (int((time.time()+(86400 if gate == 'future-header' else 0))*1000),))
        db.close()
        os.utime(old, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
    elif gate == 'unmanaged':
        Path(str(old)+'.session').unlink()
    elif gate == 'oversized-marker':
        Path(str(old)+'.session').write_bytes(b'x'*1024)
    elif gate == 'wrong-lease':
        Path(str(old)+'.session').write_bytes(b'not a managed session')
    else:
        old.write_bytes(b'not a checkpoint')
    before = old.read_bytes()
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 0
    assert old.read_bytes() == before


def test_unrelated_project_and_different_base_branch_are_preserved(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    branch = tmp_path/'old-base.qpxrecover'
    checkpoint(project, doc, obj, branch, 100)
    project.save(str(tmp_path/'branch.qpxproj'))
    current = tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, current)
    other = q.Project()
    other_doc = q.Document(q.Image.demo())
    other_obj = other_doc.add_rectangle(q.Rect(0, 0, 1, 1))
    other.add_document(other_doc)
    unrelated = tmp_path/'unrelated.qpxrecover'
    checkpoint(other, other_doc, other_obj, unrelated, 100)
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 0
    assert branch.exists() and unrelated.exists()


def test_bounded_oldest_first_batches_and_automatic_worker(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    paths = [tmp_path/f'old-{i}.qpxrecover' for i in range(5)]
    for path, days in zip(paths, [100, 90, 80, 70, 60]):
        checkpoint(project, doc, obj, path, days)
    writer = q.ProjectAutosave()
    writer.retention = policy(1, 2)
    current = tmp_path/'current.qpxrecover'
    doc.rename_annotation(obj, 'New checkpoint')
    assert writer.request(project, str(current))
    stats = writer.wait()
    assert stats.state == 'ready' and stats.checkpoints_pruned == 2 and stats.retention_ms > 0
    assert not stats.retention_error and [path.exists() for path in paths] == [False, False, True, True, True]
    assert q.Project.prune_recoveries(str(current), policy(1, 2)).removed == 2
    assert q.Project.prune_recoveries(str(current), policy(1, 2)).removed == 1
    assert current.exists()


@pytest.mark.parametrize('field,value', [('keep_latest', 0), ('keep_latest', 1025), ('max_deletions', 0),
                                        ('max_deletions', 129), ('max_age_days', 0), ('max_age_days', float('nan')),
                                        ('max_age_days', float('inf')), ('max_age_days', 36501)])
def test_policy_validation(field, value):
    config = policy()
    setattr(config, field, value)
    with pytest.raises(ValueError, match='retention'):
        config.validate()
    with pytest.raises(ValueError, match='retention'):
        q.ProjectAutosave().retention = config


def test_publication_lock_failure_is_reported_and_retryable(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, current = tmp_path/'old.qpxrecover', tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    checkpoint(project, doc, obj, current)
    with open(str(old)+'.lock', 'a+b') as held:
        if os.name != 'nt':
            import fcntl
            fcntl.flock(held.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = q.Project.prune_recoveries(str(current), policy(1))
        assert result.removed == 0 and result.failed == 1 and result.error
    assert old.exists()
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 1


def test_process_lease_survives_directory_move_and_releases_after_kill(tmp_path):
    root = tmp_path/'original'
    root.mkdir()
    project, doc, obj, image, primary = workspace(root)
    current, old = root/'current.qpxrecover', root/'old.qpxrecover'
    checkpoint(project, doc, obj, current)
    script = r'''
import sys,os,time
sys.path.insert(0,sys.argv[3])
import qupathxx as q
project=q.Project.open(sys.argv[1]); doc=project.document(project.active_image)
doc.rename_annotation(1,'Owned by live process')
writer=q.ProjectAutosave(); assert writer.request(project,sys.argv[2]) and writer.wait().state=='ready'
print(os.getpid(),flush=True)
sys.stdin.readline()
'''
    child = subprocess.Popen([sys._base_executable, '-I', '-c', script, str(primary), str(old),
                              str(Path(q.__file__).parent.parent)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        assert int(child.stdout.readline()) == child.pid
        age(old, 100)
        moved = tmp_path/'moved'
        root.rename(moved)
        old, current = moved/old.name, moved/current.name
        assert q.Project.prune_recoveries(str(current), policy(1)).removed == 0 and old.exists()
        child.kill()
        child.wait(timeout=5)
        assert q.Project.prune_recoveries(str(current), policy(1)).removed == 1
        assert not old.exists() and current.exists()
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=5)


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='requires desktop GUI')
def test_gui_retention_options_run_after_recovery_publication(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    before = primary.read_bytes()
    old = tmp_path/'expired.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    doc.rename_annotation(obj, 'Live captured work')
    current = tmp_path/'viewer.qpxrecover'
    options = q.GuiOptions()
    options.autosave_interval_seconds, options.autosave_path = 0, str(current)
    options.recovery_retention_enabled, options.recovery_keep_latest, options.recovery_max_age_days = True, 1, 7
    event = q.GuiInput()
    event.frame, event.kind = 5, q.GuiInputKind.AUTOSAVE_KEY
    options.inputs = [event]
    report = q.show(project, frame_limit=70, options=options)
    assert report.autosaves_completed == 1 and not report.autosave_error
    assert report.checkpoints_pruned == 1 and report.retention_ms > 0 and not report.retention_error
    assert not old.exists() and current.exists() and primary.read_bytes() == before
    assert doc.annotation(obj).name == 'Live captured work' and project.dirty


@contextmanager
def locked_headers(paths):
    script = r"""
import sqlite3,sys,os
connections=[]
for path in sys.argv[1:]:
    db=sqlite3.connect(path); db.execute('BEGIN EXCLUSIVE'); connections.append(db)
print(os.getpid(),flush=True)
sys.stdin.readline()
for db in connections: db.rollback(); db.close()
"""
    child = subprocess.Popen([sys._base_executable, '-I', '-c', script, *map(str, paths)],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        assert int(child.stdout.readline()) == child.pid
        yield
    finally:
        try:
            child.communicate(input='\n', timeout=5)
        except subprocess.TimeoutExpired:
            child.kill(); child.wait(timeout=5)


def test_scan_progress_cancel_preserves_publication_and_all_candidates_then_retries(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    before = primary.read_bytes()
    paths = [tmp_path/f'old-{i}.qpxrecover' for i in range(5)]
    for path in paths:
        checkpoint(project, doc, obj, path, 100)
    writer = q.ProjectAutosave()
    writer.retention = policy(1)
    current = tmp_path/'current.qpxrecover'
    doc.rename_annotation(obj, 'Captured before cancellation')
    with locked_headers(paths):
        assert writer.request(project, str(current))
        deadline = time.monotonic()+5
        while writer.busy:
            stats = writer.poll()
            if stats.checkpoint_published and stats.maintenance_phase == 'scanning' and stats.retention_inspected:
                break
            assert time.monotonic() < deadline
            time.sleep(.001)
        assert writer.busy and stats.checkpoint_published and stats.retention_inspected > 0
        assert stats.retention_removed == 0 and current.exists()
        started = time.monotonic()
        writer.cancel()
        stats = writer.wait()
        assert time.monotonic()-started < 2.5  # One existing SQLite busy wait can finish first.
        assert stats.state == 'ready' and stats.completed == 1 and stats.retention_cancelled
        assert stats.checkpoint_published and stats.cancel_requested and stats.maintenance_phase == 'cancelled'
        assert stats.retention_removed == 0 and stats.checkpoints_pruned == 0 and not stats.retention_error
        assert all(path.exists() for path in paths) and primary.read_bytes() == before
        assert q.Project.recover(str(current)).document(image).annotation(obj).name == 'Captured before cancellation'
    doc.rename_annotation(obj, 'Retry collection')
    assert writer.request(project, str(current))
    stats = writer.wait()
    assert stats.state == 'ready' and stats.completed == 2 and not stats.retention_cancelled
    assert stats.maintenance_phase == 'complete' and stats.retention_inspected == 6
    assert stats.checkpoints_pruned == stats.retention_removed == 5
    assert stats.retention_metadata_ms > 0 and stats.retention_ownership_ms > 0 and stats.retention_removal_ms > 0
    assert 0 < stats.retention_prefetch_peak <= 4
    assert stats.retention_metadata_ms+stats.retention_ownership_ms+stats.retention_removal_ms <= stats.retention_ms+1


def test_hard_link_alias_of_anchor_is_preserved_by_fast_path(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, anchor = tmp_path/'old.qpxrecover', tmp_path/'anchor.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    checkpoint(project, doc, obj, anchor, 10)
    alias = tmp_path/'zzz-alias.qpxrecover'
    os.link(anchor, alias)
    Path(str(alias)+'.session').write_bytes(Path(str(anchor)+'.session').read_bytes())
    before = anchor.read_bytes()
    result = q.Project.prune_recoveries(str(anchor), policy(1))
    assert result.removed == 1 and not old.exists()
    assert anchor.read_bytes() == alias.read_bytes() == before and not result.cancelled


def test_primary_hard_link_alias_still_matches_scope(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    old, current = tmp_path/'old.qpxrecover', tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, old, 100)
    alias = tmp_path/'alias.qpxproj'
    os.link(primary, alias)
    with sqlite3.connect(old) as db:
        db.execute('UPDATE recovery SET base_path=?', (str(alias),))
    db.close()
    age(old, 100)
    checkpoint(project, doc, obj, current)
    assert q.Project.prune_recoveries(str(current), policy(1)).removed == 1 and not old.exists()


@pytest.mark.skipif(os.name != 'nt', reason='Windows case insensitive anchor path')
def test_anchor_name_case_does_not_make_it_eligible_for_collection(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    anchor, recent, old = [tmp_path/(name+'.qpxrecover') for name in ['anchor', 'recent', 'old']]
    checkpoint(project, doc, obj, anchor, 10)
    checkpoint(project, doc, obj, recent)
    checkpoint(project, doc, obj, old, 100)
    before = anchor.read_bytes()
    result = q.Project.prune_recoveries(str(anchor.with_name(anchor.name.upper())), policy(1))
    assert result.removed == 1 and not old.exists() and anchor.read_bytes() == before and recent.exists()


def test_live_oldest_sessions_do_not_starve_inactive_candidates(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    live = [checkpoint(project, doc, obj, tmp_path/f'live-{i}.qpxrecover', 100+i, active=True) for i in range(4)]
    inactive = [tmp_path/f'inactive-{i}.qpxrecover' for i in range(2)]
    for path in inactive:
        checkpoint(project, doc, obj, path, 50)
    current = tmp_path/'current.qpxrecover'
    checkpoint(project, doc, obj, current)
    assert q.Project.prune_recoveries(str(current), policy(1, 2)).removed == 2
    assert all(not path.exists() for path in inactive)
    assert all((tmp_path/f'live-{i}.qpxrecover').exists() and not owner.busy for i, owner in enumerate(live))


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='requires desktop GUI')
def test_gui_stop_maintenance_keeps_published_work_and_underlying_undo(tmp_path):
    project, doc, obj, image, primary = workspace(tmp_path)
    before = primary.read_bytes()
    paths = [tmp_path/f'old-{i}.qpxrecover' for i in range(5)]
    for path in paths:
        checkpoint(project, doc, obj, path, 100)
    doc.rename_annotation(obj, 'Captured live work')
    current = tmp_path/'viewer.qpxrecover'
    options = q.GuiOptions()
    options.autosave_interval_seconds, options.autosave_path, options.recovery_path = 0, str(current), str(current)
    options.recovery_retention_enabled, options.recovery_keep_latest, options.recovery_max_age_days = True, 1, 7
    events = []
    for frame, kind in [(5, q.GuiInputKind.AUTOSAVE_KEY), (20, q.GuiInputKind.UNDO_KEY),
                        (40, q.GuiInputKind.STOP_RECOVERY_MAINTENANCE_KEY),
                        (55, q.GuiInputKind.STOP_RECOVERY_MAINTENANCE_KEY),
                        (70, q.GuiInputKind.STOP_RECOVERY_MAINTENANCE_KEY)]:
        event = q.GuiInput(); event.frame, event.kind = frame, kind; events.append(event)
    options.inputs = events
    with locked_headers(paths):
        report = q.show(project, frame_limit=95, capture_path=str(tmp_path/'retention-stopped.ppm'), options=options)
    assert report.autosaves_completed == 1 and not report.autosave_error
    assert report.retention_cancelled and report.retention_inspected > 0 and not report.retention_error
    assert report.checkpoints_pruned == 0 and all(path.exists() for path in paths)
    assert q.Project.recover(str(current)).document(image).annotation(obj).name == 'Captured live work'
    assert doc.annotation(obj).name == paths[-1].stem and project.dirty and primary.read_bytes() == before



def test_collection_cancellation_keeps_completed_removals_and_stops_later_candidates(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image(4, 4, [20, 40, 80]*16))
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    image = project.add_document(doc)
    primary = tmp_path/'primary.qpxproj'
    project.save(str(primary))
    before = primary.read_bytes()
    paths = [tmp_path/f'old-{i:03d}.qpxrecover' for i in range(128)]
    for i, path in enumerate(paths):
        checkpoint(project, doc, obj, path, 100+i/1000)
    writer = q.ProjectAutosave()
    writer.retention = policy(1, 128)
    current = tmp_path/'current.qpxrecover'
    doc.rename_annotation(obj, 'Published before collection')
    assert writer.request(project, str(current))
    deadline = time.monotonic()+15
    while writer.busy:
        stats = writer.poll()
        if stats.maintenance_phase == 'collecting' and stats.retention_removed > 0:
            break
        assert time.monotonic() < deadline
        time.sleep(.001)
    assert writer.busy and stats.maintenance_phase == 'collecting'
    assert 0 < stats.retention_removed < 128
    writer.cancel()
    stats = writer.wait()
    assert stats.state == 'ready' and stats.retention_cancelled and not stats.retention_error
    assert 0 < stats.retention_removed == stats.checkpoints_pruned < 128
    assert sum(not path.exists() for path in paths) == stats.retention_removed
    assert primary.read_bytes() == before
    assert q.Project.recover(str(current)).document(image).annotation(obj).name == 'Published before collection'



def test_cancelled_prefetch_releases_markers_before_directory_move_and_retry(tmp_path):
    root = tmp_path/'original'
    root.mkdir()
    project = q.Project()
    doc = q.Document(q.Image(4, 4, [20, 40, 80]*16))
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    image = project.add_document(doc)
    primary = root/'primary.qpxproj'
    project.save(str(primary))
    paths = [root/f'old-{i:03d}.qpxrecover' for i in range(64)]
    for path in paths:
        checkpoint(project, doc, obj, path, 100)
    writer = q.ProjectAutosave()
    writer.retention = policy(1)
    current = root/'current.qpxrecover'
    doc.rename_annotation(obj, 'Published with bounded prefetch')
    assert writer.request(project, str(current))
    deadline = time.monotonic()+10
    while writer.busy:
        stats = writer.poll()
        if stats.maintenance_phase == 'scanning' and stats.retention_inspected >= 8:
            break
        assert time.monotonic() < deadline
        time.sleep(.001)
    assert writer.busy and stats.checkpoint_published
    writer.cancel()
    stats = writer.wait()
    assert stats.retention_cancelled and stats.state == 'ready' and stats.retention_removed == 0
    assert 0 < stats.retention_prefetch_peak <= 4
    moved = tmp_path/'moved'
    root.rename(moved)  # No reader handle can outlive worker acknowledgement.
    current = moved/current.name
    result = q.Project.prune_recoveries(str(current), policy(1, 128))
    assert result.removed == 64 and not result.error and not result.cancelled
    assert 0 < result.marker_prefetch_peak <= 4
    assert q.Project.recover(str(current)).document(image).annotation(obj).name == 'Published with bounded prefetch'



def test_unleased_overwrite_invalidates_old_matching_marker_and_can_regain_management(tmp_path, monkeypatch):
    left, right = tmp_path/'left', tmp_path/'right'
    left.mkdir(); right.mkdir()
    project, doc, obj, image, primary = workspace(left)
    other, other_doc, other_obj, other_image, other_primary = workspace(right)
    path, other_path = left/'owned.qpxrecover', right/'owned.qpxrecover'
    writer = q.ProjectAutosave()
    doc.rename_annotation(obj, 'First leased publication')
    assert writer.request(project, str(path)) and writer.wait().state == 'ready'
    other_doc.rename_annotation(other_obj, 'Switch authority')
    assert writer.request(other, str(other_path)) and writer.wait().state == 'ready'
    blocked = tmp_path/'blocked-state'
    blocked.write_bytes(b'not a directory')
    with monkeypatch.context() as env:
        env.setenv('LOCALAPPDATA' if os.name == 'nt' else 'XDG_STATE_HOME', str(blocked))
        doc.rename_annotation(obj, 'Published without a lease')
        assert writer.request(project, str(path))
        stats = writer.wait()
        assert stats.state == 'ready' and stats.retention_error and not stats.error
    marker = Path(str(path)+'.session')
    assert marker.read_bytes().startswith(b'qupathxx recovery unmanaged v1\n')
    age(path, 100)
    anchor = left/'anchor.qpxrecover'
    checkpoint(project, doc, obj, anchor)
    assert q.Project.prune_recoveries(str(anchor), policy(1)).removed == 0 and path.exists()
    doc.rename_annotation(obj, 'Managed again')
    assert writer.request(project, str(path))
    stats = writer.wait()
    assert stats.state == 'ready' and not stats.retention_error
    assert marker.read_bytes().startswith(b'qupathxx recovery lease v1\n')
    age(path, 100)
    assert q.Project.prune_recoveries(str(anchor), policy(1)).removed == 0
    del writer
    gc.collect()
    assert q.Project.prune_recoveries(str(anchor), policy(1)).removed == 1 and not path.exists()



@pytest.mark.skipif(os.name != 'nt', reason='Windows read-only marker write protection')
def test_unleased_overwrite_does_not_publish_if_matching_tag_cannot_be_invalidated(tmp_path, monkeypatch):
    import stat
    left, right = tmp_path/'left', tmp_path/'right'
    left.mkdir(); right.mkdir()
    project, doc, obj, image, primary = workspace(left)
    other, other_doc, other_obj, other_image, other_primary = workspace(right)
    path, other_path = left/'owned.qpxrecover', right/'owned.qpxrecover'
    writer = q.ProjectAutosave()
    doc.rename_annotation(obj, 'Previous valid recovery')
    assert writer.request(project, str(path)) and writer.wait().state == 'ready'
    before = path.read_bytes()
    other_doc.rename_annotation(other_obj, 'Switch authority')
    assert writer.request(other, str(other_path)) and writer.wait().state == 'ready'
    blocked = tmp_path/'blocked-state'
    blocked.write_bytes(b'not a directory')
    marker = Path(str(path)+'.session')
    marker.chmod(stat.S_IREAD)
    try:
        monkeypatch.setenv('LOCALAPPDATA', str(blocked))
        doc.rename_annotation(obj, 'Unprotected new work')
        assert writer.request(project, str(path))
        stats = writer.wait()
        assert stats.state == 'error' and not stats.checkpoint_published
        assert path.read_bytes() == before and project.dirty
    finally:
        marker.chmod(stat.S_IREAD | stat.S_IWRITE)
