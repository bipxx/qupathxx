"""Lossless cell runs, strict archive decoding and native snapshot ownership."""
import gc
import hashlib
import sqlite3
import struct
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q

from test_cells import phantom, transform
from test_tissue import document
from test_tissue_records import snapshot as raster_snapshot
from test_tissue_levels import pyramid_levels

project_only = pytest.mark.skipif(not q.has_project, reason="native project storage")


def snapshot(r):
    p = r.parameters
    return (r.algorithm, raster_snapshot(r.nuclei), raster_snapshot(r.cells), r.raw_peaks, r.elapsed_ms,
            [getattr(p, name) for name in ("feature", "threshold", "sigma", "min_peak_radius", "min_seed_distance", "cell_expansion", "min_nucleus_pixels", "max_nucleus_pixels", "connectivity", "level", "tile_size", "max_cells", "work_bytes")],
            [getattr(p.contours, name) for name in ("clip_to_roi", "max_components", "max_edges", "max_vertices", "work_bytes")],
            [(m.label, m.result_label, m.pixel_index, m.center.x, m.center.y, m.radius) for m in r.markers],
            [(f.label, f.nucleus_pixels, f.cell_pixels, f.nucleus_rgb, f.cell_rgb, f.nucleus_channels, f.cell_channels, f.cytoplasm_rgb, f.cytoplasm_channels) for f in r.features])


def capture(doc, obj, parameters=None):
    request = q.CellRequest(doc, obj, transform(), parameters or q.CellParameters())
    result = q.detect_cells(request)
    return request, result, doc.keep_cells(request, result)


def saved(tmp_path, parameters=None, rgb=None):
    doc = document(phantom() if rgb is None else rgb)
    obj = doc.add_rectangle(q.Rect(0, 0, doc.metadata.width, doc.metadata.height))
    request, result, record = capture(doc, obj, parameters)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/"cells.qpxproj"
    project.save(str(path))
    return project, doc, image, obj, request, result, record, path


def test_native_records_are_immutable_and_request_bound():
    doc = document(phantom())
    obj = doc.add_rectangle(q.Rect(0, 0, 48, 48))
    doc.clear_history()
    revision = doc.revision
    request, result, record = capture(doc, obj)
    retained = doc.cell_record(record)
    assert record == revision+1 == doc.revision and not doc.can_undo
    assert retained.result is result and doc.latest_cells(obj).id == doc.cell_records[0].id == record
    assert retained.producer == "qupathxx/0.1.0" and retained.payload_bytes >= result.retained_bytes
    assert doc.cell_status(record) == "current"
    with pytest.raises(AttributeError):
        retained.id = 0
    result.parameters.sigma = 99
    assert retained.result.parameters.sigma == 1
    other = q.CellRequest(doc, obj, transform())
    with pytest.raises(ValueError, match="current request"):
        doc.keep_cells(other, result)
    assert doc.verify_cells(record, request, result) and doc.revision == record
    second = doc.keep_cells(request, result)
    assert second > record and doc.cell_record_count == 2
    assert doc.remove_cells(second) and doc.latest_cells(obj).id == record
    assert doc.remove_cells(record) and not doc.remove_cells(record) and doc.latest_cells(obj) is None
    del doc, request, result
    gc.collect()
    assert retained.result.cell_count == 2


@project_only
@pytest.mark.parametrize("feature", [q.TissueFeature.OPTICAL_DENSITY_SUM, q.TissueFeature.STAIN_1, q.TissueFeature.STAIN_2, q.TissueFeature.STAIN_3])
@pytest.mark.parametrize("expansion", [0, 3.5])
def test_roundtrip_retains_every_numeric_field_and_rechecks(tmp_path, feature, expansion):
    p = q.CellParameters()
    p.feature, p.threshold, p.sigma, p.cell_expansion, p.connectivity = feature, .05, .7, expansion, 4
    project, doc, image, obj, _, result, record, path = saved(tmp_path, p)
    restored = q.Project.open(str(path)).document(image)
    old = restored.cell_record(record).result
    assert snapshot(old) == snapshot(result)
    assert bool(old.cell_contours) == q.has_geos and bool(old.nucleus_contours) == q.has_geos
    assert restored.cell_status(record) == "unverified"
    revision = restored.revision
    replay = q.CellRequest.replay(restored, record)
    assert restored.verify_cells(record, replay, q.detect_cells(replay))
    assert restored.cell_status(record) == "current" and restored.revision == revision
    with pytest.raises(ValueError, match="current request"):
        restored.keep_cells(replay, old)
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 7


