"""Capture-time provenance cost and bounded shared-archive persistence."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import platform
import sqlite3
import tempfile
import time

import qupathxx as q
from qupathxx import _native


def timed(f):
    start=time.perf_counter();value=f();return value,(time.perf_counter()-start)*1000


def main():
    parser=argparse.ArgumentParser();parser.add_argument("--records",type=int,default=1000);parser.add_argument("--objects",type=int,default=0)
    parser.add_argument("--gui",action="store_true");parser.add_argument("--output",type=Path,default=Path("build/provenance.json"));args=parser.parse_args()
    if not 1<=args.records<=100000 or not 0<=args.objects<=100000:parser.error("records 1..100000, objects 0..100000")
    doc=q.Document(q.Image(512,512,[10,20,30]*512*512));doc.set_history_limits(0,0)
    if args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%512,(i//512)%512,.5,.5))) for i in range(args.objects)])
    obj=doc.add_rectangle(q.Rect(0,0,32,32));doc.set_calibration(.5,.25);transform=q.StainTransform([[1,0,0],[0,1,0]])
    request,first_ms=timed(lambda:q.AnalysisRequest(doc,obj,transform))
    captures=[timed(lambda:q.AnalysisRequest(doc,obj,transform))[1] for _ in range(100)]
    result=q.analyze(request);ids,keep_ms=timed(lambda:[doc.keep_analysis(request,result) for _ in range(args.records)])
    expected=q.provenance_json(result.provenance);doc.set_calibration(2,3)
    assert result.provenance.effective_pixel_width_um==.5 and json.loads(expected)["source"]["effective_pixel_height_um"]==.25
    project=q.Project();image=project.add_document(doc)
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                source_tree_sha256=result.provenance.environment.source_tree_sha256,records=args.records,objects=doc.object_count,
                first_request_ms=first_ms,request_p95_ms=sorted(captures)[94],request_max_ms=max(captures),provenance_payload_bytes=result.provenance.payload_bytes,
                json_bytes=len(expected.encode()),keep_ms=keep_ms,input_sha256=result.input_sha256)
    with tempfile.TemporaryDirectory(prefix="qpx-provenance-") as directory:
        path=Path(directory)/"captured.qpxproj";job=q.ProjectSaveJob();assert job.request(project,str(path));saved=job.wait();assert saved.applied,saved.error
        loaded,load_ms=timed(lambda:q.Project.open(str(path)));restored=loaded.document(image)
        assert restored.analysis_record_count==args.records and q.provenance_json(restored.analysis_record(ids[0]).result.provenance)==expected
        assert q.provenance_json(restored.analysis_record(ids[-1]).result.provenance)==expected and restored.pixel_width_um==2
        with closing(sqlite3.connect(path)) as db:
            count,encoded=db.execute("SELECT count(*),sum(length(payload)) FROM analysis_provenance").fetchone()
        assert count==args.records
        output.update(save_capture_ms=saved.capture_ms,save_write_ms=saved.write_ms,snapshot_bytes=saved.snapshot_bytes,
                      load_ms=load_ms,file_bytes=path.stat().st_size,encoded_provenance_bytes=encoded)
    if args.gui:
        e=q.GuiInput();e.frame,e.kind=4,q.GuiInputKind.SELECT_ALL_KEY;options=q.GuiOptions();options.inputs=[e]
        report=q.show(doc,frame_limit=180,options=options);assert len(report.selected_ids)==args.objects+1 and report.analysis_records==args.records and report.saved_analysis_status=="current"
        output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                      max_cpu_frame_ms=report.max_cpu_frame_ms,overlay_cache_bytes=report.overlay_cache_bytes,
                      tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8");print(json.dumps(output,indent=2))


if __name__=="__main__":main()
