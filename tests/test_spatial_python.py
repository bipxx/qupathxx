"""Installed spatial capture, paged tables, native jobs and historical projects."""
import concurrent.futures
import csv
import gc
import io
import math
import subprocess
import sys
import threading
import time

import pytest
import qupathxx as q


def scene():
    doc=q.Document(q.Image.demo())
    a=doc.add_detection(q.Roi.points([q.Point(32,32)]))
    b=doc.add_cell(q.Roi.rectangle(q.Rect(34,34,8,8)),q.Roi.rectangle(q.Rect(34,35,2,2)))
    c=doc.add_detection(q.Roi.points([q.Point(32,32)]))
    doc.set_calibration(.5,.25)
    return doc,a,b,c


@pytest.mark.parametrize('physical',[False,True])
@pytest.mark.parametrize('nucleus',[False,True])
@pytest.mark.parametrize('exclude',[False,True])
@pytest.mark.parametrize('cohort',['empty','self','coincident','all'])
def test_scalar_rows_match_independent_nearest_formula(physical,nucleus,exclude,cohort):
    doc,a,b,c=scene();options=q.SpatialDistanceOptions()
    options.units=q.SpatialDistanceUnits.MICROMETRES if physical else q.SpatialDistanceUnits.PIXELS
    options.prefer_nucleus=nucleus;options.exclude_self=exclude
    targets={'empty':[],'self':[a],'coincident':[c],'all':[c,b,a]}[cohort]
    request=q.SpatialDistanceRequest(doc,[b,a],targets,options)
    result=q.measure_spatial_distances(request)
    assert request.matches(doc) and result.belongs_to(request)
    assert result.row_count==result.source_count==2 and result.target_count==len(targets)
    assert result.input_sha256==request.input_sha256 and result.algorithm=='qpx.centroid-nearest/1'
    assert len(result.sha256)==len(result.input_sha256)==64
    points={a:(32,32),b:((35,36) if nucleus else (38,38)),c:(32,32)}
    sx,sy=(.5,.25) if physical else (1,1)
    assert (result.x_scale,result.y_scale)==(sx,sy)
    for row in result.rows():
        x,y=points[row.source_id]
        candidates=[(math.hypot((points[t][0]-x)*sx,(points[t][1]-y)*sy),t) for t in targets if not exclude or t!=row.source_id]
        assert (row.source_centroid.x,row.source_centroid.y)==(x,y)
        assert row.source_nucleus==(row.source_id==b and nucleus)
        if not candidates:
            assert row.distance is row.target_id is row.target_revision is row.target_centroid is row.target_kind is None
        else:
            distance,target=min(candidates)
            assert row.target_id==target and row.distance==pytest.approx(distance,abs=1e-9,rel=1e-12)
            assert (row.target_centroid.x,row.target_centroid.y)==points[target]
    assert request.provenance.source_only and result.provenance.document_revision==doc.revision


def test_readonly_values_pages_and_snapshot_lifetimes():
    doc,a,b,c=scene();options=q.SpatialDistanceOptions();request=q.SpatialDistanceRequest(doc,[b,a],[c,b],options)
    options.exclude_self=False;assert request.options.exclude_self
    result=q.measure_spatial_distances(request);original=result.sha256
    copied=request.source_object(0);copied.centroid.x=500
    assert request.source_object(0).centroid.x==32
    row=result.row(0);row.source_centroid.x=500;row.target_centroid.x=500
    assert result.row(0).source_centroid.x==result.row(0).target_centroid.x==32
    detached=result.options;detached.exclude_self=False;assert result.options.exclude_self
    with pytest.raises(AttributeError):row.distance=123
    with pytest.raises(AttributeError):copied.id=123
    for cls in [q.SpatialObjectSnapshot,q.SpatialDistanceRow,q.SpatialDistanceResult,q.SpatialDistanceRecord,q.SpatialDistanceStats]:
        with pytest.raises(TypeError):cls()
    assert [r.source_id for r in result.rows(1,1)]==[b] and result.rows(2,1)==[]
    for start,limit in [(0,0),(0,4097),(3,1)]:
        with pytest.raises((ValueError,IndexError)):result.rows(start,limit)
    with pytest.raises(IndexError):result.row(2)
    with pytest.raises(IndexError):request.source_object(2)
    with pytest.raises(IndexError):result.target_object(2)
    del doc,request;gc.collect()
    assert result.row(0).distance==0 and result.sha256==original and result.source_object(0).id==a
    assert row.target_id==c


