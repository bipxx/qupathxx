"""Lossless mask archives, provenance, migrations and asynchronous isolation."""
import gc
import hashlib
import sqlite3
import struct
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q

from test_tissue import document
from test_tissue_levels import pyramid_levels

project_only = pytest.mark.skipif(not q.has_project, reason="native project storage disabled")


def capture(doc, obj, parameters=None):
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), parameters or q.TissueParameters())
    result = q.detect_tissue(request)
    record = doc.keep_tissue(request, result)
    return request, result, record


def snapshot(result):
    return (result.algorithm, result.input_sha256, result.labels, result.mask, result.object_id,
            result.object_revision, result.document_revision, result.nucleus, result.level, result.downsample,
            result.roi.wkt, result.width, result.height, result.samples, result.threshold_pixels,
            result.cleaned_pixels, result.filled_pixels, result.foreground_pixels, result.raw_components,
            result.elapsed_ms, result.cleanup_ms, result.white, result.stains,
            [(c.label, c.pixels, c.bounds.x, c.bounds.y, c.bounds.width, c.bounds.height, c.centroid.x, c.centroid.y)
             for c in result.components])


def saved(tmp_path, mask=None):
    if mask is None:
        mask = np.eye(9, dtype=bool)
    doc = document(np.where(mask[..., None], 0, 255).repeat(3, axis=2))
    obj = doc.add_rectangle(q.Rect(0, 0, mask.shape[1], mask.shape[0]))
    request, result, record = capture(doc, obj)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"tissue.qpxproj"
    project.save(str(path))
    return project, doc, image, obj, request, result, record, path


def test_records_are_immutable_shared_values_and_revisions_are_canonical():
    doc = document(np.zeros((17, 19, 3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 19, 17))
    doc.clear_history()
    revision = doc.revision
    request, result, record = capture(doc, obj)
    saved_record = doc.tissue_record(record)
    assert record == revision+1 == doc.revision and not doc.can_undo
    assert doc.tissue_records[0].id == doc.latest_tissue(obj).id == record
    assert saved_record.result is result and saved_record.producer == "qupathxx/0.1.0"
    assert saved_record.payload_bytes >= result.retained_bytes and doc.tissue_status(record) == "current"
    with pytest.raises(AttributeError):
        saved_record.id = 99
    result.parameters.threshold = 100
    assert saved_record.result.parameters.threshold == .3
    other = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    with pytest.raises(ValueError, match="current request"):
        doc.keep_tissue(other, result)
    assert doc.verify_tissue(record, request, result) and doc.revision == record
    second = doc.keep_tissue(request, result)
    assert second > record and doc.latest_tissue(obj).id == second and doc.tissue_record_count == 2
    assert doc.remove_tissue(second) and doc.latest_tissue(obj).id == record
    assert doc.remove_tissue(record) and not doc.remove_tissue(record) and doc.latest_tissue(obj) is None
    del doc, result, request
    gc.collect()
    assert saved_record.result.foreground_pixels == 17*19


@project_only
@pytest.mark.parametrize("feature", range(4))
@pytest.mark.parametrize("connectivity", [4, 8])
@pytest.mark.parametrize("cleanup", [False, True])
def test_full_mask_parameters_and_components_roundtrip(tmp_path, feature, connectivity, cleanup):
    rgb = np.random.default_rng(7).integers(0, 256, (31, 43, 3), dtype=np.uint8)
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(.2, .3, 41.6, 29.4))
    parameters = q.TissueParameters()
    parameters.feature = [q.TissueFeature.OPTICAL_DENSITY_SUM, q.TissueFeature.STAIN_1,
                          q.TissueFeature.STAIN_2, q.TissueFeature.STAIN_3][feature]
    parameters.connectivity, parameters.tile_size, parameters.threshold = connectivity, 16, .8
    parameters.min_pixels, parameters.max_pixels = 2, 10000
    if cleanup:
        parameters.open_radius, parameters.close_radius = 1, 2
        parameters.fill_holes, parameters.max_hole_pixels = True, 17
    _, result, record = capture(doc, obj, parameters)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"mask.qpxproj"
    project.save(str(path))
    restored_project = q.Project.open(str(path))
    restored = restored_project.document(image)
    assert not restored_project.dirty and restored.tissue_status(record) == "unverified"
    assert snapshot(restored.tissue_record(record).result) == snapshot(result)
    replay = q.TissueRequest.replay(restored, record)
    current = q.detect_tissue(replay)
    revision = restored.revision
    assert restored.verify_tissue(record, replay, current)
    assert restored.tissue_status(record) == "current" and restored.revision == revision and not restored_project.dirty
    assert restored.tissue_record(record).result.parameters.max_hole_pixels == parameters.max_hole_pixels
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 7
        assert db.execute("PRAGMA quick_check").fetchone()[0] == "ok"


