"""Captured streamed contours reuse project checkpoint and desktop batch paths."""
import csv
import io
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
import pytest
import qupathxx as q
from qupathxx import _native
from test_raw_pixel_batch import scene, options as raw_options, request, success, close
from test_raw_pixel_python import trained

pytestmark=pytest.mark.skipif(not(q.has_tiff and q.has_project and q.has_geos),reason="quantitative TIFF/project/GEOS")
CLI=Path(_native.__file__).with_name("qupathxx_batch.exe" if os.name=="nt" else "qupathxx_batch")

def options(model):
    o=raw_options(model);o.kind=q.ProjectBatchKind.RAW_PIXEL_CONTOURS;o.raw_contour_prefix="Batch contour"
    return o

@pytest.mark.parametrize("fractional",[False,True])
def test_batch_streamed_geometry_csv_snapshot_and_hash_only_reuse(tmp_path,fractional):
    p,docs,model=scene(tmp_path,(8,))
    try:
        image,doc,root,ids=docs[0]
        x,y=(q.ScientificGridAxis(0,8,7,2),q.ScientificGridAxis(0,6,5,2)) if fractional else (q.ScientificGridAxis(0,8,8),q.ScientificGridAxis(0,6,6))
        lattice=q.ScientificRawLattice(8,6,x,y)
        if fractional:
            select=q.ScientificRawOptions();select.channels=2;features=q.RawPixelFeatureOptions();features.offsets=[0,0];features.scales=[1,1]
            model=trained(q.sample_raw_pixel_tiled_training(q.RawPixelTiledTrainingRequest(doc,ids,lattice,select,features)))
        o=options(model);o.raw_pixel_x=x;o.raw_pixel_y=y
        expected=q.trace_raw_pixel_stream(q.RawPixelTiledPredictionRequest(doc,model,lattice,o.raw_pixels,object_id=root),o.raw_contours)
        capture=request(p,model,o);before=doc.object_count;original=doc.annotation(root).name
        doc.rename_annotation(root,"later live edit")
        job=q.ProjectBatchJob();assert job.request(capture,str(tmp_path/"out"));assert job.wait().state=="succeeded";first=success(job.result)
        assert first.saved==1 and first.images[0].records==len(expected.contours.components)>0
        restored=q.Project.open(first.images[0].path)
        try:
            current=restored.document(image);assert current.annotation(root).name==original
            children=current.children(root);assert len(children)==len(expected.contours.components)
            for obj,component in zip(children,expected.contours.components):
                a=current.annotation(obj);assert a.geometry.equals(component.geometry) and a.class_name==model.forest.classes[component.label-1] and a.name.startswith("Batch contour ")
            rows=list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(first.images[0].path))))
            assert len(rows)==len(children) and {int(v["object_id"]) for v in rows}==set(children)
            assert sum(float(v["area_px2"]) for v in rows)==sum(v.geometry.area for v in expected.contours.components)
        finally:close(restored)
        again=success(q.run_project_batch(capture,str(tmp_path/"out")))
        assert again.reused==1 and again.saved==0 and again.images[0].path==first.images[0].path
        assert again.images[0].hashed_blocks==first.images[0].inferred_blocks>0 and again.images[0].inferred_blocks==0
        assert again.images[0].records==first.images[0].records and doc.object_count==before
        assert not list((tmp_path/"out").rglob("*.qws"))
    finally:close(p)

@pytest.mark.parametrize("policy,state,saved",[(q.ProjectBatchFailure.STOP,"failed",1),(q.ProjectBatchFailure.CONTINUE,"partial",2)])
def test_batch_streamed_contours_mixed_images_failure_policy(tmp_path,policy,state,saved):
    p,docs,model=scene(tmp_path,(8,8,8),bad=True)
    try:
        o=options(model);o.failure=policy;report=q.run_project_batch(request(p,model,o),str(tmp_path/"out"))
        assert report.state==state and report.saved==saved and report.failed==1
        assert all(doc.object_count==3 for _,doc,_,_ in docs)
        assert not list((tmp_path/"out").glob(".qupathxx-batch-work-*"))
    finally:close(p)

@pytest.mark.parametrize("limit",["publication","work","result","csv"])
def test_batch_contour_quotas_publish_no_partial_project(tmp_path,limit):
    p,docs,model=scene(tmp_path,(8,))
    try:
        o=options(model)
        if limit=="publication":o.raw_contour_publication_bytes=1
        elif limit=="work":v=o.raw_contours;v.work_bytes=1;o.raw_contours=v
        elif limit=="result":o.result_budget=1
        else:o.csv_limit=1
        report=q.run_project_batch(request(p,model,o),str(tmp_path/"out"))
        assert report.state=="failed" and report.saved==0 and report.images[0].error
        assert not list((tmp_path/"out").rglob("*.qpxproj")) and docs[0][1].object_count==3
    finally:close(p)

def test_batch_contour_modified_checkpoint_refuses_reuse(tmp_path):
    p,_,model=scene(tmp_path,(8,))
    try:
        capture=request(p,model,options(model));first=success(q.run_project_batch(capture,str(tmp_path/"out")))
        with sqlite3.connect(first.images[0].path) as db:db.execute("UPDATE objects SET name='tampered' WHERE id=(SELECT MAX(id) FROM objects)")
        report=q.run_project_batch(capture,str(tmp_path/"out"))
        assert report.state=="failed" and report.reused==report.saved==0 and "checksum" in report.images[0].error
    finally:close(p)

