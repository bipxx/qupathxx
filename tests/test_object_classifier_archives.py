"""Portable classifier cohorts, application audits and project/recovery isolation."""
import concurrent.futures
import gc
import hashlib
import sqlite3
import struct
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q
from test_object_classifier import workspace, parameters


def model_for(features):
    b = bytearray(struct.pack('<6I3Q', 0x46585051, 1, 2, 2, 2, 0, 1, 4*1024*1024, 4))
    def u32(*v): b.extend(struct.pack('<'+'I'*len(v), *v))
    def text(s):
        raw = s.encode('utf8'); u32(len(raw)); b.extend(raw)
    text('a'*64); text('independent-object-test/1'); u32(len(features))
    for f in features: text(f.name); text(f.unit)
    u32(2); text('A'); text('B'); u32(2, 0, 3, 4)
    for node in [(0, 1, 2, 0, 0x3f800000), (0, 0, 0, 0, 0), (0, 0, 0, 1, 0), (0, 0, 0, 1, 0)]: u32(*node)
    return q.ForestModel.from_bytes(bytes(b))


def predicted(doc, ids, options=None):
    request = q.ObjectFeatureRequest(doc, ids, options or q.ObjectFeatureOptions())
    table = q.extract_object_features(request)
    model = model_for(table.features)
    job = q.ObjectClassifierJob()
    assert job.predict(request, model)
    stats = job.wait(doc)
    assert stats.state == 'ready', stats.error
    return request, job


def test_prediction_publication_retains_audit_in_one_revision_and_undo_keeps_it():
    doc, ids = workspace()
    request, job = predicted(doc, ids)
    artifact = job.archive
    assert artifact.purpose == 'prediction' and artifact.table.sha256 == job.table.sha256
    before = doc.revision
    doc.clear_history()
    assert job.apply(doc) == ids and doc.revision == before+1
    record = doc.classifier_records[0]
    assert record.id == doc.revision and record.applied and record.changed_objects == ids
    assert record.archive.sha256 == artifact.sha256
    assert doc.undo_label == 'Apply object classifier'
    assert doc.undo() and [doc.annotation(i).class_name for i in ids] == ['small']*3+['large']*3+['']*2
    assert doc.classifier_record(record.id).archive.sha256 == artifact.sha256
    assert doc.redo() and all(doc.annotation(i).class_name == 'B' for i in ids)
    assert not request.matches(doc)
    changed = record.changed_objects; changed.clear()
    assert record.changed_objects == ids
    assert doc.remove_classifier_record(record.id) and not doc.remove_classifier_record(record.id)
    assert artifact.table.numpy().shape == (8, 5)


def test_low_level_archive_ownership_opt_out_and_stale_training_authority():
    doc, ids = workspace()
    request, job = predicted(doc, ids)
    result, model = job.result, job.model
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        artifact = pool.submit(q.archive_object_classifier, result.table, model, result).result()
    del job, result
    gc.collect()
    assert artifact.table.body_roi(0).area == 4 and artifact.table.nucleus_roi(0) is None
    assert artifact.table.cell_origin(0) is None and artifact.table.cell_feature(0) is None
    with pytest.raises(IndexError): artifact.table.body_roi(100)
    with pytest.raises(ValueError, match='training'):
        q.archive_object_classifier(artifact.table, model)
    _, another = predicted(doc, ids)
    assert another.apply(doc, retain_record=False) == ids and doc.classifier_record_count == 0
    assert another.archive is not None
    # A record-only application still advances revision and archives the decision.
    _, unchanged = predicted(doc, ids)
    revision = doc.revision
    assert unchanged.apply(doc) == [] and doc.revision == revision+1
    assert doc.classifier_records[0].changed_objects == []


@pytest.mark.skipif(not q.has_classifier_training, reason='optional classifier training backend')
def test_training_cohort_identity_and_current_request_authority():
    doc, ids = workspace()
    request = q.ObjectFeatureRequest(doc, ids)
    job = q.ObjectClassifierJob()
    assert job.train(request, parameters()) and job.wait(doc).state == 'ready'
    artifact = job.archive
    assert artifact.purpose == 'training' and artifact.result is None
    other, _ = workspace()
    with pytest.raises(ValueError): other.keep_classifier_training(q.ObjectFeatureRequest(other, ids), artifact)
    revision = doc.revision
    record_id = job.retain_training(doc, 'Original cohort')
    assert doc.revision == revision+1 and job.poll(doc).state == 'retained'
    assert doc.classifier_record(record_id).name == 'Original cohort'
    assert doc.classifier_record(record_id).archive.model.training_rows == 6
    with pytest.raises((ValueError, RuntimeError)): job.retain_training(doc)
    with pytest.raises(ValueError): doc.keep_classifier_training(request, artifact)