@project_only
@pytest.mark.parametrize("level", [1, 2])
def test_native_pyramid_grid_roi_and_markers_survive(tmp_path, pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_roi(q.Roi.ellipse(q.Rect(13.2, 17.1, 91.5, 89.6)))
    p = q.CellParameters()
    p.level, p.sigma, p.min_nucleus_pixels = level, .7, 2
    _, result, record = capture(doc, obj, p)
    project = q.Project()
    image = project.add_document(doc, source_path=str(tmp_path/"tissue-levels.tif"))
    path = tmp_path/"pyramid.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert snapshot(restored.cell_record(record).result) == snapshot(result)
    replay = q.CellRequest.replay(restored, record)
    assert restored.verify_cells(record, replay, q.detect_cells(replay))
    if q.has_geos:
        job = q.CellJob()
        assert job.request(q.CellRequest.replay(restored, record)) and job.wait(restored, obj).state == "ready"
        ids = job.keep_result(restored, obj)
        assert len(ids) == result.cell_count
        for cell in map(restored.annotation, ids):
            assert restored.annotation(obj).geometry.covers(cell.geometry) and cell.geometry.covers(cell.nucleus)


@project_only
def test_filtered_markers_empty_result_and_deleted_source_are_archivable(tmp_path):
    p = q.CellParameters()
    p.max_nucleus_pixels = 10
    project, doc, image, obj, _, result, record, path = saved(tmp_path, p)
    assert result.markers and result.cell_count == 0 and all(m.result_label == 0 for m in result.markers)
    doc.remove_annotation(obj)
    assert doc.cell_status(record) == "object_changed"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert snapshot(restored.cell_record(record).result) == snapshot(result)
    assert restored.cell_status(record) == "object_changed"
    with pytest.raises(IndexError):
        q.CellRequest.replay(restored, record)


@project_only
def test_save_and_recovery_capture_keep_removed_runs_isolated(tmp_path):
    project, doc, image, obj, request, result, record, path = saved(tmp_path)
    save = q.ProjectSaveJob()
    assert save.request(project, str(path))
    assert doc.remove_cells(record)
    newer = doc.keep_cells(request, result)
    assert save.wait().applied and project.dirty
    restored = q.Project.open(str(path)).document(image)
    assert [r.id for r in restored.cell_records] == [record]
    assert snapshot(restored.cell_record(record).result) == snapshot(result)
    checkpoint = tmp_path/"cells.qpxrecover"
    autosave = q.ProjectAutosave()
    assert autosave.request(project, str(checkpoint))
    assert doc.remove_cells(newer)
    assert autosave.wait().completed == 1
    recovered = q.Project.recover(str(checkpoint)).document(image)
    assert [r.id for r in recovered.cell_records] == [newer]
    assert snapshot(recovered.cell_record(newer).result) == snapshot(result)


@project_only
def test_schema4_migration_rolls_back_with_failed_save(tmp_path):
    project, doc, image, obj, _, _, _, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE analysis_provenance");db.execute("DROP TABLE cell_feature_links")
        db.execute("DROP TABLE cell_records")
        db.execute("PRAGMA user_version=4")
        db.execute("CREATE TRIGGER fail_cells BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected cell migration failure'); END")
    before = path.read_bytes()
    loaded = q.Project.open(str(path))
    assert loaded.document(image).cell_record_count == 0 and path.read_bytes() == before
    with pytest.raises(RuntimeError, match="injected cell"):
        loaded.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 4
        assert not db.execute("SELECT count(*) FROM sqlite_schema WHERE name='cell_records'").fetchone()[0]
        db.execute("DROP TRIGGER fail_cells")
    loaded.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 7


def rewrite_checksum(data, roi):
    geometry = roi.wkt.encode()
    data[-64:] = hashlib.sha256(struct.pack("<Q", len(geometry))+geometry+data[:-64]).hexdigest().encode()


@project_only
@pytest.mark.parametrize("offset,fmt,value", [(0,"B",0), (28,"Q",99), (44,"d",float("nan")), (76,"Q",0),
    (92,"Q",6), (132,"Q",0), (172,"Q",999), (204,"Q",2**63), (316,"Q",0), (332,"Q",1000001),
    (364,"Q",1), (436,"I",99), (440,"I",999999), (444,"I",9999999)])
def test_checksum_preserving_invalid_metadata_is_rejected(tmp_path, offset, fmt, value):
    _, _, _, _, _, result, _, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        data = bytearray(db.execute("SELECT run FROM cell_records").fetchone()[0])
        struct.pack_into("<"+fmt, data, offset, value)
        rewrite_checksum(data, result.nuclei.roi)
        db.execute("UPDATE cell_records SET run=?", (data,))
    with pytest.raises(ValueError):
        q.Project.open(str(path))


@project_only
def test_tampered_finite_feature_is_unverified_then_fails_numeric_recheck(tmp_path):
    _, _, image, obj, _, result, record, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        data = bytearray(db.execute("SELECT run FROM cell_records").fetchone()[0])
        markers = struct.unpack_from("<Q", data, 340)[0]
        feature = 436+20*markers
        assert result.features[0].cell_pixels > result.features[0].nucleus_pixels
        for offset in [64, 88, 136]:
            original = struct.unpack_from("<d", data, feature+offset)[0]
            struct.pack_into("<d", data, feature+offset, original+.125)
        rewrite_checksum(data, result.nuclei.roi)
        db.execute("UPDATE cell_records SET run=?", (data,))
    loaded = q.Project.open(str(path))
    doc = loaded.document(image)
    revision = doc.revision
    request = q.CellRequest.replay(doc, record)
    new = q.detect_cells(request)
    assert new.nuclei.input_sha256 == doc.cell_record(record).result.nuclei.input_sha256
    assert not doc.verify_cells(record, request, new)
    assert doc.cell_status(record) == "verification_failed" and doc.revision == revision and not loaded.dirty


@project_only
def test_changed_linked_pixels_are_detected_even_with_same_segmentation(tmp_path):
    source = tmp_path/"source.ppm"
    source.write_bytes(b"P6\n16 16\n255\n"+bytes([0,0,0])*256)
    project = q.Project()
    image = project.add_image(str(source))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, 16, 16))
    _, result, record = capture(doc, obj)
    path = tmp_path/"linked.qpxproj"
    project.save(str(path))
    source.write_bytes(b"P6\n16 16\n255\n"+bytes([1,1,1])*256)
    restored = q.Project.open(str(path)).document(image)
    request = q.CellRequest.replay(restored, record)
    changed = q.detect_cells(request)
    assert result.nuclei.labels == changed.nuclei.labels
    assert not restored.verify_cells(record, request, changed) and restored.cell_status(record) == "verification_failed"


