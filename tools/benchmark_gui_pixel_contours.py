"""Real-window prediction -> background contours -> classified publication and undo."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def event(frame,kind,target=q.GuiInputTarget.CANVAS,wait=q.GuiInputWait.IMMEDIATE):
    e=q.GuiInput();e.frame=frame;e.kind=kind;e.target=target;e.wait_for=wait;return e


def click(frame,target):
    return [event(frame,q.GuiInputKind.LEFT_DOWN,target,q.GuiInputWait.TARGET_READY),event(frame+1,q.GuiInputKind.LEFT_UP,target)]


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--objects',type=int,default=10000);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--capture',type=Path);args=ap.parse_args()
    if not 0<=args.objects<=100000:ap.error('objects must be0..100000')
    yy,xx=np.indices((512,512));pattern=(xx//16+yy//16)%2;rgb=np.where(pattern[:,:,None]==0,np.array([220,30,10],dtype=np.uint8),np.array([10,30,220],dtype=np.uint8))
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_history_limits(0,0);doc.set_calibration(.5,.5)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,16,16)),'Red training area');b=doc.add_roi(q.Roi.rectangle(q.Rect(16,0,16,16)),'Blue training area')
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');doc.set_class('red',0xff0000);doc.set_class('blue',0x0000ff)
    root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,512,512)),'Prediction area')
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(450,450,1,1)),kind=q.ObjectKind.CELL) for _ in range(args.objects)])
    training=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0];training.features=features;training.max_samples=128;training.max_samples_per_class=64;training.tile_size=32
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4;parameters.work_bytes=64*1024*1024
    options=q.GuiOptions();options.pixel_open=True;options.pixel_object_id=root;options.pixel_training_options=training;options.pixel_parameters=parameters;options.replay_timeout_seconds=30
    options.autosave_interval_seconds=0;options.settle_tiles=True;view=q.Viewport();view.origin=q.Point(0,0);view.zoom=3;options.start_view=view
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=training)),parameters);options.pixel_model=model
    oracle=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model))
    np.testing.assert_array_equal(oracle.labels_numpy(),np.where(pattern==0,model.classes.index('red')+1,model.classes.index('blue')+1))
    doc.set_history_limits(100,64*1024*1024);doc.clear_history()
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),event(8,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_PREVIEW_READY),*click(12,q.GuiInputTarget.PIXEL_TRACE),event(16,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_CONTOURS_READY),*click(20,q.GuiInputTarget.PIXEL_PUBLISH),event(24,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_CONTOURS_PUBLISHED)]
    if args.capture:args.capture.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();report=q.show(doc,frame_limit=240,capture_path=str(args.capture) if args.capture else '',options=options);wall=(time.perf_counter()-started)*1000
    assert report.replay_complete and report.pixel_contours_published and not report.pixel_error,report.pixel_error
    assert report.pixel_catalog_complete and report.pixel_catalog_inspected==args.objects+5 and report.pixel_catalog_max_step<=1024
    assert report.ui_overflow_items==0,report.ui_overflow_label
    assert report.pixel_gpu_bytes==0 and report.pixel_result is None and not report.pixel_overlay_visible
    traced=report.pixel_contours;geometry=traced.contours
    assert traced.prediction_sha256==oracle.sha256 and geometry.component_count==2 and len(report.pixel_published_ids)==2
    assert [c.pixels for c in geometry.components]==oracle.class_counts
    assert all(c.geometry.area==c.pixels for c in geometry.components)
    assert sum(len(c.geometry.polygon_parts) for c in geometry.components)==1024
    assert doc.children(root)==report.pixel_published_ids and doc.object_count==args.objects+5
    history_bytes=doc.history_bytes
    assert doc.undo() and doc.object_count==args.objects+3 and doc.redo() and doc.children(root)==report.pixel_published_ids
    env=q.build_environment();output=dict(schema='qpx.gui-pixel-contour-benchmark/1',objects=args.objects,total_objects=doc.object_count,wall_ms=wall,frames=report.frames,
        cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,capture_ms=report.pixel_capture_ms,
        catalog_inspected=report.pixel_catalog_inspected,catalog_max_step=report.pixel_catalog_max_step,catalog_complete=report.pixel_catalog_complete,
        model_sha256=model.sha256,prediction_sha256=oracle.sha256,contour_ms=geometry.elapsed_ms,edge_count=geometry.edge_count,vertex_count=geometry.vertex_count,
        estimated_contour_work_bytes=geometry.estimated_work_bytes,retained_geometry_bytes=geometry.retained_bytes,history_bytes=history_bytes,
        published_annotations=len(report.pixel_published_ids),polygon_parts=1024,gpu_bytes=report.pixel_gpu_bytes,peak_gpu_bytes=report.pixel_peak_gpu_bytes,
        overlay_cache_hits=report.overlay_cache_hits,overlay_cache_builds=report.overlay_cache_builds,ui_overflow=report.ui_overflow_items,renderer=report.gl_renderer,gl_version=report.gl_version,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='Single synthetic shared-workstation software-GL observation. Real ImGui buttons start source prediction, background contour tracing and owner-thread atomic class publication. Two multipolygon class annotations contain1024 square parts. Every prepublication mask label is checked and the traced output SHA must match; geometry areas and undo/redo are verified. Unlabelled cells are outside the fixed camera. Includes bounded catalog rebuild after publication. No dense visible-cell, biological, UI frame-budget, RSS or hardware acceptance claim.')
    if args.capture:output['capture']=dict(file=args.capture.name,sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest())
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf8');print(json.dumps(output,indent=2))


if __name__=='__main__':main()
