"""Measure project loading alongside a live 100000-object desktop scene."""
import argparse
import json
from pathlib import Path
import platform

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('build/project-open-benchmark.json'))
    args = parser.parse_args()
    root = Path('build').resolve()
    paths = []
    projects = []
    for label in ('live', 'opened'):
        project = q.Project()
        doc = q.Document(q.Image.demo())
        doc.set_history_limits(0, 0)
        doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f'Object {i}') for i in range(100000)])
        image = project.add_document(doc, label)
        view = q.ProjectView()
        view.center, view.zoom = q.Point(512, 64), 64
        project.set_view(image, view)
        path = root / f'project-open-{project.identity}.qpxproj'
        project.save(str(path))
        paths.append(path)
        projects.append((project, doc))
    try:
        live, doc = projects[0]
        source_identity = projects[1][0].identity
        # Drop the source graph before measurement; its validated load runs only
        # on the native worker during show(). Keep the live graph selected.
        projects.pop()
        del project, doc
        doc = projects[0][1]
        doc.rename_annotation(1, 'Unsaved old workspace')
        before = [path.read_bytes() for path in paths]
        options = q.GuiOptions()
        options.project, options.project_open_path = live, str(paths[1])
        options.width, options.height, options.autosave_interval_seconds = 2560, 1440, 0
        events = []
        for frame, kind, target in [
            (3, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputTarget.CANVAS),
            (20, q.GuiInputKind.OPEN_PROJECT_KEY, q.GuiInputTarget.CANVAS),
            (70, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
            (71, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
            (90, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputTarget.CANVAS),
        ]:
            event = q.GuiInput()
            event.frame, event.kind, event.target = frame, kind, target
            events.append(event)
        options.inputs = events
        report = q.show(doc, frame_limit=180, options=options)
        assert report.project_replacements == 1 and not report.project_error
        assert live.identity == source_identity and not live.dirty
        assert len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
        assert [path.read_bytes() for path in paths] == before
        result = dict(platform=platform.platform(), processor=platform.processor(),
                      live_objects=100000, loaded_objects=100000,
                      worker_load_ms=report.project_load_ms, cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                      max_cpu_frame_ms=report.max_cpu_frame_ms, peak_gpu_payload_bytes=report.peak_gpu_bytes,
                      selected_objects=len(report.selected_ids), visible_objects=report.visible_objects,
                      gl_renderer=report.gl_renderer,
                      scope='Local warm SQLite, dense RGB, 100000 old/live and 100000 loaded objects, narrow view; input construction and file creation excluded. Frame work includes concurrent loading and frame-boundary replacement; excludes swap wait. No cold/network/vendor or prolonged memory-pressure claim.')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))
    finally:
        for path in paths:
            path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