@pytest.mark.skipif(not q.has_project, reason='project storage disabled')
def test_project_and_recovery_keep_exact_cohorts_without_live_authority(tmp_path):
    doc, ids = workspace(); _, job = predicted(doc, ids)
    job.apply(doc, min_vote_fraction=.75)
    original = doc.classifier_records[0]
    project = q.Project(); image = project.add_document(doc, 'Cohort')
    path = tmp_path/'classifier.qpxproj'; project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0] == 13
        assert db.execute('SELECT count(*) FROM classifier_records').fetchone()[0] == 1
    loaded_project = q.Project.open(str(path)); loaded = loaded_project.document(image)
    record = loaded.classifier_record(original.id)
    assert record.archive.sha256 == original.archive.sha256 and record.applied
    np.testing.assert_array_equal(record.archive.table.numpy(), original.archive.table.numpy())
    assert record.archive.table.object_revisions == original.archive.table.object_revisions
    assert record.archive.table.class_names == original.archive.table.class_names
    assert [p.votes for p in record.archive.result.predictions] == [p.votes for p in original.archive.result.predictions]
    assert record.minimum_vote_fraction == .75 and record.changed_objects == ids
    # A loaded artifact cannot manufacture a fresh document request ticket.
    fresh = q.ObjectFeatureRequest(loaded, ids)
    with pytest.raises(ValueError): q.apply_object_classification(loaded, fresh, record.archive.result)
    doc.rename_annotation(ids[0], 'Captured cohort name')
    recovery = tmp_path/'audit.qpxrecover'; autosave = q.ProjectAutosave()
    assert autosave.request(project, str(recovery))
    assert doc.remove_classifier_record(original.id)
    assert autosave.wait().state == 'ready'
    recovered = q.Project.recover(str(recovery), str(path)).document(image)
    assert recovered.classifier_record(original.id).archive.sha256 == original.archive.sha256
    assert doc.classifier_record_count == 0
    loaded.rename_annotation(ids[1], 'Saved cohort name')
    snapshot = q.ProjectSaveJob()
    assert snapshot.request(loaded_project, str(path))
    loaded.remove_classifier_record(original.id)
    assert snapshot.wait().applied
    assert q.Project.open(str(path)).document(image).classifier_record_count == 1 and loaded_project.dirty


@pytest.mark.skipif(not q.has_project, reason='project storage disabled')
@pytest.mark.parametrize('damage', ['checksum', 'version', 'identity', 'namespace', 'truncated'])
def test_invalid_archive_rejects_without_changing_project_file(tmp_path, damage):
    doc, ids = workspace(); _, job = predicted(doc, ids); job.apply(doc)
    project = q.Project(); project.add_document(doc); path = tmp_path/'damaged.qpxproj'; project.save(str(path))
    with sqlite3.connect(path) as db:
        payload = bytearray(db.execute('SELECT payload FROM classifier_records').fetchone()[0])
        if damage == 'checksum': payload[-1] ^= 1
        elif damage == 'version':
            payload[3] = 99
            payload[-64:] = hashlib.sha256(payload[:-64]).hexdigest().encode()
        elif damage == 'identity':
            struct.pack_into('<Q', payload, 4, 0)
            payload[-64:] = hashlib.sha256(payload[:-64]).hexdigest().encode()
        elif damage == 'namespace':
            pos = 12
            length = struct.unpack_from('<Q', payload, pos)[0]; pos += 8+length+16
            count = struct.unpack_from('<Q', payload, pos)[0]; pos += 8+8*count
            archive_size = struct.unpack_from('<Q', payload, pos)[0]; start = pos+8
            cursor = start+12
            for _ in range(2):
                length = struct.unpack_from('<Q', payload, cursor)[0]; cursor += 8+length
            struct.pack_into('<Q', payload, cursor+6*8, 1)
            payload[start+archive_size-64:start+archive_size] = hashlib.sha256(payload[start:start+archive_size-64]).hexdigest().encode()
            payload[-64:] = hashlib.sha256(payload[:-64]).hexdigest().encode()
        else: payload = payload[:67]
        db.execute('UPDATE classifier_records SET payload=?', (bytes(payload),))
    before = path.read_bytes()
    with pytest.raises((ValueError, RuntimeError)): q.Project.open(str(path))
    assert path.read_bytes() == before


@pytest.mark.skipif(not q.has_project, reason='project storage disabled')
def test_schema_seven_migration_rolls_back_on_save_failure(tmp_path):
    doc, ids = workspace(); project = q.Project(); project.add_document(doc)
    path = tmp_path/'legacy.qpxproj'; project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE raw_analysis_records");db.execute("DROP TABLE tiff_views");db.execute("DROP TABLE tma_cores");db.execute("DROP TABLE tma_grids")
        db.execute('DROP TABLE pixel_stream_records');db.execute('DROP TABLE pixel_classifier_records');db.execute('DROP TABLE classifier_records'); db.execute('PRAGMA user_version=7')
        db.execute("CREATE TRIGGER injected BEFORE INSERT ON images BEGIN SELECT RAISE(ABORT,'injected classifier migration'); END")
    _, job = predicted(doc, ids); job.apply(doc)
    before = path.read_bytes()
    with pytest.raises(RuntimeError, match='injected'): project.save(str(path))
    assert path.read_bytes() == before
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0] == 7
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='classifier_records'").fetchone()[0] == 0
        db.execute('DROP TRIGGER injected')
    project.save(str(path))
    assert q.Project.open(str(path)).document(project.active_image).classifier_record_count == 1


