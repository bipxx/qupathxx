"""Measure recovery loading or reviewed deletion beside a 100000-object viewer."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--maintenance', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build/recovery-open-benchmark.json'))
    args = parser.parse_args()
    root = Path('build').resolve()
    paths, projects = [], []
    for label in ('live', 'source'):
        project = q.Project()
        doc = q.Document(q.Image.demo())
        doc.set_history_limits(0, 0)
        doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f'Object {i}') for i in range(100000)])
        image = project.add_document(doc, label)
        view = q.ProjectView()
        view.center, view.zoom = q.Point(512, 64), 64
        project.set_view(image, view)
        path = root/f'recovery-open-{project.identity}.qpxproj'
        project.save(str(path))
        paths.append(path)
        projects.append((project, doc))
    recovery = root/f'recovery-open-{project.identity}.qpxrecover'
    doc.rename_annotation(1, 'Captured recovery')
    writer = q.ProjectAutosave()
    assert writer.request(project, str(recovery)) and writer.wait().state == 'ready'
    checkpoint_bytes = recovery.stat().st_size
    source_identity = project.identity
    projects.pop()
    del project, doc
    live, doc = projects[0]
    live_identity = live.identity
    doc.rename_annotation(1, 'Unsaved live workspace')
    projects.clear()
    del doc  # Include retirement of the old graph when the project is replaced.
    before = [path.read_bytes() for path in paths]
    result = dict(platform=platform.platform(), processor=platform.processor(), live_objects=100000,
                  checkpoint_objects=100000, checkpoint_file_bytes=checkpoint_bytes)
    try:
        if args.maintenance:
            start = time.perf_counter()
            reviewed = q.Project.review_recovery(str(recovery))
            result['headless_review_ms'] = (time.perf_counter()-start)*1000
            assert reviewed.fingerprint and reviewed.objects == 100000
        options = q.GuiOptions()
        options.recovery_path, options.autosave_interval_seconds = str(recovery), 0
        options.width, options.height = 2560, 1440
        events = []
        for frame, kind, target in [
            (3, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputTarget.CANVAS),
            (20, q.GuiInputKind.REVIEW_RECOVERY_KEY if args.maintenance else q.GuiInputKind.RECOVER_KEY, q.GuiInputTarget.CANVAS),
            (70, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.CONFIRM_DISCARD_CHECKPOINT if args.maintenance else q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
            (71, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.CONFIRM_DISCARD_CHECKPOINT if args.maintenance else q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
            (90, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputTarget.CANVAS),
        ]:
            item = q.GuiInput()
            item.frame, item.kind, item.target = frame, kind, target
            events.append(item)
        options.inputs = events
        report = q.show(live, frame_limit=180, options=options)
        assert not report.project_error and not report.recovery_maintenance_error
        assert [path.read_bytes() for path in paths] == before
        assert live.dirty and len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
        if args.maintenance:
            assert report.checkpoints_deleted == 1 and not recovery.exists() and live.identity == live_identity
            assert live.document(live.active_image).annotation(1).name == 'Unsaved live workspace'
            result['checkpoints_deleted'] = report.checkpoints_deleted
        else:
            assert report.project_replacements == 1 and live.identity == source_identity and recovery.exists()
            assert live.document(live.active_image).annotation(1).name == 'Captured recovery'
            result['worker_recovery_load_ms'] = report.recovery_load_ms
        result.update(cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      peak_gpu_payload_bytes=report.peak_gpu_bytes, selected_objects=len(report.selected_ids),
                      visible_objects=report.visible_objects, gl_renderer=report.gl_renderer,
                      scope='Warm local SQLite, dense RGB, live 100000-object selection and separate 100000-object checkpoint. Source construction/publication excluded. No external document owner. Includes concurrent recovery loading/staged replacement and old-graph retirement or reviewed file deletion; frame work excludes buffer-swap wait. Cold/network/vendor and prolonged memory pressure excluded.')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))
    finally:
        for path in paths+[recovery, Path(str(recovery)+'.lock')]:
            path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
