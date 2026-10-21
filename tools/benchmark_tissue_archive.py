"""Shared-mask publication, immutable save capture and lossless reload timings."""
import argparse
from contextlib import closing
import json
from pathlib import Path
import platform
import sqlite3
import tempfile
import time

import numpy as np
import qupathxx as q


def timed(action):
    start = time.perf_counter()
    value = action()
    return value, (time.perf_counter()-start)*1000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", type=int, default=0)
    parser.add_argument("--records", type=int, default=1)
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--checkerboard", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/tissue-archive.json"))
    args = parser.parse_args()
    if not 0 <= args.objects <= 100000 or not 1 <= args.records <= 8:
        parser.error("objects must be 0..100000 and records 1..8")
    yy, xx = np.mgrid[:2048, :2048]
    mask = (xx+yy) % 2 == 0 if args.checkerboard else (xx % 137 < 30) & (yy % 113 < 24)
    rgb = np.where(mask[..., None], 0, 255).repeat(3, axis=2).astype(np.uint8)
    project = q.Project()
    doc = q.Document(q.Image(2048, 2048, rgb.ravel().tolist()))
    image = project.add_document(doc)
    doc.set_history_limits(0, 0)
    if args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % 2048, i//2048, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(0, 0, 2048, 2048))
    request = q.TissueRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    result = q.detect_tissue(request)
    assert result.component_count == (1 if args.checkerboard else 285) and result.foreground_pixels == int(mask.sum())
    ids, keep_ms = timed(lambda: [doc.keep_tissue(request, result) for _ in range(args.records)])
    assert all(doc.tissue_record(i).result is result for i in ids)
    output = dict(platform=platform.platform(), pattern="checkerboard" if args.checkerboard else "rectangles", pixels=mask.size, objects=doc.object_count, records=args.records,
                  input_sha256=result.input_sha256, mask_worker_ms=result.elapsed_ms, keep_total_ms=keep_ms,
                  mask_retained_bytes=result.retained_bytes, record_payload_bytes=doc.tissue_record(ids[0]).payload_bytes)
    with tempfile.TemporaryDirectory(prefix="qpx-tissue-archive-") as directory:
        path = Path(directory)/"archive.qpxproj"
        job = q.ProjectSaveJob()
        assert job.request(project, str(path))
        status = job.wait()
        assert status.applied and not project.dirty, status.error
        loaded, load_ms = timed(lambda: q.Project.open(str(path)))
        restored = loaded.document(image)
        assert restored.tissue_record_count == args.records and restored.tissue_status(ids[0]) == "unverified"
        assert all(restored.tissue_record(record).result.labels == result.labels for record in ids)
        replay = q.TissueRequest.replay(restored, ids[0])
        fresh = q.detect_tissue(replay)
        verified, verify_ms = timed(lambda: restored.verify_tissue(ids[0], replay, fresh))
        assert verified and not loaded.dirty
        with closing(sqlite3.connect(path)) as db:
            encoded = [row[0] for row in db.execute("SELECT length(mask) FROM tissue_records")]
        output.update(capture_ms=status.capture_ms, write_ms=status.write_ms, snapshot_bytes=status.snapshot_bytes,
                      file_bytes=path.stat().st_size, encoded_mask_bytes=encoded, load_ms=load_ms,
                      verify_ms=verify_ms, decoded_retained_bytes=restored.tissue_record(ids[0]).result.retained_bytes)
    if args.gui:
        options = q.GuiOptions()
        events = []
        for frame, kind, wait in [(4, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputWait.IMMEDIATE),
                                 (8, q.GuiInputKind.DETECT_TISSUE_KEY, q.GuiInputWait.IMMEDIATE),
                                 (10, q.GuiInputKind.KEEP_TISSUE_KEY, q.GuiInputWait.TISSUE_READY)]:
            event = q.GuiInput()
            event.frame, event.kind, event.wait_for = frame, kind, wait
            events.append(event)
        options.inputs = events
        view = q.Viewport()
        view.zoom, view.origin = 32, q.Point(-2048*8, -min(32, (args.objects//2048)/2)*32)
        options.start_view = view
        report = q.show(doc, frame_limit=180, options=options)
        assert report.saved_tissue_status == "current" and report.saved_tissue_id and doc.tissue_record_count == args.records+1
        assert len(report.selected_ids) == args.objects+1 and report.tissue_input_sha256 == result.input_sha256
        output.update(selected=len(report.selected_ids), visible=report.visible_objects,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      image_gpu_bytes=report.peak_gpu_bytes, tissue_gpu_bytes=report.tissue_gpu_bytes,
                      gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
