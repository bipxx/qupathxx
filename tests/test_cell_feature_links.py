import sqlite3

import pytest
import qupathxx as q

from test_cell_records import capture
from test_cells import phantom, job_for
from test_tissue import document

pytestmark=pytest.mark.skipif(not q.has_geos,reason="cell publication requires GEOS")


def setup():
    doc=document(phantom(64))
    obj=doc.add_rectangle(q.Rect(0,0,64,64))
    request,result,record=capture(doc,obj)
    return doc,obj,request,result,record


def signature(f):
    return (f.label,f.nucleus_pixels,f.cell_pixels,f.nucleus_rgb,f.cell_rgb,f.cytoplasm_rgb,
            f.nucleus_channels,f.cell_channels,f.cytoplasm_channels)


def test_live_publication_retains_one_run_and_undo_restores_feature_identity():
    doc,obj,job=job_for()
    assert job.wait(doc,obj).state=="ready"
    doc.clear_history()
    ids=job.keep_result(doc,obj)
    assert len(ids)==job.result.cell_count>0 and doc.cell_record_count==1
    record=doc.cell_records[0]
    for cell,f in zip(ids,job.result.features):
        link=doc.annotation(cell).cell_feature_link
        assert (link.record_id,link.label)==(record.id,f.label)
        assert doc.cell_object_status(cell)=="current" and signature(doc.cell_object_feature(cell))==signature(f)
    assert doc.undo() and doc.object_count==1 and doc.cell_record_count==1 and not doc.can_undo
    assert doc.redo() and doc.children(obj)==ids
    assert all(doc.annotation(i).cell_feature_link.record_id==record.id for i in ids)


def test_retained_result_is_reused_and_renames_classification_reparent_preserve_links():
    doc,obj,request,result,record=setup()
    ids=doc.publish_cells(request,result,class_name="Tumor")
    assert doc.cell_record_count==1
    cell=ids[0];a=doc.annotation(cell)
    doc.execute([q.ObjectEdit.replace_geometry(cell,a.geometry,"renamed",0,"Other",a.kind,a.nucleus)])
    doc.classify_objects([cell],"Changed")
    assert doc.cell_object_status(cell)=="current" and doc.annotation(cell).cell_feature_link.record_id==record
    assert signature(doc.cell_object_feature(cell))==signature(result.features[0])
    assert doc.undo() and doc.undo() and doc.annotation(cell).parent_id==obj


def test_nucleus_edit_reports_historical_features_and_undo_recovers_status():
    doc,_,_,result,record=setup()
    cell=doc.publish_cell_record(record)[0]
    doc.update_nucleus(cell,None)
    assert doc.annotation(cell).cell_feature_link.record_id==record
    assert doc.cell_object_status(cell)=="geometry_changed"
    assert signature(doc.cell_object_feature(cell))==signature(result.features[0])
    assert doc.undo() and doc.cell_object_status(cell)=="current"
    doc.update_roi(cell,doc.annotation(cell).geometry)  # A no-op keeps identity.
    assert doc.cell_object_status(cell)=="current"


def test_archive_removal_leaves_an_explicit_missing_link_without_borrowed_features():
    doc,_,_,_,record=setup()
    cell=doc.publish_cell_record(record)[0]
    assert doc.remove_cells(record)
    assert doc.cell_object_status(cell)=="archive_missing" and doc.annotation(cell).cell_feature_link.record_id==record
    with pytest.raises(IndexError):doc.cell_object_feature(cell)
    assert doc.remove_annotation(cell) and doc.undo() and doc.cell_object_status(cell)=="archive_missing"


def test_manual_cells_have_no_captured_features():
    doc=document(phantom(64))
    cell=doc.add_cell(q.Roi.rectangle(q.Rect(1,1,4,4)))
    assert doc.annotation(cell).cell_feature_link is None and doc.cell_object_status(cell)=="no_features"
    with pytest.raises(ValueError,match="no cell feature"):doc.cell_object_feature(cell)


