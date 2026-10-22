"""Installed real-window pixel workflow, bounded catalog and native mask oracle."""
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
    rgb=np.empty((512,512,3),dtype=np.uint8);rgb[:,:128]=[220,30,10];rgb[:,128:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_history_limits(0,0);doc.set_calibration(.5,.5)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,128,128)),'Red training area');b=doc.add_roi(q.Roi.rectangle(q.Rect(128,0,128,128)),'Blue training area')
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');doc.set_class('red',0xff0000);doc.set_class('blue',0x0000ff)
    root=doc.add_roi(q.Roi.ellipse(q.Rect(4,4,248,120)),'Prediction area')
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(450,450,1,1)),kind=q.ObjectKind.CELL) for _ in range(args.objects)])
    training=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0,1];training.features=features;training.max_samples=128;training.max_samples_per_class=64;training.tile_size=32
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4;parameters.work_bytes=64*1024*1024
    options=q.GuiOptions();options.pixel_open=True;options.pixel_object_id=root;options.pixel_training_options=training;options.pixel_parameters=parameters;options.replay_timeout_seconds=30
    options.autosave_interval_seconds=0;options.settle_tiles=True;view=q.Viewport();view.origin=q.Point(0,0);view.zoom=3;options.start_view=view
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_TRAIN),event(8,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_TRAINED),*click(12,q.GuiInputTarget.PIXEL_PREDICT),event(16,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_PREVIEW_READY)]
    if args.capture:args.capture.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();report=q.show(doc,frame_limit=100,capture_path=str(args.capture) if args.capture else '',options=options);wall=(time.perf_counter()-started)*1000
    assert report.replay_complete and report.pixel_preview_ready and report.pixel_overlay_visible and not report.pixel_error,report.pixel_error
    assert report.pixel_catalog_complete and report.pixel_catalog_inspected==args.objects+3 and report.pixel_catalog_max_step<=1024
    assert report.pixel_training_table.row_count==128 and report.pixel_training_table.sampled_class_counts==[64,64]
    assert report.ui_overflow_items==0,report.ui_overflow_label
    result=report.pixel_result;bounds=result.level_bounds;y,x=np.indices((result.height,result.width));gx=x+int(bounds.x);gy=y+int(bounds.y)
    valid=np.array([[doc.annotation(root).geometry.contains(q.Point(xx+.5,yy+.5)) for xx in row] for row,yy in zip(gx,gy[:,0])]);expected=np.where(gx<128,2,1).astype(np.uint32);expected[~valid]=0
    np.testing.assert_array_equal(result.labels_numpy(),expected);assert report.pixel_gpu_bytes==result.width*result.height*4
    env=q.build_environment();output=dict(schema='qpx.gui-pixel-benchmark/1',objects=args.objects,total_objects=doc.object_count,wall_ms=wall,frames=report.frames,
        cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,capture_ms=report.pixel_capture_ms,
        catalog_inspected=report.pixel_catalog_inspected,catalog_max_step=report.pixel_catalog_max_step,catalog_complete=report.pixel_catalog_complete,
        training_rows=report.pixel_training_table.row_count,training_sha256=report.pixel_training_table.sha256,model_sha256=report.pixel_model.sha256,prediction_sha256=result.sha256,
        result_width=result.width,result_height=result.height,classified_pixels=result.classified_pixels,gpu_bytes=report.pixel_gpu_bytes,peak_gpu_bytes=report.pixel_peak_gpu_bytes,
        preview_tiles=report.pixel_uploaded_tiles,preview_generation=report.pixel_preview_generation,overlay_cache_hits=report.overlay_cache_hits,overlay_cache_builds=report.overlay_cache_builds,
        ui_overflow=report.ui_overflow_items,renderer=report.gl_renderer,gl_version=report.gl_version,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='One synthetic observation on shared workstation. Native ImGui buttons start annotation sampling/training and source prediction, followed by real GL preview. Unlabelled cells are outside the fixed camera view; this measures catalog work amid a large object map, not dense visible drawing. Budget counts1024 objects per catalog step and caps preview uploads at four256px tiles/frame. Every mask label is checked; GPU payload is exact for this crop. No biological, UI frame-budget, process RSS or hardware acceptance claim.')
    if args.capture:output['capture']=dict(file=args.capture.name,sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest())
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf8');print(json.dumps(output,indent=2))


if __name__=='__main__':main()