def test_batch_contour_multiple_source_rois_keep_resume_digest(tmp_path):
    p,docs,model=scene(tmp_path,(8,))
    try:
        doc=docs[0][1];doc.add_roi(q.Roi.rectangle(q.Rect(1.1,1.3,4.2,3.1)))
        capture=request(p,model,options(model));a=success(q.run_project_batch(capture,str(tmp_path/"out")));b=success(q.run_project_batch(capture,str(tmp_path/"out")))
        assert a.images[0].objects==2 and b.reused==1 and a.images[0].records==b.images[0].records>0 and a.images[0].pixel_sha256==b.images[0].pixel_sha256
    finally:close(p)

def test_batch_contour_cli_and_option_manifest(tmp_path):
    p,docs,model=scene(tmp_path,(8,))
    try:
        source=tmp_path/"source.qpx";p.save(str(source));before=source.read_bytes();model_path=tmp_path/"model.qrm";q.save_raw_pixel_classifier(model,str(model_path))
        args=[str(CLI),str(source),str(tmp_path/"out"),"--kind","raw-contours","--raw-pixel-model",str(model_path),"--raw-pixel-tile-width","2","--raw-pixel-tile-height","2","--raw-pixel-work-bytes",str(16*1024**2),"--raw-contour-prefix","Batch contour"]
        for attempt in range(2):
            result=subprocess.run(args,capture_output=True,text=True,encoding="utf-8",timeout=60);assert result.returncode==0,result.stdout+result.stderr
            report=json.loads(result.stdout.splitlines()[-1]);assert report["state"]=="succeeded" and report["saved" if attempt==0 else "reused"]==1
        assert source.read_bytes()==before and docs[0][1].object_count==3
        capture=request(p,model,options(model));manifest=json.loads(capture.parameters_json);assert manifest["raw_contours"]["prefix"]=="Batch contour"
        changed=options(model);v=changed.raw_contours;v.contours.clip_to_roi=False;changed.raw_contours=v
        assert request(p,model,changed).parameters_sha256!=capture.parameters_sha256
    finally:close(p)

@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="opt-in GUI")
def test_gui_streamed_contour_batch_saves_and_reuses(tmp_path):
    from test_gui_batch import options as gui_options
    p,docs,model=scene(tmp_path,(8,))
    try:
        ui=gui_options(tmp_path/"out",options(model));ui.width=1280;ui.height=1000
        for attempt in range(2):
            folder=Path(os.environ.get("QUPATHXX_STREAM_BATCH_EVIDENCE_DIR",tmp_path));folder.mkdir(exist_ok=True,parents=True)
            report=q.show(p,frame_limit=40,capture_path=str(folder/f"batch-{attempt}.ppm"),options=ui)
            assert report.batch_state=="succeeded",report.batch_error
            assert getattr(report.batch_result,"saved" if attempt==0 else "reused")==1
            assert report.batch_result.images[0].records>0 and report.ui_overflow_items==0
        assert docs[0][1].object_count==3
    finally:close(p)


def test_batch_contour_active_cancellation_discards_image_and_releases_reader(tmp_path):
    from test_raw_pixel_python import tiff
    p,_,model=scene(tmp_path,(8,));close(p)
    pixels=tmp_path/"large.tif";tiff(pixels,1024,768)
    project=q.Project();image=project.add_image(str(pixels),"tiff");doc=project.document(image);doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1024,768)))
    o=options(model);v=o.raw_pixels;v.tile_width=v.tile_height=16;o.raw_pixels=v
    try:
        capture=request(project,model,o);job=q.ProjectBatchJob();assert job.request(capture,str(tmp_path/"out"))
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            stats=job.poll()
            if stats.phase=="analyzing" and job.busy:break
            assert stats.state not in ("failed","succeeded"),stats.error
            time.sleep(.001)
        else:pytest.fail("did not observe active analysis")
        job.cancel();assert job.wait().state=="cancelled";report=job.result
        assert report is not None and report.saved==0 and report.state=="cancelled"
        assert not list((tmp_path/"out").rglob("*.qpxproj")) and doc.object_count==1
    finally:close(project)
    pixels.rename(tmp_path/"reader-released.tif")


def test_batch_contour_source_change_recomputes_and_settings_change_separates_output(tmp_path):
    p,_,model=scene(tmp_path,(8,));source=tmp_path/"source.qpx";p.save(str(source))
    first=success(q.run_project_batch(request(p,model,options(model)),str(tmp_path/"out")));close(p)
    pixels=tmp_path/"source-0.tif";data=bytearray(pixels.read_bytes());data[160]^=64;pixels.write_bytes(data)
    current=q.Project.open(str(source))
    try:
        second=success(q.run_project_batch(request(current,model,options(model)),str(tmp_path/"out")))
        assert second.saved==1 and second.reused==0 and second.images[0].pixel_sha256!=first.images[0].pixel_sha256
        altered=options(model);altered.raw_contour_prefix="Changed prefix"
        third=success(q.run_project_batch(request(current,model,altered),str(tmp_path/"out")))
        assert third.saved==1 and third.reused==0 and third.images[0].path!=second.images[0].path
    finally:close(current)
