"""Bounded native file hashing, worker polling and optional dense GUI timing."""
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
    parser=argparse.ArgumentParser();parser.add_argument("--mib",type=int,default=256);parser.add_argument("--objects",type=int,default=0)
    parser.add_argument("--gui",action="store_true");parser.add_argument("--output",type=Path,default=Path("build/file-fingerprint.json"));args=parser.parse_args()
    if not 16<=args.mib<=4096 or not 0<=args.objects<=100000:parser.error("mib 16..4096, objects 0..100000")
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256,
                file_bytes=args.mib*1024*1024,buffer_bytes=1024*1024)
    with tempfile.TemporaryDirectory(prefix="qpx-file-fingerprint-") as directory:
        folder=Path(directory);source=folder/"pixels.bin";destination=folder/"observed.json";block=bytes(range(256))*4096;expected=hashlib.sha256()
        with source.open("wb") as stream:
            for _ in range(args.mib):stream.write(block);expected.update(block)
        digest=expected.hexdigest();first=q.fingerprint_file(str(source));assert first.sha256==digest and first.bytes==output["file_bytes"]
        job=q.FileFingerprintJob();start=time.perf_counter();assert job.request(str(source),str(destination));submit_ms=(time.perf_counter()-start)*1000;max_poll=0;progress=[]
        while True:
            start=time.perf_counter();status=job.poll();max_poll=max(max_poll,(time.perf_counter()-start)*1000);progress.append(status.bytes)
            assert status.bytes<=status.total_bytes
            if not job.busy:break
            time.sleep(.001)
        assert status.state=="succeeded",status.error
        result=job.result;assert result.sha256==digest and result.identity==first.identity and progress==sorted(progress)
        assert destination.read_bytes()==q.file_fingerprint_json(result).encode()
        output.update(sha256=digest,identity_scheme=result.identity_scheme,first_ms=first.elapsed_ms,submit_ms=submit_ms,max_poll_ms=max_poll,
                      hash_ms=result.elapsed_ms,worker_ms=status.elapsed_ms,mib_per_second=args.mib/(result.elapsed_ms/1000),json_bytes=destination.stat().st_size)
        if args.gui:
            doc=q.Document(q.Image(512,512,[255]*512*512*3));doc.set_history_limits(0,0)
            if args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%512,(i//512)%512,.5,.5))) for i in range(args.objects)])
            def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
                e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
            config=q.GuiOptions();config.file_fingerprint_path=str(source);config.file_fingerprint_output=str(destination);config.replay_timeout_seconds=60
            config.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(20,q.GuiInputKind.FINGERPRINT_FILE_KEY),event(22,q.GuiInputKind.MOVE,q.GuiInputWait.FILE_FINGERPRINT_READY)]
            report=q.show(doc,frame_limit=180,options=config)
            assert report.file_fingerprint_state=="succeeded" and report.file_fingerprint_result.sha256==digest and len(report.selected_ids)==args.objects
            assert json.loads(destination.read_text(encoding="utf-8"))["sha256"]==digest
            output.update(objects=doc.object_count,selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                          max_cpu_frame_ms=report.max_cpu_frame_ms,gui_worker_ms=report.file_fingerprint_ms,overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8");print(json.dumps(output,indent=2))


if __name__=="__main__":main()
