"""Measure metadata-only bounded retention beside a 100000-object autosave."""
import argparse
import json
import os
from pathlib import Path
import platform
import sqlite3
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--checkpoints', type=int, default=1000)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--cancel-after', type=int, default=0, help='Stop headless scanning after this many observed headers')
    parser.add_argument('--output', type=Path, default=Path('build/retention-benchmark.json'))
    args = parser.parse_args()
    if not 32 <= args.checkpoints <= 10000:
        parser.error('checkpoints must be between 32 and 10000')
    if args.cancel_after < 0 or args.cancel_after >= args.checkpoints or (args.gui and args.cancel_after):
        parser.error('cancel-after must be a smaller nonnegative header count and requires headless mode')
    expected_removals = 0 if args.cancel_after else min(32, args.checkpoints+1-10)
    project = q.Project()
    source = q.Document(q.Image(4, 4, [20, 40, 80]*16))
    source.add_rectangle(q.Rect(0, 0, 1, 1), 'Original')
    old_image = project.add_document(source)
    root = Path('build').resolve()/('retention-'+project.identity)
    root.mkdir()
    primary, current = root/'primary.qpxproj', root/'current.qpxrecover'
    project.save(str(primary))
    before = primary.read_bytes()
    paths = []
    try:
        # Each historical checkpoint is published by a distinct native session;
        # only age metadata/mtime are adjusted to avoid waiting sixty days.
        for i in range(args.checkpoints):
            path = root/f'old-{i:05d}.qpxrecover'
            paths.append(path)
            source.rename_annotation(1, f'Historical session {i}')
            writer = q.ProjectAutosave()
            assert writer.request(project, str(path)) and writer.wait().state == 'ready'
            del writer
            stamp = time.time()-60*86400-i
            with sqlite3.connect(path) as db:
                db.execute('UPDATE recovery SET created_ms=?', (int(stamp*1000),))
            db.close()
            os.utime(path, (stamp, stamp))
        project.remove_image(old_image)
        doc = q.Document(q.Image.demo())
        doc.set_history_limits(0, 0)
        doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), str(i)) for i in range(100000)])
        image = project.add_document(doc)
        result = dict(platform=platform.platform(), historical_checkpoints=args.checkpoints, objects=100000,
                      keep_latest=10, max_age_days=30, max_deletions=32)
        if args.gui:
            options = q.GuiOptions()
            options.autosave_path, options.autosave_interval_seconds = str(current), 0
            options.recovery_retention_enabled = True
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
            report = q.show(project, frame_limit=900, options=options)
            assert report.autosaves_completed == 1 and not report.autosave_error
            assert report.checkpoints_pruned == expected_removals and not report.retention_error
            assert len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
            result.update(retention_ms=report.retention_ms, capture_ms=report.autosave_capture_ms,
                          marker_prefetch_peak=report.retention_prefetch_peak,
                          metadata_ms=report.retention_metadata_ms, ownership_ms=report.retention_ownership_ms,
                          removal_ms=report.retention_removal_ms, inspected=report.retention_inspected,
                          cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                          peak_gpu_payload_bytes=report.peak_gpu_bytes, selected_objects=len(report.selected_ids),
                          visible_objects=report.visible_objects, gl_renderer=report.gl_renderer)
        else:
            writer = q.ProjectAutosave()
            config = q.RecoveryRetention()
            config.enabled = True
            writer.retention = config
            start = time.perf_counter()
            assert writer.request(project, str(current))
            published, polls, cancelled_at = None, [], None
            phases, max_live_inspected = set(), 0
            while writer.busy:
                if published is None and current.exists():
                    published = (time.perf_counter()-start)*1000
                poll_start = time.perf_counter()
                stats = writer.poll()
                polls.append((time.perf_counter()-poll_start)*1000)
                phases.add(stats.maintenance_phase)
                max_live_inspected = max(max_live_inspected, stats.retention_inspected)
                if args.cancel_after and cancelled_at is None and stats.maintenance_phase == 'scanning' and stats.retention_inspected >= args.cancel_after:
                    writer.cancel()
                    cancelled_at = time.perf_counter()
                time.sleep(.001)
            assert stats.state == 'ready' and not stats.retention_error and stats.checkpoint_published
            assert stats.checkpoints_pruned == expected_removals
            assert stats.retention_cancelled == bool(args.cancel_after)
            if args.cancel_after:
                assert cancelled_at is not None
                result['cancel_to_completion_ms'] = (time.perf_counter()-cancelled_at)*1000
            result.update(retention_ms=stats.retention_ms, capture_ms=stats.capture_ms, worker_ms=stats.write_ms,
                          marker_prefetch_peak=stats.retention_prefetch_peak,
                          checkpoint_visible_ms=published, max_poll_ms=max(polls), cancelled=stats.retention_cancelled,
                          metadata_ms=stats.retention_metadata_ms, ownership_ms=stats.retention_ownership_ms,
                          removal_ms=stats.retention_removal_ms, inspected=stats.retention_inspected,
                          observed_phases=sorted(phases), max_live_inspected=max_live_inspected)
            del writer
        assert sum(not path.exists() for path in paths) == expected_removals and current.exists()
        assert 0 < result['marker_prefetch_peak'] <= 4
        assert primary.read_bytes() == before and project.dirty
        assert q.Project.recover(str(current)).document(image).object_count == 100000
        result['scope'] = ('Warm local Windows SQLite/dense RGB. Historical fixtures are separately published '
                           'small native checkpoints from distinct closed sessions; only creation timestamps '
                           'and mtimes are aged. Fixture creation excluded. '
                           'One native 100000-object checkpoint publishes before bounded metadata-only retention. '
                           'No historical graphs/pixels are decoded. GUI excludes swap wait. Cold/network/vendor '
                           'and prolonged pressure excluded; no general latency bound.')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(result, indent=2))
    finally:
        # Only exact owned files in the verified uniquely created directory.
        for path in paths+[current]:
            assert path.parent == root
            for suffix in ['', '.lock', '.session']:
                Path(str(path)+suffix).unlink(missing_ok=True)
        primary.unlink(missing_ok=True)
        root.rmdir()


if __name__ == '__main__':
    main()
