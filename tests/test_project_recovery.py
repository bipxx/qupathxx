"""Asynchronous native recovery, conflicts and failure preservation."""
import hashlib
import os
from pathlib import Path
import shutil
import sqlite3
import stat
import subprocess
import sys

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason="requires project storage")


def workspace(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image(32, 32, [30, 60, 90]*32*32))
    image = project.add_document(doc, "Embedded")
    obj = doc.add_rectangle(q.Rect(0, 0, 20, 20), "Original")
    primary = tmp_path / "primary.qpxproj"
    project.save(str(primary))
    doc.rename_annotation(obj, "Captured")
    return project, doc, image, obj, primary


def ready(autosave):
    result = autosave.wait()
    assert result.state == "ready", result.error
    return result


def test_snapshot_isolation_and_primary_saved_state_remain_unchanged(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    before = primary.read_bytes()
    view = q.ProjectView()
    view.center, view.zoom = q.Point(10, 15), 2.5
    project.set_view(image, view)
    cell = doc.add_cell(q.Roi.ellipse(q.Rect(2, 2, 10, 10)), q.Roi.rectangle(q.Rect(3, 3, 2, 2)), parent_id=obj, class_name="Tumor")
    doc.set_class("Tumor", 0x123456)
    doc.set_calibration(.5, .25)
    source = tmp_path / "pixels.ppm"
    source.write_bytes(b"P6\n8 8\n255\n" + bytes([40, 80, 160])*64)
    second = project.add_image(str(source))
    directory = tmp_path / "other-directory"
    directory.mkdir()
    path = directory / "\u6062\u590d.qpxrecover"
    autosave = q.ProjectAutosave()
    assert autosave.request(project, str(path)) and autosave.busy
    doc.rename_annotation(obj, "Later unsaved edit")
    doc.set_class("Tumor", 0xabcdef)
    doc.set_calibration(2, 2)
    doc.update_roi(cell, q.Roi.rectangle(q.Rect(2, 2, 10, 10)))
    project.rename_image(image, "Later image name")
    project.remove_image(second)
    result = ready(autosave)
    assert result.snapshot_bytes > 0 and result.capture_ms >= 0 and result.write_ms > 0
    assert project.path == str(primary) and project.generation == 1 and project.dirty
    assert primary.read_bytes() == before and q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"
    info = q.Project.inspect_recovery(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA application_id").fetchone()[0] == 0x51505258
    db.close()
    assert info.identity == project.identity and info.session == autosave.session
    assert info.base_path == str(primary) and info.base_generation == 1 and info.base_status == "matching"
    assert info.images == 2 and info.objects == 2 and info.created_unix_ms > 0
    restored = q.Project.recover(str(path))
    assert restored.dirty and restored.generation == 1 and restored.path == str(primary)
    assert restored.recovery_source == str(path)
    assert restored.document(image).annotation(obj).name == "Captured"
    assert restored.document(image).annotation(cell).geometry.kind == q.RoiKind.ELLIPSE
    assert restored.document(image).annotation(cell).nucleus.area == 4
    assert restored.document(image).object_class("Tumor").color == 0x123456
    assert restored.document(image).pixel_width_um == .5 and restored.view(image).zoom == 2.5
    assert restored.images[0].name == "Embedded"
    region = q.RegionRequest(0, 0, 0, 1, 1)
    assert list(restored.document(second).server.read_region(region).data)[:3] == [40, 80, 160]
    with pytest.raises(ValueError, match="recover"):
        q.Project.open(str(path))
    with pytest.raises(ValueError, match="recovery"):
        project.save(str(path))
    assert not list(directory.glob("*.work-*"))


def test_one_job_unchanged_skip_foreign_session_and_clear(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    path = tmp_path / "state.qpxrecover"
    assert autosave.request(project, str(path))
    assert not autosave.request(project, str(path))
    assert ready(autosave).completed == 1
    before = path.read_bytes()
    assert not autosave.request(project, str(path)) and path.read_bytes() == before
    other = q.ProjectAutosave()
    assert other.request(project, str(path))
    failure = other.wait()
    assert failure.state == "error" and "session" in failure.error
    with pytest.raises(ValueError, match="identity"):
        q.Project.discard_recovery(str(path), "0"*32)
    assert path.read_bytes() == before
    doc.rename_annotation(obj, "Second snapshot")
    assert autosave.request(project, str(path))
    assert ready(autosave).completed == 2
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == "Second snapshot"
    autosave.clear()
    assert autosave.wait().state == "idle" and not path.exists()
    assert project.dirty and project.generation == 1
    assert autosave.request(project, str(path))
    ready(autosave)
    q.Project.discard_recovery(str(path), project.identity)
    assert autosave.request(project, str(path))  # Missing published file is recreated.
    ready(autosave)
    autosave.clear()


def test_clean_project_and_snapshot_budget_do_not_write_files(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    project.save(str(primary))
    autosave = q.ProjectAutosave()
    path = tmp_path / "none.qpxrecover"
    assert not autosave.request(project, str(path)) and not path.exists()
    doc.rename_annotation(obj, "Dirty")
    tiny = q.ProjectAutosave(snapshot_budget=1)
    with pytest.raises(ValueError, match="budget"):
        tiny.request(project, str(path))
    assert not path.exists() and not list(tmp_path.glob("*.work-*")) and project.dirty
    with pytest.raises(ValueError):
        q.ProjectAutosave(snapshot_budget=0)


def test_paths_and_unrelated_destinations_are_preserved(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    with pytest.raises(ValueError):
        autosave.request(project, str(primary))
    alias = tmp_path / "alias.qpxrecover"
    os.link(primary, alias)
    before = primary.read_bytes()
    with pytest.raises(ValueError, match="primary"):
        autosave.request(project, str(alias))
    source = tmp_path / "source.qpxrecover"
    source.write_bytes(b"P6\n8 8\n255\n" + bytes([30, 60, 90])*64)
    project.add_image(str(source), reader="ppm")
    with pytest.raises(ValueError, match="source"):
        autosave.request(project, str(source))
    unrelated = tmp_path / "unrelated.qpxrecover"
    unrelated.write_text("Keep this unrelated file", encoding="utf-8")
    assert autosave.request(project, str(unrelated))
    assert autosave.wait().state == "error"
    assert unrelated.read_text() == "Keep this unrelated file" and primary.read_bytes() == before
    assert not list(tmp_path.glob("*.work-*"))


def test_failed_background_write_is_retryable_and_does_not_clean_primary(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    directory = tmp_path / "absent"
    path = directory / "state.qpxrecover"
    assert autosave.request(project, str(path))
    result = autosave.wait()
    assert result.state == "error" and result.error and project.dirty and project.generation == 1
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"
    directory.mkdir()
    assert autosave.request(project, str(path))
    ready(autosave)


@pytest.mark.skipif(os.name != "nt", reason="Windows read-only replacement failure")
def test_failed_atomic_replacement_keeps_previous_recovery(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    path = tmp_path / "previous.qpxrecover"
    autosave.request(project, str(path))
    ready(autosave)
    before = path.read_bytes()
    doc.rename_annotation(obj, "Newer work")
    os.chmod(path, stat.S_IREAD)
    try:
        assert autosave.request(project, str(path))
        result = autosave.wait()
        assert result.state == "error" and "replacement" in result.error
        assert path.read_bytes() == before
        assert q.Project.recover(str(path)).document(image).annotation(obj).name == "Captured"
    finally:
        os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
    assert not list(tmp_path.glob("*.work-*"))
    assert autosave.request(project, str(path))
    ready(autosave)
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == "Newer work"


def test_changed_primary_requires_save_as_for_recovered_branch(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    path = tmp_path / "branch.qpxrecover"
    autosave.request(project, str(path))
    ready(autosave)
    doc.rename_annotation(obj, "New primary")
    project.save(str(primary))
    assert q.Project.inspect_recovery(str(path)).base_status == "changed"
    recovered = q.Project.recover(str(path))
    with pytest.raises(RuntimeError, match="changed"):
        recovered.save(str(primary))
    assert q.Project.open(str(primary)).document(image).annotation(obj).name == "New primary"
    alternative = tmp_path / "recovered-branch.qpxproj"
    recovered.save(str(alternative))
    assert q.Project.open(str(alternative)).document(image).annotation(obj).name == "Captured"


def test_unsaved_default_cache_survives_restart(tmp_path, monkeypatch):
    monkeypatch.setenv("LOCALAPPDATA" if os.name == "nt" else "XDG_STATE_HOME", str(tmp_path))
    project = q.Project()
    image = project.add_document(q.Document(q.Image(4, 4, [40, 80, 160]*16)))
    project.document(image).add_rectangle(q.Rect(0, 0, 2, 2), "Unsaved work")
    autosave = q.ProjectAutosave()
    path = Path(autosave.default_path(project))
    assert path.is_relative_to(tmp_path)
    assert autosave.request(project)
    ready(autosave)
    info = q.Project.inspect_recovery(str(path))
    assert info.base_status == "unsaved" and info.base_generation == 0
    assert any(item.path == str(path) for item in q.Project.find_recoveries())
    restored = q.Project.recover(str(path))
    assert restored.path == "" and restored.generation == 0 and restored.dirty
    target = tmp_path / "first-save.qpxproj"
    restored.save(str(target))
    assert q.Project.open(str(target)).document(image).annotation(1).name == "Unsaved work"
    autosave.clear()


def test_discovery_and_invalid_recovery_metadata(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    first, second = q.ProjectAutosave(), q.ProjectAutosave()
    first.request(project)
    second.request(project)
    ready(first)
    ready(second)
    corrupt = tmp_path / (primary.name + ".recovery-corrupt.qpxrecover")
    corrupt.write_bytes(b"invalid")
    items = q.Project.find_recoveries(str(primary))
    assert {item.path for item in items} == {first.stats.path, second.stats.path, str(corrupt)}
    assert next(item for item in items if item.path == str(corrupt)).error
    assert len(q.Project.find_recoveries(str(primary), limit=1)) == 1
    assert q.Project.find_recoveries(str(primary), limit=1)[0].created_unix_ms == max(item.created_unix_ms for item in items)
    damaged = tmp_path / "bad-header.qpxrecover"
    shutil.copyfile(first.stats.path, damaged)
    with sqlite3.connect(damaged) as db:
        db.execute("UPDATE recovery SET object_count=999")
    with pytest.raises(ValueError, match="metadata"):
        q.Project.inspect_recovery(str(damaged))
    with pytest.raises(ValueError):
        q.Project.recover(str(damaged))
    with pytest.raises(ValueError):
        q.Project.find_recoveries(str(primary), limit=0)


def test_v1_primary_not_migrated_by_autosave_and_restore_is_atomic(tmp_path):
    fixture = Path(__file__).parent / "fixtures/project-v1.qpxproj"
    primary = tmp_path / "legacy.qpxproj"
    shutil.copyfile(fixture, primary)
    before = hashlib.sha256(primary.read_bytes()).digest()
    project = q.Project.open(str(primary))
    old_doc = project.document(1)
    old_doc.rename_annotation(1, "Captured legacy")
    autosave = q.ProjectAutosave()
    path = tmp_path / "legacy.qpxrecover"
    autosave.request(project, str(path))
    ready(autosave)
    old_doc.rename_annotation(1, "Later live legacy")
    assert hashlib.sha256(primary.read_bytes()).digest() == before
    project.restore_recovery(str(path))
    assert project.document(1).annotation(1).name == "Captured legacy" and project.dirty
    assert old_doc.annotation(1).name == "Later live legacy"
    with pytest.raises((RuntimeError, ValueError)):
        project.restore_recovery(str(tmp_path / "missing.qpxrecover"))
    assert project.document(1).annotation(1).name == "Captured legacy"


def test_recovery_survives_abnormal_process_exit_and_releases_lock(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    path = tmp_path / "crash.qpxrecover"
    code = "import os,qupathxx as q,sys; p=q.Project.open(sys.argv[1]); p.document(1).rename_annotation(1,'Crash work'); a=q.ProjectAutosave(); a.request(p,sys.argv[2]); s=a.wait(); assert s.state=='ready',s.error; os._exit(17)"
    result = subprocess.run([sys.executable, "-I", "-c", code, str(primary), str(path)], timeout=30)
    assert result.returncode == 17
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == "Crash work"
    q.Project.discard_recovery(str(path), project.identity)
    assert not path.exists()


def test_termination_around_replacement_keeps_a_whole_snapshot(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    before = primary.read_bytes()
    path = tmp_path / "interrupted.qpxrecover"
    code = """
import qupathxx as q,sys,time
p=q.Project.open(sys.argv[1]); d=p.document(1); d.set_history_limits(0,0)
d.rename_annotation(1,'Before write'); a=q.ProjectAutosave()
a.request(p,sys.argv[2]); s=a.wait(); assert s.state=='ready',s.error
print('previous-ready',flush=True)
d.execute([q.ObjectEdit.create(q.Rect(i%32,(i//32)%32,1,1)) for i in range(100000)])
assert a.request(p,sys.argv[2]); print('writing',flush=True)
time.sleep(30)
"""
    process = subprocess.Popen([sys.executable, "-I", "-c", code, str(primary), str(path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert process.stdout.readline().strip() == "previous-ready"
        assert process.stdout.readline().strip() == "writing"
        assert process.poll() is None
        process.terminate()
        process.communicate(timeout=30)
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=30)
    recovered = q.Project.recover(str(path))
    assert recovered.document(image).object_count in (1, 100001)
    assert recovered.document(image).annotation(obj).name == "Before write"
    assert primary.read_bytes() == before
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA quick_check").fetchone()[0] == "ok"
    db.close()  # sqlite3's context manager ends a transaction, not the connection.
    q.Project.discard_recovery(str(path), project.identity)


def test_cancel_preserves_previous_or_completed_snapshot(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(i%32, (i//32)%32, 1, 1)) for i in range(100000)])
    autosave = q.ProjectAutosave()
    path = tmp_path / "cancel.qpxrecover"
    autosave.request(project, str(path))
    ready(autosave)
    before = path.read_bytes()
    doc.rename_annotation(obj, "After cancellation request")
    autosave.request(project, str(path))
    autosave.cancel()
    result = autosave.wait()
    assert result.state in ("cancelled", "ready")  # Publication can win the cancellation race.
    if result.state == "cancelled":
        assert path.read_bytes() == before
    recovered = q.Project.recover(str(path))
    assert recovered.document(image).object_count == 100001
    assert recovered.document(image).annotation(obj).name in ("Captured", "After cancellation request")
    assert project.dirty and project.generation == 1


def test_closed_missing_link_does_not_block_canonical_recovery(tmp_path):
    source = tmp_path / "missing.ppm"
    source.write_bytes(b"P6\n8 8\n255\n" + bytes([30, 60, 90])*64)
    project = q.Project()
    image = project.add_image(str(source))
    doc = project.document(image)
    doc.add_rectangle(q.Rect(0, 0, 2, 2), "No pixels needed")
    doc.server.close()
    source.unlink()
    autosave = q.ProjectAutosave()
    path = tmp_path / "missing.qpxrecover"
    assert autosave.request(project, str(path))
    ready(autosave)
    restored = q.Project.recover(str(path))
    assert restored.document(image).annotation(1).name == "No pixels needed" and not restored.images[0].available


def test_request_does_not_query_locked_recovery_database_on_owner_thread(tmp_path):
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    path = tmp_path / "locked.qpxrecover"
    autosave.request(project, str(path))
    ready(autosave)
    doc.rename_annotation(obj, "Next capture")
    db = sqlite3.connect(path)
    try:
        db.execute("BEGIN EXCLUSIVE")
        # SQLite reads are blocked until this owner thread releases the lock.
        # request must return with its worker in flight, rather than reading here.
        assert autosave.request(project, str(path)) and autosave.busy
        db.rollback()
    finally:
        db.close()
    ready(autosave)
    assert q.Project.recover(str(path)).document(image).annotation(obj).name == "Next capture"


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="requires real desktop GUI")
def test_recovery_launchers_render_unsaved_snapshot(tmp_path):
    import qupathxx._native as native
    project, doc, image, obj, primary = workspace(tmp_path)
    autosave = q.ProjectAutosave()
    path = tmp_path / "launch.qpxrecover"
    autosave.request(project, str(path))
    ready(autosave)
    subprocess.run([sys.executable, "-I", "-m", "qupathxx", "--recover", str(path), "--smoke"], check=True, timeout=30)
    executable = Path(native.__file__).parent / ("qupathxx_app.exe" if os.name == "nt" else "qupathxx_app")
    assert executable.exists()
    subprocess.run([str(executable), "--recover", str(path), "--smoke"], check=True, timeout=30)
    assert project.dirty and q.Project.open(str(primary)).document(image).annotation(obj).name == "Original"
