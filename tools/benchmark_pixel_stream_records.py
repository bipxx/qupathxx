"""Bounded descriptor persistence and a clipped real-window project catalog."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import time
import uuid
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


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--file',type=Path,required=True);ap.add_argument('--records',type=int,default=10000);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--project',type=Path);ap.add_argument('--gui',action='store_true');ap.add_argument('--capture',type=Path);args=ap.parse_args()
    if not 1<=args.records<=10000:ap.error('records must be1..10000')
    began=time.perf_counter();link=q.prepare_pixel_stream_link(str(args.file));prepare_ms=(time.perf_counter()-began)*1000
    if link.source_width>4096 or link.source_height>4096:ap.error('probe uses a bounded synthetic dense document; source must be at most4096-square')
    doc=q.Document(q.Image.from_numpy(np.full((link.source_height,link.source_width,3),80,np.uint8)));project=q.Project();image=project.add_document(doc)
    began=time.perf_counter()
    for i in range(args.records):doc.keep_pixel_stream(link,'Historical stream '+str(i))
    adopt_ms=(time.perf_counter()-began)*1000
    path=args.project or Path('build/pixel-stream-records')/(uuid.uuid4().hex+'.qpxproj');path.parent.mkdir(parents=True,exist_ok=True)
    began=time.perf_counter();project.save(str(path));save_ms=(time.perf_counter()-began)*1000
    with sqlite3.connect(path) as db:
        version=db.execute('PRAGMA user_version').fetchone()[0];count,total,maximum=db.execute('SELECT count(*),sum(length(payload)),max(length(payload)) FROM pixel_stream_records').fetchone()
    assert version==10 and count==args.records and total<args.records*4096 and maximum<4096
    began=time.perf_counter();loaded=q.Project.open(str(path));open_ms=(time.perf_counter()-began)*1000;restored=loaded.document(image)
    assert restored.pixel_stream_record_count==args.records and all(r.link.same_content(link) for r in restored.pixel_stream_records)
    record=restored.pixel_stream_records[0];assert record.link.path==str(args.file.resolve())
    gui=None
    if args.gui:
        o=q.GuiOptions();o.pixel_stream_browser_open=True;o.autosave_interval_seconds=0;o.settle_tiles=True;o.replay_timeout_seconds=45
        # Keep image rendering small while the stream may describe a larger source.
        view=q.Viewport();view.zoom=3;view.origin=q.Point(0,0);o.start_view=view
        ready=q.GuiInput();ready.frame=8;ready.kind=q.GuiInputKind.MOVE;ready.wait_for=q.GuiInputWait.PIXEL_STREAM_READ_READY
        o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN_RECORD),ready]
        if args.capture:args.capture.parent.mkdir(parents=True,exist_ok=True)
        revision=restored.revision
        report=q.show(loaded,frame_limit=100,capture_path=str(args.capture) if args.capture else '',options=o)
        assert report.replay_complete and report.pixel_stream_read.verified and report.pixel_stream_read.link.same_content(link) and not report.pixel_error
        assert report.pixel_stream_record_catalog_complete and report.pixel_stream_record_catalog_max_step<=1024 and report.ui_overflow_items==0
        assert report.pixel_stream_read_gpu_bytes<=1024*1024 and report.pixel_gpu_bytes==0 and restored.pixel_stream_record_count==args.records and restored.revision==revision
        gui=dict(document_unchanged=restored.revision==revision,project_dirty=loaded.dirty,frames=report.frames,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,catalog_max_step=report.pixel_stream_record_catalog_max_step,catalog_complete=report.pixel_stream_record_catalog_complete,
            gpu_bytes=report.pixel_stream_read_gpu_bytes,peak_gpu_bytes=report.pixel_stream_read_peak_gpu_bytes,ui_overflow=report.ui_overflow_items,renderer=report.gl_renderer)
        if args.capture:gui['capture']=dict(file=args.capture.name,sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest())
    env=q.build_environment();out=dict(schema='qpx.pixel-stream-record-benchmark/1',records=args.records,stream_bytes=link.summary.bytes,stream_blocks=link.summary.blocks,content_sha256=link.summary.sha256,
        descriptor_payload_bytes=link.payload_bytes,record_payload_bytes=record.payload_bytes,total_encoded_descriptor_bytes=total,max_encoded_descriptor_bytes=maximum,project_bytes=path.stat().st_size,project_schema=version,
        prepare_ms=prepare_ms,adopt_all_ms=adopt_ms,save_ms=save_ms,open_ms=open_ms,gui=gui,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='Synthetic descriptor/catalog/persistence observation. All links reference one fully verified file; project embeds only its synthetic source image and bounded descriptor rows, not external stream arrays or model. Relative-path relocation is checked by tests. Optional GUI uses clipped dropdown/catalog steps and one historical block preview. No RSS, many distinct files, network storage, vendor WSI, hardware/frame-budget or biological acceptance. Project and external output retained.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps(out,indent=2))


if __name__=='__main__':main()