@pytest.mark.parametrize("failure",["name","history"])
def test_live_publication_failure_rolls_back_archive_objects_and_revision(failure):
    doc,obj,request,result,record=setup()
    doc.remove_cells(record)
    if failure=="history":doc.set_history_limits(100,1)
    before=(doc.revision,doc.object_count,doc.cell_record_count,doc.can_undo,doc.can_redo)
    with pytest.raises((ValueError,RuntimeError,IndexError)):
        doc.publish_cells(request,result,name_prefix="x"*4097 if failure=="name" else "Cell")
    assert (doc.revision,doc.object_count,doc.cell_record_count,doc.can_undo,doc.can_redo)==before
    if failure=="history":doc.set_history_limits(100,10*1024*1024)
    ids=doc.publish_cells(request,result)
    assert ids[0]==obj+1 and doc.cell_record_count==1


@pytest.mark.skipif(not q.has_project,reason="project storage")
@pytest.mark.parametrize("missing",[False,True])
def test_schema6_roundtrip_preserves_links_status_and_snapshot_after_removal(tmp_path,missing):
    doc,obj,_,result,record=setup()
    ids=doc.publish_cell_record(record)
    if missing:doc.remove_cells(record)
    project=q.Project();image=project.add_document(doc)
    path=tmp_path/"links.qpxproj"
    project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0]== 11
        assert db.execute("SELECT count(*) FROM cell_feature_links").fetchone()[0]==len(ids)
    loaded=q.Project.open(str(path)).document(image)
    assert loaded.children(obj)==ids
    for i,f in zip(ids,result.features):
        link=loaded.annotation(i).cell_feature_link
        assert (link.record_id,link.label)==(record,f.label)
        assert loaded.cell_object_status(i)==("archive_missing" if missing else "unverified")
        if not missing:assert signature(loaded.cell_object_feature(i))==signature(f)
    if not missing:
        with pytest.raises(ValueError,match="current verified"):loaded.publish_cell_record(record)
        replay=q.CellRequest.replay(loaded,record);assert loaded.verify_cells(record,replay,q.detect_cells(replay))
        assert all(loaded.cell_object_status(i)=="current" for i in ids)
        assert len(loaded.publish_cell_record(record))==len(ids)
    checkpoint=tmp_path/"links.qpxrecover"
    doc.rename_annotation(ids[-1],"dirty before capture")
    auto=q.ProjectAutosave();assert auto.request(project,str(checkpoint))
    doc.remove_annotation(ids[0])
    assert auto.wait().completed==1
    recovered=q.Project.recover(str(checkpoint)).document(image)
    assert recovered.annotation(ids[0]).cell_feature_link.record_id==record


@pytest.mark.skipif(not q.has_project,reason="project storage")
@pytest.mark.parametrize("mutation",[
    "UPDATE cell_feature_links SET label=0",
    "UPDATE cell_feature_links SET label=999999",
    "UPDATE cell_feature_links SET record_id='0'",
    "UPDATE cell_feature_links SET record_id='18446744073709551615'",
    "UPDATE cell_feature_links SET object_id='999999' WHERE label=1",
    "UPDATE cell_feature_links SET image_id='999999'",
    "UPDATE cell_feature_links SET object_id='1' WHERE label=1",
])
def test_malformed_cell_links_are_rejected_without_rewriting_file(tmp_path,mutation):
    doc,_,_,_,record=setup();doc.publish_cell_record(record)
    project=q.Project();project.add_document(doc)
    path=tmp_path/"bad.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:db.execute(mutation)
    before=path.read_bytes()
    with pytest.raises((ValueError,RuntimeError,IndexError)):q.Project.open(str(path))
    assert path.read_bytes()==before


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_schema5_upgrade_is_transactional(tmp_path):
    doc,_,_,_,_=setup();project=q.Project();project.add_document(doc)
    path=tmp_path/"v5.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE tma_cores");db.execute("DROP TABLE tma_grids")
        db.execute("DROP TABLE pixel_stream_records");db.execute("DROP TABLE pixel_classifier_records");db.execute("DROP TABLE classifier_records");db.execute("DROP TABLE analysis_provenance");db.execute("DROP TABLE cell_feature_links");db.execute("PRAGMA user_version=5")
        db.execute("CREATE TRIGGER fail_links BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected link migration'); END")
    loaded=q.Project.open(str(path));before=path.read_bytes()
    with pytest.raises(RuntimeError,match="injected link migration"):loaded.save(str(path))
    assert path.read_bytes()==before
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0]==5
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='cell_feature_links'").fetchone()[0]==0
        db.execute("DROP TRIGGER fail_links")
    loaded.save(str(path))
    with sqlite3.connect(path) as db:assert db.execute("PRAGMA user_version").fetchone()[0]== 11


