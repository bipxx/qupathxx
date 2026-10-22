"""Joined live cell identity and immutable archived measurements."""
import csv
import gc
import io
import os
import sqlite3

import pytest
import qupathxx as q
from test_cell_feature_links import setup


def options(current=False):
    value=q.CellTableOptions();value.published_objects=True;value.require_current=current;return value


def rows(value):
    reader=csv.DictReader(io.StringIO(value,newline=""));data=list(reader)
    assert len(reader.fieldnames)==len(set(reader.fieldnames)) and all(None not in r for r in data)
    return data


def published():
    doc,source,request,result,record=setup()
    ids=doc.publish_cells(request,result,class_name="Tumor")
    return doc,source,result,record,ids


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_join_preserves_every_historical_column_and_captures_live_metadata():
    doc,source,result,record,ids=published();doc.set_class("Tumor",0x123456)
    doc.rename_annotation(ids[0],'named,"cell"\r\n\u96ea');doc.set_calibration(.5,.25)
    historical=rows(q.cell_measurements_csv(doc));request=q.CellTableRequest(doc,options=options())
    actual=rows(q.cell_measurements_csv(request))
    assert request.row_count==len(ids) and request.record_count==1 and request.image_count==1
    assert len(actual[0])==92 and [int(r["object_id"]) for r in actual]==ids
    for row,old,cell in zip(actual,historical,ids):
        assert {k:row[k] for k in old if k!="table_schema"}=={k:v for k,v in old.items() if k!="table_schema"}
        a=doc.annotation(cell);shape=doc.shape_measurements(cell)
        assert row["table_schema"]=="qpx.cell-object-table/1" and row["object_name"]==a.name
        assert int(row["object_revision"])==a.revision and int(row["object_parent_id"])==source
        assert row["object_class"]=="Tumor" and int(row["object_class_color"])==0x123456
        assert row["object_status_at_capture"]=="current" and float(row["export_pixel_width_um"])==.5
        for compartment,prefix in [(shape.roi,"object"),(shape.nucleus,"object_nucleus")]:
            assert float(row[prefix+"_area_px2"])==compartment.area_px2
            assert float(row[prefix+"_length_px"])==compartment.length_px
            assert float(row[prefix+"_area_um2"])==compartment.area_um2
            assert float(row[prefix+"_length_um"])==compartment.length_um
    assert actual[0]["object_name"]=='named,"cell"\r\n\u96ea'


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_duplicate_publications_are_distinct_rows_and_run_filters_are_explicit():
    doc,source,result,record,ids=published();again=doc.publish_cell_record(record)
    # A second retained archive can share the exact request/result from a fresh run.
    request=q.CellRequest.replay(doc,record);other=doc.keep_cells(request,q.detect_cells(request))
    third=doc.publish_cell_record(other)
    manual=doc.add_cell(q.Roi.rectangle(q.Rect(1,1,2,2)))
    all_rows=rows(q.cell_measurements_csv(doc,options=options()))
    assert [int(r["object_id"]) for r in all_rows]==ids+again+third+[manual]
    filtered=rows(q.cell_measurements_csv(doc,[record,record],options()))
    assert [int(r["object_id"]) for r in filtered]==ids+again and {r["record_id"] for r in filtered}=={str(record)}
    with pytest.raises(IndexError):q.CellTableRequest(doc,[999999],options())


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_geometry_change_status_does_not_rewrite_historical_values_and_strict_mode_rejects():
    doc,_,_,record,ids=published();before=rows(q.cell_measurements_csv(doc,options=options()))
    doc.update_nucleus(ids[0],None)
    after=rows(q.cell_measurements_csv(doc,options=options()))
    assert after[0]["object_status_at_capture"]=="geometry_changed" and after[0]["status_at_capture"]=="current"
    assert after[0]["object_nucleus_area_px2"]==after[0]["object_nucleus_area_um2"]==""
    assert after[0]["nucleus_samples"]==before[0]["nucleus_samples"]
    with pytest.raises(ValueError,match="unchanged geometry"):q.CellTableRequest(doc,options=options(True))
    assert doc.undo();assert q.CellTableRequest(doc,options=options(True)).row_count==len(ids)
    assert doc.redo() and rows(q.cell_measurements_csv(doc,options=options()))[0]["object_status_at_capture"]=="geometry_changed"


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_removed_archive_retains_object_link_and_blank_measurements():
    doc,_,_,record,ids=published();doc.remove_cells(record)
    request=q.CellTableRequest(doc,options=options());actual=rows(q.cell_measurements_csv(request))
    assert request.record_count==0 and request.row_count==len(ids)
    for i,row in enumerate(actual,1):
        assert row["record_id"]==str(record) and row["cell_label"]==str(i)
        assert row["status_at_capture"]==row["object_status_at_capture"]=="archive_missing"
        assert row["nucleus_samples"]==row["producer"]==row["source_object_id"]==""
        assert float(row["object_area_px2"])>0 and row["image_width_px"]=="64"
    with pytest.raises(ValueError):q.CellTableRequest(doc,options=options(True))


