"""Publication, feature lookup and native snapshots for linked cell objects."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import tempfile
import time

import numpy as np
import qupathxx as q
from qupathxx import _native


def timed(action):
    start=time.perf_counter();value=action();return value,(time.perf_counter()-start)*1000


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--objects",type=int,default=0)
    parser.add_argument("--gui",action="store_true")
    parser.add_argument("--output",type=Path,default=Path("build/cell-links.json"))
    args=parser.parse_args()
    if not 0<=args.objects<=100000:parser.error("objects must be 0..100000")
    size=256;yy,xx=np.indices((size,size))
    mask=np.abs(xx%4-2)+np.abs(yy%4-2)<=1
    rgb=np.where(mask[...,None],0,255).repeat(3,axis=2).astype(np.uint8)
    doc=q.Document(q.Image(size,size,rgb.ravel().tolist()));doc.set_history_limits(0,0)
    if args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%size,(i//size)%size,.5,.5))) for i in range(args.objects)])
    obj=doc.add_rectangle(q.Rect(0,0,size,size))
    p=q.CellParameters();p.sigma,p.cell_expansion,p.min_nucleus_pixels=0,0,1
    request=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),p)
    result=q.detect_cells(request);assert result.cell_count==4096
    ids,publish_ms=timed(lambda:doc.publish_cells(request,result))
    assert len(ids)==4096 and doc.cell_record_count==1
    record=doc.cell_records[0]
    assert all((doc.annotation(i).cell_feature_link.record_id,doc.annotation(i).cell_feature_link.label)==(record.id,f.label) for i,f in zip(ids,result.features))
    statuses,lookup_ms=timed(lambda:[doc.cell_object_status(i) for i in ids]);assert set(statuses)=={"current"}
    features,feature_ms=timed(lambda:[doc.cell_object_feature(i) for i in ids])
    assert all(f.nucleus_pixels==f.cell_pixels==5 and f.cytoplasm_rgb is None for f in features)
    project=q.Project();image=project.add_document(doc)
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                objects=doc.object_count,linked_cells=len(ids),records=doc.cell_record_count,publish_ms=publish_ms,
                status_lookup_total_ms=lookup_ms,feature_lookup_total_ms=feature_ms,input_sha256=result.nuclei.input_sha256)
    with tempfile.TemporaryDirectory(prefix="qpx-cell-links-") as directory:
        path=Path(directory)/"links.qpxproj";job=q.ProjectSaveJob();assert job.request(project,str(path))
        status=job.wait();assert status.applied and not project.dirty,status.error
        loaded,load_ms=timed(lambda:q.Project.open(str(path)));restored=loaded.document(image)
        assert restored.object_count==doc.object_count and restored.cell_record_count==1
        assert all(restored.cell_object_status(i)=="unverified" and restored.annotation(i).cell_feature_link.label==f.label and
                   restored.cell_object_feature(i).nucleus_rgb==f.nucleus_rgb for i,f in zip(ids,features))
        output.update(save_capture_ms=status.capture_ms,save_write_ms=status.write_ms,snapshot_bytes=status.snapshot_bytes,
                      file_bytes=path.stat().st_size,load_ms=load_ms)
    if args.gui:
        e=q.GuiInput();e.frame,e.kind=4,q.GuiInputKind.SELECT_ALL_KEY
        options=q.GuiOptions();options.inputs=[e]
        report=q.show(doc,frame_limit=180,options=options)
        assert len(report.selected_ids)==doc.object_count and report.visible_objects==doc.object_count
        assert report.primary_selection==ids[-1]
        output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                      max_cpu_frame_ms=report.max_cpu_frame_ms,canvas_gpu_bytes=report.overlay_cache_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(output,indent=2))


if __name__=="__main__":main()
