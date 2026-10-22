"""Native project ROI/tissue snapshot, streaming export and selected-view timings."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import platform
import tempfile
import time

import qupathxx as q
from qupathxx import _native


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--runs',type=int,default=1000);parser.add_argument('--images',type=int,default=1000)
    parser.add_argument('--objects',type=int,default=0);parser.add_argument('--gui',action='store_true');parser.add_argument('--output',type=Path,default=Path('build/measurement-tables.json'));args=parser.parse_args()
    if not 1<=args.runs<=5000 or not 8<=args.images<=1000 or not 0<=args.objects<=100000:parser.error('runs 1..5000, images 8..1000, objects 0..100000')
    size=256;rgb=[]
    for y in range(size):
        for x in range(size):rgb.extend([0 if x%4==2 and y%4==2 else 255]*3)
    project=q.Project();transform=q.StainTransform([[1,0,0],[0,1,0]])
    for i in range(8):
        doc=q.Document(q.Image(size,size,rgb));doc.set_history_limits(0,0);doc.set_calibration(.5,.25)
        if i==0 and args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(j%size,(j//size)%size,.5,.5))) for j in range(args.objects)])
        obj=doc.add_rectangle(q.Rect(0,0,size,size));source=q.AnalysisRequest(doc,obj,transform);result=q.analyze(source)
        for _ in range(args.runs):doc.keep_analysis(source,result)
        tissue=q.TissueRequest(doc,obj,transform);mask=q.detect_tissue(tissue);assert len(mask.components)==4096;doc.keep_tissue(tissue,mask)
        project.add_document(doc,f'Image {i+1}')
    for i in range(8,args.images):project.add_document(q.Document(q.Image(1,1,[255]*3)),f'Empty {i+1}')
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256,images=args.images,active_objects=args.objects+1)
    with tempfile.TemporaryDirectory(prefix='qpx-measurement-tables-') as folder:
        for kind,name,count in [(q.MeasurementTableKind.ROI,'roi',args.runs*8),(q.MeasurementTableKind.TISSUE,'tissue',32768)]:
            options=q.MeasurementTableOptions();options.require_current=True
            request=q.MeasurementTableRequest(project,kind,options=options);assert request.row_count==count
            captures=[q.MeasurementTableRequest(project,kind,options=options).capture_ms for _ in range(20)]
            path=Path(folder)/(name+'.csv');job=q.MeasurementTableExportJob();assert job.request(request,str(path));max_poll=0
            while True:
                started=time.perf_counter();status=job.poll();max_poll=max(max_poll,(time.perf_counter()-started)*1000)
                if not job.busy:break
                time.sleep(.001)
            assert status.state=='succeeded',status.error
            data=path.read_bytes();rows=list(csv.DictReader(io.StringIO(data.decode(),newline='')));assert len(rows)==count
            assert all(row['status_at_capture']=='current' and float(row['roi_area_um2'])==8192 for row in rows)
            if name=='tissue':assert all(row['component_samples']=='1' and float(row['component_area_um2'])==.125 for row in rows)
            assert data==q.measurements_csv(request).encode()
            record=dict(rows=count,records=request.record_count,capture_p95_ms=sorted(captures)[18],snapshot_bytes=request.snapshot_bytes,bytes=status.bytes,write_ms=status.write_ms,max_poll_ms=max_poll,sha256=hashlib.sha256(data).hexdigest())
            if args.gui:
                def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
                    value=q.GuiInput();value.frame,value.kind,value.wait_for=frame,kind,wait;return value
                export=event(20,q.GuiInputKind.EXPORT_MEASUREMENT_TABLE_KEY);export.shift=True
                config=q.GuiOptions();config.measurement_table_path=str(path);config.measurement_table_kind=0 if name=='roi' else 1;config.measurement_table_require_current=True
                config.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),export,event(22,q.GuiInputKind.MOVE,q.GuiInputWait.MEASUREMENT_TABLE_SAVED)]
                report=q.show(project,frame_limit=180,options=config)
                assert report.measurement_table_state=='succeeded' and report.measurement_table_rows==count and len(report.selected_ids)==args.objects+1
                assert path.read_bytes()==q.measurements_csv(project,kind,options=options).encode()
                record.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,gui_capture_ms=report.measurement_table_capture_ms,gui_write_ms=report.measurement_table_write_ms,overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer)
            output[name]=record
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8');print(json.dumps(output,indent=2))

if __name__=='__main__':main()
