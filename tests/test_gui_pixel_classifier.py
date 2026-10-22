"""Actual ImGui actions, source masks and rendered-overlay/cache oracles."""
import os
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_classifier_training or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='requires GUI/training and QUPATHXX_TEST_GUI=1')


def event(frame,kind,target=q.GuiInputTarget.CANVAS,wait=q.GuiInputWait.IMMEDIATE):
    e=q.GuiInput();e.frame=frame;e.kind=kind;e.target=target;e.wait_for=wait;return e


def click(frame,target):
    return [event(frame,q.GuiInputKind.LEFT_DOWN,target,q.GuiInputWait.TARGET_READY),event(frame+1,q.GuiInputKind.LEFT_UP,target)]


def ready(frame,condition):return event(frame,q.GuiInputKind.MOVE,wait=condition)


def workspace(size=96):
    height=64 if size==96 else size
    rgb=np.empty((height,size,3),dtype=np.uint8);rgb[:,:size//2]=[220,30,10];rgb[:,size//2:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(.5,.5)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,size//2,height)),'Red labels');b=doc.add_roi(q.Roi.rectangle(q.Rect(size//2,0,size-size//2,height)),'Blue labels')
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');doc.set_class('red',0xff0000);doc.set_class('blue',0x0000ff)
    root=doc.add_roi(q.Roi.ellipse(q.Rect(4,4,size-8,height-8)),'Prediction area')
    training=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0,1];training.features=features
    training.max_samples=128;training.max_samples_per_class=64;training.tile_size=32
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4
    options=q.GuiOptions();options.pixel_open=True;options.pixel_object_id=root;options.pixel_training_options=training;options.pixel_parameters=parameters
    options.autosave_interval_seconds=0;options.replay_timeout_seconds=20
    return doc,[a,b],root,options


def model_for(doc,ids,options):return q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,ids,options=options.pixel_training_options)),options.pixel_parameters)


def test_real_panel_trains_predicts_previews_and_saves(tmp_path):
    doc,ids,root,options=workspace();revision=doc.revision;path=tmp_path/'trained.qpc';options.pixel_model_path=str(path)
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_TRAIN),ready(8,q.GuiInputWait.PIXEL_TRAINED),
        *click(12,q.GuiInputTarget.PIXEL_PREDICT),ready(16,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(20,q.GuiInputTarget.PIXEL_SAVE_MODEL),ready(24,q.GuiInputWait.PIXEL_MODEL_SAVED)]
    report=q.show(doc,frame_limit=60,options=options)
    assert report.replay_complete and not report.pixel_error and not report.pixel_io_error
    assert report.pixel_training_state==report.pixel_prediction_state=='ready'
    assert report.pixel_model.forest.training_rows==128 and report.pixel_training_table.sampled_class_counts==[64,64]
    assert report.pixel_result.object_id==root and report.pixel_preview_ready and report.pixel_overlay_visible
    assert 0<report.pixel_gpu_bytes<=options.pixel_preview_budget and report.pixel_uploaded_tiles==1
    assert q.load_pixel_classifier(str(path)).sha256==report.pixel_model.sha256 and report.pixel_io_state=='saved'
    assert doc.revision==revision and [doc.annotation(i).class_name for i in ids]==['red','blue']
    assert report.ui_overflow_items==0


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_loaded_model_predicts_in_responsive_panel(tmp_path,dimensions,scale):
    doc,ids,_,options=workspace();model=model_for(doc,ids,options);path=tmp_path/'model.qpc';q.save_pixel_classifier(model,str(path))
    options.width,options.height=dimensions;options.ui_scale=scale;options.pixel_model_path=str(path)
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_LOAD_MODEL),ready(8,q.GuiInputWait.PIXEL_MODEL_LOADED),*click(12,q.GuiInputTarget.PIXEL_PREDICT),ready(16,q.GuiInputWait.PIXEL_PREVIEW_READY)]
    report=q.show(doc,frame_limit=50,options=options)
    assert report.replay_complete and not report.pixel_error and not report.pixel_io_error
    assert report.pixel_model.sha256==model.sha256 and report.pixel_preview_ready and report.pixel_overlay_visible
    assert report.ui_overflow_items==0,report.ui_overflow_label


