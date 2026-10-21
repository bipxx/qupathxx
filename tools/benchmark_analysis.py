"""Tiled native analysis latency and optional live selected-view publication."""
import argparse
import json
from pathlib import Path
import platform
import time

import numpy as np
import qupathxx as q


def event(frame, kind):
    e = q.GuiInput()
    e.frame, e.kind = frame, kind
    return e


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=2048)
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--output", type=Path, default=Path("build/analysis.json"))
    args = parser.parse_args()
    if not 64 <= args.size <= 4096 or not 0 <= args.objects <= 1000000:
        parser.error("require size 64..4096 and objects 0..1000000")
    rgb = np.random.default_rng(125).integers(0, 256, (args.size, args.size, 3), dtype=np.uint8)
    doc = q.Document(q.Image(args.size, args.size, rgb.ravel().tolist()))
    doc.set_history_limits(0, 0)
    if args.gui and args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % args.size, (i//args.size) % args.size, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(0, 0, args.size, args.size))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]], [250, 245, 230])
    start = time.perf_counter()
    request = q.AnalysisRequest(doc, obj, transform)
    capture_ms = (time.perf_counter()-start)*1000
    expected = q.analyze(request)
    job = q.AnalysisJob()
    start = time.perf_counter()
    assert job.request(request)
    request_ms = (time.perf_counter()-start)*1000
    polls = []
    while True:
        start = time.perf_counter()
        status = job.poll(doc, obj)
        polls.append((time.perf_counter()-start)*1000)
        if not job.busy:
            break
        time.sleep(.001)
    assert status.state == "ready", status.error
    actual = job.result
    np.testing.assert_array_equal(actual.means, expected.means)
    assert actual.samples == args.size**2 and actual.tiles == request.total_tiles
    result = dict(platform=platform.platform(), processor=platform.processor(), pixels=args.size**2,
                  algorithm=actual.algorithm, white=actual.white, stains=actual.stains, means=actual.means,
                  samples=actual.samples, tiles=actual.tiles, tile_size=request.tile_size,
                  capture_ms=capture_ms, request_ms=request_ms, worker_ms=status.elapsed_ms,
                  max_poll_ms=max(polls), polls=len(polls), completed_tiles=status.completed_tiles,
                  # Input RGBA + float RGB output, excluding reader-specific scratch.
                  max_tile_payload_bytes=256*256*16)
    if args.gui:
        options = q.GuiOptions()
        options.analysis_kind, options.analysis_white = 1, [250, 245, 230]
        options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.ANALYZE_KEY)]
        view = q.Viewport()
        view.zoom = 32
        view.origin = q.Point(-args.size*8, -min(32, (args.objects//args.size)/2)*32)
        options.start_view = view
        report = q.show(doc, frame_limit=180, options=options)
        assert report.analysis_state == "ready", report.analysis_error
        assert report.analysis_samples == actual.samples and report.analysis_object == obj
        assert len(report.selected_ids) == args.objects+1
        np.testing.assert_array_equal(report.analysis_means, actual.means)
        result.update(objects=doc.object_count, selected=len(report.selected_ids),
                      visible=report.visible_objects, gpu_bytes=report.peak_gpu_bytes,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      gui_worker_ms=report.analysis_ms, gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
