"""Native cell pipeline timing with independently specified disk nuclei."""
import argparse
import json
from pathlib import Path
import platform
import time

import numpy as np
import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--overview", action="store_true", help="Keep all 100000 synthetic objects in view")
    parser.add_argument("--output", type=Path, default=Path("build/cells.json"))
    args = parser.parse_args()
    if not 64 <= args.size <= 1536 or args.size % 64 or not 0 <= args.objects <= 100000:
        parser.error("size must be a multiple of 64 in 64..1536; objects 0..100000")
    yy, xx = np.indices((args.size, args.size))
    nuclei = (xx % 64-32)**2+(yy % 64-32)**2 <= 100
    expanded = (xx % 64-32)**2+(yy % 64-32)**2 <= 225
    rgb = np.where(nuclei[..., None], [30, 40, 60], [255, 255, 255]).astype(np.uint8)
    doc = q.Document(q.Image(args.size, args.size, rgb.ravel().tolist()))
    doc.set_history_limits(0, 0)
    if args.gui and args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % args.size, (i//args.size) % args.size, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(0, 0, args.size, args.size))
    p = q.CellParameters()
    p.sigma, p.cell_expansion = 0, 5
    start = time.perf_counter()
    request = q.CellRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p)
    capture_ms = (time.perf_counter()-start)*1000
    job = q.CellJob()
    start = time.perf_counter()
    assert job.request(request)
    submit_ms = (time.perf_counter()-start)*1000
    max_poll = 0
    while True:
        start = time.perf_counter()
        status = job.poll(doc, obj)
        max_poll = max(max_poll, (time.perf_counter()-start)*1000)
        if not job.busy:
            break
        time.sleep(.001)
    assert status.state == "ready", status.error
    result = job.result
    expected = (yy//64)*(args.size//64)+(xx//64)+1
    actual = np.frombuffer(result.nuclei.labels, dtype=np.uint32).reshape(nuclei.shape)
    bodies = np.frombuffer(result.cells.labels, dtype=np.uint32).reshape(nuclei.shape)
    np.testing.assert_array_equal(actual, np.where(nuclei, expected, 0))
    assert result.cell_count == (args.size//64)**2
    # Euclidean dilation is measured from sampled nucleus pixels, rather than
    # from analytic circle centers: independently enumerate all offsets <=5.
    body_mask = np.zeros_like(nuclei)
    for dy in range(-5, 6):
        for dx in range(-5, 6):
            if dx*dx+dy*dy <= 25:
                body_mask |= np.roll(nuclei, (dy, dx), axis=(0, 1))
    assert not np.any(body_mask & ~expanded)
    np.testing.assert_array_equal(bodies, np.where(body_mask, expected, 0))
    assert result.retained_bytes <= request.estimated_work_bytes <= p.work_bytes
    output = dict(platform=platform.platform(), processor=platform.processor(), pixels=args.size**2,
                  algorithm=result.algorithm, sigma=p.sigma, expansion=p.cell_expansion,
                  cells=result.cell_count, nucleus_pixels=int(nuclei.sum()), cell_pixels=int(body_mask.sum()),
                  capture_ms=capture_ms, submit_ms=submit_ms, worker_ms=status.elapsed_ms,
                  max_poll_ms=max_poll, estimated_work_bytes=request.estimated_work_bytes,
                  retained_bytes=result.retained_bytes, input_sha256=result.nuclei.input_sha256)
    if args.gui:
        def event(frame, kind, wait=q.GuiInputWait.IMMEDIATE):
            e = q.GuiInput()
            e.frame, e.kind, e.wait_for = frame, kind, wait
            return e
        options = q.GuiOptions()
        options.cell_parameters = p
        if not args.overview:
            view = q.Viewport()
            view.zoom, view.origin = 32, q.Point(-args.size*8, -min(32, (args.objects//args.size)/2)*32)
            options.start_view = view
        options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_CELLS_KEY),
                          event(10, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputWait.CELLS_READY)]
        report = q.show(doc, frame_limit=180, options=options)
        assert report.cell_state == "ready" and report.cell_preview_ready, report.cell_error
        assert report.cell_result.nuclei.labels == result.nuclei.labels
        assert report.cell_result.cells.labels == result.cells.labels
        assert len(report.selected_ids) == args.objects+1 and report.cell_gpu_bytes <= 32*1024*1024
        output.update(objects=doc.object_count, selected=len(report.selected_ids),
                      overview=args.overview, visible_objects=report.visible_objects,
                      cell_gpu_bytes=report.cell_gpu_bytes, image_gpu_bytes=report.peak_gpu_bytes,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      gui_worker_ms=report.cell_result.elapsed_ms, gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
