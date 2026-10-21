"""Project capture and streamed cell CSV, with optional dense active viewer."""
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
    parser=argparse.ArgumentParser();parser.add_argument("--images",type=int,default=8)
    parser.add_argument("--objects",type=int,default=0);parser.add_argument("--gui",action="store_true")
    parser.add_argument("--output",type=Path,default=Path("build/project-cell-tables.json"));args=parser.parse_args()
    if not 8<=args.images<=1000 or not 0<=args.objects<=100000:parser.error("images 8..1000, objects 0..100000")
    size=256;y,x=np.indices((size,size));mask=np.abs(x%4-2)+np.abs(y%4-2)<=1
    rgb=np.where(mask[...,None],0,255).repeat(3,axis=2).astype(np.uint8);project=q.Project();run_images=[]
    p=q.CellParameters();p.sigma,p.cell_expansion,p.min_nucleus_pixels=0,0,1
    for i in range(8):
        doc=q.Document(q.Image(size,size,rgb.ravel().tolist()));doc.set_history_limits(0,0)
        if i==0 and args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(j%size,(j//size)%size,.5,.5))) for j in range(args.objects)])
        obj=doc.add_rectangle(q.Rect(0,0,size,size));source=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),p)
        result=q.detect_cells(source);assert result.cell_count==4096;doc.keep_cells(source,result)
        run_images.append(project.add_document(doc,name=f"Cells {i+1}"))
    for i in range(8,args.images):project.add_document(q.Document(q.Image(1,1,[255]*3)),name=f"Empty {i+1}")
    request=q.CellTableRequest(project);assert request.image_count==args.images and request.record_count==8 and request.row_count==32768
    captures=[q.CellTableRequest(project).capture_ms for _ in range(20)]
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                images=request.image_count,records=request.record_count,rows=request.row_count,objects=args.objects+1,
                capture_ms=request.capture_ms,capture_p95_ms=sorted(captures)[18],snapshot_bytes=request.snapshot_bytes)
    with tempfile.TemporaryDirectory(prefix="qpx-project-tables-") as directory:
        path=Path(directory)/"cells.csv";job=q.CellTableExportJob();start=time.perf_counter();assert job.request(request,str(path));submit_ms=(time.perf_counter()-start)*1000
        max_poll=0
        while True:
            start=time.perf_counter();status=job.poll();max_poll=max(max_poll,(time.perf_counter()-start)*1000)
            if not job.busy:break
            time.sleep(.001)
        assert status.state=="succeeded",status.error
        data=path.read_bytes();rows=list(csv.DictReader(io.StringIO(data.decode(),newline="")))
        assert len(rows)==32768 and {int(row["image_id"]) for row in rows}==set(run_images)
        assert all(row["project_identity"]==project.identity and row["nucleus_samples"]==row["cell_samples"]=="5" and row["cytoplasm_samples"]=="0" for row in rows)
        assert data==q.cell_measurements_csv(request).encode()
        output.update(bytes=status.bytes,submit_ms=submit_ms,write_ms=status.write_ms,max_poll_ms=max_poll,rows_per_second=32768/(status.write_ms/1000),sha256=hashlib.sha256(data).hexdigest())
        if args.gui:
            def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
                e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
            export=event(20,q.GuiInputKind.EXPORT_CELL_TABLE_KEY);export.shift=True
            options=q.GuiOptions();options.cell_table_path=str(path)
            options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),export,event(22,q.GuiInputKind.MOVE,q.GuiInputWait.CELL_TABLE_SAVED)]
            report=q.show(project,frame_limit=180,options=options)
            assert report.cell_table_state=="succeeded" and report.cell_table_rows==32768 and len(report.selected_ids)==args.objects+1
            assert path.read_bytes()==q.cell_measurements_csv(project).encode()
            output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                          max_cpu_frame_ms=report.max_cpu_frame_ms,gui_capture_ms=report.cell_table_capture_ms,gui_write_ms=report.cell_table_write_ms,
                          canvas_gpu_bytes=report.overlay_cache_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8");print(json.dumps(output,indent=2))


if __name__=="__main__":main()
