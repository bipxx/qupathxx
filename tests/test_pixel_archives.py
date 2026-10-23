"""Pixel cohorts/masks: portable immutable artifacts, strict authority, schema9 and recovery."""
import gc
import hashlib
import sqlite3
import struct
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q
from test_pixel_workflows import scene

pytestmark=pytest.mark.skipif(not q.has_classifier_training,reason="optional classifier training backend")
project_only=pytest.mark.skipif(not q.has_project,reason="optional project backend")


def training():
    doc,a,b,options=scene();request=q.PixelTrainingRequest(doc,[a,b],options=options);table=q.sample_pixel_training(request)
    params=q.pixel_forest_parameters();params.trees=8;params.max_depth=4
    model=q.train_pixel_classifier(table,params);artifact=q.archive_pixel_training(table,model)
    return doc,[a,b],request,table,model,artifact


def prediction(doc,model):
    obj=doc.add_roi(q.Roi.polygon([q.Point(3,3),q.Point(60,4),q.Point(56,44),q.Point(8,40)],[[q.Point(12,12),q.Point(12,19),q.Point(19,19),q.Point(19,12)]]))
    request=q.PixelPredictionRequest(doc,obj,model);result=q.predict_pixel_region(request)
    return obj,request,result,q.archive_pixel_prediction(result)


def test_training_snapshot_roundtrip_original_cohort_numeric_model_and_lifetime():
    doc,ids,request,table,model,archive=training()
    assert archive.purpose=="training" and archive.table is table and archive.result is None
    assert archive.model.sha256==model.sha256 and archive.payload_bytes>=len(archive.to_bytes())
    assert [a.id for a in archive.annotations]==ids
    for a,obj_id in zip(archive.annotations,ids):
        obj=doc.annotation(obj_id);assert a.revision==obj.revision and a.kind==obj.kind and a.class_name==obj.class_name and a.geometry.wkt==obj.geometry.wkt
    reopened=q.PixelClassifierArchive.from_bytes(archive.to_bytes())
    assert reopened.sha256==archive.sha256 and reopened.to_bytes()==archive.to_bytes()
    assert q.archive_pixel_training(reopened.table,reopened.model).to_bytes()==archive.to_bytes()
    for method in ["numpy","labels_numpy","coordinates_numpy","annotation_ids_numpy"]:
        np.testing.assert_array_equal(getattr(reopened.table,method)(),getattr(table,method)())
        assert not getattr(reopened.table,method)().flags.writeable
    assert reopened.table.sampled_class_counts==table.sampled_class_counts and reopened.table.input_sha256==table.input_sha256
    assert q.train_pixel_classifier(reopened.table,model.forest.parameters).forest.training_sha256==model.forest.training_sha256
    original=reopened.annotations[0].geometry.wkt;doc.update_roi(ids[0],q.Roi.rectangle(q.Rect(0,0,3,3)))
    assert archive.annotations[0].geometry.wkt==original
    values=reopened.table.numpy();del archive,reopened,doc,table,request;gc.collect();assert values.shape==(120,6)


def test_prediction_roundtrip_masks_holes_threshold_and_no_restored_authority():
    doc,_,_,_,model,_=training();obj,request,result,archive=prediction(doc,model)
    assert archive.table is None and archive.result is result and archive.purpose=="prediction"
    loaded=q.PixelClassifierArchive.from_bytes(archive.to_bytes());assert loaded.sha256==archive.sha256
    assert loaded.result.sha256==result.sha256 and loaded.result.input_sha256==result.input_sha256
    assert q.archive_pixel_prediction(loaded.result).to_bytes()==archive.to_bytes()
    for method in ["labels_numpy","valid_numpy","vote_fractions_numpy"]:
        np.testing.assert_array_equal(getattr(loaded.result,method)(),getattr(result,method)())
    assert not loaded.result.matches(request) and result.matches(request)
    revision=doc.revision
    with pytest.raises(ValueError):doc.keep_pixel_prediction(request,loaded)
    assert doc.revision==revision
    if q.has_geos:
        traced=q.trace_pixel_prediction(loaded.result)
        with pytest.raises(RuntimeError):q.publish_pixel_contours(doc,request,traced)
    saved=loaded.result.labels_numpy();del doc,request,result,loaded,archive;gc.collect();assert saved.size>0


@pytest.mark.parametrize("kind",["training","prediction"])
def test_record_keep_consumes_current_revision_remove_and_historical_values(kind):
    doc,ids,request,table,model,archive=training()
    keep=doc.keep_pixel_training
    if kind=="prediction":_,request,_,archive=prediction(doc,model);keep=doc.keep_pixel_prediction
    doc.clear_history();revision=doc.revision;record=keep(request,archive,"Retained pixels")
    assert record==doc.revision==revision+1 and not doc.can_undo and doc.pixel_classifier_record_count==1
    saved=doc.pixel_classifier_record(record);assert saved.name=="Retained pixels" and saved.archive is archive
    with pytest.raises(ValueError):keep(request,archive)
    assert doc.remove_pixel_classifier_record(record) and not doc.remove_pixel_classifier_record(record)
    assert doc.pixel_classifier_records==[] and doc.pixel_classifier_record_count==0 and saved.archive.sha256==archive.sha256


