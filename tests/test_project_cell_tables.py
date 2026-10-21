"""Project-scoped native tables retain per-image historical identity."""
import csv
import gc
import io
import sqlite3
import subprocess
import sys

import pytest
import qupathxx as q

from test_cells import phantom
from test_tissue import document
from test_cell_records import capture

pytestmark=pytest.mark.skipif(not q.has_project,reason="native projects")


def parsed(text):return list(csv.DictReader(io.StringIO(text,newline="")))


def workspace():
    project=q.Project();docs=[];images=[];records=[]
    for size,name in [(64,'First,"image"\r\nrow'),(80,"\u7ec6\u80de")]:
        doc=document(phantom(size));obj=doc.add_rectangle(q.Rect(0,0,size,size))
        _,_,record=capture(doc,obj)
        images.append(project.add_document(doc,name=name));docs.append(doc);records.append(record)
    empty=document(phantom(16));images.append(project.add_document(empty,name="no runs"));docs.append(empty)
    return project,docs,images,records


def test_project_rows_disambiguate_colliding_record_ids_and_match_every_document_field():
    project,docs,images,records=workspace()
    assert records[0]==records[1]
    request=q.CellTableRequest(project)
    assert request.project_scope and request.image_count==3 and request.record_count==2
    assert request.document_revision==0 and request.project_identity==project.identity and request.project_generation==project.generation
    actual=parsed(q.cell_measurements_csv(request))
    expected=[]
    names={image.id:image.name for image in project.images}
    for image,doc in zip(images,docs):
        for row in parsed(q.cell_measurements_csv(doc)):
            row.update(project_identity=project.identity,project_generation=str(project.generation),project_revision=str(request.project_revision),
                       image_id=str(image),image_name=names[image],table_schema="qpx.project-cell-table/1")
            expected.append(row)
    assert actual==expected and request.row_count==len(actual)>0
    assert list(actual[0])[:5]==["project_identity","project_generation","project_revision","image_id","image_name"]
    assert len(actual[0])==76 and {int(r["image_id"]) for r in actual}==set(images[:2])


def test_project_selection_sorts_deduplicates_and_rejects_unknown_images():
    project,_,images,_=workspace()
    selected=q.cell_measurements_csv(project,image_ids=[images[1],images[0],images[1]])
    assert selected==q.cell_measurements_csv(project)
    assert {int(r["image_id"]) for r in parsed(q.cell_measurements_csv(project,[images[1]]))}=={images[1]}
    with pytest.raises(IndexError):q.CellTableRequest(project,[images[0],999999])
    with pytest.raises((ValueError,RuntimeError,IndexError),match="10000"):q.CellTableRequest(project,[images[0]]*10001)


def test_project_snapshot_survives_image_rename_remove_run_removal_and_source_close():
    project,docs,images,records=workspace();request=q.CellTableRequest(project)
    before=q.cell_measurements_csv(request)
    project.rename_image(images[0],"changed")
    docs[0].remove_cells(records[0]);docs[1].server.close();project.remove_image(images[1])
    assert q.cell_measurements_csv(request)==before
    now=parsed(q.cell_measurements_csv(project));assert not now
    del project,docs;gc.collect();assert q.cell_measurements_csv(request)==before


def test_project_snapshot_and_file_limits_are_aggregate_and_exact(tmp_path):
    project,docs,_,_=workspace();captured=q.CellTableRequest(project);expected=q.cell_measurements_csv(captured)
    options=q.CellTableOptions();options.snapshot_budget=captured.snapshot_bytes;options.byte_limit=len(expected.encode())
    request=q.CellTableRequest(project,options=options)
    path=tmp_path/"\u6d4b\u91cf.csv";q.save_cell_measurements_csv(request,str(path));assert path.read_bytes()==expected.encode()
    options.snapshot_budget-=1
    with pytest.raises((ValueError,RuntimeError,IndexError),match="snapshot"):q.CellTableRequest(project,options=options)
    options.snapshot_budget=max(q.CellTableRequest(doc).snapshot_bytes for doc in docs)+1024
    with pytest.raises((ValueError,RuntimeError,IndexError),match="snapshot"):q.CellTableRequest(project,options=options)
    options.snapshot_budget=captured.snapshot_bytes;options.byte_limit-=1
    with pytest.raises((ValueError,RuntimeError,IndexError),match="byte limit"):q.save_cell_measurements_csv(project,str(path),options=options)
    assert path.read_bytes()==expected.encode() and not list(tmp_path.glob("*.qpx-tmp-*"))


def test_current_requirement_rejects_whole_project_and_preserves_destination(tmp_path):
    project,docs,_,_=workspace();path=tmp_path/"old.csv";path.write_bytes(b"sentinel")
    options=q.CellTableOptions();options.require_current=True
    assert q.CellTableRequest(project,options=options).record_count==2
    docs[1].server.close()
    with pytest.raises(ValueError,match="current verified"):q.save_cell_measurements_csv(project,str(path),options=options)
    assert path.read_bytes()==b"sentinel" and not list(tmp_path.glob("*.qpx-tmp-*"))
    assert {r["status_at_capture"] for r in parsed(q.cell_measurements_csv(project))}=={"current","source_closed"}