def test_manual_cells_are_present_and_unknown_calibration_and_absent_nuclei_are_blank():
    doc=q.Document(q.Image.demo());cell=doc.add_cell(q.Roi.rectangle(q.Rect(1,2,4,6)))
    doc.add_rectangle(q.Rect(0,0,10,10));doc.add_detection(q.Roi.rectangle(q.Rect(1,1,2,2)))
    request=q.CellTableRequest(doc,options=options());row,=rows(q.cell_measurements_csv(request))
    assert request.row_count==1 and request.record_count==0 and row["object_id"]==str(cell)
    assert row["object_status_at_capture"]==row["status_at_capture"]=="no_features"
    assert row["record_id"]==row["cell_label"]==row["object_class_color"]==""
    assert row["object_nucleus_area_px2"]==row["object_area_um2"]==row["export_pixel_width_um"]==""
    assert float(row["object_area_px2"])==24 and float(row["object_length_px"])==20
    with pytest.raises(ValueError):q.CellTableRequest(doc,options=options(True))
    assert len(q.cell_measurements_csv(q.Document(q.Image.demo()),options=options()).splitlines())==1


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_snapshot_survives_all_live_edits_removal_source_close_and_wrapper_destruction():
    doc,source,_,record,ids=published();doc.set_calibration(.5,.25)
    request=q.CellTableRequest(doc,options=options());expected=q.cell_measurements_csv(request)
    doc.rename_annotation(ids[0],"changed");doc.set_class("Tumor",1);doc.set_calibration(8,9)
    doc.update_nucleus(ids[0],None);doc.remove_cells(record);doc.remove_annotation(source);doc.server.close()
    del doc;gc.collect();assert q.cell_measurements_csv(request)==expected


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason="project cell publication")
def test_project_join_disambiguates_local_object_ids_and_survives_catalog_changes():
    project=q.Project();docs=[];images=[];expected=[]
    for name in ['one,"image"\nline','\u96ea']:
        doc,_,_,_,_=published();image=project.add_document(doc,name);docs.append(doc);images.append(image)
        for row in rows(q.cell_measurements_csv(doc,options=options())):
            row.update(project_identity=project.identity,project_generation=str(project.generation),project_revision="0",
                       image_id=str(image),image_name=name,table_schema="qpx.project-cell-object-table/1")
            expected.append(row)
    request=q.CellTableRequest(project,[images[1],images[0],images[1]],options())
    for row in expected:row["project_revision"]=str(request.project_revision)
    actual=rows(q.cell_measurements_csv(request));assert actual==expected and len(actual[0])==97
    assert request.record_count==2 and request.image_count==2
    project.rename_image(images[0],"changed");project.remove_image(images[1]);docs[0].remove_cells(docs[0].cell_records[0].id)
    assert rows(q.cell_measurements_csv(request))==expected


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason="project cell publication")
def test_reopen_and_recovery_keep_join_identity_but_require_pixel_verification(tmp_path):
    doc,_,_,record,ids=published();project=q.Project();image=project.add_document(doc);path=tmp_path/"joined.qpxproj";project.save(str(path))
    loaded=q.Project.open(str(path));restored=loaded.document(image)
    actual=rows(q.cell_measurements_csv(loaded,options=options()))
    assert [int(r["object_id"]) for r in actual]==ids and {r["object_status_at_capture"] for r in actual}=={"unverified"}
    with pytest.raises(ValueError):q.CellTableRequest(loaded,options=options(True))
    request=q.CellRequest.replay(restored,record);assert restored.verify_cells(record,request,q.detect_cells(request))
    assert q.CellTableRequest(loaded,options=options(True)).row_count==len(ids)
    restored.rename_annotation(ids[0],"checkpoint");writer=q.ProjectAutosave();checkpoint=tmp_path/"joined.qpxrecover"
    assert writer.request(loaded,str(checkpoint)) and writer.wait().state=="ready"
    recovered=q.Project.recover(str(checkpoint));assert rows(q.cell_measurements_csv(recovered,options=options()))[0]["object_name"]=="checkpoint"


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_joined_snapshot_and_output_quotas_are_exact_and_worker_is_atomic(tmp_path):
    doc,_,_,_,ids=published();request=q.CellTableRequest(doc,options=options());expected=q.cell_measurements_csv(request)
    limits=options();limits.snapshot_budget=request.snapshot_bytes
    assert q.CellTableRequest(doc,options=limits).snapshot_bytes==request.snapshot_bytes
    limits.snapshot_budget-=1
    with pytest.raises((ValueError,RuntimeError),match="budget"):q.CellTableRequest(doc,options=limits)
    limits=options();limits.byte_limit=len(expected.encode());exact=q.CellTableRequest(doc,options=limits)
    assert q.cell_measurements_csv(exact)==expected
    path=tmp_path/"joined.csv";path.write_bytes(b"old");job=q.CellTableExportJob();assert job.request(exact,str(path))
    status=job.wait();assert status.state=="succeeded" and status.rows==len(ids) and path.read_bytes()==expected.encode()
    limits.byte_limit-=1;short=q.CellTableRequest(doc,options=limits);assert job.request(short,str(path))
    assert job.wait().state=="failed" and path.read_bytes()==expected.encode() and not list(tmp_path.glob("*.qpx-tmp-*"))


