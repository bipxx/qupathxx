"""Installed F5 retention, historical mask reuse and bounded object indexing."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import numpy as np
import qupathxx as q
from qupathxx import _native


def event(frame,kind,target=q.GuiInputTarget.CANVAS,wait=q.GuiInputWait.IMMEDIATE):
    e=q.GuiInput();e.frame=frame;e.kind=kind;e.target=target;e.wait_for=wait;return e


def click(frame,target):
    return [event(frame,q.GuiInputKind.LEFT_DOWN,target,q.GuiInputWait.TARGET_READY),event(frame+1,q.GuiInputKind.LEFT_UP,target)]


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--objects',type=int,default=10000);ap.add_argument('--size',type=int,default=1024);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--capture',type=Path);args=ap.parse_args()
    if not 0<=args.objects<=100000 or not 64<=args.size<=2048:ap.error('objects must be 0..100000 and size 64..2048')
    size=args.size;rgb=np.empty((size,size,3),dtype=np.uint8);rgb[:,:size//2]=[220,30,10];rgb[:,size//2:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_history_limits(0,0);doc.set_calibration(.5,.5)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,size//2,size)));b=doc.add_roi(q.Roi.rectangle(q.Rect(size//2,0,size//2,size)))
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,size,size)))
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(size-2,size-2,1,1)),kind=q.ObjectKind.CELL) for _ in range(args.objects)])
    training=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0];training.features=features;training.max_samples=128;training.max_samples_per_class=64
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=training)),parameters)
    oracle=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model));revision=doc.revision
    options=q.GuiOptions();options.pixel_open=True;options.pixel_model=model;options.pixel_object_id=root;options.autosave_interval_seconds=0;options.replay_timeout_seconds=60
    options.settle_tiles=True;view=q.Viewport();view.origin=q.Point(0,0);view.zoom=3;options.start_view=view
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),event(8,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_KEEP_PREDICTION),event(16,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_RECORD_KEPT),
        *click(20,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),event(24,q.GuiInputKind.MOVE,wait=q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY)]
    if args.capture:args.capture.parent.mkdir(parents=True,exist_ok=True)
    report=q.show(doc,frame_limit=240,capture_path=str(args.capture) if args.capture else '',options=options)
    assert report.replay_complete and not report.pixel_error,report.pixel_error
    assert report.pixel_archive_state=='kept' and doc.revision==revision+1 and doc.pixel_classifier_record_count==1
    assert report.pixel_result is None and report.pixel_historical_result.sha256==oracle.sha256
    np.testing.assert_array_equal(report.pixel_historical_result.labels_numpy(),oracle.labels_numpy())
    np.testing.assert_array_equal(report.pixel_historical_result.vote_fractions_numpy(),oracle.vote_fractions_numpy())
    assert report.pixel_preview_ready and report.pixel_overlay_visible and not report.pixel_contours_published
    assert report.pixel_catalog_complete and report.pixel_catalog_max_step<=1024
    assert report.pixel_record_catalog_complete and report.pixel_record_catalog_max_step<=1024
    assert report.ui_overflow_items==0,report.ui_overflow_label
    record=doc.pixel_classifier_record(report.pixel_record_id);env=q.build_environment()
    output=dict(schema='qpx.gui-pixel-archive-benchmark/1',objects=args.objects,size=size,frames=report.frames,
        cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,capture_ms=report.pixel_capture_ms,
        catalog_max_step=report.pixel_catalog_max_step,record_catalog_max_step=report.pixel_record_catalog_max_step,
        historical_mask_sha256=oracle.sha256,archive_sha256=record.archive.sha256,retained_bytes=record.archive.payload_bytes,
        gpu_bytes=report.pixel_gpu_bytes,peak_gpu_bytes=report.pixel_peak_gpu_bytes,ui_overflow=report.ui_overflow_items,renderer=report.gl_renderer,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='Synthetic shared-workstation software-GL observation. F5 predicts, worker-prepares and retains a mask, then displays the historical artifact. All labels/votes agree with a fresh native oracle. Unlabelled cells lie outside the fixed camera. This does not establish dense visible-object, frame-budget, RSS, hardware or biological acceptance.')
    if args.capture:output['capture']=dict(file=args.capture.name,sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest())
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8');print(json.dumps(output,indent=2))


if __name__=='__main__':main()