@project_only
@pytest.mark.parametrize("level", [1, 2])
def test_native_pyramid_mask_and_clipped_geometry_roundtrip(tmp_path, pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_rectangle(q.Rect(13.2, 17.1, 231.5, 189.6))
    p = q.TissueParameters()
    p.level, p.threshold = level, 0
    _, result, record = capture(doc, obj, p)
    project = q.Project()
    image = project.add_document(doc, source_path=str(tmp_path/"tissue-levels.tif"))
    path = tmp_path/"pyramid.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    old = restored.tissue_record(record).result
    assert snapshot(old) == snapshot(result)
    with pytest.raises(ValueError, match="current request"):
        restored.verify_tissue(record, q.TissueRequest.replay(restored, record), q.detect_tissue(q.TissueRequest.replay(restored, record)))
    replay = q.TissueRequest.replay(restored, record)
    assert restored.verify_tissue(record, replay, q.detect_tissue(replay))
    if q.has_geos:
        assert q.trace_tissue(old).components[0].geometry.wkt == q.trace_tissue(result).components[0].geometry.wkt


@project_only
@pytest.mark.parametrize("nucleus", [False, True])
def test_analytic_roi_compartment_and_three_stain_basis_are_preserved(tmp_path, nucleus):
    doc = q.Document(q.Image.demo())
    obj = doc.add_cell(q.Roi.ellipse(q.Rect(100, 100, 120, 80)), q.Roi.ellipse(q.Rect(130, 120, 20, 10)))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11], [.30, .10, .90]], [250, 245, 230])
    p = q.TissueParameters()
    p.feature = q.TissueFeature.STAIN_3
    request = q.TissueRequest(doc, obj, transform, p, nucleus)
    result = q.detect_tissue(request)
    record = doc.keep_tissue(request, result)
    doc.set_calibration(.5, .25)
    assert doc.tissue_status(record) == "current"
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"compartment.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    old = restored.tissue_record(record).result
    assert snapshot(old) == snapshot(result) and old.roi.kind == q.RoiKind.ELLIPSE
    replay = q.TissueRequest.replay(restored, record)
    assert restored.verify_tissue(record, replay, q.detect_tissue(replay))


@project_only
@pytest.mark.parametrize("mask,compressed", [(np.ones((65, 67), dtype=bool), True),
    (np.indices((16, 16)).sum(axis=0) % 2 == 0, False), (np.zeros((17, 19), dtype=bool), True)])
def test_raw_and_run_length_payloads_are_independently_decodable(tmp_path, mask, compressed):
    _, _, _, _, _, result, _, path = saved(tmp_path, mask)
    with sqlite3.connect(path) as db:
        blob = db.execute("SELECT mask FROM tissue_records").fetchone()[0]
    assert blob[:4] == b"QTM\1"
    mode, count = struct.unpack_from("<QQ", blob, 428)
    assert bool(mode) == compressed
    if compressed:
        runs = np.frombuffer(blob[444:-64], dtype="<u4").reshape(-1, 2)
        assert len(runs) == count and np.all(runs[:, 0] > 0)
        labels = np.repeat(runs[:, 1], runs[:, 0])
    else:
        labels = np.frombuffer(blob[444:-64], dtype="<u4")
        assert len(labels) == count
    np.testing.assert_array_equal(labels, np.frombuffer(result.labels, dtype=np.uint32))
    if compressed:
        assert len(blob) < len(result.labels)


@project_only
@pytest.mark.parametrize("offset,value", [(28, 2), (36, 99), (52, 6), (68, 257), (84, 2),
    (100, 0), (116, 0), (124, 1000001), (132, 0), (140, 999), (172, 2**63),
    (284, 2**63), (316, 0), (324, 0), (348, 1), (356, 1000001), (428, 2), (436, 2**63)])