def test_archive_payload_cap_is_atomic_and_reclaimed():
    doc = document(np.zeros((256,256,3), dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0, 0, 256, 256))
    request, result, first = capture(doc, obj)
    allowed = (256*1024*1024)//doc.cell_record(first).payload_bytes
    assert 1 < allowed < 10000
    for _ in range(allowed-1):
        doc.keep_cells(request, result)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError, IndexError), match="cell archive exceeds"):
        doc.keep_cells(request, result)
    assert doc.revision == revision and doc.cell_record_count == allowed
    assert doc.remove_cells(first) and doc.keep_cells(request, result) > revision


@pytest.mark.parametrize("change", ["close", "rename", "delete"])
def test_archived_runs_keep_history_when_source_becomes_unusable(change):
    doc = document(phantom())
    obj = doc.add_rectangle(q.Rect(0,0,48,48))
    _, result, record = capture(doc,obj)
    if change == "close":
        doc.server.close()
    elif change == "rename":
        doc.rename_annotation(obj,"Changed")
        doc.undo()
    else:
        doc.remove_annotation(obj)
    assert doc.cell_status(record) == ("source_closed" if change == "close" else "object_changed")
    with pytest.raises((ValueError,RuntimeError,IndexError)):
        q.CellRequest.replay(doc,record)
    assert doc.cell_record(record).result is result and result.nuclei.labels