def test_admission_calibration_stale_and_owner_guards():
    doc,a,b,c=scene()
    for sources,targets in [([],[]),([a,a],[b]),([a],[b,b]),([9999],[])]:
        with pytest.raises((ValueError,IndexError)):q.SpatialDistanceRequest(doc,sources,targets)
    for field,value in [('snapshot_bytes',1),('work_bytes',1),('max_distance_tests',0)]:
        options=q.SpatialDistanceOptions();setattr(options,field,value)
        with pytest.raises(ValueError):q.SpatialDistanceRequest(doc,[a],[b],options)
    doc.clear_calibration();options=q.SpatialDistanceOptions();options.units=q.SpatialDistanceUnits.MICROMETRES
    with pytest.raises(ValueError,match='calibration'):q.SpatialDistanceRequest(doc,[a],[b],options)
    request=q.SpatialDistanceRequest(doc,[a,b],[a,b,c]);result=q.measure_spatial_distances(request)
    job=q.SpatialDistanceJob();assert job.result is None and not job.busy
    assert job.request(request) and job.busy and not job.request(request)
    assert job.wait(doc).state=='ready' and job.result.sha256==result.sha256
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        for operation in [lambda:request.matches(doc),lambda:job.poll(doc),lambda:job.busy,lambda:job.result,lambda:job.request(request)]:
            with pytest.raises(RuntimeError,match='owner thread'):pool.submit(operation).result()
        foreign=pool.submit(q.measure_spatial_distances,request).result()
        assert foreign.sha256==result.sha256
    doc.rename_annotation(a,'changed')
    assert not request.matches(doc) and job.poll(doc).state=='stale' and job.result is None
    with pytest.raises(ValueError):doc.keep_spatial(request,result)
    assert result.row_count==2


def test_job_cancel_retry_and_budget_failure():
    doc,a,b,c=scene();request=q.SpatialDistanceRequest(doc,[a,b],[c,b,a]);job=q.SpatialDistanceJob()
    assert job.request(request);job.cancel();assert job.wait(doc).state=='cancelled' and job.result is None and not job.busy
    assert job.request(request) and job.wait(doc).state=='ready'
    options=q.SpatialDistanceOptions();options.max_distance_tests=1
    bounded=q.SpatialDistanceRequest(doc,[a,b],[c,b,a],options)
    assert job.request(bounded);stats=job.wait(doc)
    assert stats.state=='failed' and 'budget' in stats.error and job.result is None and stats.distance_tests<=1


@pytest.mark.parametrize('action',['edit','cancel'])
def test_wait_releases_gil_and_checks_document_only_after_reacquiring(action):
    doc=q.Document(q.Image.demo());point=q.Roi.points([q.Point(32,32)])
    doc.execute([q.ObjectEdit.create_geometry(point) for _ in range(8000)],'coincident fixture')
    ids=[a.id for a in doc.annotations];request=q.SpatialDistanceRequest(doc,ids,ids);job=q.SpatialDistanceJob()
    gate=threading.Event();changed=threading.Event()
    def change():
        gate.wait();time.sleep(.02)
        if action=='edit':doc.remove_annotation(ids[-1])
        else:job.cancel()
        changed.set()
    thread=threading.Thread(target=change);thread.start();assert job.request(request);gate.set()
    stats=job.wait(doc);thread.join(timeout=10)
    assert changed.is_set() and not thread.is_alive()
    assert stats.state==('stale' if action=='edit' else 'cancelled') and job.result is None


