"""Reviewed checkpoint deletion checks immutable content under the writer lock."""
import os
from pathlib import Path
import sqlite3

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage disabled')


def checkpoint(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(1, 1, 10, 10), 'Original')
    primary = tmp_path/'primary.qpxproj'
    project.save(str(primary))
    doc.rename_annotation(obj, 'Captured A')
    writer = q.ProjectAutosave()
    path = tmp_path/'review.qpxrecover'
    assert writer.request(project, str(path)) and writer.wait().state == 'ready'
    return project, doc, image, obj, primary, writer, path


def test_reviewed_delete_removes_only_checkpoint_and_owner_can_publish_again(tmp_path):
    project, doc, image, obj, primary, writer, path = checkpoint(tmp_path)
    before, checkpoint_bytes = primary.read_bytes(), path.read_bytes()
    assert q.Project.inspect_recovery(str(path)).fingerprint == ''
    reviewed = q.Project.review_recovery(str(path))
    assert reviewed.fingerprint and q.Project.review_recovery(str(path)).fingerprint == reviewed.fingerprint
    assert path.read_bytes() == checkpoint_bytes and reviewed.objects == 1 and reviewed.images == 1
    q.Project.discard_recovery_checked(reviewed)
    assert not path.exists() and primary.read_bytes() == before and doc.annotation(obj).name == 'Captured A'
    assert writer.request(project, str(path)) and writer.wait().state == 'ready'
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == 'Captured A'


def test_same_header_size_and_mtime_cannot_hide_changed_checkpoint_content(tmp_path):
    project, doc, image, obj, primary, writer, path = checkpoint(tmp_path)
    reviewed, previous_stat = q.Project.review_recovery(str(path)), path.stat()
    doc.rename_annotation(obj, 'Captured B')
    assert writer.request(project, str(path)) and writer.wait().state == 'ready'
    db = sqlite3.connect(path)
    db.execute('UPDATE recovery SET created_ms=?', (reviewed.created_unix_ms,))
    db.commit()
    db.close()
    os.utime(path, ns=(previous_stat.st_atime_ns, previous_stat.st_mtime_ns))
    current = q.Project.review_recovery(str(path))
    assert (current.identity, current.session, current.created_unix_ms, current.images, current.objects) == (
        reviewed.identity, reviewed.session, reviewed.created_unix_ms, reviewed.images, reviewed.objects)
    assert path.stat().st_size == previous_stat.st_size
    assert current.fingerprint != reviewed.fingerprint
    with pytest.raises(RuntimeError, match='changed since review'):
        q.Project.discard_recovery_checked(reviewed)
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == 'Captured B'
    q.Project.discard_recovery_checked(current)
    assert not path.exists()


def test_inspection_without_content_review_cannot_authorize_checked_deletion(tmp_path):
    project, doc, image, obj, primary, writer, path = checkpoint(tmp_path)
    info = q.Project.inspect_recovery(str(path))
    with pytest.raises(ValueError, match='review'):
        q.Project.discard_recovery_checked(info)
    assert path.exists()
    for target in [primary, tmp_path/'missing.qpxrecover', tmp_path/'corrupt.qpxrecover']:
        if target.name == 'corrupt.qpxrecover':
            target.write_bytes(b'not a database')
        before = target.read_bytes() if target.exists() else None
        with pytest.raises((ValueError, RuntimeError)):
            q.Project.review_recovery(str(target))
        assert (target.read_bytes() if target.exists() else None) == before


def test_review_and_delete_obey_existing_writer_lock(tmp_path):
    project, doc, image, obj, primary, writer, path = checkpoint(tmp_path)
    reviewed = q.Project.review_recovery(str(path))
    before = path.read_bytes()
    with open(str(path)+'.lock', 'a+b') as held:
        if os.name != 'nt':
            import fcntl
            fcntl.flock(held.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        for operation in [lambda: q.Project.review_recovery(str(path)), lambda: q.Project.discard_recovery_checked(reviewed)]:
            with pytest.raises(RuntimeError, match='locked'):
                operation()
    assert path.read_bytes() == before
    q.Project.discard_recovery_checked(reviewed)
    assert not path.exists()


@pytest.mark.skipif(os.name != 'nt', reason='Windows file sharing check')
def test_failed_shared_handle_removal_preserves_checkpoint_for_retry(tmp_path):
    import ctypes
    from ctypes import wintypes
    project, doc, image, obj, primary, writer, path = checkpoint(tmp_path)
    reviewed, before = q.Project.review_recovery(str(path)), path.read_bytes()
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    create = kernel.CreateFileW
    create.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                       wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    create.restype = wintypes.HANDLE
    close = kernel.CloseHandle
    close.argtypes, close.restype = [wintypes.HANDLE], wintypes.BOOL
    # Share reads/writes for validation, but deny deleting/replacing this inode.
    handle = create(str(path), 0x80000000, 3, None, 3, 128, None)
    assert handle not in (None, ctypes.c_void_p(-1).value)
    try:
        with pytest.raises(RuntimeError):
            q.Project.discard_recovery_checked(reviewed)
        assert path.read_bytes() == before
    finally:
        assert close(handle)
    q.Project.discard_recovery_checked(reviewed)
    assert not path.exists()