@pytest.mark.parametrize("change",["request","edit","undo","close","document","decoded","wrong_kind"])
def test_training_authority_rejection_never_changes_document(change):
    doc,ids,request,table,model,archive=training()
    if change=="request":request=q.PixelTrainingRequest(doc,ids,options=request.options)
    elif change in ("edit","undo"):
        doc.rename_annotation(ids[0],"Changed")
        if change=="undo":doc.undo()
    elif change=="close":doc.server.close()
    elif change=="document":
        other=q.Document(doc.server)
        for obj in doc.annotations:other.add_roi(obj.geometry)
        doc=other
    elif change=="decoded":archive=q.PixelClassifierArchive.from_bytes(archive.to_bytes())
    else:_,_,_,archive=prediction(doc,model)
    revision=doc.revision
    with pytest.raises(ValueError):doc.keep_pixel_training(request,archive)
    assert doc.revision==revision and doc.pixel_classifier_record_count==0


@pytest.mark.parametrize("name",["","x"*257,"bad\x00name"])
def test_bad_record_name_rolls_back_without_retiring_current_request(name):
    doc,_,request,_,_,archive=training();revision=doc.revision
    with pytest.raises(ValueError):doc.keep_pixel_training(request,archive,name)
    assert doc.revision==revision and request.matches(doc) and doc.pixel_classifier_record_count==0


def resign(b):
    b[-64:]=hashlib.sha256(b[:-64]).hexdigest().encode();return bytes(b)


def blob_end(b,at):return at+8+struct.unpack_from("<Q",b,at)[0]


@pytest.mark.parametrize("kind",["training","prediction"])
@pytest.mark.parametrize("damage",["empty","truncated","checksum","kind","annotation_id","annotation_revision","huge_count","model"])
def test_bounded_decoding_rejects_malformed_resigned_artifacts(kind,damage):
    doc,_,_,_,model,archive=training()
    if kind=="prediction":_,_,_,archive=prediction(doc,model)
    b=bytearray(archive.to_bytes())
    if damage=="empty":b=bytearray()
    elif damage=="truncated":b=b[:-73]
    elif damage=="checksum":b[-1]^=1
    elif damage=="kind":struct.pack_into("<Q",b,4,2);b=bytearray(resign(b))
    elif damage=="model":b[20]^=1;b=bytearray(resign(b))
    else:
        at=blob_end(b,12);at=blob_end(b,at) # model and source provenance
        if damage=="huge_count":struct.pack_into("<Q",b,at,2**63)
        else:struct.pack_into("<Q",b,at+8+(8 if damage=="annotation_revision" else 0),0)
        b=bytearray(resign(b))
    with pytest.raises((ValueError,RuntimeError)):q.PixelClassifierArchive.from_bytes(bytes(b))


def _project_recovery_async_save_and_retained_removed_cohort(tmp_path):
    doc,ids,request,_,model,archive=training();train_id=doc.keep_pixel_training(request,archive)
    obj,request,result,mask=prediction(doc,model);prediction_id=doc.keep_pixel_prediction(request,mask)
    project=q.Project();image=project.add_document(doc);path=tmp_path/"pixels.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:assert db.execute("PRAGMA user_version").fetchone()[0]==12 and db.execute("SELECT count(*) FROM pixel_classifier_records").fetchone()[0]==2
    opened=q.Project.open(str(path));loaded=opened.document(image)
    assert loaded.pixel_classifier_record(train_id).archive.sha256==archive.sha256 and loaded.pixel_classifier_record(prediction_id).archive.sha256==mask.sha256
    assert not opened.dirty
    snapshot=q.ProjectSaveJob();assert snapshot.request(project,str(path));doc.remove_objects(ids+[obj]);doc.remove_pixel_classifier_record(train_id)
    assert snapshot.wait().state=="saved" and project.dirty
    captured=q.Project.open(str(path)).document(image);assert captured.object_count==3 and captured.pixel_classifier_record_count==2
    project.save(str(path));removed=q.Project.open(str(path)).document(image)
    assert removed.object_count==0 and removed.pixel_classifier_record(prediction_id).archive.sha256==mask.sha256
    project.rename_image(image,"Recovery snapshot")
    recovery=tmp_path/"pixels.qpxrecover";autosave=q.ProjectAutosave();assert autosave.request(project,str(recovery));doc.remove_pixel_classifier_record(prediction_id)
    assert autosave.wait().state=="ready"
    recovered=q.Project.recover(str(recovery),str(path)).document(image)
    assert recovered.object_count==0 and recovered.pixel_classifier_record_count==1 and recovered.pixel_classifier_record(prediction_id).archive.sha256==mask.sha256