@project_only
@pytest.mark.parametrize("other", ["analysis", "tissue"])
def test_archive_ids_share_document_revision_namespace(tmp_path, other):
    project, doc, image, obj, _, _, record, path = saved(tmp_path)
    if other == "analysis":
        request = q.AnalysisRequest(doc,obj,transform())
        competing = doc.keep_analysis(request,q.analyze(request))
    else:
        request = q.TissueRequest(doc,obj,transform())
        competing = doc.keep_tissue(request,q.detect_tissue(request))
    assert competing > record
    project.save(str(path))
    assert q.Project.open(str(path)).document(image).cell_record_count == 1
    with sqlite3.connect(path) as db:
        db.execute("UPDATE cell_records SET id=?",(str(competing),))
    with pytest.raises(ValueError,match="duplicate cell"):
        q.Project.open(str(path))


@project_only
@pytest.mark.parametrize("field", ["encoding", "entries", "run_length", "label", "trailing"])
def test_checksum_preserving_label_transport_corruption_is_rejected(tmp_path, field):
    _, _, _, _, _, result, _, path = saved(tmp_path)
    with sqlite3.connect(path) as db:
        data = bytearray(db.execute("SELECT run FROM cell_records").fetchone()[0])
        markers, components = struct.unpack_from("<QQ",data,340)
        plane = 436+20*markers
        for _ in range(components):
            nucleus, body = struct.unpack_from("<QQ",data,plane)
            plane += 112+(48 if body>nucleus else 0)
        assert struct.unpack_from("<Q",data,plane)[0] == 1
        if field == "encoding": struct.pack_into("<Q",data,plane,2)
        elif field == "entries": struct.pack_into("<Q",data,plane+8,0)
        elif field == "run_length": struct.pack_into("<I",data,plane+16,0)
        elif field == "label": struct.pack_into("<I",data,plane+20,components+1)
        else: data[-64:-64] = b"x"
        rewrite_checksum(data,result.nuclei.roi)
        db.execute("UPDATE cell_records SET run=?",(data,))
    with pytest.raises(ValueError):
        q.Project.open(str(path))


@project_only
def test_cell_runs_participate_in_save_snapshot_budget(tmp_path):
    doc = document(np.zeros((128,128,3),dtype=np.uint8))
    obj = doc.add_rectangle(q.Rect(0,0,128,128))
    project = q.Project()
    image = project.add_document(doc)
    save = q.ProjectSaveJob(100000)
    path = tmp_path/"budget.qpxproj"
    assert save.request(project,str(path)) and save.wait().applied
    capture(doc,obj)
    revision = doc.revision
    with pytest.raises((ValueError,RuntimeError,IndexError),match="snapshot exceeds memory budget"):
        save.request(project,str(path))
    assert doc.revision == revision and doc.cell_record_count == 1 and project.dirty
    assert q.Project.open(str(path)).document(image).cell_record_count == 0


@project_only
def test_save_worker_destruction_uses_native_results_without_python_wrappers(tmp_path):
    script = r'''
import gc, pathlib, sys
import qupathxx as q
for iteration in range(10):
    project=q.Project()
    doc=q.Document(q.Image(32,32,[0]*32*32*3))
    image=project.add_document(doc)
    obj=doc.add_rectangle(q.Rect(0,0,32,32))
    request=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
    result=q.detect_cells(request)
    ids=[doc.keep_cells(request,result) for _ in range(3)]
    job=q.ProjectSaveJob()
    path=pathlib.Path(sys.argv[1])/f"worker-{iteration}.qpxproj"
    assert job.request(project,str(path))
    for record in ids: assert doc.remove_cells(record)
    del result,request
    gc.collect()
    assert job.wait().applied
    loaded=q.Project.open(str(path)).document(image)
    assert loaded.cell_record_count==3
    assert all(r.result.nuclei.foreground_pixels==1024 for r in loaded.cell_records)
    del loaded,job,doc,project
    gc.collect()
'''
    child = subprocess.run([sys.executable,"-I","-c",script,str(tmp_path)], capture_output=True, text=True, timeout=20)
    assert child.returncode == 0, child.stdout+child.stderr
