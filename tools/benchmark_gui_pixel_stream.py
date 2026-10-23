"""Real F5 background export with a large offscreen object catalog and block oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import time
import uuid
import numpy as np
import qupathxx as q
from qupathxx import _native


def click(frame,target):
    result=[]
    for offset,kind in enumerate([q.GuiInputKind.LEFT_DOWN,q.GuiInputKind.LEFT_UP]):
        e=q.GuiInput();e.frame=frame+offset;e.kind=kind;e.target=target
        if not offset:e.wait_for=q.GuiInputWait.TARGET_READY
        result.append(e)
    return result


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=1024);ap.add_argument('--objects',type=int,default=10000);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--file',type=Path);ap.add_argument('--capture',type=Path);args=ap.parse_args()
    if not 128<=args.size<=4096 or not 0<=args.objects<=100000:ap.error('size must be128..4096;objects0..100000')
    n=args.size;rgb=np.empty((n,n,3),np.uint8);rgb[:,:n//2]=[220,30,10];rgb[:,n//2:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));del rgb;doc.set_history_limits(0,0)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(n//2-32,0,32,32)));b=doc.add_roi(q.Roi.rectangle(q.Rect(n//2,0,32,32)))
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,n,n)))
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(n-2,n-2,1,1)),kind=q.ObjectKind.CELL) for _ in range(args.objects)])
    training=q.PixelTrainingOptions();f=q.PixelFeatureOptions();f.sigmas=[0,1];f.gradient_magnitude=True;f.laplacian=True;training.features=f;training.max_samples=128;training.max_samples_per_class=64
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=training)),parameters)
    stream=q.PixelStreamOptions();stream.block_size=128;p=q.PixelPredictionOptions();p.work_bytes=8*1024*1024;stream.prediction=p
    path=args.file or Path('build/pixel-stream-files')/(uuid.uuid4().hex+'.qps');path.parent.mkdir(parents=True,exist_ok=True)
    o=q.GuiOptions();o.pixel_open=True;o.pixel_model=model;o.pixel_object_id=root;o.pixel_stream_options=stream;o.pixel_stream_path=str(path)
    o.autosave_interval_seconds=0;o.settle_tiles=True;o.replay_timeout_seconds=45
    view=q.Viewport();view.zoom=3;view.origin=q.Point(0,0);o.start_view=view
    done=q.GuiInput();done.frame=8;done.kind=q.GuiInputKind.MOVE;done.wait_for=q.GuiInputWait.PIXEL_STREAM_FINISHED
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_EXPORT_STREAM),done];revision=doc.revision
    if args.capture:args.capture.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();report=q.show(doc,frame_limit=100,options=o,capture_path=str(args.capture) if args.capture else '');wall=(time.perf_counter()-started)*1000
    stats=report.pixel_stream;assert report.replay_complete and stats.state=='saved' and stats.current and not report.pixel_error,report.pixel_error
    assert report.pixel_result is None and report.pixel_historical_result is None and report.pixel_gpu_bytes==report.pixel_peak_gpu_bytes==0
    assert report.pixel_catalog_complete and report.pixel_catalog_inspected==args.objects+3 and report.pixel_catalog_max_step<=1024 and report.ui_overflow_items==0
    saved=q.PixelStreamFile(str(path));saved.verify();maximum=0
    for i in range(stats.total_blocks):
        block=saved.read_block(i);maximum=max(maximum,block.width*block.height*9)
        oracle=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,p,block.bounds))
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:np.testing.assert_array_equal(getattr(block,method)(),getattr(oracle,method)())
        del block,oracle
    assert saved.verified and stats.summary.valid_pixels==n*n and stats.summary.sha256==saved.declared_summary.sha256 and doc.revision==revision
    env=q.build_environment();out=dict(schema='qpx.gui-pixel-stream-benchmark/1',size=n,objects=args.objects,blocks=stats.total_blocks,file_bytes=stats.summary.bytes,content_sha256=stats.summary.sha256,
        estimated_producer_work_bytes=stats.estimated_work_bytes,max_block_array_bytes=maximum,budget_bytes=p.work_bytes,
        wall_ms=wall,worker_ms=stats.elapsed_ms,frames=report.frames,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,
        catalog_max_step=report.pixel_catalog_max_step,catalog_inspected=report.pixel_catalog_inspected,gpu_bytes=report.pixel_gpu_bytes,peak_gpu_bytes=report.pixel_peak_gpu_bytes,
        ui_overflow=report.ui_overflow_items,renderer=report.gl_renderer,native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='Single synthetic shared-workstation software-GL observation. Real F5 button, one native worker, offscreen cells, bounded1024-object catalog steps, no output preview and all saved arrays compared with independent windows. Source/model, file metadata and worker bookkeeping are outside producer allowance. Not dense visible annotation rendering, RSS, vendor WSI, hardware/frame-budget or biological acceptance. Output retained.')
    if args.capture:out['capture']=dict(file=args.capture.name,sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest())
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps(out,indent=2))


if __name__=='__main__':main()