@pytest.mark.parametrize('calibration',[(.5,.25),(1e-200,1e200)])
def test_manual_ellipse_physical_shapes_match_native_measurements(calibration):
    doc=q.Document(q.Image.demo());cell=doc.add_cell(q.Roi.ellipse(q.Rect(1,2,4,6)))
    doc.set_calibration(*calibration);row,=rows(q.cell_measurements_csv(doc,options=options()));shape=doc.shape_measurements(cell).roi
    assert float(row["object_area_um2"])==shape.area_um2 and float(row["object_length_um"])==shape.length_um


@pytest.mark.skipif(not q.has_gui or not q.has_geos or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="desktop cell publication")
@pytest.mark.parametrize("project_scope",[False,True])
def test_desktop_join_export_needs_no_selected_source_and_matches_python(tmp_path,project_scope):
    doc,_,_,_,_=published();path=tmp_path/"gui-joined.csv";config=q.GuiOptions()
    config.cell_table_path=str(path);config.cell_table_published_objects=True
    export=q.GuiInput();export.frame,export.kind,export.shift=4,q.GuiInputKind.EXPORT_CELL_TABLE_KEY,project_scope
    ready=q.GuiInput();ready.frame,ready.kind,ready.wait_for=6,q.GuiInputKind.MOVE,q.GuiInputWait.CELL_TABLE_SAVED
    config.inputs=[export,ready]
    target=doc
    if project_scope:
        target=q.Project();target.add_document(doc,"joined image")
    expected=q.cell_measurements_csv(target,options=options())
    report=q.show(target,frame_limit=24,options=config)
    assert report.cell_table_state=="succeeded" and not report.cell_table_error and report.selected_ids==[]
    # Opening the viewer persists its initial camera/display view in the catalog.
    if project_scope:expected=q.cell_measurements_csv(target,options=options())
    assert path.read_bytes()==expected.encode()


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_closed_source_exports_historical_values_and_kind_conversion_removes_join_row():
    doc,_,_,_,ids=published();doc.server.close()
    actual=rows(q.cell_measurements_csv(doc,options=options()))
    assert {r["object_status_at_capture"] for r in actual}=={"source_closed"} and all(int(r["cell_samples"])>0 for r in actual)
    with pytest.raises(ValueError):q.CellTableRequest(doc,options=options(True))
    a=doc.annotation(ids[0]);doc.execute([q.ObjectEdit.replace_geometry(a.id,a.geometry,a.name,a.parent_id,a.class_name,q.ObjectKind.DETECTION)])
    assert [int(r["object_id"]) for r in rows(q.cell_measurements_csv(doc,options=options()))]==ids[1:]
    assert doc.undo() and q.CellTableRequest(doc,options=options()).row_count==len(ids)


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_empty_project_join_header_and_image_selection_are_explicit():
    project=q.Project();request=q.CellTableRequest(project,options=options())
    text=q.cell_measurements_csv(request);assert request.row_count==request.record_count==request.image_count==0
    assert len(next(csv.reader(io.StringIO(text))))==97 and rows(text)==[]
    doc=q.Document(q.Image.demo());doc.add_cell(q.Roi.rectangle(q.Rect(1,1,2,2)))
    image=project.add_document(doc,"manual")
    assert q.CellTableRequest(project,[image,image],options()).row_count==1
    with pytest.raises(IndexError):q.CellTableRequest(project,[image,999999],options())


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason="project cell publication")
def test_full_width_live_object_ids_revisions_and_order_keep_all_decimal_digits(tmp_path):
    doc,_,_,_,ids=published();project=q.Project();project.add_document(doc);path=tmp_path/"wide.qpxproj";project.save(str(path))
    maximum=str(2**64-1);wide=str(2**64-2)
    with sqlite3.connect(path) as db:
        db.execute("UPDATE objects SET id=?,revision=? WHERE id=?",(wide,wide,str(ids[0])))
        db.execute("UPDATE cell_feature_links SET object_id=? WHERE object_id=?",(wide,str(ids[0])))
        db.execute("UPDATE images SET next_object=?,revision=?",(maximum,maximum))
    actual=rows(q.cell_measurements_csv(q.Project.open(str(path)),options=options()))
    assert actual[-1]["object_id"]==actual[-1]["object_revision"]==wide
    assert all(r["export_document_revision"]==maximum for r in actual)
    assert [int(r["object_id"]) for r in actual]==sorted([int(wide),*ids[1:]])


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason="project cell publication")
def test_join_does_not_open_missing_linked_source_and_aggregate_budget_is_exact(tmp_path):
    from test_cells import phantom
    from test_cell_records import capture
    source=tmp_path/"source.ppm";source.write_bytes(b"P6\n64 64\n255\n"+phantom(64).tobytes())
    project=q.Project();image=project.add_image(str(source),reader="ppm");doc=project.document(image)
    obj=doc.add_rectangle(q.Rect(0,0,64,64));request,result,_=capture(doc,obj);ids=doc.publish_cells(request,result)
    project.add_document(q.Document(q.Image.demo()),"empty")
    path=tmp_path/"linked.qpxproj";project.save(str(path));source.unlink();loaded=q.Project.open(str(path))
    request=q.CellTableRequest(loaded,options=options());actual=rows(q.cell_measurements_csv(request))
    assert request.image_count==2 and request.row_count==len(ids) and all(r["object_status_at_capture"]=="unverified" for r in actual)
    limits=options();limits.snapshot_budget=request.snapshot_bytes
    assert q.CellTableRequest(loaded,options=limits).snapshot_bytes==request.snapshot_bytes
    limits.snapshot_budget-=1
    with pytest.raises((ValueError,RuntimeError),match="budget"):q.CellTableRequest(loaded,options=limits)


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason="project cell publication")
def test_numeric_only_archive_joins_features_when_contours_are_unavailable(tmp_path):
    import struct
    from test_cell_records import rewrite_checksum
    doc,_,result,_,ids=published();project=q.Project();image=project.add_document(doc);path=tmp_path/"numeric.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        data=bytearray(db.execute("SELECT run FROM cell_records").fetchone()[0]);struct.pack_into("<Q",data,156,4)
        rewrite_checksum(data,result.nuclei.roi);db.execute("UPDATE cell_records SET run=?",(data,))
    restored=q.Project.open(str(path)).document(image);actual=rows(q.cell_measurements_csv(restored,options=options()))
    assert len(actual)==len(ids) and {r["object_status_at_capture"] for r in actual}=={"geometry_unavailable"}
    assert all(int(r["cell_samples"])==f.cell_pixels for r,f in zip(actual,result.features))
    with pytest.raises(ValueError):q.CellTableRequest(restored,options=options(True))