def test_retained_project_history_missing_source_and_atomic_csv(tmp_path):
    doc,a,b,c=scene();project=q.Project();image=project.add_document(doc,'Geometry only',str(tmp_path/'missing.ppm'),'ppm')
    request=q.SpatialDistanceRequest(doc,[a,b],[a,b,c]);result=q.measure_spatial_distances(request)
    record_id=doc.keep_spatial(request,result,'Nearest cohort');record=doc.spatial_record(record_id)
    assert doc.spatial_record_count==1 and doc.spatial_records[0].id==record_id and doc.spatial_status(record_id)=='current'
    assert record.result.sha256==result.sha256 and record.name=='Nearest cohort' and record.captured_next_id>c
    path=tmp_path/'study.qpxproj';project.save(str(path));restored=q.Project.open(str(path));other=restored.document(image)
    assert other.spatial_status(record_id)=='unverified' and not request.matches(other)
    replay=q.SpatialDistanceRequest.replay(other,record_id);assert other.verify_spatial(record_id,replay,q.measure_spatial_distances(replay))
    assert other.spatial_status(record_id)=='current' and not restored.dirty and not (tmp_path/'missing.ppm').exists()
    csv_path=tmp_path/'distances.csv';text=q.spatial_distance_csv(record.result)
    rows=list(csv.DictReader(io.StringIO(text)));assert len(rows)==2 and rows[0]['distance']=='0' and rows[0]['status']=='matched'
    assert rows[0]['x_scale']==rows[0]['y_scale']=='1' and rows[0]['result_sha256']==result.sha256
    assert q.save_spatial_distance_csv(record.result,str(csv_path))==len(text.encode()) and csv_path.read_bytes()==text.encode()
    with pytest.raises(RuntimeError):q.save_spatial_distance_csv(record.result,str(csv_path))
    with pytest.raises(ValueError):q.save_spatial_distance_csv(record.result,str(csv_path),byte_limit=1,replace=True)
    assert csv_path.read_bytes()==text.encode() and not list(tmp_path.glob('*.qpx-tmp-*'))
    empty=q.SpatialDistanceRequest(other,[a],[]);missing=q.measure_spatial_distances(empty)
    absent=list(csv.DictReader(io.StringIO(q.spatial_distance_csv(missing))))[0]
    assert absent['distance']==absent['target_id']=='' and absent['status']=='no_target'
    assert q.save_spatial_distance_csv(missing,str(csv_path),replace=True)>0
    other.remove_annotation(c);assert other.spatial_status(record_id)=='objects_or_calibration_changed'
    with pytest.raises((ValueError,IndexError)):q.SpatialDistanceRequest.replay(other,record_id)
    assert other.remove_spatial(record_id) and not other.remove_spatial(record_id)
    del project,restored,other,doc,request,replay;gc.collect()
    assert record.result.row_count==2 and q.spatial_distance_csv(record.result)==text


def test_closed_source_is_valid_for_geometry_only_requests():
    doc,a,b,c=scene();request=q.SpatialDistanceRequest(doc,[a,b],[c]);doc.server.close()
    result=q.measure_spatial_distances(request);record=doc.keep_spatial(request,result)
    assert doc.spatial_status(record)=='current'
    replay=q.SpatialDistanceRequest.replay(doc,record);assert doc.verify_spatial(record,replay,q.measure_spatial_distances(replay))


def test_worker_shutdown_releases_python_owner_before_native_join():
    code='''
import gc
import qupathxx as q
doc=q.Document(q.Image.demo());roi=q.Roi.points([q.Point(32,32)])
doc.execute([q.ObjectEdit.create_geometry(roi) for _ in range(8000)])
ids=[a.id for a in doc.annotations];request=q.SpatialDistanceRequest(doc,ids,ids)
job=q.SpatialDistanceJob();assert job.request(request)
del doc,request;gc.collect();del job;gc.collect()
print('spatial worker retired')
'''
    child=subprocess.run([sys.executable,'-I','-c',code],capture_output=True,text=True,timeout=30)
    assert child.returncode==0,child.stdout+child.stderr
    assert child.stdout.strip()=='spatial worker retired' and 'leaked' not in child.stderr.lower()
