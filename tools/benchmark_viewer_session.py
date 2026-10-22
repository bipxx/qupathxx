"""Live queue/GUI measurements on a native-owned 100000-object project."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import tempfile
import threading
import time

import qupathxx as q
from qupathxx import _native


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--objects',type=int,default=100000);ap.add_argument('--output',type=Path,default=Path('build/viewer-session-benchmark.json'));ap.add_argument('--capture',default='');args=ap.parse_args();assert 0<=args.objects<=100000
    with tempfile.TemporaryDirectory(prefix='qupathxx-live-bench-') as directory:
        root=Path(directory);doc=q.Document(q.Image(512,512,[32]*512*512*3));doc.set_history_limits(0,0)
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%512,(i//512)%512,.5,.5)),kind=q.ObjectKind.DETECTION) for i in range(args.objects)]) if args.objects else None
        obj=doc.add_rectangle(q.Rect(0,0,512,512));project=q.Project();project.add_document(doc);project.save(str(root/'source.qpxproj'))
        session=q.ViewerSession(str(root/'source.qpxproj'));samples=[];latencies=[];errors=[];result={}
        def done(ticket):
            e=session.wait(ticket,30);assert e.state=='succeeded',(e.action,e.state,e.error);return session.take(ticket)
        def producer():
            try:
                done(session.catalog())
                for _ in range(100):
                    start=time.perf_counter();ticket=session.inspect(1,obj);samples.append((time.perf_counter()-start)*1000);done(ticket);latencies.append((time.perf_counter()-start)*1000)
                e=done(session.execute(1,[q.ObjectEdit.create(q.Rect(10,10,32,32),'Python edit')]))
                e=done(session.analyze(1,obj,q.StainTransform([[1,0,0],[0,1,0]])));assert e.samples==512*512;result['analysis_event_ms']=e.elapsed_ms
                e=done(session.read_region(1,q.RegionRequest(0,0,0,512,512)));assert e.pixels.numpy().shape==(512,512,4)
                done(session.save(str(root/'saved.qpxproj')))
                result['stats']=session.stats
            except BaseException as error:errors.append(error)
            finally:
                if not args.capture:
                    try:session.close(True)
                    except RuntimeError:pass
        options=q.GuiOptions();options.settle_tiles=True;e=q.GuiInput();e.frame=4;e.kind=q.GuiInputKind.SELECT_ALL_KEY;e.wait_for=q.GuiInputWait.STARTUP_FINISHED;options.inputs=[e]
        worker=threading.Thread(target=producer,daemon=True);worker.start();report=q.show(session,frame_limit=600,capture_path=args.capture,options=options);worker.join(30)
        assert not worker.is_alive()
        if errors:raise errors[0]
        assert len(report.selected_ids)==args.objects+1 and session.stats.state=='closed'
        saved=q.Project.open(str(root/'saved.qpxproj'));assert len(saved.document(1).annotations)==args.objects+2 and len(saved.document(1).analysis_records)==1
        stats=result.pop('stats');result.update(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256,measurement_scope='finite_capture' if args.capture else 'mixed_close_after_jobs',frames=report.frames,objects=args.objects+2,selected=len(report.selected_ids),visible=report.visible_objects,queries=100,submit_p95_ms=sorted(samples)[94],round_trip_p95_ms=sorted(latencies)[94],max_dispatch_ms=stats.max_command_ms,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,startup_frames=report.startup_frames,startup_ms=report.startup_ms,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print(json.dumps(result,indent=2))


if __name__=='__main__':main()
