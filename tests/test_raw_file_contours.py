"""Historical QWS1 geometry has a distinct type; apply re-executes live capture."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import os
import subprocess
import numpy as np
from qupathxx import _native
import pytest
import qupathxx as q
from test_gui_raw_pixel import scene, model_for
from raw_file_mutation import mutate_header

pytestmark=pytest.mark.skipif(not(q.has_tiff and q.has_geos and q.has_project and q.has_classifier_training),reason="TIFF/GEOS/project and actual forest backend")

@pytest.fixture
def case(tmp_path):
    project,image,doc,ids,root,settings=scene(tmp_path);model=model_for(doc,ids,settings)
    stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=2;stream.work_bytes=16*1024**2
    limits=q.RawPixelStreamFileLimits();limits.work_bytes=2*1024**2
    options=q.RawPixelStreamContourOptions();options.boundary_work_bytes=2*1024**2;options.work_bytes=32*1024**2;contour=options.contours;contour.work_bytes=2*1024**2;options.contours=contour
    yield project,image,doc,root,settings.lattice,model,stream,limits,options
    doc.server.close()

def save(case,tmp_path,region=None,max_new=None):
    _,_,doc,root,lattice,model,stream,limits,_=case
    request=q.RawPixelTiledPredictionRequest(doc,model,lattice,stream,region,root)
    options=q.RawPixelStreamSaveOptions();options.limits=limits
    if max_new is not None:options.max_new_windows=max_new
    path=tmp_path/"saved.qws";q.save_raw_pixel_stream(request,str(path),options)
    return path,request

@pytest.mark.parametrize("crop",[False,True])
def test_historical_geometry_preserves_axes_clipping_and_needs_no_source(case,tmp_path,crop):
    _,_,doc,root,_,model,_,limits,options=case
    doc.update_roi(root,q.Roi.rectangle(q.Rect(1.1,1.3,4.2,3.1)))
    region=q.RawPixelStreamRegion(1,1,3,2) if crop else None
    path,request=save(case,tmp_path,region);live=q.trace_raw_pixel_stream(request,options)
    file=q.RawPixelStreamFile(str(path),limits);assert not file.verified
    pixels=Path(doc.server.source_locator);doc.server.close();missing=pixels.with_suffix(".missing");pixels.rename(missing)
    try:
        historical=q.trace_raw_pixel_stream_file(file,options)
        assert file.verified and historical.link.summary.complete and historical.completed_samples==live.completed_samples
        assert historical.link.summary.input_sha256==live.input_sha256 and historical.class_names==model.forest.classes
        assert historical.contours.object_id==0 and historical.contours.input_sha256==""
        assert len(historical.contours.components)==len(live.contours.components)
        for a,b in zip(historical.contours.components,live.contours.components):assert a.label==b.label and a.pixels==b.pixels and a.geometry.equals(b.geometry)
        assert historical.payload_bytes<=historical.estimated_work_bytes<=options.work_bytes
        with pytest.raises(TypeError):q.RawPixelFileContours()
        with pytest.raises(AttributeError):historical.completed_samples=0
        names=historical.class_names;names[0]="changed";assert historical.class_names==model.forest.classes
        with pytest.raises(TypeError):q.publish_raw_pixel_stream_contours(doc,request,historical)
    finally:missing.rename(pixels)

def test_historical_reader_job_uses_distinct_transfer_and_owner(case,tmp_path):
    _,_,_,_,_,_,_,limits,options=case;path,_=save(case,tmp_path)
    file=q.RawPixelStreamFile(str(path),limits);job=q.RawPixelStreamFileReadJob();assert job.take_contours() is None
    assert job.trace(file,options) and job.wait().state=="ready" and job.take_block() is None
    with ThreadPoolExecutor(1) as pool:
        with pytest.raises(RuntimeError):pool.submit(job.take_contours).result()
    result=job.take_contours();assert result is not None and job.take_contours() is None
    assert result.link.summary.class_counts==file.declared_summary.class_counts

def test_replay_requires_verification_and_fresh_exact_capture_then_undo(case,tmp_path):
    _,_,doc,root,_,_,_,limits,options=case;path,_=save(case,tmp_path,q.RawPixelStreamRegion(1,1,3,2))
    file=q.RawPixelStreamFile(str(path),limits)
    with pytest.raises(ValueError):q.capture_raw_pixel_stream_replay(doc,file)
    historical=q.trace_raw_pixel_stream_file(file,options);request=q.capture_raw_pixel_stream_replay(doc,file)
    live=q.trace_raw_pixel_stream_replay(request,file,options);assert live.input_sha256==historical.link.summary.input_sha256 and live.prediction_sha256==historical.link.summary.sha256
    before=doc.object_count;ids=q.publish_raw_pixel_stream_contours(doc,request,live,"Replayed");assert len(ids)==len(historical.contours.components)>0
    assert doc.children(root)==ids;doc.undo();assert doc.object_count==before;doc.redo();assert doc.children(root)==ids

@pytest.mark.parametrize("change",["roi","calibration"])
def test_replay_rejects_changed_owner_parameters(case,tmp_path,change):
    _,_,doc,root,_,_,_,limits,_=case;path,_=save(case,tmp_path);file=q.RawPixelStreamFile(str(path),limits);file.verify()
    if change=="roi":doc.update_roi(root,q.Roi.rectangle(q.Rect(0,0,4,4)))
    else:doc.set_calibration(.5,.25)
    with pytest.raises((ValueError,RuntimeError)):q.capture_raw_pixel_stream_replay(doc,file)
    assert doc.object_count==3

def test_replay_job_retires_late_history_changes_before_publication(case,tmp_path):
    _,_,doc,_,_,_,_,limits,options=case;path,_=save(case,tmp_path);file=q.RawPixelStreamFile(str(path),limits);file.verify();request=q.capture_raw_pixel_stream_replay(doc,file)
    job=q.RawPixelStreamContourJob();assert job.request_replay(request,file,options) and job.wait(doc).state=="ready";retained=job.result;assert retained is not None
    with path.open("ab") as f:f.write(b"changed")
    assert job.poll(doc).state=="failed" and job.result is None
    with pytest.raises(RuntimeError):q.publish_raw_pixel_stream_contours(doc,request,retained)
    assert doc.object_count==3

@pytest.mark.parametrize("limit",["total","boundary"])
def test_historical_work_limits_and_partial_prefix_refusal(case,tmp_path,limit):
    _,_,_,_,_,_,_,limits,options=case;path,_=save(case,tmp_path);file=q.RawPixelStreamFile(str(path),limits)
    if limit=="total":options.work_bytes=1
    else:options.boundary_work_bytes=1
    with pytest.raises((ValueError,RuntimeError)):q.trace_raw_pixel_stream_file(file,options)
    path.unlink();path,_=save(case,tmp_path,max_new=1);partial=q.RawPixelStreamFile(str(path),limits)
    with pytest.raises(ValueError):q.trace_raw_pixel_stream_file(partial)


def test_moved_history_opens_and_corrupt_or_missing_history_refuses(case,tmp_path):
    _,_,_,_,_,_,_,limits,options=case;path,_=save(case,tmp_path);moved=tmp_path/"moved.qws";path.rename(moved)
    with pytest.raises((RuntimeError,ValueError)):q.RawPixelStreamFile(str(path),limits)
    file=q.RawPixelStreamFile(str(moved),limits);result=q.trace_raw_pixel_stream_file(file,options);assert Path(result.link.path).samefile(moved)
    with moved.open("r+b") as f:f.seek(150);byte=f.read(1);f.seek(150);f.write(bytes([byte[0]^64]))
    with pytest.raises((RuntimeError,ValueError)):q.trace_raw_pixel_stream_file(file,options)


@pytest.mark.parametrize("part",["dimensions","options","model","last_body_byte"])
@pytest.mark.parametrize("operation",["check_unchanged","link","read_block","verify","trace"])
def test_changed_header_is_refused_at_every_reader_boundary(case,tmp_path,part,operation):
    *_,limits,options=case;path,_=save(case,tmp_path)
    reader=q.RawPixelStreamFile(str(path),limits);reader.verify();assert reader.verified
    original=path.read_bytes();body_end=12+int.from_bytes(original[4:12],"little")
    offset={"dimensions":12,"options":150,"model":180,"last_body_byte":body_end-1}[part]
    assert 12<=offset<body_end
    mutate_header(path,offset)
    action={"check_unchanged":reader.check_unchanged,"link":reader.link,
            "read_block":lambda:reader.read_block(0),"verify":reader.verify,
            "trace":lambda:q.trace_raw_pixel_stream_file(reader,options)}[operation]
    with pytest.raises((RuntimeError,ValueError)):action()
    assert not reader.verified


@pytest.mark.parametrize("part",["second_chunk","last_body_byte"])
def test_large_header_checks_every_chunk_with_bounded_work(case,tmp_path,part):
    values=list(case);recipe=values[5].recipe;columns=len(recipe.feature_schema)
    samples=np.repeat(np.arange(128,dtype=np.float32)[:,None],columns,axis=1)
    labels=(np.arange(128)%2).astype(np.uint32)
    parameters=q.ForestParameters();parameters.trees=256;parameters.max_depth=8
    forest=q.train_forest(samples,labels,recipe.feature_schema,["A","B"],parameters)
    values[5]=q.RawPixelClassifier(forest,recipe);values[7].work_bytes=8*1024**2
    path,_=save(values,tmp_path);reader=q.RawPixelStreamFile(str(path),values[7])
    reader.verify();assert reader.verified and reader.payload_bytes<=values[7].work_bytes
    original=path.read_bytes();body_end=12+int.from_bytes(original[4:12],"little")
    assert body_end>2*65536
    mutate_header(path,65536 if part=="second_chunk" else body_end-1)
    with pytest.raises((RuntimeError,ValueError)):reader.verify()
    assert not reader.verified


def test_cli_historical_wkt_without_source_and_bounded_json(case,tmp_path):
    _,_,doc,_,_,_,_,limits,options=case;path,_=save(case,tmp_path)
    expected=q.trace_raw_pixel_stream_file(q.RawPixelStreamFile(str(path),limits),options)
    cli=Path(_native.__file__).with_name("qupathxx_batch.exe" if os.name=="nt" else "qupathxx_batch")
    pixels=Path(doc.server.source_locator);doc.server.close();missing=pixels.with_suffix(".missing");pixels.rename(missing)
    args=[str(cli),"--inspect-raw-contours",str(path),"--file-work-bytes",str(limits.work_bytes),"--total-work-bytes",str(options.work_bytes),"--boundary-work-bytes",str(options.boundary_work_bytes),"--contour-work-bytes",str(options.contours.work_bytes)]
    try:
        result=subprocess.run(args,capture_output=True,text=True,encoding="utf-8",timeout=60);assert result.returncode==0,result.stdout+result.stderr
        report=json.loads(result.stdout.splitlines()[-1]);assert report["schema"]=="qpx.raw-pixel-file-contours/1" and report["historical"] and report["state"]=="historical"
        assert report["components"]==len(expected.contours.components)
        for a,b in zip(report["geometry"],expected.contours.components):assert a["label"]==b.label and a["samples"]==b.pixels and a["wkt"]==b.geometry.wkt
        refused=subprocess.run([*args,"--json-bytes","1"],capture_output=True,text=True,encoding="utf-8",timeout=60);assert refused.returncode==1 and json.loads(refused.stdout.splitlines()[-1])["state"]=="failed"
    finally:missing.rename(pixels)


def test_cli_replay_saves_new_project_and_rejects_sampling_overrides(case,tmp_path):
    project,image,doc,root,_,_,_,limits,options=case;path,request=save(case,tmp_path,q.RawPixelStreamRegion(1,1,3,2))
    source=tmp_path/"source.qpx";project.save(str(source));before=source.read_bytes();file_before=path.read_bytes();expected=q.trace_raw_pixel_stream(request,options)
    cli=Path(_native.__file__).with_name("qupathxx_batch.exe" if os.name=="nt" else "qupathxx_batch");output=tmp_path/"replayed.qpx"
    args=[str(cli),"--replay-raw-contours",str(source),str(image),str(path),str(output),"--file-work-bytes",str(limits.work_bytes)]
    done=subprocess.run(args,capture_output=True,text=True,encoding="utf-8",timeout=60);assert done.returncode==0,done.stdout+done.stderr
    report=json.loads(done.stdout.splitlines()[-1]);assert report["state"]=="saved" and report["prediction_sha256"]==expected.prediction_sha256
    saved=q.Project.open(str(output))
    try:
        current=saved.document(image);assert current.children(root)==report["object_ids"]
        for obj,component in zip(report["object_ids"],expected.contours.components):assert current.annotation(obj).geometry.equals(component.geometry)
    finally:saved.document(image).server.close()
    assert source.read_bytes()==before and path.read_bytes()==file_before and doc.object_count==3
    refused=subprocess.run([*args[:6],"--x-axis","0,8,7,2"],capture_output=True,text=True,encoding="utf-8",timeout=60);assert refused.returncode==2


GUI=pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="opt-in GUI")

@GUI
@pytest.mark.parametrize("apply",[False,True])
@pytest.mark.parametrize("retained",[False,True])
def test_desktop_historical_preview_and_fresh_reverified_apply(case,tmp_path,apply,retained):
    from test_gui_raw_pixel import options as gui_options,click
    from test_gui_pixel_classifier import ready
    project,_,doc,root,lattice,_,_,limits,options=case;path,request=save(case,tmp_path)
    expected=q.trace_raw_pixel_stream(request,options)
    settings=q.RawPixelTrainingSettings();settings.lattice=lattice
    ui=gui_options(settings,root,tmp_path);raw=ui.raw_pixel;raw.stream_path=str(path);raw.read_limits=limits;raw.stream_contour_options=options;raw.model=None
    if retained:
        file=q.RawPixelStreamFile(str(path),limits);file.verify();raw.record_id=doc.keep_raw_pixel_stream(file.link());raw.stream_path=""
    ui.raw_pixel=raw
    ui.inputs=[*click(4,q.GuiInputTarget.RAW_PIXEL_RECORDS_TAB if retained else q.GuiInputTarget.RAW_PIXEL_FILES_TAB),*click(8,q.GuiInputTarget.RAW_PIXEL_TRACE_FILE),ready(12,q.GuiInputWait.RAW_PIXEL_CONTOURS_READY)]
    if apply:ui.inputs+=[*click(16,q.GuiInputTarget.RAW_PIXEL_REPLAY_FILE),ready(20,q.GuiInputWait.RAW_PIXEL_CONTOURS_READY),*click(24,q.GuiInputTarget.RAW_PIXEL_APPLY_FILE),ready(28,q.GuiInputWait.RAW_PIXEL_CONTOURS_PUBLISHED)]
    folder=Path(os.environ.get("QUPATHXX_HISTORY_EVIDENCE_DIR",tmp_path));folder.mkdir(parents=True,exist_ok=True)
    report=q.show(project,frame_limit=45,capture_path=str(folder/f"historical-{int(retained)}-{int(apply)}.ppm"),options=ui)
    assert report.replay_complete and not report.raw_pixel.error,report.raw_pixel.error
    assert report.raw_pixel.model is None and report.raw_pixel.contour_preview_ready and report.ui_overflow_items==0
    if apply:
        assert report.raw_pixel.file_replay and report.raw_pixel.stream_contours is not None and report.raw_pixel.historical_contours is None
        assert report.raw_pixel.contours_published and doc.children(root)==report.raw_pixel.published_ids
        for obj,component in zip(report.raw_pixel.published_ids,expected.contours.components):assert doc.annotation(obj).geometry.equals(component.geometry)
    else:assert report.raw_pixel.historical_contours is not None and report.raw_pixel.stream_contours is None and not report.raw_pixel.published_ids and doc.object_count==3

@GUI
@pytest.mark.parametrize("retained",[False,True])
def test_managed_history_preview_fresh_replay_apply_and_saved_geometry(case,tmp_path,retained):
    from test_viewer_session import completed,run
    project,image,doc,root,_,_,_,limits,options=case;path,_=save(case,tmp_path,q.RawPixelStreamRegion(1,1,3,2));source=tmp_path/"source.qpx"
    file=q.RawPixelStreamFile(str(path),limits);file.verify();record=doc.keep_raw_pixel_stream(file.link()) if retained else 0;input_path="" if retained else str(path)
    project.save(str(source));doc.server.close();saved=tmp_path/"replayed.qpx"
    session=q.ViewerSession(str(source));held={}
    def producer():
        preview=completed(session,session.contour_raw_pixel_file(image,input_path,limits,options,record_id=record))
        assert preview.raw_pixel_file_contours is not None and preview.raw_pixel_stream_contours is None and not preview.committed and not preview.object_ids
        held["historical"]=preview.raw_pixel_file_contours
        applied=completed(session,session.contour_raw_pixel_file(image,input_path,limits,options,record_id=record,replay=True,apply=True,expected_revision=preview.document_revision))
        assert applied.raw_pixel_file_contours is None and applied.raw_pixel_stream_contours is not None and applied.committed and len(applied.object_ids)==applied.components>0
        held["ids"]=applied.object_ids;held["geometry"]=[c.geometry for c in applied.raw_pixel_stream_contours.contours.components]
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    run(session,producer)
    assert held["historical"].completed_samples==6
    reopened=q.Project.open(str(saved))
    try:
        current=reopened.document(image);assert current.children(root)==held["ids"]
        for obj,geometry in zip(held["ids"],held["geometry"]):assert current.annotation(obj).geometry.equals(geometry)
    finally:reopened.document(image).server.close()

@GUI
def test_managed_history_quota_and_unverified_apply_refuse(case,tmp_path):
    from test_viewer_session import completed,run
    project,image,doc,_,_,_,_,limits,options=case;path,_=save(case,tmp_path);source=tmp_path/"source.qpx";project.save(str(source));doc.server.close();bound=q.ViewerLimits();bound.result_bytes=65536;session=q.ViewerSession(str(source),bound)
    with pytest.raises(ValueError):session.contour_raw_pixel_file(image,str(path),apply=True)
    def producer():
        ticket=session.contour_raw_pixel_file(image,str(path),limits,options,replay=True,apply=True);event=session.wait(ticket,15)
        assert event.state=="failed" and not event.committed and not event.object_ids and "result budget" in event.error
        session.take(ticket);completed(session,session.close(True))
    run(session,producer)


@pytest.mark.parametrize("mode",["history-cancel","history-close","replay-cancel","replay-close"])
def test_historical_and_replay_active_shutdown_in_child(case,tmp_path,mode):
    import sys
    from test_raw_pixel_python import tiff
    model=case[5];pixels=tmp_path/"large.tif";tiff(pixels,1024,768);project=q.Project();image=project.add_image(str(pixels),"tiff");doc=project.document(image);root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1024,768)))
    lattice=q.ScientificRawLattice(1024,768,q.ScientificGridAxis(0,1024,896,2),q.ScientificGridAxis(0,768,640,2));stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=16;stream.work_bytes=16*1024**2
    request=q.RawPixelTiledPredictionRequest(doc,model,lattice,stream,object_id=root);file=tmp_path/"large.qws";q.save_raw_pixel_stream(request,str(file));source=tmp_path/"source.qpx";project.save(str(source));doc.server.close()
    folder=Path(os.environ.get("QUPATHXX_HISTORY_SHUTDOWN_DIR",tmp_path));folder.mkdir(parents=True,exist_ok=True);receipt=folder/(mode+".json")
    done=subprocess.run([sys.executable,"-I",str(Path(__file__).with_name("raw_file_contour_shutdown_child.py")),mode,str(source),str(file),str(receipt)],capture_output=True,text=True,encoding="utf-8",timeout=45)
    assert done.returncode==0,done.stdout+done.stderr
    result=json.loads(receipt.read_text());assert result["observed_busy"] and result["observed_state"]=="running" and result["shutdown_completed"]
    pixels.rename(tmp_path/"source-released.tif");file.rename(tmp_path/"history-released.qws")


def test_replay_changed_source_pixels_refuse_but_historical_geometry_remains(case,tmp_path):
    project,image,doc,_,_,_,_,limits,options=case;path,_=save(case,tmp_path);file=q.RawPixelStreamFile(str(path),limits);historical=q.trace_raw_pixel_stream_file(file,options)
    before=[c.geometry.wkt for c in historical.contours.components];source=tmp_path/"source.qpx";project.save(str(source));pixels=Path(doc.server.source_locator);doc.server.close();original=pixels.read_bytes();changed=bytearray(original);changed[160]^=64;pixels.write_bytes(changed)
    current=None
    try:
        current=q.Project.open(str(source));live_doc=current.document(image)
        with pytest.raises((ValueError,RuntimeError)):
            request=q.capture_raw_pixel_stream_replay(live_doc,file);result=q.trace_raw_pixel_stream_replay(request,file,options);q.publish_raw_pixel_stream_contours(live_doc,request,result)
        assert live_doc.object_count==3
        again=q.trace_raw_pixel_stream_file(file,options);assert [c.geometry.wkt for c in again.contours.components]==before
    finally:
        if current is not None:current.document(image).server.close()
        pixels.write_bytes(original)


def test_replay_changed_model_is_refused_before_job_start(case,tmp_path):
    _,_,doc,root,lattice,model,stream,limits,options=case;path,_=save(case,tmp_path);file=q.RawPixelStreamFile(str(path),limits);file.verify()
    ids=[a.id for a in doc.annotations if a.class_name in ("A","B")];selection=q.ScientificRawOptions();selection.channels=2;features=q.RawPixelFeatureOptions();features.offsets=[0,0];features.scales=[1,1]
    table=q.sample_raw_pixel_tiled_training(q.RawPixelTiledTrainingRequest(doc,ids,lattice,selection,features));parameters=q.ForestParameters();parameters.trees=7;parameters.max_depth=3;parameters.work_bytes=2*1024**2
    alternate=q.train_raw_pixel_tiled_classifier(table,parameters);assert alternate.sha256!=model.sha256
    request=q.RawPixelTiledPredictionRequest(doc,alternate,lattice,stream,file.region,root)
    with pytest.raises(ValueError):q.trace_raw_pixel_stream_replay(request,file,options)
    job=q.RawPixelStreamContourJob()
    with pytest.raises(ValueError):job.request_replay(request,file,options)
    assert not job.busy and job.result is None and doc.object_count==3 and file.verified


def test_replay_ready_result_rejects_later_document_revision(case,tmp_path):
    _,_,doc,root,_,_,_,limits,options=case;path,_=save(case,tmp_path);file=q.RawPixelStreamFile(str(path),limits);file.verify();request=q.capture_raw_pixel_stream_replay(doc,file)
    job=q.RawPixelStreamContourJob();assert job.request_replay(request,file,options) and job.wait(doc).state=="ready";retained=job.result;assert retained is not None
    doc.rename_annotation(root,"edited after replay")
    assert job.poll(doc).state=="stale" and job.result is None
    with pytest.raises((ValueError,RuntimeError)):q.publish_raw_pixel_stream_contours(doc,request,retained)
    assert doc.object_count==3 and doc.annotation(root).name=="edited after replay"
