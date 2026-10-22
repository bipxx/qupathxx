"""Native batch capture/poll/checkpoint/restart on a synthetic object catalog."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import tempfile
import time

import qupathxx as q
from qupathxx import _native


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--images',type=int,default=16);ap.add_argument('--objects',type=int,default=100000);ap.add_argument('--gui',action='store_true');ap.add_argument('--output',type=Path,default=Path('build/project-batch-benchmark.json'));args=ap.parse_args()
    assert 1<=args.images<=100 and 0<=args.objects<=100000
    project=q.Project();first=None
    for n in range(args.images):
        doc=q.Document(q.Image(512,512,[32+n]*512*512*3));doc.set_history_limits(0,0)
        if n==0:
            first=doc
            doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%512,(i//512)%512,.5,.5)),kind=q.ObjectKind.DETECTION) for i in range(args.objects)])
        doc.add_rectangle(q.Rect(0,0,512,512));project.add_document(doc,f'image {n}')
    t=q.StainTransform([[1,0,0],[0,1,0]]);o=q.ProjectBatchOptions();o.kind=q.ProjectBatchKind.TISSUE
    captures=[]
    for _ in range(20):
        start=time.perf_counter();request=q.ProjectBatchRequest(project,t,options=o);captures.append((time.perf_counter()-start)*1000)
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256,images=args.images,background_detections=args.objects,source_rois=request.object_count,snapshot_bytes=request.snapshot_bytes,capture_p95_ms=sorted(captures)[18])
    with tempfile.TemporaryDirectory(prefix='qupathxx-batch-bench-') as directory:
        job=q.ProjectBatchJob();start=time.perf_counter();assert job.request(request,directory);output['submit_ms']=(time.perf_counter()-start)*1000;max_poll=0
        while job.busy:
            start=time.perf_counter();s=job.poll();max_poll=max(max_poll,(time.perf_counter()-start)*1000);time.sleep(.001)
        assert s.state=='succeeded',s.error
        output.update(first_ms=job.result.elapsed_ms,max_poll_ms=max_poll,saved=job.result.saved,checkpoint_bytes=sum(Path(i.path).stat().st_size for i in job.result.images))
        assert job.request(request,directory);assert job.wait().state=='succeeded';assert job.result.reused==args.images;output['resume_ms']=job.result.elapsed_ms
        if args.gui:
            o.resume=False;request=q.ProjectBatchRequest(project,t,options=o);assert job.request(request,directory)
            options=q.GuiOptions();e=q.GuiInput();e.frame=4;e.kind=q.GuiInputKind.SELECT_ALL_KEY;options.inputs=[e]
            report=q.show(first,frame_limit=180,options=options);assert job.wait().state=='succeeded'
            assert len(report.selected_ids)==args.objects+1
            output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer,concurrent_batch_ms=job.result.elapsed_ms)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf8');print(json.dumps(output,indent=2))


if __name__=='__main__':main()
