"""Measure explicit native saves alongside live model queries or desktop edits."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build/project-save-benchmark.json'))
    args = parser.parse_args()
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f'Object {i}') for i in range(100000)])
    image = project.add_document(doc)
    path = Path('build').resolve()/f'background-save-{project.identity}.qpxproj'
    project.save(str(path))
    doc.rename_annotation(ids[-1], 'Captured')
    result = dict(platform=platform.platform(), processor=platform.processor(), objects=100000,
                  snapshot_budget_bytes=256*1024*1024, embedded_rgb_bytes=1024*768*3)
    try:
        if args.gui:
            options = q.GuiOptions()
            options.width, options.height, options.autosave_interval_seconds = 2560, 1440, 0
            view = q.Viewport()
            view.zoom, view.origin = 64, q.Point(-32000, -3000)
            options.start_view = view
            inputs = []
            for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (20, q.GuiInputKind.SAVE_KEY), (21, q.GuiInputKind.WHEEL)]:
                event = q.GuiInput()
                event.frame, event.kind = frame, kind
                if kind == q.GuiInputKind.WHEEL:
                    event.wheel = .05
                inputs.append(event)
            options.inputs = inputs
            report = q.show(project, frame_limit=180, options=options)
            assert report.saves_completed == 1 and not report.project_error
            assert project.dirty and len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
            result.update(capture_ms=report.save_capture_ms, worker_write_and_release_ms=report.save_write_ms,
                          cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                          selected_objects=len(report.selected_ids), visible_objects=report.visible_objects,
                          peak_gpu_payload_bytes=report.peak_gpu_bytes, gl_renderer=report.gl_renderer)
        else:
            job = q.ProjectSaveJob()
            start = time.perf_counter()
            assert job.request(project, str(path))
            result['request_ms'] = (time.perf_counter()-start)*1000
            doc.rename_annotation(ids[-1], 'Later live edit')
            polls, queries = [], 0
            while job.busy:
                doc.query_region(q.Rect(500, 50, 20, 20))
                queries += 1
                start = time.perf_counter()
                job.poll()
                polls.append((time.perf_counter()-start)*1000)
                time.sleep(.001)
            stats = job.stats
            assert stats.state == 'saved' and stats.applied and project.dirty
            result.update(capture_ms=stats.capture_ms, worker_write_and_release_ms=stats.write_ms,
                          estimated_snapshot_bytes=stats.snapshot_bytes, max_poll_ms=max(polls, default=0),
                          concurrent_region_queries=queries)
        saved = q.Project.open(str(path))
        assert saved.generation == project.generation == 2 and saved.document(image).annotation(ids[-1]).name == 'Captured'
        result['primary_file_bytes'] = path.stat().st_size
        result['scope'] = 'Warm local SQLite explicit save of 100000 dense-image rectangles; immutable native snapshot, no history or spatial-index clone. Live post-capture changes remain dirty. Worker timing includes native snapshot release; desktop CPU includes capture and concurrent write, excludes swap wait. Cold/network/vendor, power loss and prolonged memory pressure excluded.'
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))
    finally:
        path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
