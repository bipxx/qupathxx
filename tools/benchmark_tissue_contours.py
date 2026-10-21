"""Synthetic contour conversion/publication recording with independent areas."""
import argparse
import json
from pathlib import Path
import platform
import time

import numpy as np
import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("build/tissue-contours.json"))
    parser.add_argument("--objects", type=int, default=0)
    parser.add_argument("--gui", action="store_true")
    args = parser.parse_args()
    if not 0 <= args.objects <= 100000:
        parser.error("objects must be 0..100000")
    yy, xx = np.mgrid[:2048, :2048]
    mask = (xx % 137 < 30) & (yy % 113 < 24)
    rgb = np.where(mask[..., None], 0, 255).repeat(3, axis=2).astype(np.uint8)
    doc = q.Document(q.Image(2048, 2048, rgb.ravel().tolist()))
    doc.set_history_limits(0, 0)
    if args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % 2048, i//2048, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(0, 0, 2048, 2048))
    doc.set_history_limits(128, 256*1024*1024)
    job = q.TissueJob()
    assert job.request(q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])))
    assert job.wait(doc, obj).state == "ready"
    mask_ms = job.result.elapsed_ms
    start = time.perf_counter()
    assert job.request_contours()
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
    result = job.contours
    assert result.component_count == 285 and result.edge_count == result.vertex_count == 1140
    assert sum(c.geometry.area for c in result.components) == int(mask.sum()) == 200700
    start = time.perf_counter()
    ids = job.keep_contours(doc, obj)
    publish_ms = (time.perf_counter()-start)*1000
    assert len(ids) == 285 and doc.object_count == args.objects+286
    start = time.perf_counter()
    assert doc.undo() and doc.object_count == args.objects+1
    undo_ms = (time.perf_counter()-start)*1000
    start = time.perf_counter()
    assert doc.redo() and doc.children(obj) == ids
    redo_ms = (time.perf_counter()-start)*1000
    output = dict(platform=platform.platform(), pixels=mask.size, foreground_pixels=int(mask.sum()),
                  input_sha256=result.input_sha256, algorithm=result.algorithm,
                  components=result.component_count, edges=result.edge_count, vertices=result.vertex_count,
                  mask_worker_ms=mask_ms, contour_submit_ms=submit_ms, contour_worker_ms=result.elapsed_ms,
                  max_poll_ms=max_poll, publish_ms=publish_ms, undo_ms=undo_ms, redo_ms=redo_ms,
                  existing_objects=args.objects+1, estimated_work_bytes=result.estimated_work_bytes, retained_bytes=result.retained_bytes)
    if args.gui:
        assert doc.undo()
        options = q.GuiOptions()
        events = []
        for frame, kind, wait in [(4, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputWait.IMMEDIATE),
                                 (8, q.GuiInputKind.DETECT_TISSUE_KEY, q.GuiInputWait.IMMEDIATE),
                                 (10, q.GuiInputKind.TRACE_TISSUE_KEY, q.GuiInputWait.TISSUE_READY),
                                 (12, q.GuiInputKind.KEEP_TISSUE_CONTOURS_KEY, q.GuiInputWait.TISSUE_CONTOURS_READY)]:
            event = q.GuiInput()
            event.frame, event.kind, event.wait_for = frame, kind, wait
            events.append(event)
        options.inputs = events
        view = q.Viewport()
        view.zoom, view.origin = 32, q.Point(-2048*8, -min(32, (args.objects//2048)/2)*32)
        options.start_view = view
        report = q.show(doc, frame_limit=180, options=options)
        assert report.tissue_state == "ready" and report.tissue_contours_published, report.tissue_error
        assert report.tissue_annotations_added == 285 and report.tissue_contour_vertices == 1140
        assert report.tissue_input_sha256 == result.input_sha256 and len(report.selected_ids) == args.objects+1
        assert doc.object_count == args.objects+286 and len(doc.children(obj)) == 285
        assert sum(doc.annotation(i).geometry.area for i in doc.children(obj)) == 200700
        output.update(selected=len(report.selected_ids), objects=doc.object_count, visible=report.visible_objects,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      gui_contour_ms=report.tissue_contour_ms, image_gpu_bytes=report.peak_gpu_bytes,
                      tissue_gpu_bytes=report.tissue_gpu_bytes, gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
