"""Measure bounded owner-thread capture and asynchronous native recovery IO."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/recovery-benchmark.json"))
    args = parser.parse_args()
    root = Path("build").resolve()
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f"Object {i}", class_name="Tumor") for i in range(100000)])
    image = project.add_document(doc)
    primary = root / f"recovery-benchmark-{project.identity}.qpxproj"
    path = root / f"recovery-benchmark-{project.identity}.qpxrecover"
    project.save(str(primary))
    doc.rename_annotation(ids[-1], "Captured")
    result = dict(platform=platform.platform(), processor=platform.processor(), objects=100000,
                  embedded_rgb_bytes=1024*768*3, snapshot_budget_bytes=256*1024*1024)
    if args.gui:
        options = q.GuiOptions()
        options.project, options.autosave_path, options.autosave_interval_seconds = project, str(path), 0
        options.width, options.height = 2560, 1440
        view = q.Viewport()
        view.zoom, view.origin = 64, q.Point(-32000, -3000)
        options.start_view = view
        events = []
        for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (20, q.GuiInputKind.AUTOSAVE_KEY)]:
            event = q.GuiInput()
            event.frame, event.kind = frame, kind
            events.append(event)
        options.inputs = events
        report = q.show(doc, frame_limit=180, options=options)
        assert report.autosaves_completed == 1 and not report.autosave_error
        assert len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
        result.update(capture_ms=report.autosave_capture_ms, cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                      max_cpu_frame_ms=report.max_cpu_frame_ms, peak_gpu_payload_bytes=report.peak_gpu_bytes,
                      selected_objects=len(report.selected_ids), visible_objects=report.visible_objects, gl_renderer=report.gl_renderer)
    else:
        autosave = q.ProjectAutosave()
        start = time.perf_counter()
        assert autosave.request(project, str(path))
        result["request_ms"] = (time.perf_counter()-start)*1000
        doc.rename_annotation(ids[-1], "Later live edit")
        # Exercise the independent live model while its snapshot is being written.
        queries, polls = 0, []
        while autosave.busy:
            start = time.perf_counter()
            stats = autosave.poll()
            polls.append((time.perf_counter()-start)*1000)
            assert doc.query_region(q.Rect(10, 10, 10, 10))
            queries += 1
            time.sleep(.001)
        assert stats.state == "ready", stats.error
        result.update(capture_ms=stats.capture_ms, worker_write_and_release_ms=stats.write_ms,
                      estimated_snapshot_bytes=stats.snapshot_bytes, max_poll_ms=max(polls), concurrent_queries=queries)
    restored = q.Project.recover(str(path))
    assert restored.document(image).object_count == 100000 and restored.document(image).annotation(ids[-1]).name == "Captured"
    assert restored.dirty and project.dirty and project.generation == 1
    assert q.Project.open(str(primary)).document(image).annotation(ids[-1]).name == "Object 99999"
    result["recovery_file_bytes"] = path.stat().st_size
    result["scope"] = ("100000 classified unit rectangles, one immutable dense source and local SQLite recovery file. Owner capture copies canonical maps/text and shares immutable geometry/pixels; budget estimates retained payload and no history/index is cloned. One background job includes durable SQLite commits, integrity check, atomic publication and snapshot release. Primary remains unchanged/dirty state retained. Cold/network storage, vendor readers, power-loss and long-running pressure are excluded. " +
                       ("GUI: 100000 selected, narrow view; frame metrics include capture and concurrent IO after ten warm-up frames, excluding swap wait." if args.gui else "Headless: concurrent live spatial queries; request includes capture/dispatch, poll timing includes Python result conversion."))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    q.Project.discard_recovery(str(path), project.identity)
    # Unique, explicitly constructed files in the verified build directory only.
    assert path.parent == primary.parent == root
    primary.unlink()
    Path(str(path)+".lock").unlink()


if __name__ == "__main__":
    main()
