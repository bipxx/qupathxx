"""Durable ROI provenance, strong input checks and transactional migration."""
import hashlib
import sqlite3
import struct

import numpy as np
import pytest
import qupathxx as q


def project_with_record(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image(32, 24, [10, 20, 30]*32*24))
    image = project.add_document(doc)
    obj = doc.add_cell(q.Roi.rectangle(q.Rect(1, 2, 23, 19)), q.Roi.rectangle(q.Rect(3, 4, 4, 5)))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]], [250, 240, 230])
    request = q.AnalysisRequest(doc, obj, transform, tile_size=16)
    result = q.analyze(request)
    record = doc.keep_analysis(request, result)
    return project, doc, image, obj, record, tmp_path/"analysis.qpxproj"


def test_digest_matches_independent_hashlib_stream():
    rgb = np.random.default_rng(15).integers(0, 256, (27, 35, 3), dtype=np.uint8)
    doc = q.Document(q.Image(35, 27, rgb.ravel().tolist()))
    obj = doc.add_rectangle(q.Rect(2.2, 3.1, 29.5, 21.6))
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), tile_size=16)
    result = q.analyze(request)
    digest = hashlib.sha256(b"qpx.rgba-tiles/1")
    x0, y0, x1, y1 = 2, 3, 32, 25
    digest.update(struct.pack("<7Q", 35, 27, x0, y0, x1, y1, 16))
    for y in range(y0, y1, 16):
        for x in range(x0, x1, 16):
            w, h = min(16, x1-x), min(16, y1-y)
            digest.update(struct.pack("<4Q", x, y, w, h))
            rgba = np.empty((h, w, 4), dtype=np.uint8)
            rgba[..., :3] = rgb[y:y+h, x:x+w]
            rgba[..., 3] = 255
            digest.update(rgba.tobytes())
    assert result.input_sha256 == digest.hexdigest()


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_record_roundtrip_and_exact_recheck(tmp_path):
    project, doc, image, obj, record, path = project_with_record(tmp_path)
    assert doc.analysis_status(record) == "current"
    assert doc.analysis_record_count == 1 and project.dirty
    original = doc.analysis_record(record)
    assert original.id > original.result.document_revision
    assert doc.latest_analysis(obj).id == record
    project.save(str(path))
    assert not project.dirty
    restored_project = q.Project.open(str(path))
    restored = restored_project.document(image)
    assert not restored_project.dirty
    assert restored.analysis_status(record) == "unverified"
    saved = restored.analysis_record(record)
    assert saved.roi.equals(original.roi) and saved.tile_size == 16
    assert saved.result.input_sha256 == original.result.input_sha256
    np.testing.assert_array_equal(saved.result.means, original.result.means)
    revision = restored.revision
    replay = q.AnalysisRequest.replay(restored, record)
    job = q.AnalysisJob()
    assert job.request(replay) and job.wait(restored, obj).state == "ready"
    assert job.verify_result(restored, record)
    assert restored.revision == revision and not restored_project.dirty
    assert restored.analysis_status(record) == "current"
    # A second reopening must recheck independently; trust is never persisted.
    assert q.Project.open(str(path)).document(image).analysis_status(record) == "unverified"
    assert restored.remove_analysis(record) and not restored.remove_analysis(record)
    assert restored_project.dirty and restored.analysis_record_count == 0
    assert restored.latest_analysis(obj) is None
    restored_project.save(str(path))
    assert q.Project.open(str(path)).document(image).analysis_record_count == 0


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_changed_pixels_with_same_size_and_timestamp_fail_recheck(tmp_path):
    path = tmp_path/"linked.ppm"
    path.write_bytes(b"P6\n32 24\n255\n"+bytes([10, 20, 30]*32*24))
    project = q.Project()
    image = project.add_image(str(path))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, 32, 24))
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    record = doc.keep_analysis(request, q.analyze(request))
    project_path = tmp_path/"linked.qpxproj"
    project.save(str(project_path))
    before = path.stat()
    path.write_bytes(b"P6\n32 24\n255\n"+bytes([11, 20, 30]*32*24))
    import os
    os.utime(path, ns=(before.st_atime_ns, before.st_mtime_ns))
    assert path.stat().st_size == before.st_size and path.stat().st_mtime_ns == before.st_mtime_ns
    restored = q.Project.open(str(project_path)).document(image)
    replay = q.AnalysisRequest.replay(restored, record)
    current = q.analyze(replay)
    assert not restored.verify_analysis(record, replay, current)
    assert restored.analysis_status(record) == "verification_failed"
    assert restored.analysis_record(record).result.input_sha256 != current.input_sha256
    # Fresh computation is available, but the historical measurement is retained.
    assert restored.keep_analysis(replay, current) > record
    assert restored.analysis_record_count == 2


