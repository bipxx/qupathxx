"""Installed native streamed contours: real typed TIFF and actual trained forest."""
from concurrent.futures import ThreadPoolExecutor
import gc
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest
import qupathxx as q
from test_raw_pixel_python import raw, trained, tiff

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_geos,reason="typed TIFF and GEOS")


def setup(raw, region=None):
    doc, server, _, full, tiled, path=raw
    model=trained(q.sample_raw_pixel_tiled_training(tiled))
    stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=2;stream.work_bytes=16*1024**2
    request=q.RawPixelTiledPredictionRequest(doc,model,tiled.lattice,stream,region)
    options=q.RawPixelStreamContourOptions();options.boundary_work_bytes=2*1024**2
    contour=q.TissueContourOptions();contour.work_bytes=2*1024**2;options.contours=contour;options.work_bytes=32*1024**2
    return doc,server,path,model,request,options


def test_complete_stream_commitments_geometry_and_readonly_snapshots(raw):
    doc,_,_,model,request,options=setup(raw)
    result=q.trace_raw_pixel_stream(request,options)
    stream=q.RawPixelPredictionStream(request)
    for block in stream:pass
    assert result.request_sha256==request.sha256
    assert result.input_sha256==stream.input_sha256 and result.prediction_sha256==stream.sha256
    assert result.class_counts==stream.class_counts and result.completed_samples==35
    assert result.completed_windows==request.total_windows and result.valid_samples==stream.valid_samples
    assert result.payload_bytes<=result.estimated_work_bytes<=options.work_bytes
    _,_,_,full,_,_=raw
    capture=q.RawPixelPredictionRequest(doc,model,full.grid)
    reference=q.trace_raw_pixel_prediction(capture,q.predict_raw_pixel_region(capture))
    assert len(result.contours.components)==len(reference.contours.components)
    for a,b in zip(result.contours.components,reference.contours.components):
        assert a.label==b.label and a.pixels==b.pixels and a.geometry.equals(b.geometry)
    copied=result.region;copied.first_x=99
    assert result.region.first_x==0
    counts=result.class_counts;counts[0]=999
    assert result.class_counts==stream.class_counts
    with pytest.raises(AttributeError):result.completed_samples=0
    with pytest.raises(TypeError):q.RawPixelStreamContours()


def test_job_optional_result_owner_cancel_and_retained_lifetime(raw):
    doc,_,_,_,request,options=setup(raw)
    job=q.RawPixelStreamContourJob();assert job.result is None
    assert job.request(request,options)
    status=job.wait(doc);result=job.result
    assert status.state=="ready" and result is not None
    assert status.produced_windows==request.total_windows and status.delivered_samples==35
    with ThreadPoolExecutor(1) as pool:
        future=pool.submit(lambda:job.result)
        with pytest.raises(RuntimeError,match="owner thread"):future.result()
    digest=result.prediction_sha256
    assert job.request(request,options)
    job.await_completion();job.cancel()
    assert job.poll(doc).state=="cancelled" and job.result is None
    del job;gc.collect()
    assert result.prediction_sha256==digest and result.completed_samples==35


def test_exact_capture_budget_atomic_apply_undo_redo(raw):
    doc,_,_,model,request,options=setup(raw)
    result=q.trace_raw_pixel_stream(request,options)
    foreign=q.RawPixelTiledPredictionRequest(doc,model,request.lattice,request.options)
    assert foreign.sha256==request.sha256
    revision,count=doc.revision,doc.object_count
    with pytest.raises(ValueError,match="matching live stream capture"):
        q.publish_raw_pixel_stream_contours(doc,foreign,result)
    with pytest.raises(ValueError):q.publish_raw_pixel_stream_contours(doc,request,result,"")
    with pytest.raises((ValueError,RuntimeError)):
        q.publish_raw_pixel_stream_contours(doc,request,result,work_bytes=1)
    assert doc.revision==revision and doc.object_count==count
    ids=q.publish_raw_pixel_stream_contours(doc,request,result)
    assert len(ids)==len(result.contours.components) and doc.revision==revision+1
    assert doc.object_count==count+len(ids)
    with pytest.raises(ValueError):q.publish_raw_pixel_stream_contours(doc,request,result)
    assert doc.undo() and doc.object_count==count
    assert doc.redo() and doc.object_count==count+len(ids)


def test_cropped_global_phase_and_late_file_refusal(raw):
    doc,_,path,_,request,options=setup(raw,q.RawPixelStreamRegion(2,1,3,2))
    direct=q.trace_raw_pixel_stream(request,options)
    assert direct.region.first_x==2 and direct.region.first_y==1
    assert direct.completed_samples==6 and direct.lattice.x.samples==7 and direct.lattice.y.samples==5
    job=q.RawPixelStreamContourJob();assert job.request(request,options)
    job.await_completion()
    with path.open('ab') as stream:stream.write(b'changed')
    assert job.poll(doc).state=="stale" and job.result is None
    revision,count=doc.revision,doc.object_count
    with pytest.raises(ValueError):q.publish_raw_pixel_stream_contours(doc,request,direct)
    assert doc.revision==revision and doc.object_count==count


def test_admission_and_document_stale_refusal(raw):
    doc,_,_,_,request,options=setup(raw)
    bad=q.RawPixelStreamContourOptions();bad.work_bytes=1
    with pytest.raises((ValueError,RuntimeError)):q.trace_raw_pixel_stream(request,bad)
    job=q.RawPixelStreamContourJob()
    with pytest.raises((ValueError,RuntimeError)):job.request(request,bad)
    assert job.result is None and not job.busy
    assert job.request(request,options)
    job.await_completion()
    doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1,1)))
    assert job.poll(doc).state=="stale" and job.result is None


@pytest.mark.parametrize('mode',['cancel','close'])
def test_active_worker_shutdown_releases_gil_and_reader(raw,tmp_path,mode):
    _,_,_,model,_,_=setup(raw)
    image=tmp_path/'large.tif';tiff(image,width=1024,height=768)
    model_path=tmp_path/'model.qrm';q.save_raw_pixel_classifier(model,str(model_path))
    command=[sys.executable,'-I',str(Path(__file__).with_name('raw_stream_contour_shutdown_child.py')),mode,str(image),str(model_path)]
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=45)
    output=os.environ.get('QUPATHXX_STREAM_CONTOUR_EVIDENCE_DIR')
    if output:
        dest=Path(output);dest.mkdir(parents=True,exist_ok=True)
        (dest/(mode+'-stdout.log')).write_text(result.stdout,encoding='utf-8')
        (dest/(mode+'-stderr.log')).write_text(result.stderr,encoding='utf-8')
        (dest/(mode+'-command.json')).write_text(json.dumps({'command':command,'exit':result.returncode},indent=2)+'\n',encoding='utf-8')
    assert result.returncode==0,result.stderr
    record=json.loads(result.stdout)
    assert record['observed_busy'] and record['observed_state']=='running' and record['shutdown_completed']
    assert record['mode']==mode and record['total_windows']>1000
