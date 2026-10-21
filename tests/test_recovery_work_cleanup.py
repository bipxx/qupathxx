"""Crash debris is removed only under the publication lock and with provenance."""
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage disabled')


def work_directory(target, token='a'*32):
    work = Path(str(target)+'.work-'+token)
    work.mkdir()
    (work/'owner').write_bytes(b'qupathxx recovery work v1\n'+token.encode()+b'\n')
    return work


def test_cleanup_removes_known_files_and_preserves_primary_and_checkpoint(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1), 'Original')
    project.add_document(doc)
    primary = tmp_path/'original.qpxproj'
    project.save(str(primary))
    doc.rename_annotation(obj, 'Captured')
    target = tmp_path/'snapshot.qpxrecover'
    writer = q.ProjectAutosave()
    assert writer.request(project, str(target)) and writer.wait().state == 'ready'
    before = primary.read_bytes(), target.read_bytes()
    for token in ['a'*32, 'b'*32]:
        work = work_directory(target, token)
        for suffix in ['', '-journal', '-wal', '-shm']:
            (work/('snapshot.qpxproj'+suffix)).write_bytes(b'interrupted')
    assert q.Project.cleanup_recovery_work(str(target)) == 2
    assert not list(tmp_path.glob('*.work-*'))
    assert q.Project.cleanup_recovery_work(str(target)) == 0
    assert (primary.read_bytes(), target.read_bytes()) == before
    assert project.dirty and doc.annotation(obj).name == 'Captured'


@pytest.mark.parametrize('kind', ['unmarked', 'wrong-marker', 'extra-file', 'nested', 'wrong-token', 'other-target'])
def test_unrecognized_work_is_preserved_without_partial_deletion(tmp_path, kind):
    target = tmp_path/'snapshot.qpxrecover'
    work = work_directory(target if kind != 'other-target' else tmp_path/'other.qpxrecover',
                          'g'*32 if kind == 'wrong-token' else 'a'*32)
    snapshot = work/'snapshot.qpxproj'
    snapshot.write_bytes(b'preserve')
    if kind == 'unmarked':
        (work/'owner').unlink()
    elif kind == 'wrong-marker':
        (work/'owner').write_bytes(b'not the owner')
    elif kind == 'extra-file':
        (work/'notes.txt').write_text('keep')
    elif kind == 'nested':
        (work/'nested').mkdir()
    assert q.Project.cleanup_recovery_work(str(target)) == 0
    assert snapshot.read_bytes() == b'preserve'
    assert work.is_dir()


@pytest.mark.parametrize('entry', ['directory', 'snapshot', 'owner'])
def test_links_are_not_followed(tmp_path, entry):
    target = tmp_path/'snapshot.qpxrecover'
    outside = tmp_path/'outside'
    outside.mkdir()
    external = outside/('snapshot.qpxproj' if entry == 'directory' else 'data')
    external.write_bytes(b'keep')
    if entry == 'directory':
        (outside/'owner').write_bytes(b'qupathxx recovery work v1\n'+b'a'*32+b'\n')
    work = work_directory(target)
    (work/'snapshot.qpxproj').write_bytes(b'keep snapshot')
    link = work if entry == 'directory' else work/('snapshot.qpxproj' if entry == 'snapshot' else 'owner')
    if entry == 'directory':
        # A junction does not need Windows developer-mode symlink privileges.
        (work/'snapshot.qpxproj').unlink()
        (work/'owner').unlink()
        work.rmdir()
        if os.name == 'nt':
            subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(outside)], check=True, capture_output=True)
        else:
            link.symlink_to(outside, target_is_directory=True)
    else:
        link.unlink()
        try:
            link.symlink_to(external)
        except OSError:
            pytest.skip('file symlink creation unavailable')
    try:
        assert q.Project.cleanup_recovery_work(str(target)) == 0
        assert external.read_bytes() == b'keep'
        assert work.exists()
    finally:
        if entry == 'directory':
            if os.name == 'nt':
                link.rmdir()  # Remove this junction only, never traverse it.
            else:
                link.unlink()