def test_empty_project_and_documents_write_their_distinct_headers():
    project=q.Project();request=q.CellTableRequest(project)
    assert request.project_scope and request.image_count==request.record_count==request.row_count==0
    assert parsed(q.cell_measurements_csv(request))==[]
    assert len(next(csv.reader(io.StringIO(q.cell_measurements_csv(request)))))==76
    doc=document(phantom(16));single=q.CellTableRequest(doc)
    assert not single.project_scope and single.image_count==1 and not single.project_identity and single.project_generation==single.project_revision==0
    assert len(next(csv.reader(io.StringIO(q.cell_measurements_csv(single)))))==71


def test_saved_project_capture_keeps_image_identity_generation_and_unverified_status(tmp_path):
    project,_,images,_=workspace();path=tmp_path/"runs.qpxproj";project.save(str(path))
    loaded=q.Project.open(str(path));request=q.CellTableRequest(loaded)
    rows=parsed(q.cell_measurements_csv(request))
    assert request.project_identity==project.identity and request.project_generation==1
    assert {r["status_at_capture"] for r in rows}=={"unverified"}
    assert {int(r["image_id"]) for r in rows}==set(images[:2])
    expected=q.cell_measurements_csv(request);loaded.save(str(path));assert loaded.generation==2 and q.cell_measurements_csv(request)==expected


def test_project_worker_owns_no_python_project_or_document_wrappers(tmp_path):
    script=r'''
import gc,pathlib,sys
import qupathxx as q
project=q.Project()
for i in range(8):
    doc=q.Document(q.Image(32,32,[0]*32*32*3));obj=doc.add_rectangle(q.Rect(0,0,32,32))
    source=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));result=q.detect_cells(source);doc.keep_cells(source,result)
    project.add_document(doc)
request=q.CellTableRequest(project);path=pathlib.Path(sys.argv[1])/"worker.csv";job=q.CellTableExportJob()
assert request.record_count==8 and job.request(request,str(path))
del project,doc,source,result,request;gc.collect()
status=job.wait();assert status.state=="succeeded" and status.rows==8 and path.exists()
'''
    child=subprocess.run([sys.executable,"-I","-c",script,str(tmp_path)],capture_output=True,text=True,timeout=20)
    assert child.returncode==0,child.stdout+child.stderr


def test_project_job_metrics_failure_reuse_and_capture_after_workspace_replacement(tmp_path):
    project,docs,_,_=workspace();request=q.CellTableRequest(project);expected=q.cell_measurements_csv(request)
    path=tmp_path/"project.csv";job=q.CellTableExportJob();assert job.request(request,str(path))
    del request,project,docs;gc.collect();status=job.wait()
    assert status.state=="succeeded" and status.completed==1 and status.rows==status.total_rows==len(parsed(expected))
    assert status.document_revision==0 and path.read_bytes()==expected.encode()
    project,_,_,_=workspace();options=q.CellTableOptions();options.byte_limit=1
    assert job.request(q.CellTableRequest(project,options=options),str(path));assert job.wait().state=="failed"
    assert path.read_bytes()==expected.encode()
    assert job.request(q.CellTableRequest(q.Project()),str(path)) and job.wait().completed==2


def test_unsigned_project_and_image_ids_are_exact(tmp_path):
    project,_,_,_=workspace();path=tmp_path/"wide.qpxproj";project.save(str(path))
    maximum=str(2**64-1);image=str(2**64-2)
    with sqlite3.connect(path) as db:
        for table,column in [("cell_records","image_id"),("objects","image_id"),("classes","image_id"),("levels","image_id")]:
            db.execute(f"UPDATE {table} SET {column}=? WHERE {column}='1'",(image,))
        db.execute("UPDATE images SET id=? WHERE id='1'",(image,));db.execute("UPDATE project SET next_image=?,active=?",(maximum,image))
    loaded=q.Project.open(str(path));rows=parsed(q.cell_measurements_csv(loaded,[int(image)]))
    assert rows and {r["image_id"] for r in rows}=={image}



def test_project_export_does_not_reopen_missing_linked_source(tmp_path):
    rgb=phantom(64);source=tmp_path/"source.ppm";source.write_bytes(b"P6\n64 64\n255\n"+rgb.tobytes())
    project=q.Project();image=project.add_image(str(source),reader="ppm");doc=project.document(image)
    obj=doc.add_rectangle(q.Rect(0,0,64,64));capture(doc,obj)
    path=tmp_path/"linked.qpxproj";project.save(str(path));source.unlink()
    loaded=q.Project.open(str(path));assert not loaded.images[0].available
    rows=parsed(q.cell_measurements_csv(loaded))
    assert rows and all(r["status_at_capture"]=="unverified" and int(r["image_id"])==image for r in rows)


def test_all_runs_sort_within_image_and_empty_runs_count_without_synthetic_rows():
    import numpy as np
    project,docs,images,_=workspace()
    source_id=docs[0].cell_records[0].result.nuclei.object_id
    _,_,new=capture(docs[0],source_id)
    blank=document(np.full((16,16,3),255,dtype=np.uint8));obj=blank.add_rectangle(q.Rect(0,0,16,16));capture(blank,obj)
    project.add_document(blank,name="empty run")
    request=q.CellTableRequest(project)
    assert request.record_count==4 and request.image_count==4
    rows=parsed(q.cell_measurements_csv(request));first=[int(r["record_id"]) for r in rows if int(r["image_id"])==images[0]]
    assert first==sorted(first) and first[-1]==new
    assert len(rows)==request.row_count and {int(r["image_id"]) for r in rows}==set(images[:2])