def ppm(path):
    magic,size,maximum,payload=Path(path).read_bytes().split(b'\n',3)
    assert magic==b'P6' and maximum==b'255';w,h=map(int,size.split());return np.frombuffer(payload,dtype=np.uint8).reshape(h,w,3)


def sample_world(pixels,report,x,y):
    scale=report.framebuffer_scale
    xx=round((report.canvas_content_bounds.x+report.final_origin.x+x*report.final_zoom)*scale)
    yy=round((report.canvas_content_bounds.y+report.final_origin.y+y*report.final_zoom)*scale)
    return pixels[yy,xx].astype(int)


def test_rendered_mask_and_overlay_toggle_invalidate_cached_canvas(tmp_path):
    captures=[]
    for hidden in [False,True]:
        doc,ids,root,options=workspace();doc.update_roi(root,q.Roi.polygon([q.Point(4,4),q.Point(92,12),q.Point(84,60),q.Point(12,52)],[[q.Point(13,15),q.Point(25,15),q.Point(25,27),q.Point(13,27)]]));options.pixel_model=model_for(doc,ids,options);options.settle_tiles=True
        options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY)]
        if hidden:options.inputs+=click(12,q.GuiInputTarget.PIXEL_TOGGLE_OVERLAY)
        options.inputs+=[event(20,q.GuiInputKind.PIXEL_CLASSIFIERS_KEY)]
        path=tmp_path/('hidden.ppm' if hidden else 'visible.ppm');report=q.show(doc,frame_limit=45,capture_path=str(path),options=options)
        assert report.pixel_preview_ready and report.pixel_overlay_visible!=hidden and report.overlay_cache_hits>0
        captures.append((ppm(path),report))
    visible,shown=captures[0];baseline,hidden=captures[1]
    for x in [20,76]:
        overlay=sample_world(visible,shown,x,32);raw=sample_world(baseline,hidden,x,32)
        assert np.max(np.abs(overlay-raw))>=10
        expected=[236,16,5] if x==20 else [5,16,236]
        np.testing.assert_allclose(overlay,expected,atol=3)
    labels=shown.pixel_result.labels_numpy();bounds=shown.pixel_result.level_bounds;checked=0
    for y in range(7,59,3):
        for x in range(6,91,3):
            raw=sample_world(baseline,hidden,x+.5,y+.5)
            color=np.array([220,30,10] if x<48 else [10,30,220])
            if np.max(np.abs(raw-color))>1:continue # Exclude annotation strokes/text.
            label=labels[y-int(bounds.y),x-int(bounds.x)]
            target=np.array([0,0,255] if label==1 else [255,0,0])
            expected=np.rint(raw*(1-115/255)+target*115/255) if label else raw
            np.testing.assert_allclose(sample_world(visible,shown,x+.5,y+.5),expected,atol=3)
            checked+=1
    assert checked>300
    np.testing.assert_array_equal(sample_world(visible,shown,20,20),sample_world(baseline,hidden,20,20))
    # Outside the polygon both captures retain the unclassified image.
    np.testing.assert_array_equal(sample_world(visible,shown,6,55),sample_world(baseline,hidden,6,55))


def test_invalid_model_file_preserves_current_model_and_pixels(tmp_path):
    doc,ids,_,options=workspace();model=model_for(doc,ids,options);options.pixel_model=model
    path=tmp_path/'image.tif';path.write_bytes(b'preserve this image');options.pixel_model_path=str(path)
    options.inputs=click(4,q.GuiInputTarget.PIXEL_LOAD_MODEL)
    report=q.show(doc,frame_limit=30,options=options)
    assert report.pixel_io_state=='failed' and report.pixel_io_error and report.pixel_model.sha256==model.sha256
    assert path.read_bytes()==b'preserve this image' and report.pixel_result is None


def test_preview_budget_failure_keeps_numeric_result_without_gpu_overflow():
    doc,ids,_,options=workspace();options.pixel_model=model_for(doc,ids,options);options.pixel_preview_budget=1
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREDICTED)]
    report=q.show(doc,frame_limit=35,options=options)
    assert report.pixel_result is not None and not report.pixel_preview_ready
    assert report.pixel_gpu_bytes==0 and not report.pixel_overlay_visible and 'preview budget' in report.pixel_error


