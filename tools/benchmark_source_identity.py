"""Cached reader provenance and an associated hash in a selected native viewer."""
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
    parser=argparse.ArgumentParser();parser.add_argument('--mib',type=int,default=128);parser.add_argument('--objects',type=int,default=100000);parser.add_argument('--gui',action='store_true');parser.add_argument('--output',type=Path,default=Path('build/source-identity.json'));args=parser.parse_args()
    if not 1<=args.mib<=1024 or not 0<=args.objects<=100000:parser.error('mib 1..1024, objects 0..100000')
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256)
    with tempfile.TemporaryDirectory(prefix='qpx-reader-source-') as folder:
        path=Path(folder)/'source.ppm';size=512;header=b'P6\n512 512\n255\n';raster=bytes([40,80,160])*size*size;expected=hashlib.sha256()
        # Current PPM reader ignores trailing bytes. Padding gives the hash a
        # large input without requiring a large dense raster in the viewer.
        with path.open('wb') as stream:
            for chunk in [header,raster]:stream.write(chunk);expected.update(chunk)
            remaining=args.mib*1024*1024-len(header)-len(raster);block=bytes(1024*1024)
            while remaining:
                chunk=block[:min(len(block),remaining)];stream.write(chunk);expected.update(chunk);remaining-=len(chunk)
        # Warm both endpoints before collecting like-temperature timings. The
        # project endpoint also includes reader/catalog setup, not only a guard.
        warm=q.Image.read_ppm(str(path));del warm
        warm=q.Project();warm.add_image(str(path),reader='ppm');del warm
        baseline=[];observed=[]
        for _ in range(20):
            start=time.perf_counter();image=q.Image.read_ppm(str(path));baseline.append((time.perf_counter()-start)*1000);del image
            start=time.perf_counter();project=q.Project();image=project.add_image(str(path),reader='ppm');observed.append((time.perf_counter()-start)*1000)
        doc=project.document(image);doc.set_history_limits(0,0)
        if args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%size,(i//size)%size,.5,.5))) for i in range(args.objects)])
        obj=doc.add_rectangle(q.Rect(0,0,size,size));source=doc.server.reader_source;assert source.scope=='single_file'
        transform=q.StainTransform([[1,0,0],[0,1,0]]);captures=[];snapshots=[]
        for _ in range(1000):
            start=time.perf_counter();snapshot=doc.server.reader_source;snapshots.append((time.perf_counter()-start)*1000)
            start=time.perf_counter();request=q.AnalysisRequest(doc,obj,transform);captures.append((time.perf_counter()-start)*1000)
            assert request.provenance.reader_source.generation==source.generation
        job=q.FileFingerprintJob();destination=Path(folder)/'association.json';start=time.perf_counter();assert job.request_source(source,str(destination));request_ms=(time.perf_counter()-start)*1000;max_poll=0
        while True:
            start=time.perf_counter();status=job.poll();max_poll=max(max_poll,(time.perf_counter()-start)*1000)
            if not job.busy:break
            time.sleep(.001)
        assert status.state=='succeeded' and job.association.status=='matching_metadata' and job.result.sha256==expected.hexdigest()
        assert destination.read_bytes()==q.source_file_association_json(job.association).encode()
        output.update(bytes=job.result.bytes,sha256=job.result.sha256,scope=source.scope,baseline_ppm_open_p95_ms=sorted(baseline)[18],observed_project_open_p95_ms=sorted(observed)[18],snapshot_p95_ms=sorted(snapshots)[949],analysis_capture_p95_ms=sorted(captures)[949],source_payload_bytes=source.payload_bytes,hash_request_ms=request_ms,max_poll_ms=max_poll,hash_worker_ms=status.elapsed_ms,json_bytes=destination.stat().st_size)
        if args.gui:
            def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
                e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
            options=q.GuiOptions();options.file_fingerprint_path=str(path);options.file_fingerprint_output=str(destination);options.file_fingerprint_associate_source=True
            options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(20,q.GuiInputKind.FINGERPRINT_FILE_KEY),event(22,q.GuiInputKind.MOVE,q.GuiInputWait.FILE_FINGERPRINT_READY)]
            report=q.show(project,frame_limit=180,options=options)
            assert report.file_fingerprint_state=='succeeded' and report.file_fingerprint_association.status=='matching_metadata'
            assert report.file_fingerprint_association.source.generation==source.generation and report.file_fingerprint_result.sha256==expected.hexdigest()
            assert len(report.selected_ids)==args.objects+1 and destination.read_bytes()==q.source_file_association_json(report.file_fingerprint_association).encode()
            output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,gui_hash_ms=report.file_fingerprint_ms,overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8');print(json.dumps(output,indent=2))

if __name__=='__main__':main()