@project_only
@pytest.mark.parametrize("damage",["checksum","identity","revision","orphan"])
def test_project_record_corruption_is_readonly_rejected(tmp_path,damage):
    doc,_,request,_,_,archive=training();record=doc.keep_pixel_training(request,archive)
    project=q.Project();project.add_document(doc);path=tmp_path/"corrupt.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        payload=bytearray(db.execute("SELECT payload FROM pixel_classifier_records").fetchone()[0])
        if damage=="checksum":payload[-1]^=1
        elif damage=="revision":struct.pack_into("<Q",payload,4,2**63);payload=bytearray(resign(payload))
        elif damage=="identity":db.execute("UPDATE pixel_classifier_records SET id=?",(str(record+1),))
        else:db.execute("UPDATE pixel_classifier_records SET image_id='999'")
        db.execute("UPDATE pixel_classifier_records SET payload=?",(payload,))
    before=path.read_bytes()
    with pytest.raises((ValueError,RuntimeError)):q.Project.open(str(path))
    assert path.read_bytes()==before


@project_only
def test_schema8_migration_is_transactional_and_preserves_existing_data(tmp_path):
    doc,_,request,_,_,archive=training();project=q.Project();image=project.add_document(doc);path=tmp_path/"legacy.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE tiff_views");db.execute("DROP TABLE tma_cores");db.execute("DROP TABLE tma_grids")
        db.execute("DROP TABLE pixel_stream_records");db.execute("DROP TABLE pixel_classifier_records");db.execute("PRAGMA user_version=8")
    opened=q.Project.open(str(path));loaded=opened.document(image)
    assert loaded.pixel_classifier_record_count==0
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_save BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected save failure'); END")
    with pytest.raises(RuntimeError):opened.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0]==8
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='pixel_classifier_records'").fetchone()[0]==0
        db.execute("DROP TRIGGER fail_save")
    opened.save(str(path))
    with sqlite3.connect(path) as db:assert db.execute("PRAGMA user_version").fetchone()[0]==12
    assert q.Project.open(str(path)).document(image).object_count==2



def test_training_factory_rejects_unrelated_cohort_and_resolution():
    doc,ids,request,table,model,_=training();options=request.options;options.seed=19
    other=q.sample_pixel_training(q.PixelTrainingRequest(doc,ids,options=options));parameters=model.forest.parameters
    unrelated=q.train_pixel_classifier(other,parameters)
    with pytest.raises(ValueError):q.archive_pixel_training(table,unrelated)
    resolution=q.PixelResolution();resolution.downsample=2
    changed=q.PixelClassifier(model.forest,model.options,model.transform,resolution)
    with pytest.raises(ValueError):q.archive_pixel_training(table,changed)


@project_only
def test_batch_preserves_pixels_and_binds_them_into_checkpoint_digest(tmp_path):
    doc,_,request,_,_,archive=training();record=doc.keep_pixel_training(request,archive)
    project=q.Project();image=project.add_document(doc)
    batch=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]))
    report=q.run_project_batch(batch,str(tmp_path));assert report.state=="succeeded",q.project_batch_report_json(report)
    path=report.images[0].path;loaded=q.Project.open(path).document(image)
    assert loaded.pixel_classifier_record(record).archive.sha256==archive.sha256 and q.batch_checkpoint_csv(path)
    with sqlite3.connect(path) as db:db.execute("DELETE FROM pixel_classifier_records")
    with pytest.raises((ValueError,RuntimeError)):q.batch_checkpoint_csv(path)


@project_only
def test_batch_materialization_budget_includes_pixel_payload(tmp_path):
    doc,_,request,_,_,archive=training();doc.keep_pixel_training(request,archive)
    project=q.Project();project.add_document(doc);options=q.ProjectBatchOptions();options.materialize_budget=archive.payload_bytes
    batch=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=options)
    report=q.run_project_batch(batch,str(tmp_path))
    assert report.state=="failed" and report.failed==1 and "budget" in report.images[0].error



def payload_start(b):
    at=blob_end(b,12);at=blob_end(b,at);count=struct.unpack_from("<Q",b,at)[0];at+=8
    for _ in range(count):
        at+=24;at=blob_end(b,at);at=blob_end(b,at)
    return at