def test_checksum_preserving_malformed_metadata_is_rejected(tmp_path, offset, value):
    _, doc, _, obj, _, _, _, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        blob = bytearray(db.execute("SELECT mask FROM tissue_records").fetchone()[0])
        struct.pack_into("<Q", blob, offset, value)
        geometry = doc.annotation(obj).geometry.wkt.encode()
        blob[-64:] = hashlib.sha256(struct.pack("<Q", len(geometry))+geometry+blob[:-64]).hexdigest().encode()
        db.execute("UPDATE tissue_records SET mask=?", (blob,))
    with pytest.raises(ValueError, match="tissue|invalid"):
        q.Project.open(str(path))


@project_only
@pytest.mark.parametrize("damage", ["checksum", "truncated", "trailing", "orphan", "identity", "producer"])
def test_damaged_and_orphaned_archive_rows_are_rejected(tmp_path, damage):
    *_, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        blob = db.execute("SELECT mask FROM tissue_records").fetchone()[0]
        if damage == "checksum":
            db.execute("UPDATE tissue_records SET mask=?", (blob[:-1]+b"X",))
        elif damage == "truncated":
            db.execute("UPDATE tissue_records SET mask=?", (blob[:100],))
        elif damage == "trailing":
            db.execute("UPDATE tissue_records SET mask=?", (blob+b"x",))
        elif damage == "orphan":
            db.execute("UPDATE tissue_records SET image_id='999999'")
        elif damage == "identity":
            db.execute("UPDATE tissue_records SET id='999999'")
        else:
            db.execute("UPDATE tissue_records SET producer='' ")
    with pytest.raises(ValueError):
        q.Project.open(str(path))


@project_only
def test_external_pixel_change_fails_reverification_even_when_mask_is_identical(tmp_path):
    source = tmp_path/"pixels.ppm"
    source.write_bytes(b"P6\n9 9\n255\n"+bytes([0, 0, 0])*81)
    project = q.Project()
    image = project.add_image(str(source))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, 9, 9))
    _, old, record = capture(doc, obj)
    path = tmp_path/"linked.qpxproj"
    project.save(str(path))
    source.write_bytes(b"P6\n9 9\n255\n"+bytes([1, 1, 1])*81)
    restored = q.Project.open(str(path)).document(image)
    request = q.TissueRequest.replay(restored, record)
    fresh = q.detect_tissue(request)
    assert fresh.labels == old.labels and fresh.input_sha256 != old.input_sha256
    assert not restored.verify_tissue(record, request, fresh)
    assert restored.tissue_status(record) == "verification_failed"