def test_training_cancellation_does_not_publish_a_new_model():
    doc,_,_,options=workspace(1024);training=options.pixel_training_options
    features=training.features;features.sigmas=[0,2,4];features.gradient_magnitude=True;training.features=features;options.pixel_training_options=training
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_TRAIN),*click(7,q.GuiInputTarget.PIXEL_CANCEL)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.pixel_training_state=='cancelled' and report.pixel_model is None and report.pixel_training_table is None
    assert report.pixel_gpu_bytes==0 and report.replay_complete


def test_annotation_catalog_has_bounded_per_frame_work():
    doc,_,_,options=workspace()
    doc.set_history_limits(0,0)
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(80,50,1,1)),kind=q.ObjectKind.CELL) for _ in range(12000)])
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_TRAIN),ready(8,q.GuiInputWait.PIXEL_TRAINED)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.pixel_catalog_complete and report.pixel_catalog_inspected==12003
    assert report.pixel_catalog_max_step==1024 and report.pixel_training_table.row_count==128


def test_image_switch_retires_pixel_mask_and_gpu_preview():
    doc,ids,_,options=workspace();options.pixel_model=model_for(doc,ids,options)
    other,_,_,_=workspace();project=q.Project();first=project.add_document(doc,'First');project.add_document(other,'Second');project.active_image=first
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),event(16,q.GuiInputKind.NEXT_IMAGE_KEY)]
    report=q.show(project,frame_limit=45,options=options)
    assert report.image_switches==1 and report.pixel_result is None and not report.pixel_preview_ready and report.pixel_gpu_bytes==0



def test_multiple_preview_tiles_finish_within_payload_budget():
    doc,ids,_,options=workspace(1024);options.pixel_model=model_for(doc,ids,options)
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY)]
    report=q.show(doc,frame_limit=45,options=options)
    assert not report.pixel_error and report.pixel_preview_ready and report.pixel_uploaded_tiles==16
    assert report.pixel_gpu_bytes==1016*1016*4 and report.pixel_peak_gpu_bytes<=options.pixel_preview_budget


def test_closed_unused_panel_does_not_scan_object_catalog():
    doc,_,_,options=workspace();options.pixel_open=False
    report=q.show(doc,frame_limit=15,options=options)
    assert report.pixel_catalog_inspected==report.pixel_catalog_max_step==0 and not report.pixel_catalog_complete
    assert report.pixel_gpu_bytes==0 and report.pixel_model is None


@pytest.mark.skipif(not q.has_geos,reason="optional GEOS contour backend")
@pytest.mark.parametrize("undo,redo",[(False,False),(True,False),(True,True)])
def test_panel_traces_and_publishes_one_class_annotation_batch(undo,redo):
    doc,ids,root,options=workspace();options.pixel_model=model_for(doc,ids,options);doc.clear_history()
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_TRACE),ready(16,q.GuiInputWait.PIXEL_CONTOURS_READY),
        *click(20,q.GuiInputTarget.PIXEL_PUBLISH),ready(24,q.GuiInputWait.PIXEL_CONTOURS_PUBLISHED)]
    if undo:options.inputs += [event(28,q.GuiInputKind.UNDO_KEY)]
    if redo:options.inputs += [event(32,q.GuiInputKind.REDO_KEY)]
    report=q.show(doc,frame_limit=55,options=options)
    assert report.replay_complete and not report.pixel_error and report.pixel_contour_state=="published"
    assert report.pixel_contours_published and len(report.pixel_published_ids)==2 and report.pixel_contours.contours.component_count==2
    assert report.pixel_result is None and report.pixel_gpu_bytes==0 and not report.pixel_overlay_visible
    assert doc.object_class("red").color==0xff0000 and doc.object_class("blue").color==0x0000ff
    if undo and not redo:
        assert doc.children(root)==[] and doc.object_count==3 and doc.can_redo,(doc.undo_label,doc.redo_label,doc.revision,report.replay_complete)
    else:
        assert doc.children(root)==report.pixel_published_ids and doc.object_count==5
        for obj in report.pixel_published_ids:
            a=doc.annotation(obj);assert a.class_name in ("red","blue") and a.parent_id==root
    assert report.ui_overflow_items==0,report.ui_overflow_label


