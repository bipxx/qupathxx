"""Native CSV throughput and owner capture latency for archived cell features."""
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


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--size",type=int,default=256)
    parser.add_argument("--records",type=int,default=8)
    parser.add_argument("--objects",type=int,default=0)
    parser.add_argument("--gui",action="store_true")
    parser.add_argument("--output",type=Path,default=Path("build/cell-tables.json"))
    args=parser.parse_args()
    if args.size not in [128,256,512] or not 1<=args.records<=8 or not 0<=args.objects<=100000:
        parser.error("size 128/256/512, records 1..8, objects 0..100000")
    yy,xx=np.indices((args.size,args.size))
    mask=np.abs(xx % 4-2)+np.abs(yy % 4-2)<=1
    rgb=np.where(mask[...,None],0,255).repeat(3,axis=2).astype(np.uint8)
    doc=q.Document(q.Image(args.size,args.size,rgb.ravel().tolist()))
    doc.set_history_limits(0,0)
    if args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % args.size,(i//args.size) % args.size,.5,.5))) for i in range(args.objects)])
    obj=doc.add_rectangle(q.Rect(0,0,args.size,args.size))
    p=q.CellParameters()
    p.sigma,p.cell_expansion,p.min_nucleus_pixels=0,0,1
    p.max_cells=max(10000,(args.size//4)**2)
    contours=p.contours;contours.max_components=p.max_cells;p.contours=contours
    source=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),p)
    result=q.detect_cells(source)
    assert result.cell_count==(args.size//4)**2 and all(f.nucleus_pixels==5 and f.cell_pixels==5 for f in result.features)
    ids=[doc.keep_cells(source,result) for _ in range(args.records)]
    captured=q.CellTableRequest(doc)
    native=next(Path(q.__file__).parent.glob("_native*.pyd"),None)
    output=dict(native_sha256=hashlib.sha256(native.read_bytes()).hexdigest() if native else None,platform=platform.platform(),pixels=mask.size,cells_per_run=result.cell_count,records=args.records,
                rows=captured.row_count,snapshot_bytes=captured.snapshot_bytes,capture_ms=captured.capture_ms,
                detector_ms=result.elapsed_ms,input_sha256=result.nuclei.input_sha256,objects=doc.object_count)
    with tempfile.TemporaryDirectory(prefix="qpx-cell-tables-") as directory:
        path=Path(directory)/"features.csv"
        job=q.CellTableExportJob()
        start=time.perf_counter();assert job.request(captured,str(path));submit_ms=(time.perf_counter()-start)*1000
        max_poll=0
        while True:
            start=time.perf_counter();status=job.poll();max_poll=max(max_poll,(time.perf_counter()-start)*1000)
            if not job.busy:break
            time.sleep(.001)
        assert status.state=="succeeded",status.error
        data=path.read_bytes()
        parsed=list(csv.DictReader(io.StringIO(data.decode(),newline="")))
        assert len(parsed)==captured.row_count and status.bytes==len(data)
        assert {int(r["record_id"]) for r in parsed}==set(ids)
        assert all(r["nucleus_samples"]==r["cell_samples"]=="5" and r["cytoplasm_samples"]=="0" and r["cytoplasm_mean_r"]=="" for r in parsed)
        assert data==q.cell_measurements_csv(captured).encode()
        output.update(bytes=status.bytes,submit_ms=submit_ms,write_ms=status.write_ms,max_poll_ms=max_poll,
                      rows_per_second=captured.row_count/(status.write_ms/1000),sha256=hashlib.sha256(data).hexdigest())
        if args.gui:
            options=q.GuiOptions();options.cell_table_path=str(path)
            def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
                e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
            options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(20,q.GuiInputKind.EXPORT_CELL_TABLE_KEY),
                            event(22,q.GuiInputKind.MOVE,q.GuiInputWait.CELL_TABLE_SAVED)]
            report=q.show(doc,frame_limit=180,options=options)
            assert report.cell_table_state=="succeeded" and report.cell_table_rows==result.cell_count
            assert len(report.selected_ids)==args.objects+1
            assert path.read_bytes()==q.cell_measurements_csv(doc,[ids[-1]]).encode()
            output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                          max_cpu_frame_ms=report.max_cpu_frame_ms,gui_capture_ms=report.cell_table_capture_ms,
                          gui_write_ms=report.cell_table_write_ms,canvas_gpu_bytes=report.overlay_cache_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(output,indent=2))


if __name__=="__main__":main()