@pytest.mark.skipif(not q.has_project or not q.has_geos, reason='requires project and GEOS')
def test_rgb_archive_preserves_measurements_and_deduplicated_source_manifest(tmp_path):
    from test_cell_feature_links import setup, signature
    doc, parent, request, result, source_id = setup()
    ids = doc.publish_cell_record(source_id)
    options = q.ObjectFeatureOptions(); options.cell_rgb = options.cytoplasm_rgb = True
    _, job = predicted(doc, ids, options); job.apply(doc)
    artifact = doc.classifier_records[0].archive
    origin = artifact.table.cell_origin(0)
    assert origin.record_id == source_id and origin.label == 1 and origin.source.record_id == source_id
    assert origin.source.object_id == parent and origin.source.algorithm == result.algorithm
    assert origin.source.input_sha256 == result.nuclei.input_sha256
    assert origin.source.roi.area == 64*64 and artifact.table.nucleus_roi(0) is not None
    assert signature(artifact.table.cell_feature(0)) == signature(result.features[0])
    parameters_copy = origin.source.parameters; parameters_copy.threshold += 1
    assert origin.source.parameters.threshold == result.parameters.threshold
    # Retained classifier provenance survives removal of the original raster archive.
    assert doc.remove_cells(source_id)
    project = q.Project(); image = project.add_document(doc); path = tmp_path/'rgb.qpxproj'; project.save(str(path))
    loaded = q.Project.open(str(path)).document(image).classifier_records[0].archive
    assert loaded.sha256 == artifact.sha256
    for row in range(len(ids)):
        assert signature(loaded.table.cell_feature(row)) == signature(result.features[row])
        assert loaded.table.cell_origin(row).source.input_sha256 == result.nuclei.input_sha256
    np.testing.assert_array_equal(loaded.table.numpy(), artifact.table.numpy())


def test_failed_history_budget_never_inserts_a_prediction_audit():
    doc, ids = workspace()
    _, job = predicted(doc, ids)
    doc.set_history_limits(128, 1)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError), match='history'):
        job.apply(doc)
    assert doc.classifier_record_count == 0 and doc.revision == revision
    assert doc.annotation(ids[0]).class_name == 'small' and job.poll(doc).state == 'ready'
    doc.set_history_limits(128, 64*1024*1024)
    assert job.apply(doc) == ids and doc.classifier_record_count == 1


@pytest.mark.skipif(not q.has_project or not q.has_geos, reason='requires project and GEOS')
def test_legacy_non_utf8_source_metadata_fails_before_audit_publication(tmp_path):
    from test_cell_feature_links import setup
    doc, parent, _, _, source_id = setup()
    ids = doc.publish_cell_record(source_id)
    project = q.Project(); image = project.add_document(doc); path = tmp_path/'legacy-text.qpxproj'
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("UPDATE cell_records SET producer=CAST(X'ff' AS TEXT)")
    loaded = q.Project.open(str(path)).document(image)
    replay = q.CellRequest.replay(loaded, source_id)
    assert loaded.verify_cells(source_id, replay, q.detect_cells(replay))
    options = q.ObjectFeatureOptions(); options.cell_rgb = True
    request = q.ObjectFeatureRequest(loaded, ids, options)
    model = model_for(q.extract_object_features(request).features)
    job = q.ObjectClassifierJob(); revision = loaded.revision
    assert job.predict(request, model)
    status = job.wait(loaded)
    assert status.state == 'failed' and status.error and job.archive is None
    assert loaded.revision == revision and loaded.classifier_record_count == 0


@pytest.mark.skipif(not q.has_project, reason='project storage disabled')
def test_batch_preserves_classifier_records_and_seals_their_contents(tmp_path):
    doc, ids = workspace(); _, job = predicted(doc, ids); job.apply(doc)
    audit = doc.classifier_records[0]
    project = q.Project(); image = project.add_document(doc, 'Archived classifier')
    selection = q.ProjectBatchSelection(); selection.image_id = image; selection.object_ids = ids[:1]
    options = q.ProjectBatchOptions(); options.object_kind = q.ObjectKind.CELL
    request = q.ProjectBatchRequest(project, q.StainTransform([[1,0,0],[0,1,0]]), [selection], options)
    result = q.run_project_batch(request, str(tmp_path/'batch'))
    assert result.state == 'succeeded', q.project_batch_report_json(result)
    path = Path(result.images[0].path)
    restored = q.Project.open(str(path)).document(image)
    assert restored.classifier_record(audit.id).archive.sha256 == audit.archive.sha256
    assert q.batch_checkpoint_csv(str(path))
    with sqlite3.connect(path) as db: db.execute('DELETE FROM classifier_records')
    with pytest.raises((ValueError, RuntimeError)): q.batch_checkpoint_csv(str(path))
