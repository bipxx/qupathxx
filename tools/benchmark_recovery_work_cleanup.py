"""Measure marked work cleanup beside native autosave and a large selection."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directories', type=int, default=1000)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build/recovery-work-benchmark.json'))
    args = parser.parse_args()
    if not 1 <= args.directories <= 10000:
        parser.error('directories must be between 1 and 10000')
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), str(i)) for i in range(100000)])
    image = project.add_document(doc)
    root = Path('build').resolve()/('recovery-work-'+project.identity)
    root.mkdir()
    primary, target = root/'primary.qpxproj', root/'snapshot.qpxrecover'
    project.save(str(primary))
    before = primary.read_bytes()
    doc.rename_annotation(1, 'Captured')
    work_paths = []

    def populate():
        work_paths.clear()
        for i in range(args.directories):
            token = f'{i:032x}'
            work = Path(str(target)+'.work-'+token)
            work.mkdir()
            work_paths.append(work)
            (work/'owner').write_bytes(b'qupathxx recovery work v1\n'+token.encode()+b'\n')
            for suffix in ['', '-journal', '-wal', '-shm']:
                (work/('snapshot.qpxproj'+suffix)).write_bytes(b'interrupted write')

    result = dict(platform=platform.platform(), objects=100000, work_directories=args.directories,
                  known_files_per_directory=5)
    try:
        populate()
        start = time.perf_counter()
        assert q.Project.cleanup_recovery_work(str(target)) == args.directories
        result['standalone_cleanup_ms'] = (time.perf_counter()-start)*1000
        populate()
        if args.gui:
            options = q.GuiOptions()
            options.autosave_path, options.autosave_interval_seconds = str(target), 0
            options.width, options.height = 2560, 1440
            view = q.ProjectView()
            view.center, view.zoom = q.Point(512, 64), 64
            project.set_view(image, view)
            events = []
            for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (20, q.GuiInputKind.AUTOSAVE_KEY)]:
                event = q.GuiInput()
                event.frame, event.kind = frame, kind
                events.append(event)
            options.inputs = events
            report = q.show(project, frame_limit=360, options=options)
            assert report.autosaves_completed == 1 and not report.autosave_error
            assert len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
            result.update(capture_ms=report.autosave_capture_ms, cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                          max_cpu_frame_ms=report.max_cpu_frame_ms, selected_objects=len(report.selected_ids),
                          visible_objects=report.visible_objects, peak_gpu_payload_bytes=report.peak_gpu_bytes,
                          gl_renderer=report.gl_renderer)
        else:
            writer = q.ProjectAutosave()
            request_start = time.perf_counter()
            assert writer.request(project, str(target))
            polls, publication_ms = [], None
            while writer.busy:
                if publication_ms is None and target.exists():
                    publication_ms = (time.perf_counter()-request_start)*1000
                start = time.perf_counter()
                stats = writer.poll()
                polls.append((time.perf_counter()-start)*1000)
                time.sleep(.001)
            assert stats.state == 'ready' and not stats.work_cleanup_error
            assert stats.work_directories_cleaned == min(32, args.directories)
            result.update(capture_ms=stats.capture_ms, worker_write_cleanup_release_ms=stats.write_ms,
                          max_poll_ms=max(polls), checkpoint_visible_ms=publication_ms if publication_ms is not None
                          else (time.perf_counter()-request_start)*1000)
        remaining = sum(work.exists() for work in work_paths)
        assert remaining == max(0, args.directories-32)
        result['automatic_directories_cleaned'] = args.directories-remaining
        result['explicit_remainder_cleaned'] = q.Project.cleanup_recovery_work(str(target))
        assert result['explicit_remainder_cleaned'] == remaining
        assert all(not work.exists() for work in work_paths)
        assert primary.read_bytes() == before and project.dirty
        loaded = q.Project.recover(str(target)).document(image)
        assert loaded.object_count == 100000 and loaded.annotation(1).name == 'Captured'
        result['scope'] = ('Warm local dense RGB and SQLite with 100000 unit rectangles; marked interrupted '
                           'work directories each contain a marker and four known files. Creation excluded. '
                           'Standalone full cleanup and a second pass with bounded automatic cleanup after durable '
                           'publication are separate workloads. Explicit remainder cleanup is outside GUI timing. '
                           'GPU payload excludes CPU models; GUI frame work excludes swap wait. '
                           'Cold/network/vendor, unmarked legacy debris and cross-session discovery excluded.')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(result, indent=2))
    finally:
        # Only our exact files in this uniquely created root, no recursive deletion.
        for work in work_paths:
            assert work.parent == root
            for name in ['owner', 'snapshot.qpxproj', 'snapshot.qpxproj-journal', 'snapshot.qpxproj-wal', 'snapshot.qpxproj-shm']:
                (work/name).unlink(missing_ok=True)
            if work.exists():
                work.rmdir()
        for path in [primary, target, Path(str(target)+'.lock')]:
            path.unlink(missing_ok=True)
        root.rmdir()


if __name__ == '__main__':
    main()