@pytest.mark.parametrize("field",["work","tiles","calibration"])
def test_training_decoder_rejects_resigned_inconsistent_audit_metadata(field):
    doc,a,b,options=scene();doc.set_calibration(.5,.25);request=q.PixelTrainingRequest(doc,[a,b],options=options);table=q.sample_pixel_training(request)
    parameters=q.pixel_forest_parameters();parameters.trees=8;parameters.max_depth=4;model=q.train_pixel_classifier(table,parameters)
    data=bytearray(q.archive_pixel_training(table,model).to_bytes());start=payload_start(data)
    offset={"calibration":8,"tiles":104,"work":112}[field]
    value=0 if field!="tiles" else table.scanned_pixels+1
    struct.pack_into("<Q",data,start+offset,value)
    with pytest.raises((ValueError,RuntimeError)):q.PixelClassifierArchive.from_bytes(resign(data))


@pytest.mark.parametrize("field",["snapshot","feature_budget","tiles","nan"])
def test_prediction_decoder_rejects_resigned_inconsistent_audit_metadata(field):
    doc,_,_,_,model,_=training();_,_,result,archive=prediction(doc,model);data=bytearray(archive.to_bytes());start=payload_start(data)
    if field=="nan":
        invalid=int(np.flatnonzero(result.valid_numpy().ravel()==0)[0]);array_start=start+264+8*len(model.classes)
        struct.pack_into("<I",data,array_start+invalid*9+5,0x7fa00001)
    else:
        offset={"snapshot":64,"feature_budget":80,"tiles":88}[field]
        value=result.width*result.height*9 if field=="snapshot" else 1 if field=="feature_budget" else 999999
        struct.pack_into("<Q",data,start+offset,value)
    with pytest.raises((ValueError,RuntimeError)):q.PixelClassifierArchive.from_bytes(resign(data))



@pytest.mark.parametrize("kind",["training","prediction"])
def test_artifact_preparation_outlives_source_and_owner_request(kind):
    doc,_,request,table,model,archive=training()
    result=None
    if kind=="prediction":_,request,result,archive=prediction(doc,model)
    doc.server.close();del doc,request,archive;gc.collect()
    prepared=q.archive_pixel_training(table,model) if kind=="training" else q.archive_pixel_prediction(result)
    assert q.PixelClassifierArchive.from_bytes(prepared.to_bytes()).sha256==prepared.sha256



@project_only
def test_project_recovery_async_save_and_retained_removed_cohort(tmp_path):
    # wait() holds the caller's GIL. Native worker destruction must not need it.
    import subprocess
    import sys
    tests=Path(__file__).resolve().parent
    code="import sys;from pathlib import Path;sys.path.insert(0,"+repr(str(tests))+");from test_pixel_archives import _project_recovery_async_save_and_retained_removed_cohort as check;check(Path(sys.argv[1]))"
    completed=subprocess.run([sys.executable,"-I","-c",code,str(tmp_path)],capture_output=True,text=True,timeout=30)
    assert completed.returncode==0,completed.stdout+completed.stderr



@project_only
@pytest.mark.parametrize("kind",["training","prediction"])
def test_captured_counter_prevents_reuse_of_deleted_unselected_id(tmp_path,kind):
    doc,ids,request,_,model,_=training();target=0
    if kind=="prediction":target,_,_,_=prediction(doc,model)
    ghost=doc.add_rectangle(q.Rect(1,1,1,1));doc.remove_annotation(ghost)
    if kind=="training":
        request=q.PixelTrainingRequest(doc,ids,options=request.options);table=q.sample_pixel_training(request)
        model=q.train_pixel_classifier(table,model.forest.parameters);archive=q.archive_pixel_training(table,model)
        record=doc.keep_pixel_training(request,archive)
    else:
        request=q.PixelPredictionRequest(doc,target,model);archive=q.archive_pixel_prediction(q.predict_pixel_region(request));record=doc.keep_pixel_prediction(request,archive)
    assert doc.pixel_classifier_record(record).captured_next_id==ghost+1
    project=q.Project();project.add_document(doc);path=tmp_path/"counter.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:db.execute("UPDATE images SET next_object=?",(str(ghost),))
    with pytest.raises(ValueError,match="bounds"):q.Project.open(str(path))


@project_only
def test_pixel_record_identity_cannot_collide_with_analysis_namespace(tmp_path):
    doc,ids,request,_,_,archive=training();pixel=doc.keep_pixel_training(request,archive)
    analysis=q.AnalysisRequest(doc,ids[0],q.StainTransform([[1,0,0],[0,1,0]]));other=doc.keep_analysis(analysis,q.analyze(analysis))
    project=q.Project();project.add_document(doc);path=tmp_path/"namespace.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        payload=bytearray(db.execute("SELECT payload FROM pixel_classifier_records").fetchone()[0]);struct.pack_into("<Q",payload,4,other)
        db.execute("UPDATE pixel_classifier_records SET id=?,payload=?",(str(other),resign(payload)))
    with pytest.raises(ValueError,match="duplicate pixel record"):q.Project.open(str(path))
