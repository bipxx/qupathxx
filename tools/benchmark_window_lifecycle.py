"""Paired active/minimized native loops, retaining exact source and binary identity."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import time

import qupathxx as q
from qupathxx import _native


def event(frame, kind):
    value = q.GuiInput()
    value.frame, value.kind = frame, kind
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--objects', type=int, default=10000)
    parser.add_argument('--frames', type=int, default=180)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.objects <= 100000 or not 60 <= args.frames <= 1000 or not 1 <= args.repetitions <= 10:
        parser.error('objects 1..100000, frames 60..1000, repetitions 1..10')
    doc = q.Document(q.Image(1024, 1024, [35, 60, 90]*1024*1024))
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect((i % 500)*2, (i//500)*2, 1, 1)) for i in range(args.objects)])
    revision = doc.revision
    args.output.parent.mkdir(parents=True, exist_ok=True)
    data = dict(schema='qpx.window-lifecycle-benchmark/1', platform=platform.platform(), processor=platform.processor(),
                objects=args.objects, logical_frames=args.frames, repetitions=args.repetitions,
                benchmark_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                source_tree_sha256=q.build_environment().source_tree_sha256,
                build_git_revision=q.build_environment().git_revision,
                native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(), runs=[])
    framebuffer_sha = None
    for repetition in range(args.repetitions):
        # Alternate order to retain, rather than hide, initialization/driver variation.
        for minimized in ([False, True] if repetition % 2 == 0 else [True, False]):
            options = q.GuiOptions()
            options.settle_tiles = True
            inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY)]
            if minimized:
                inputs += [event(12, q.GuiInputKind.MINIMIZE_WINDOW), event(args.frames-12, q.GuiInputKind.RESTORE_WINDOW)]
            pointer = event(args.frames-3, q.GuiInputKind.MOVE)
            pointer.x, pointer.y, pointer.wait_for = .99, .99, q.GuiInputWait.WINDOW_READY
            options.inputs = inputs+[pointer]
            capture = args.output.with_suffix(f'.{repetition}.{"minimized" if minimized else "active"}.ppm')
            wall, cpu = time.perf_counter(), time.process_time()
            report = q.show(doc, frame_limit=args.frames, capture_path=str(capture), options=options)
            wall, cpu = time.perf_counter()-wall, time.process_time()-cpu
            assert report.replay_complete and report.tiles_settled and not report.window_iconified
            assert doc.revision == revision and len(report.selected_ids) == args.objects
            assert report.suspended_iterations > 0 if minimized else report.suspended_iterations == 0
            captured_sha = hashlib.sha256(capture.read_bytes()).hexdigest()
            if framebuffer_sha is None:
                framebuffer_sha = captured_sha
            assert captured_sha == framebuffer_sha, 'restored viewer changed the quiescent framebuffer'
            data['runs'].append(dict(repetition=repetition, minimized=minimized, wall_seconds=wall,
                process_cpu_seconds=cpu, cpu_core_fraction=cpu/wall, rendered_frames=report.frames,
                logical_frames=report.replay_frame, suspended_iterations=report.suspended_iterations,
                suspended_work_ms=report.suspended_work_ms, suspended_wait_ms=report.suspended_wait_ms,
                rendered_cpu_p95_ms=report.cpu_frame_p95_ms, max_rendered_cpu_ms=report.max_cpu_frame_ms,
                gpu_tile_peak_bytes=report.peak_gpu_bytes, framebuffer=[report.framebuffer_width,report.framebuffer_height],
                framebuffer_sha256=captured_sha, gl_renderer=report.gl_renderer))
    data['median_cpu_core_fraction'] = {label: statistics.median(r['cpu_core_fraction'] for r in data['runs'] if r['minimized'] == flag)
                                        for label, flag in [('active', False), ('minimized', True)]}
    data['scope'] = ('Synthetic static selected rectangles. Per-invocation wall/process CPU includes context setup, initial drawing, '
                     'driver work, waiting, capture and teardown; excludes scene construction. Process CPU sums all process threads. '
                     'Minimized work is wall time before the event wait, including model/command/save servicing; wait time is separate. '
                     'Rendered-frame timings exclude minimized iterations and presentation. No first-render or hardware GPU speedup claim.')
    args.output.write_text(json.dumps(data, indent=2)+'\n', encoding='utf8')
    print(json.dumps(data, indent=2))


if __name__ == '__main__':
    main()