def test_status_edits_history_and_removal(tmp_path):
    project, doc, _, obj, record, _ = project_with_record(tmp_path)
    doc.set_calibration(.2, .7)
    assert doc.analysis_status(record) == "current"
    doc.add_rectangle(q.Rect(0, 0, 1, 1))
    assert doc.analysis_status(record) == "current"
    doc.rename_annotation(obj, "Changed")
    assert doc.analysis_status(record) == "object_changed"
    with pytest.raises(ValueError, match="object has changed"):
        q.AnalysisRequest.replay(doc, record)
    doc.undo()
    assert doc.analysis_status(record) == "object_changed"
    doc.remove_annotation(obj)
    assert doc.analysis_record(record).result.object_id == obj
    assert doc.analysis_status(record) == "object_changed"
    assert doc.remove_analysis(record)


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_async_save_and_recovery_capture_records(tmp_path):
    project, doc, image, obj, record, path = project_with_record(tmp_path)
    save = q.ProjectSaveJob()
    assert save.request(project, str(path))
    doc.remove_analysis(record)
    assert save.wait().state == "saved"
    assert project.dirty
    assert q.Project.open(str(path)).document(image).analysis_record_count == 1
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), nucleus=True)
    second = doc.keep_analysis(request, q.analyze(request))
    autosave = q.ProjectAutosave()
    checkpoint = tmp_path/"record.qpxrecover"
    assert autosave.request(project, str(checkpoint))
    assert autosave.wait().completed == 1
    recovered = q.Project.recover(str(checkpoint)).document(image)
    assert recovered.analysis_record_count == 1
    assert recovered.analysis_record(second).result.nucleus
    assert recovered.analysis_status(second) == "unverified"


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
@pytest.mark.parametrize("assignment", ["samples='0'", "samples='99999999'", "tiles='0'", "tile_size=0", "kind=99", "nucleus=2", "input_sha256='bad'", "algorithm='unknown'", "s00=99", "white_r=0", "object_id='999999'", "id='999999'", "object_revision='999999'", "document_revision='999999'", "elapsed_ms=-1", "mean_r=1e999"])
def test_malformed_records_are_rejected(tmp_path, assignment):
    project, _, _, _, _, path = project_with_record(tmp_path)
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("UPDATE analysis_records SET "+assignment)
    with pytest.raises((ValueError, RuntimeError)):
        q.Project.open(str(path))


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_orphan_records_rejected(tmp_path):
    project, _, _, _, _, path = project_with_record(tmp_path)
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("UPDATE analysis_records SET image_id='99999'")
    with pytest.raises(ValueError, match="orphaned analysis"):
        q.Project.open(str(path))


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_v2_migration_rollback_and_success(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    doc.add_rectangle(q.Rect(0, 0, 5, 5))
    path = tmp_path/"v2.qpxproj"
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE tissue_records")
        db.execute("DROP TABLE analysis_records")
        db.execute("PRAGMA user_version=2")
        db.execute("CREATE TRIGGER fail_migration BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected archive migration failure'); END")
    before = path.read_bytes()
    restored = q.Project.open(str(path))
    assert path.read_bytes() == before and restored.document(image).analysis_record_count == 0
    with pytest.raises(RuntimeError, match="injected archive"):
        restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 2
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='analysis_records'").fetchone()[0] == 0
        db.execute("DROP TRIGGER fail_migration")
    restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 4
        assert db.execute("PRAGMA quick_check").fetchone()[0] == "ok"


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_finite_summary_tampering_is_detected_by_recheck(tmp_path):
    project, _, image, obj, record, path = project_with_record(tmp_path)
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("UPDATE analysis_records SET mean_r=mean_r+1")
    restored = q.Project.open(str(path)).document(image)
    assert restored.analysis_status(record) == "unverified"
    replay = q.AnalysisRequest.replay(restored, record)
    result = q.analyze(replay)
    assert result.input_sha256 == restored.analysis_record(record).result.input_sha256
    assert not restored.verify_analysis(record, replay, result)
    assert restored.analysis_status(record) == "verification_failed"


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_deleted_object_history_survives_roundtrip(tmp_path):
    project, doc, image, obj, record, path = project_with_record(tmp_path)
    doc.remove_annotation(obj)
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert restored.object_count == 0 and restored.analysis_record_count == 1
    assert restored.analysis_status(record) == "object_changed"
    assert restored.analysis_record(record).roi.area > 0


def test_stale_keep_is_atomic(tmp_path):
    _, doc, _, obj, _, _ = project_with_record(tmp_path)
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    result = q.analyze(request)
    doc.rename_annotation(obj, "Changed")
    revision, count = doc.revision, doc.analysis_record_count
    with pytest.raises(ValueError, match="stale"):
        doc.keep_analysis(request, result)
    assert doc.revision == revision and doc.analysis_record_count == count


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_schema2_recovery_without_archives_remains_readable(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image(2, 1, [10, 20, 30]*2))
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(0, 0, 2, 1))
    checkpoint = tmp_path/"legacy.qpxrecover"
    autosave = q.ProjectAutosave()
    assert autosave.request(project, str(checkpoint)) and autosave.wait().completed == 1
    with sqlite3.connect(checkpoint) as db:
        db.execute("DROP TABLE tissue_records")
        db.execute("DROP TABLE analysis_records")
        db.execute("PRAGMA user_version=2")
    info = q.Project.inspect_recovery(str(checkpoint))
    assert info.images == 1 and info.objects == 1
    recovered = q.Project.recover(str(checkpoint)).document(image)
    assert recovered.analysis_record_count == 0 and recovered.annotation(obj).geometry.area == 2


@pytest.mark.skipif(not q.has_project, reason="project storage disabled")
def test_archive_is_included_in_snapshot_budget(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image(2, 1, [10, 20, 30]*2))
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(0, 0, 2, 1))
    path = tmp_path/"budget.qpxproj"
    save = q.ProjectSaveJob(32000)
    assert save.request(project, str(path)) and save.wait().applied
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    result = q.analyze(request)
    for _ in range(1000):
        doc.keep_analysis(request, result)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError, IndexError), match="snapshot exceeds memory budget"):
        save.request(project, str(path))
    assert doc.revision == revision and doc.analysis_record_count == 1000 and project.dirty
    assert q.Project.open(str(path)).document(image).analysis_record_count == 0
