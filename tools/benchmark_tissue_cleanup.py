"""Analytic masks and radius-scaling evidence for native tissue cleanup."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import time

import numpy as np
import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/tissue-cleanup.json"))
    args = parser.parse_args()
    size, margin = 2048, 96
    mask = np.zeros((size, size), dtype=bool)
    mask[margin:-margin, margin:-margin] = True
    mask[1016:1032, 1016:1032] = False
    mask[5, 5] = mask[6, 20] = mask[30, 10] = True
    rgb = np.where(mask[..., None], 0, 255).repeat(3, axis=2).astype(np.uint8)
    doc = q.Document(q.Image(size, size, rgb.ravel().tolist()))
    doc.set_history_limits(0, 0)
    if args.gui:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % size, i//size, .5, .5))) for i in range(100000)])
    obj = doc.add_rectangle(q.Rect(0, 0, size, size))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    expected = np.zeros_like(mask)
    expected[margin:-margin, margin:-margin] = True
    recordings = []
    digest = None
    for radius in [0, 1, 8, 64]:
        parameters = q.TissueParameters()
        parameters.open_radius = parameters.close_radius = radius
        parameters.fill_holes = radius != 0
        request = q.TissueRequest(doc, obj, transform, parameters)
        workers, cleanups, submissions, polls = [], [], [], []
        for _ in range(3):
            job = q.TissueJob()
            began = time.perf_counter()
            assert job.request(request)
            submissions.append((time.perf_counter()-began)*1000)
            maximum = 0
            while True:
                began = time.perf_counter()
                status = job.poll(doc, obj)
                maximum = max(maximum, (time.perf_counter()-began)*1000)
                if not job.busy:
                    break
                time.sleep(.001)
            assert status.state == "ready", status.error
            result = job.result
            actual = np.frombuffer(result.labels, dtype=np.uint32).reshape(mask.shape)
            np.testing.assert_array_equal(actual != 0, expected if radius else mask)
            assert result.cleaned_pixels == result.foreground_pixels == int((expected if radius else mask).sum())
            assert result.filled_pixels == (256 if radius == 1 else 0)
            assert result.component_count == (1 if radius else 4)
            assert result.retained_bytes <= request.estimated_work_bytes <= parameters.work_bytes
            if digest is None:
                digest = result.input_sha256
            assert digest == result.input_sha256
            workers.append(status.elapsed_ms)
            cleanups.append(result.cleanup_ms)
            polls.append(maximum)
        recordings.append(dict(radius=radius, worker_ms=workers, cleanup_ms=cleanups,
                               worker_median_ms=statistics.median(workers), cleanup_median_ms=statistics.median(cleanups),
                               submit_ms=submissions, max_poll_ms=max(polls),
                               estimated_work_bytes=request.estimated_work_bytes, retained_bytes=result.retained_bytes,
                               cleaned_pixels=result.cleaned_pixels, filled_pixels=result.filled_pixels))
    output = dict(platform=platform.platform(), processor=platform.processor(), pixels=size*size,
                  input_sha256=digest, threshold_pixels=int(mask.sum()), recordings=recordings)
    if args.gui:
        options = q.GuiOptions()
        options.tissue_open_radius = options.tissue_close_radius = 8
        options.tissue_fill_holes = True
        view = q.Viewport()
        view.zoom, view.origin = 32, q.Point(-512*32, -24*32)
        options.start_view = view
        events = []
        for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (8, q.GuiInputKind.DETECT_TISSUE_KEY)]:
            event = q.GuiInput()
            event.frame, event.kind = frame, kind
            events.append(event)
        options.inputs = events
        report = q.show(doc, frame_limit=180, options=options)
        assert report.tissue_state == "ready" and report.tissue_preview_ready, report.tissue_error
        assert report.tissue_foreground_pixels == report.tissue_cleaned_pixels == int(expected.sum())
        assert report.tissue_filled_pixels == 0 and report.tissue_components == 1
        assert report.tissue_input_sha256 == digest and len(report.selected_ids) == 100001
        assert report.tissue_gpu_bytes <= 32*1024*1024
        output.update(objects=doc.object_count, selected=len(report.selected_ids), visible=report.visible_objects,
                      gui_worker_ms=report.tissue_ms, gui_cleanup_ms=report.tissue_cleanup_ms,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      tissue_gpu_bytes=report.tissue_gpu_bytes, image_gpu_bytes=report.peak_gpu_bytes,
                      gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
