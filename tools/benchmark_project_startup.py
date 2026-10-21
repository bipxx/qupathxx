"""Measure native initial project/recovery loading after the first painted frame."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--recovery', action='store_true')
    parser.add_argument('--objects', type=int, default=100000)
    parser.add_argument('--output', type=Path, default=Path('build/project-startup.json'))
    args = parser.parse_args()
    if not 1000 <= args.objects <= 1000000:
        parser.error('objects must be between 1000 and 1000000')
    seed = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), str(i)) for i in range(args.objects)])
    image = seed.add_document(doc)
    view = q.ProjectView()
    view.center, view.zoom = q.Point(512, 64), 64
    seed.set_view(image, view)
    root = Path('build').resolve()/('startup-'+seed.identity)
    root.mkdir()
    primary, recovery = root/'study.qpxproj', root/'checkpoint.qpxrecover'
    try:
        seed.save(str(primary))
        before = primary.read_bytes()
        if args.recovery:
            doc.rename_annotation(1, 'Recovered')
            writer = q.ProjectAutosave()
            assert writer.request(seed, str(recovery)) and writer.wait().state == 'ready'
            del writer
        del doc, seed
        target = q.Project()
        options = q.GuiOptions()
        options.width, options.height = 2560, 1440
        options.startup_kind = 'recovery' if args.recovery else 'project'
        options.startup_path = str(recovery if args.recovery else primary)
        options.autosave_interval_seconds = 0
        event = q.GuiInput()
        event.frame, event.kind = 120, q.GuiInputKind.SELECT_ALL_KEY
        options.inputs = [event]
        start = time.perf_counter()
        report = q.show(target, frame_limit=180, options=options)
        wall_ms = (time.perf_counter()-start)*1000
        assert report.startup_succeeded and not report.project_error and report.startup_frames >= 1
        assert report.startup_objects == args.objects and report.startup_images == 1
        assert target.document(image).object_count == args.objects and report.active_image == image
        assert len(report.selected_ids) == args.objects and report.uploaded_tiles > 0
        assert 0 < report.visible_objects < 2000 and report.peak_gpu_bytes <= 64*1024*1024
        assert primary.read_bytes() == before and target.dirty == args.recovery
        if args.recovery:
            assert target.document(image).annotation(1).name == 'Recovered'
        result = dict(platform=platform.platform(), objects=args.objects, kind=options.startup_kind,
                      loading_frames=report.startup_frames, total_frames=report.frames,
                      worker_load_ms=report.startup_ms, gui_session_ms=wall_ms,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      selected_objects=len(report.selected_ids), visible_objects=report.visible_objects,
                      peak_gpu_payload_bytes=report.peak_gpu_bytes, gl_renderer=report.gl_renderer,
                      scope='Warm local schema-2 project with embedded demo pixels and unit rectangles. '
                            'Native file loading starts after the first window swap; preparation excluded. '
                            'All objects selected after adoption in a narrow view. Frame work excludes swap wait; '
                            'GPU payload excludes CPU graphs. Frame samples include initial startup/adoption frames. Cold/network and slow vendor metadata excluded.')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(result, indent=2))
    finally:
        for path in [primary, recovery, Path(str(recovery)+'.lock'), Path(str(recovery)+'.session')]:
            assert path.parent == root
            path.unlink(missing_ok=True)
        root.rmdir()


if __name__ == '__main__':
    main()