@pytest.mark.parametrize("change", ["rename", "delete", "close", "selection"])
def test_job_publication_rejects_stale_or_cancelled_captures(change):
    doc = document(np.zeros((17, 19, 3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 19, 17))
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert job.wait(doc, obj).state == "ready"
    record = job.keep_result(doc, obj)
    selected = obj
    if change == "rename":
        doc.rename_annotation(obj, "Edited")
        doc.undo()
    elif change == "delete":
        doc.remove_annotation(obj)
    elif change == "close":
        doc.server.close()
    else:
        selected = 0
    with pytest.raises(RuntimeError, match="no current"):
        job.keep_result(doc, selected)
    assert doc.tissue_record_count == 1
    assert doc.tissue_status(record) == ("source_closed" if change == "close" else "current" if change == "selection" else "object_changed")


@project_only
def test_background_save_and_recovery_keep_captured_masks_during_live_removal(tmp_path):
    project, doc, image, obj, request, result, first, path = saved(tmp_path)
    middle = doc.keep_tissue(request, result)
    tail = doc.keep_tissue(request, result)
    save = q.ProjectSaveJob()
    assert save.request(project, str(path))
    assert doc.remove_tissue(first)
    second = doc.keep_tissue(request, result)
    assert save.wait().applied and project.dirty
    restored = q.Project.open(str(path)).document(image)
    assert [r.id for r in restored.tissue_records] == [first, middle, tail]
    assert restored.tissue_record(first).result.labels == result.labels
    checkpoint = tmp_path/"mask.qpxrecover"
    recovery = q.ProjectAutosave()
    assert recovery.request(project, str(checkpoint))
    assert doc.remove_tissue(second)
    assert recovery.wait().completed == 1
    recovered = q.Project.recover(str(checkpoint)).document(image)
    assert [r.id for r in recovered.tissue_records] == [middle, tail, second]
    assert snapshot(recovered.tissue_record(second).result) == snapshot(result)


@project_only
def test_schema3_read_and_transactional_migration(tmp_path):
    project, doc, image, obj, _, _, record, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE analysis_provenance");db.execute("DROP TABLE cell_feature_links")
        db.execute("DROP TABLE cell_records")
        db.execute("DROP TABLE tissue_records")
        db.execute("PRAGMA user_version=3")
        db.execute("CREATE TRIGGER fail_tissue BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected tissue migration failure'); END")
    before = path.read_bytes()
    restored = q.Project.open(str(path))
    assert path.read_bytes() == before and restored.document(image).tissue_record_count == 0
    with pytest.raises(RuntimeError, match="injected tissue"):
        restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 3
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='tissue_records'").fetchone()[0] == 0
        db.execute("DROP TRIGGER fail_tissue")
    restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 7
    assert q.Project.open(str(path)).document(image).tissue_record_count == 0


@project_only
def test_analysis_and_mask_records_share_canonical_revision_identity(tmp_path):
    project, doc, image, obj, _, result, record, path = saved(tmp_path)
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    analysis = doc.keep_analysis(request, q.analyze(request))
    assert analysis > record
    project.save(str(path))
    loaded = q.Project.open(str(path)).document(image)
    assert loaded.analysis_record_count == loaded.tissue_record_count == 1
    with sqlite3.connect(path) as db:
        db.execute("UPDATE tissue_records SET id=?", (str(analysis),))
    with pytest.raises(ValueError, match="duplicate tissue"):
        q.Project.open(str(path))


@project_only
def test_mask_archive_participates_in_snapshot_budget(tmp_path):
    doc = document(np.zeros((128, 128, 3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 128, 128))
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"budget.qpxproj"
    save = q.ProjectSaveJob(180000)
    assert save.request(project, str(path)) and save.wait().applied
    _, _, record = capture(doc, obj)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError, IndexError), match="snapshot exceeds memory budget"):
        save.request(project, str(path))
    assert doc.revision == revision and doc.tissue_record_count == 1 and project.dirty
    assert q.Project.open(str(path)).document(image).tissue_record_count == 0


def test_logical_archive_payload_limit_is_atomic_and_reclaimed_after_removal():
    doc = document(np.zeros((512, 512, 3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 512, 512))
    request, result, first = capture(doc, obj)
    payload = doc.tissue_record(first).payload_bytes
    allowed = (256*1024*1024)//payload
    assert 1 < allowed < 10000
    for _ in range(allowed-1):
        doc.keep_tissue(request, result)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError, IndexError), match="tissue archive exceeds"):
        doc.keep_tissue(request, result)
    assert doc.revision == revision and doc.tissue_record_count == allowed
    assert doc.remove_tissue(first)
    assert doc.keep_tissue(request, result) > revision and doc.tissue_record_count == allowed


@project_only
def test_worker_destruction_does_not_need_python_mask_wrappers(tmp_path):
    script = r'''
import gc
import pathlib
import sys
import qupathxx as q
for iteration in range(12):
    project = q.Project()
    doc = q.Document(q.Image(64, 64, [0]*64*64*3))
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(0, 0, 64, 64))
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    result = q.detect_tissue(request)
    ids = [doc.keep_tissue(request, result) for _ in range(3)]
    job = q.ProjectSaveJob()
    path = pathlib.Path(sys.argv[1])/f"worker-{iteration}.qpxproj"
    assert job.request(project, str(path))
    for record in ids:
        assert doc.remove_tissue(record)
    del result, request
    gc.collect()
    assert job.wait().applied
    loaded = q.Project.open(str(path)).document(image)
    assert loaded.tissue_record_count == 3
    assert all(record.result.foreground_pixels == 4096 for record in loaded.tissue_records)
    del job, loaded, doc, project
    gc.collect()
'''
    result = subprocess.run([sys.executable, "-I", "-c", script, str(tmp_path)], capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout+result.stderr