def test_unrepresentable_physical_metrics_fail_on_worker_without_replacing_file(tmp_path):
    doc=q.Document(q.Image.demo());doc.add_cell(q.Roi.ellipse(q.Rect(1,2,4,6)));doc.set_calibration(1e308,1e308)
    request=q.CellTableRequest(doc,options=options());path=tmp_path/"overflow.csv";path.write_bytes(b"sentinel")
    job=q.CellTableExportJob();assert job.request(request,str(path));status=job.wait()
    assert status.state=="failed" and status.error and path.read_bytes()==b"sentinel" and not list(tmp_path.glob("*.qpx-tmp-*"))


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason="project cell publication")
def test_deferred_rows_preserve_unverified_status_after_live_recheck_and_class_changes(tmp_path):
    doc,_,_,record,ids=published();project=q.Project();image=project.add_document(doc);path=tmp_path/"captured-status.qpxproj";project.save(str(path))
    restored=q.Project.open(str(path)).document(image);captured=q.CellTableRequest(restored,options=options())
    expected=q.cell_measurements_csv(captured)
    request=q.CellRequest.replay(restored,record);assert restored.verify_cells(record,request,q.detect_cells(request))
    restored.rename_class("Tumor","renamed");restored.set_class("renamed",0);restored.update_nucleus(ids[0],None)
    assert q.cell_measurements_csv(captured)==expected
    assert {r["object_status_at_capture"] for r in rows(expected)}=={"unverified"}


@pytest.mark.skipif(not q.has_geos,reason="cell publication")
def test_strict_filtered_capture_ignores_stale_neighbors_in_retained_block():
    doc,_,_,old,old_ids=published();request=q.CellRequest.replay(doc,old)
    fresh=doc.keep_cells(request,q.detect_cells(request));fresh_ids=doc.publish_cell_record(fresh)
    doc.classify_objects(old_ids,"not selected");doc.update_nucleus(old_ids[0],None);doc.remove_cells(old)
    doc.add_cell(q.Roi.rectangle(q.Rect(1,1,2,2)),name="not selected manual")
    captured=q.CellTableRequest(doc,[fresh],options(True));expected=q.cell_measurements_csv(captured)
    actual=rows(expected);assert [int(r["object_id"]) for r in actual]==fresh_ids and captured.record_count==1
    assert all(r["object_class"]=="" and r["object_status_at_capture"]=="current" for r in actual)
    doc.remove_cells(fresh);doc.remove_objects(fresh_ids);assert q.cell_measurements_csv(captured)==expected
