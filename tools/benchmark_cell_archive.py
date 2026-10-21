"""Lossless cell-run storage, shared capture and optional selected viewer."""
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
    parser.add_argument("--output", type=Path, default=Path("build/cell-archive.json"))
    args = parser.parse_args()
    if not 0 <= args.objects <= 100000 or not 1 <= args.records <= 8:
        parser.error("objects 0..100000; records 1..8")
    yy, xx = np.indices((1024,1024))
    mask = (xx % 64-32)**2+(yy % 64-32)**2 <= 100
    rgb = np.where(mask[...,None], [30,40,60], [255,255,255]).astype(np.uint8)
    project = q.Project()
    doc = q.Document(q.Image(1024,1024,rgb.ravel().tolist()))
    image = project.add_document(doc)
    doc.set_history_limits(0,0)
    if args.objects:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % 1024, i//1024, .5, .5))) for i in range(args.objects)])
    obj = doc.add_rectangle(q.Rect(0,0,1024,1024))
    p = q.CellParameters()
    p.sigma = 0
    request = q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),p)
    result = q.detect_cells(request)
    expected = np.where(mask,(yy//64)*16+(xx//64)+1,0)
    np.testing.assert_array_equal(np.frombuffer(result.nuclei.labels,dtype=np.uint32).reshape(mask.shape),expected)
    assert result.cell_count == 256
    records, keep_ms = timed(lambda: [doc.keep_cells(request,result) for _ in range(args.records)])
    assert all(doc.cell_record(i).result is result for i in records)
    output = dict(platform=platform.platform(),pixels=mask.size,objects=doc.object_count,records=args.records,cells=result.cell_count,
                  worker_ms=result.elapsed_ms,keep_total_ms=keep_ms,retained_bytes=result.retained_bytes,
                  record_payload_bytes=doc.cell_record(records[0]).payload_bytes,input_sha256=result.nuclei.input_sha256)
    with tempfile.TemporaryDirectory(prefix="qpx-cell-archive-") as directory:
        path = Path(directory)/"cells.qpxproj"
        save = q.ProjectSaveJob()
        assert save.request(project,str(path))
        status = save.wait()
        assert status.applied and not project.dirty,status.error
        loaded, load_ms = timed(lambda: q.Project.open(str(path)))
        restored = loaded.document(image)
        assert restored.cell_record_count == args.records and restored.cell_status(records[0]) == "unverified"
        for record in restored.cell_records:
            assert record.result.nuclei.labels == result.nuclei.labels and record.result.cells.labels == result.cells.labels
            assert [(f.nucleus_channels,f.cell_channels,f.cytoplasm_channels) for f in record.result.features] == [(f.nucleus_channels,f.cell_channels,f.cytoplasm_channels) for f in result.features]
        replay = q.CellRequest.replay(restored,records[0])
        fresh = q.detect_cells(replay)
        verified, verify_ms = timed(lambda: restored.verify_cells(records[0],replay,fresh))
        assert verified and not loaded.dirty
        with closing(sqlite3.connect(path)) as db:
            encoded = [row[0] for row in db.execute("SELECT length(run) FROM cell_records")]
        output.update(capture_ms=status.capture_ms,write_ms=status.write_ms,snapshot_bytes=status.snapshot_bytes,
                      file_bytes=path.stat().st_size,encoded_run_bytes=encoded,load_ms=load_ms,verify_ms=verify_ms,
                      decoded_retained_bytes=restored.cell_record(records[0]).result.retained_bytes)
    if args.gui:
        def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
            e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
        options=q.GuiOptions()
        options.cell_parameters=p
        options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.DETECT_CELLS_KEY),
                        event(10,q.GuiInputKind.KEEP_CELL_RUN_KEY,q.GuiInputWait.CELLS_READY)]
        report=q.show(doc,frame_limit=180,options=options)
        assert report.saved_cell_status == "current" and doc.cell_record_count == args.records+1
        assert len(report.selected_ids) == args.objects+1 and report.cell_result.nuclei.labels == result.nuclei.labels
        output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                      max_cpu_frame_ms=report.max_cpu_frame_ms,image_gpu_bytes=report.peak_gpu_bytes,cell_gpu_bytes=report.cell_gpu_bytes,
                      canvas_gpu_bytes=report.overlay_cache_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(output,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(output,indent=2))


if __name__ == "__main__":
    main()
