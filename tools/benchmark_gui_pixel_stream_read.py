"""Real saved-file browser, isolated label-color oracle and bounded GL payload."""
import argparse
import hashlib
import json
from pathlib import Path
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def click(frame,target):
    events=[]
    for i,kind in enumerate([q.GuiInputKind.LEFT_DOWN,q.GuiInputKind.LEFT_UP]):
        e=q.GuiInput();e.frame=frame+i;e.kind=kind;e.target=target
        if not i:e.wait_for=q.GuiInputWait.TARGET_READY
        events.append(e)
    return events


def ready(frame,wait):
    e=q.GuiInput();e.frame=frame;e.kind=q.GuiInputKind.MOVE;e.wait_for=wait;return e


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--file',type=Path,required=True);ap.add_argument('--index',type=int,default=0);ap.add_argument('--objects',type=int,default=10000);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--capture',type=Path,required=True);args=ap.parse_args()
    if not 0<=args.objects<=100000:ap.error('objects must be0..100000')
    reader=q.PixelStreamFile(str(args.file));reference=reader.read_block(args.index)
    doc=q.Document(q.Image.from_numpy(np.full((512,512,3),80,np.uint8)));doc.set_history_limits(0,0)
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(450,450,1,1)),kind=q.ObjectKind.CELL) for _ in range(args.objects)])
    o=q.GuiOptions();o.pixel_stream_browser_open=True;o.pixel_stream_read_path=str(args.file);o.pixel_stream_read_index=args.index
    o.autosave_interval_seconds=0;o.settle_tiles=True;o.replay_timeout_seconds=45
    view=q.Viewport();view.zoom=3;view.origin=q.Point(0,0);o.start_view=view
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(12,q.GuiInputTarget.PIXEL_STREAM_VERIFY),ready(16,q.GuiInputWait.PIXEL_STREAM_VERIFIED)]
    if reader.declared_summary.blocks>1:
        forward=args.index<reader.declared_summary.blocks-1
        o.inputs += [*click(20,q.GuiInputTarget.PIXEL_STREAM_NEXT if forward else q.GuiInputTarget.PIXEL_STREAM_PREVIOUS),ready(24,q.GuiInputWait.PIXEL_STREAM_READ_READY),
            *click(28,q.GuiInputTarget.PIXEL_STREAM_PREVIOUS if forward else q.GuiInputTarget.PIXEL_STREAM_NEXT),ready(32,q.GuiInputWait.PIXEL_STREAM_READ_READY)]
    revision=doc.revision;args.capture.parent.mkdir(parents=True,exist_ok=True)
    began=time.perf_counter();report=q.show(doc,frame_limit=100,capture_path=str(args.capture),options=o);wall=(time.perf_counter()-began)*1000;s=report.pixel_stream_read
    assert report.replay_complete and s.state=='ready' and s.verified and s.block_index==args.index and not report.pixel_error,report.pixel_error
    for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:np.testing.assert_array_equal(getattr(s.block,method)(),getattr(reference,method)())
    assert report.pixel_stream_read_preview_ready and report.pixel_stream_read_gpu_bytes<=s.block.width*s.block.height*4 and report.pixel_stream_read_peak_gpu_bytes<=1024*1024
    assert report.pixel_result is None and report.pixel_historical_result is None and report.pixel_model is None and report.pixel_gpu_bytes==0 and doc.revision==revision
    assert report.ui_overflow_items==0 and report.pixel_catalog_inspected==0
    magic,size,maximum,payload=args.capture.read_bytes().split(b'\n',3);assert magic==b'P6' and maximum==b'255';w,h=map(int,size.split());pixels=np.frombuffer(payload,np.uint8).reshape(h,w,3)
    bounds=report.pixel_stream_read_preview_bounds;scale=report.framebuffer_scale;labels=s.block.labels_numpy();sampled=0
    colors=[(76,120,168),(245,133,24),(228,87,86),(114,183,178),(84,162,75),(238,202,59),(178,121,162),(255,157,166)]
    step=max(1,int(min(bounds.width,bounds.height)*scale/64))
    for y in range(int(np.ceil(bounds.y*scale))+2,int((bounds.y+bounds.height)*scale)-2,step):
        for x in range(int(np.ceil(bounds.x*scale))+2,int((bounds.x+bounds.width)*scale)-2,step):
            row=min(s.block.height-1,int(((y+.5)/scale-bounds.y)/bounds.height*s.block.height));col=min(s.block.width-1,int(((x+.5)/scale-bounds.x)/bounds.width*s.block.width))
            label=int(labels[row,col]);expected=colors[(label-1)%8] if label else (30,32,36);np.testing.assert_array_equal(pixels[y,x],expected);sampled+=1
    assert sampled>0
    env=q.build_environment();out=dict(schema='qpx.gui-pixel-stream-read-benchmark/1',file=args.file.name,blocks=s.declared_summary.blocks,content_sha256=s.declared_summary.sha256,
        block_index=s.block_index,block_width=s.block.width,block_height=s.block.height,block_sha256=s.block.sha256,verified=s.verified,objects=args.objects,
        wall_ms=wall,frames=report.frames,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,
        gpu_bytes=report.pixel_stream_read_gpu_bytes,peak_gpu_bytes=report.pixel_stream_read_peak_gpu_bytes,main_mask_gpu_bytes=report.pixel_gpu_bytes,
        catalog_inspected=report.pixel_catalog_inspected,ui_overflow=report.ui_overflow_items,rendered_color_samples=sampled,renderer=report.gl_renderer,
        capture=dict(file=args.capture.name,sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest()),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='One synthetic shared-workstation software-GL observation with offscreen cells. Real open/navigation/verification controls, source-independent block preview, exact block-array reference and sampled rendered label colors. At most one512-square block preview/four256px uploads, no current-slide mask or classifier adoption. No RSS, dense visible drawing, vendor-slide throughput, hardware/frame-budget or biological acceptance.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps(out,indent=2))


if __name__=='__main__':main()