@pytest.mark.skipif(not q.has_interchange,reason="GeoJSON")
def test_geojson_explicitly_reports_lost_run_links_and_import_cannot_rebind_local_ids():
    doc,_,_,_,record=setup();ids=doc.publish_cell_record(record)
    with pytest.raises(ValueError,match="feature links"):q.export_geojson(doc)
    report=q.export_geojson(doc,allow_losses=True)
    assert any(i.code=="cell_features_dropped" and i.count==len(ids) for i in report.issues)
    target=document(phantom(64));imported=q.import_geojson(target,report.text)
    assert all(target.annotation(i).cell_feature_link is None for i in imported.imported_ids)


@pytest.mark.skipif(not q.has_project,reason="project transport")
def test_publication_revision_exhaustion_restores_private_archive_insertion(tmp_path):
    doc,obj,_,_,_=setup();project=q.Project();image=project.add_document(doc)
    path=tmp_path/"exhausted.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:db.execute("UPDATE images SET revision=?",(str(2**64-2),))
    loaded=q.Project.open(str(path)).document(image)
    request=q.CellRequest(loaded,obj,q.StainTransform([[1,0,0],[0,1,0]]))
    result=q.detect_cells(request);before=(loaded.revision,loaded.object_count,loaded.cell_record_count)
    with pytest.raises(OverflowError):loaded.publish_cells(request,result)
    assert (loaded.revision,loaded.object_count,loaded.cell_record_count)==before


@pytest.mark.parametrize("change",["rename","close"])
def test_feature_status_tracks_source_authority_without_rewriting_values(change):
    doc,obj,_,result,record=setup();cell=doc.publish_cell_record(record)[0]
    if change=="rename":doc.rename_annotation(obj,"changed")
    else:doc.server.close()
    assert doc.cell_object_status(cell)==("object_changed" if change=="rename" else "source_closed")
    assert signature(doc.cell_object_feature(cell))==signature(result.features[0])


def test_conversion_to_other_object_kind_drops_link_and_undo_restores_it():
    doc,_,_,_,record=setup();cell=doc.publish_cell_record(record)[0];a=doc.annotation(cell)
    doc.execute([q.ObjectEdit.replace_geometry(cell,a.geometry,a.name,a.parent_id,a.class_name,q.ObjectKind.DETECTION)])
    assert doc.annotation(cell).cell_feature_link is None and doc.cell_object_status(cell)=="no_features"
    assert doc.undo() and doc.annotation(cell).cell_feature_link.record_id==record and doc.cell_object_status(cell)=="current"


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_numeric_archive_remains_readable_when_derived_contours_exceed_limits(tmp_path):
    import struct
    from test_cell_records import rewrite_checksum
    doc,_,_,result,record=setup();cell=doc.publish_cell_record(record)[0]
    project=q.Project();image=project.add_document(doc);path=tmp_path/"numeric-only.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        data=bytearray(db.execute("SELECT run FROM cell_records").fetchone()[0])
        struct.pack_into("<Q",data,156,4)  # Original contour vertex budget.
        rewrite_checksum(data,result.nuclei.roi)
        db.execute("UPDATE cell_records SET run=?",(data,))
    loaded=q.Project.open(str(path)).document(image)
    assert loaded.cell_object_status(cell)=="geometry_unavailable"
    assert signature(loaded.cell_object_feature(cell))==signature(result.features[0])
    assert loaded.cell_record(record).result.cell_contours is None
