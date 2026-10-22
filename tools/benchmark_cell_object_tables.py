"""Joined cell table capture, native worker throughput and live GUI latency."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import platform
import tempfile
import time

import numpy as np
import qupathxx as q
from qupathxx import _native


def main():
    parser=argparse.ArgumentParser();parser.add_argument("--images",type=int,default=8);parser.add_argument("--copies",type=int,default=1)
    parser.add_argument("--objects",type=int,default=0);parser.add_argument("--gui",action="store_true")
    parser.add_argument("--output",type=Path,default=Path("build/cell-object-tables.json"));args=parser.parse_args()
    if not 8<=args.images<=1000 or not 1<=args.copies<=4 or not 0<=args.objects<=100000:parser.error("images 8..1000, copies 1..4, objects 0..100000")
    size=256;y,x=np.indices((size,size));mask=np.abs(x%4-2)+np.abs(y%4-2)<=1
    rgb=np.where(mask[...,None],0,255).repeat(3,axis=2).astype(np.uint8);project=q.Project();image_ids=[]
    p=q.CellParameters();p.sigma,p.cell_expansion,p.min_nucleus_pixels=0,0,1
    for i in range(8):
        doc=q.Document(q.Image(size,size,rgb.ravel().tolist()));doc.set_history_limits(0,0);doc.set_calibration(.5,.25)
        if i==0 and args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(j%size,(j//size)%size,.5,.5))) for j in range(args.objects)])
        source=doc.add_rectangle(q.Rect(0,0,size,size));request=q.CellRequest(doc,source,q.StainTransform([[1,0,0],[0,1,0]]),p)
        result=q.detect_cells(request);assert result.cell_count==4096;record=doc.keep_cells(request,result)
        for _ in range(args.copies):doc.publish_cell_record(record,class_name="Detected")
        image_ids.append(project.add_document(doc,f"Joined image {i+1}"))
    for i in range(8,args.images):project.add_document(q.Document(q.Image(1,1,[255]*3)),f"Empty {i+1}")
    options=q.CellTableOptions();options.published_objects=True;options.require_current=True
    request=q.CellTableRequest(project,options=options);count=32768*args.copies
    assert request.image_count==args.images and request.record_count==8 and request.row_count==count
    captures=[q.CellTableRequest(project,options=options).capture_ms for _ in range(20)]
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                source_tree_sha256=q.build_environment().source_tree_sha256,images=args.images,records=8,rows=count,copies=args.copies,
                active_objects=args.objects+1+4096*args.copies,capture_ms=request.capture_ms,capture_p95_ms=sorted(captures)[18],snapshot_bytes=request.snapshot_bytes)
    with tempfile.TemporaryDirectory(prefix="qpx-joined-tables-") as directory:
        path=Path(directory)/"joined.csv";job=q.CellTableExportJob();assert job.request(request,str(path));max_poll=0
        while True:
            start=time.perf_counter();status=job.poll();max_poll=max(max_poll,(time.perf_counter()-start)*1000)
            if not job.busy:break
            time.sleep(.001)
        assert status.state=="succeeded",status.error
        data=path.read_bytes();rows=list(csv.DictReader(io.StringIO(data.decode(),newline="")))
        assert len(rows)==count and {int(row["image_id"]) for row in rows}==set(image_ids)
        assert all(row["object_status_at_capture"]=="current" and row["object_class"]=="Detected" and row["nucleus_samples"]==row["cell_samples"]=="5" and float(row["object_area_um2"])==.625 for row in rows)
        assert len({(r["image_id"],r["object_id"]) for r in rows})==count
        assert data==q.cell_measurements_csv(request).encode()
        output.update(bytes=status.bytes,write_ms=status.write_ms,max_poll_ms=max_poll,rows_per_second=count/(status.write_ms/1000),sha256=hashlib.sha256(data).hexdigest())
        if args.gui:
            def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
                e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
            export=event(20,q.GuiInputKind.EXPORT_CELL_TABLE_KEY);export.shift=True
            config=q.GuiOptions();config.cell_table_path=str(path);config.cell_table_published_objects=True;config.cell_table_require_current=True
            config.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),export,event(22,q.GuiInputKind.MOVE,q.GuiInputWait.CELL_TABLE_SAVED)]
            report=q.show(project,frame_limit=180,options=config)
            assert report.cell_table_state=="succeeded" and report.cell_table_rows==count and len(report.selected_ids)==output["active_objects"]
            assert path.read_bytes()==q.cell_measurements_csv(project,options=options).encode()
            output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                          max_cpu_frame_ms=report.max_cpu_frame_ms,gui_capture_ms=report.cell_table_capture_ms,gui_write_ms=report.cell_table_write_ms,
                          overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8");print(json.dumps(output,indent=2))


if __name__=="__main__":main()