def test_cleanup_obeys_writer_lock_and_rejects_wrong_destination_type(tmp_path):
    target = tmp_path/'snapshot.qpxrecover'
    work = work_directory(target)
    with open(str(target)+'.lock', 'a+b') as held:
        if os.name != 'nt':
            import fcntl
            fcntl.flock(held.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        with pytest.raises(RuntimeError, match='locked'):
            q.Project.cleanup_recovery_work(str(target))
    assert work.exists()
    for path in ['', str(tmp_path/'primary.qpxproj')]:
        with pytest.raises(ValueError):
            q.Project.cleanup_recovery_work(path)
    assert q.Project.cleanup_recovery_work(str(target)) == 1


@pytest.mark.skipif(os.name != 'nt', reason='Windows delete-sharing failure')
def test_cleanup_failure_does_not_block_publication_and_retries(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1), 'First')
    project.add_document(doc)
    target = tmp_path/'snapshot.qpxrecover'
    work = work_directory(target)
    snapshot = work/'snapshot.qpxproj'
    snapshot.write_bytes(b'held')
    writer = q.ProjectAutosave()
    with snapshot.open('rb'):
        assert writer.request(project, str(target))
        stats = writer.wait()
        assert stats.state == 'ready' and stats.work_cleanup_error
        assert stats.work_directories_cleaned == 0
        assert snapshot.read_bytes() == b'held' and (work/'owner').exists()
    doc.rename_annotation(obj, 'Retry')
    assert writer.request(project, str(target))
    stats = writer.wait()
    assert stats.state == 'ready' and not stats.work_cleanup_error
    assert stats.work_directories_cleaned == 1 and not work.exists()


def test_automatic_cleanup_is_bounded_and_explicit_sweep_finishes(tmp_path):
    target = tmp_path/'snapshot.qpxrecover'
    works = [work_directory(target, f'{i:032x}') for i in range(40)]
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.add_rectangle(q.Rect(0, 0, 1, 1))
    project.add_document(doc)
    writer = q.ProjectAutosave()
    assert writer.request(project, str(target))
    stats = writer.wait()
    assert stats.state == 'ready' and not stats.work_cleanup_error
    assert stats.work_directories_cleaned == 32
    assert sum(work.exists() for work in works) == 8
    assert q.Project.recover(str(target)).document(project.active_image).object_count == 1
    assert q.Project.cleanup_recovery_work(str(target)) == 8
    assert not any(work.exists() for work in works)


@pytest.mark.parametrize('automatic', [False, True])
def test_killed_native_autosave_leaves_reclaimable_work(tmp_path, automatic):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1), 'Original')
    image = project.add_document(doc)
    primary = tmp_path/'primary.qpxproj'
    project.save(str(primary))
    before = primary.read_bytes()
    target = tmp_path/'killed.qpxrecover'
    script = r'''
import sys,os
sys.path.insert(0,sys.argv[3])
import qupathxx as q
print(os.getpid(),flush=True)
project=q.Project.open(sys.argv[1]); doc=project.document(project.active_image)
doc.set_history_limits(0,0)
doc.execute([q.ObjectEdit.create(q.Rect(0,0,1,1),str(i)) for i in range(200000)])
job=q.ProjectAutosave(); assert job.request(project,sys.argv[2])
assert job.wait().state=='ready'
'''
    # Windows venv python.exe is a redirector; kill the actual interpreter.
    child = subprocess.Popen([sys._base_executable, '-I', '-c', script, str(primary), str(target),
                              str(Path(q.__file__).parent.parent)], stdout=subprocess.PIPE, text=True)
    work = None
    try:
        assert int(child.stdout.readline()) == child.pid
        deadline = time.monotonic()+20
        while child.poll() is None and time.monotonic() < deadline:
            for candidate in tmp_path.glob(target.name+'.work-*'):
                if (candidate/'owner').exists() and (candidate/'snapshot.qpxproj').exists():
                    work = candidate
                    break
            if work is not None:
                break
            time.sleep(.001)
        assert work is not None, 'native writer did not expose an in-flight snapshot'
        with pytest.raises(RuntimeError, match='locked'):
            q.Project.cleanup_recovery_work(str(target))
        child.kill()
        child.wait(timeout=5)
        assert work.is_dir() and not target.exists()
        if automatic:
            doc.rename_annotation(obj, 'Next session')
            writer = q.ProjectAutosave()
            assert writer.request(project, str(target))
            stats = writer.wait()
            assert stats.state == 'ready' and not stats.work_cleanup_error
            assert stats.work_directories_cleaned == 1
            assert q.Project.recover(str(target)).document(image).annotation(obj).name == 'Next session'
        else:
            assert q.Project.cleanup_recovery_work(str(target)) == 1
            assert not target.exists()
        assert not work.exists() and primary.read_bytes() == before
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=5)
