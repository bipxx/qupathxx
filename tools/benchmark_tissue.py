"""Bounded tissue job timing, independent rectangle labels and selected viewer."""
import argparse
import json
from pathlib import Path
import platform
import time

import numpy as np
import qupathxx as q


def event(frame, kind):
    value = q.GuiInput()
    value.frame, value.kind = frame, kind
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=2048)
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/tissue.json"))
    args = parser.parse_args()
    if not 64 <= args.size <= 2048 or not 0 <= args.objects <= 100000:
        parser.error("require size 64..2048 and objects 0..100000")
    yy, xx = np.mgrid[:args.size, :args.size]
    mask = (xx % 137 < 30) & (yy % 113 < 24)
    rgb = np.where(mask[..., None], 0, 255).repeat(3, axis=2).astype(np.uint8)
    doc = q.Document(q.Image(args.size, args.size, rgb.ravel().tolist()))
    doc.set_history_limits(0, 0)
    if args.gui and args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % args.size, (i//args.size) % args.size, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(0, 0, args.size, args.size))
    p = q.TissueParameters()
    start = time.perf_counter()
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p)
    capture_ms = (time.perf_counter()-start)*1000
    job = q.TissueJob()
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
    columns = (args.size-1)//137+1
    expected = np.where(mask, (yy//113)*columns+(xx//137)+1, 0).astype(np.uint32)
    actual = np.frombuffer(result.labels, dtype=np.uint32).reshape(mask.shape)
    np.testing.assert_array_equal(actual, expected)
    assert result.foreground_pixels == int(mask.sum())
    assert result.component_count == columns*((args.size-1)//113+1)
    assert result.retained_bytes <= request.estimated_work_bytes <= p.work_bytes
    output = dict(platform=platform.platform(), processor=platform.processor(), pixels=args.size**2,
                  algorithm=result.algorithm, feature="OD sum", threshold=p.threshold, connectivity=p.connectivity,
                  tile_size=p.tile_size, input_sha256=result.input_sha256, components=result.component_count,
                  samples=result.samples, foreground_pixels=result.foreground_pixels, capture_ms=capture_ms,
                  submit_ms=submit_ms, worker_ms=status.elapsed_ms, max_poll_ms=max_poll,
                  estimated_work_bytes=request.estimated_work_bytes, retained_bytes=result.retained_bytes,
                  label_checksum=int(actual.sum(dtype=np.uint64)))
    if args.gui:
        options = q.GuiOptions()
        options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY)]
        view = q.Viewport()
        view.zoom, view.origin = 32, q.Point(-args.size*8, -min(32, (args.objects//args.size)/2)*32)
        options.start_view = view
        report = q.show(doc, frame_limit=180, options=options)
        assert report.tissue_state == "ready" and report.tissue_preview_ready, report.tissue_error
        assert report.tissue_samples == result.samples and report.tissue_foreground_pixels == result.foreground_pixels
        assert report.tissue_components == result.component_count and report.tissue_input_sha256 == result.input_sha256
        assert len(report.selected_ids) == args.objects+1 and report.tissue_gpu_bytes <= 32*1024*1024
        output.update(objects=doc.object_count, selected=len(report.selected_ids), visible=report.visible_objects,
                      image_gpu_bytes=report.peak_gpu_bytes, tissue_gpu_bytes=report.tissue_gpu_bytes,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      gui_worker_ms=report.tissue_ms, gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
