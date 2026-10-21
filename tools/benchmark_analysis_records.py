"""Stress archive indexing, durable snapshots and replay without startup hashes."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def event(frame, kind):
    value = q.GuiInput()
    value.frame, value.kind = frame, kind
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--records", type=int, default=1000)
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/analysis-records.json"))
    args = parser.parse_args()
    if not 1 <= args.records <= 100000 or not 0 <= args.objects <= 100000:
        parser.error("require records 1..100000 and objects 0..100000")
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    image = project.add_document(doc)
    if args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % 1000, i//1000, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(100, 100, 128, 128))
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[.65, .70, .29], [.07, .99, .11]]))
    value = q.analyze(request)
    start = time.perf_counter()
    for _ in range(args.records):
        record_id = doc.keep_analysis(request, value)
    keep_ms = (time.perf_counter()-start)*1000
    assert doc.analysis_record_count == args.records and doc.analysis_status(record_id) == "current"
    cap_rejected = False
    if args.records == 100000:
        before = doc.revision
        try:
            doc.keep_analysis(request, value)
        except (ValueError, RuntimeError, IndexError) as error:
            assert "archive exceeds" in str(error)
            cap_rejected = True
        assert cap_rejected and doc.revision == before and doc.analysis_record_count == args.records
    start = time.perf_counter()
    for _ in range(10000):
        assert doc.latest_analysis(obj).id == record_id
    lookup_ms = (time.perf_counter()-start)*1000
    work = Path("build/analysis-records")/project.identity
    work.mkdir(parents=True)
    path = work/"project.qpxproj"
    save = q.ProjectSaveJob()
    assert save.request(project, str(path))
    max_poll = 0
    while True:
        start = time.perf_counter()
        status = save.poll()
        max_poll = max(max_poll, (time.perf_counter()-start)*1000)
        if not save.busy:
            break
        time.sleep(.001)
    assert status.applied and not project.dirty, status.error
    start = time.perf_counter()
    loaded = q.Project.open(str(path))
    load_ms = (time.perf_counter()-start)*1000
    restored = loaded.document(image)
    assert restored.analysis_record_count == args.records
    assert restored.analysis_status(record_id) == "unverified" and not loaded.dirty
    replay = q.AnalysisRequest.replay(restored, record_id)
    recheck = q.analyze(replay)
    start = time.perf_counter()
    assert restored.verify_analysis(record_id, replay, recheck)
    verify_ms = (time.perf_counter()-start)*1000
    assert not loaded.dirty
    result = dict(platform=platform.platform(), processor=platform.processor(), objects=doc.object_count,
                  records=args.records, samples=value.samples, input_sha256=value.input_sha256,
                  append_ms=keep_ms, latest_10000_ms=lookup_ms, capture_ms=status.capture_ms,
                  worker_write_ms=status.write_ms, max_save_poll_ms=max_poll, load_ms=load_ms,
                  recheck_worker_ms=recheck.elapsed_ms, verify_ms=verify_ms, file_bytes=path.stat().st_size,
                  cap_rejected=cap_rejected)
    if args.gui:
        options = q.GuiOptions()
        options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.RECHECK_ANALYSIS_KEY)]
        view = q.Viewport()
        view.zoom, view.origin = 32, q.Point(-512*32, -32*32)
        options.start_view = view
        report = q.show(restored, frame_limit=180, options=options)
        assert report.saved_analysis_status == "current" and report.analysis_records == args.records
        assert report.analysis_samples == value.samples and len(report.selected_ids) == args.objects+1
        assert report.analysis_means == value.means and not loaded.dirty
        result.update(selected=len(report.selected_ids), visible=report.visible_objects, gpu_bytes=report.peak_gpu_bytes,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      gui_worker_ms=report.analysis_ms, gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