@pytest.mark.skipif(not q.has_geos,reason="optional GEOS contour backend")
def test_contour_work_budget_failure_preserves_prediction():
    doc,ids,_,options=workspace();options.pixel_model=model_for(doc,ids,options);revision=doc.revision
    limits=q.TissueContourOptions();limits.work_bytes=1;options.pixel_contour_options=limits
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),*click(12,q.GuiInputTarget.PIXEL_TRACE)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.replay_complete and report.pixel_contour_state=="failed" and "budget" in report.pixel_error
    assert report.pixel_result is not None and report.pixel_preview_ready and report.pixel_contours is None
    assert not report.pixel_contours_published and doc.revision==revision


@pytest.mark.skipif(not q.has_geos,reason="optional GEOS contour backend")
@pytest.mark.parametrize("raw",[False,True])
def test_narrow_panel_exposes_trace_publish_and_clipping(raw):
    doc,ids,root,options=workspace();options.pixel_model=model_for(doc,ids,options);options.width=960;options.height=720;options.ui_scale=1.25
    limits=q.TissueContourOptions();limits.clip_to_roi=not raw;options.pixel_contour_options=limits
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_TRACE),ready(16,q.GuiInputWait.PIXEL_CONTOURS_READY),
        *click(20,q.GuiInputTarget.PIXEL_PUBLISH),ready(24,q.GuiInputWait.PIXEL_CONTOURS_PUBLISHED)]
    report=q.show(doc,frame_limit=50,options=options)
    assert report.replay_complete and report.pixel_contours_published and not report.pixel_error
    assert all(doc.annotation(i).parent_id==(0 if raw else root) for i in report.pixel_published_ids)
    assert report.pixel_contours.contours.options.clip_to_roi!=raw and report.ui_overflow_items==0,report.ui_overflow_label


@pytest.mark.skipif(not (q.has_geos and q.has_project),reason="optional GEOS/project backends")
@pytest.mark.parametrize("published",[False,True])
def test_image_switch_retires_contours_and_publication_receipt(published):
    doc,ids,_,options=workspace();options.pixel_model=model_for(doc,ids,options)
    project=q.Project();first=project.add_document(doc);project.add_document(q.Document(q.Image.from_numpy(np.full((64,96,3),120,dtype=np.uint8))));project.active_image=first
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_TRACE),ready(16,q.GuiInputWait.PIXEL_CONTOURS_READY)]
    if published:options.inputs += [*click(20,q.GuiInputTarget.PIXEL_PUBLISH),ready(24,q.GuiInputWait.PIXEL_CONTOURS_PUBLISHED)]
    options.inputs += [event(28,q.GuiInputKind.NEXT_IMAGE_KEY)]
    report=q.show(project,frame_limit=55,options=options)
    assert report.replay_complete and report.image_switches==1
    assert report.pixel_contours is None and not report.pixel_contours_published and report.pixel_published_ids==[]
    assert report.pixel_result is None and report.pixel_gpu_bytes==0
    assert doc.object_count==(5 if published else 3)


@pytest.mark.skipif(not q.has_geos,reason="optional GEOS contour backend")
def test_empty_contour_publication_reports_noop_without_document_edit():
    doc,ids,root,options=workspace();options.pixel_model=model_for(doc,ids,options)
    shell=[q.Point(3,3),q.Point(6,3),q.Point(6,6),q.Point(3,6)]
    hole=[q.Point(3.1,3.1),q.Point(3.1,5.9),q.Point(5.9,5.9),q.Point(5.9,3.1)]
    doc.update_roi(root,q.Roi.multipolygon([q.PolygonPart(shell,[hole])]))
    doc.clear_history();revision=doc.revision
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_TRACE),ready(16,q.GuiInputWait.PIXEL_CONTOURS_READY),
        *click(20,q.GuiInputTarget.PIXEL_PUBLISH),ready(24,q.GuiInputWait.PIXEL_CONTOURS_PUBLISHED)]
    report=q.show(doc,frame_limit=50,options=options)
    assert report.replay_complete and report.pixel_contours_published and report.pixel_published_ids==[]
    assert report.pixel_contours.contours.component_count==0 and not report.pixel_error
    assert doc.revision==revision and not doc.can_undo and report.pixel_result.classified_pixels==0
