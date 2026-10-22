"""Time the desktop batch launcher with a large independently rendered scene."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import tempfile

import qupathxx as q
from qupathxx import _native


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--objects', type=int, default=100000)
    parser.add_argument('--output', type=Path, default=Path('build/gui-batch-benchmark.json'))
    parser.add_argument('--profile', type=Path, default=Path('build/gui-batch-frames.json'))
    parser.add_argument('--capture', default='')
    args = parser.parse_args()
    assert 0 <= args.objects <= 100000
    with tempfile.TemporaryDirectory(prefix='qpx-gui-batch-') as directory:
        root = Path(directory)
        doc = q.Document(q.Image(512, 512, [32]*512*512*3))
        doc.set_history_limits(0, 0)
        if args.objects:
            doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%512, (i//512)%512, .5, .5)),
                         kind=q.ObjectKind.DETECTION) for i in range(args.objects)])
        doc.add_rectangle(q.Rect(0, 0, 512, 512))
        project = q.Project()
        project.add_document(doc, 'Large scene')
        project.save(str(root/'source.qpxproj'))
        options = q.GuiOptions()
        options.batch_output_directory = str(root/'results')
        options.replay_timeout_seconds = 60
        options.settle_tiles = True
        def event(frame, kind):
            value = q.GuiInput()
            value.frame, value.kind = frame, kind
            return value
        ready = event(5, q.GuiInputKind.MOVE)
        ready.wait_for = q.GuiInputWait.BATCH_FINISHED
        options.inputs = [event(2, q.GuiInputKind.SELECT_ALL_KEY), event(4, q.GuiInputKind.RUN_BATCH_KEY), ready]
        args.profile.parent.mkdir(parents=True, exist_ok=True)
        previous = os.environ.get('QUPATHXX_GUI_PROFILE')
        os.environ['QUPATHXX_GUI_PROFILE'] = str(args.profile.resolve())
        try:
            report = q.show(project, frame_limit=8, capture_path=args.capture, options=options)
        finally:
            if previous is None:
                os.environ.pop('QUPATHXX_GUI_PROFILE', None)
            else:
                os.environ['QUPATHXX_GUI_PROFILE'] = previous
        assert report.batch_state == 'succeeded' and not report.batch_error
        batch = report.batch_result
        assert batch.saved == 1 and batch.failed == batch.reused == 0
        restored = q.Project.open(batch.images[0].path).document(1)
        assert len(restored.annotations) == args.objects+1 and len(restored.analysis_records) == 1
        assert not doc.analysis_records and len(report.selected_ids) == args.objects+1
        frames = json.loads(args.profile.read_text())['frames']
        times = sorted(frame['total_ms'] for frame in frames)
        result = dict(platform=platform.platform(),
                      native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                      source_tree_sha256=q.build_environment().source_tree_sha256,
                      measurement_scope='desktop_batch_until_finished_plus_three_logical_frames',
                      stored_objects=args.objects+1, source_rois=1, selected=len(report.selected_ids),
                      frames=report.frames, profiled_frames=len(frames), capture_ms=batch.capture_ms,
                      batch_ms=batch.elapsed_ms, snapshot_bytes=batch.snapshot_bytes,
                      all_frame_p95_ms=times[int((len(times)-1)*.95)], all_frame_max_ms=max(times),
                      report_cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                      report_max_cpu_frame_ms=report.max_cpu_frame_ms,
                      overlay_cache_bytes=report.overlay_cache_bytes, tile_gpu_bytes=report.peak_gpu_bytes,
                      gl_renderer=report.gl_renderer)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
