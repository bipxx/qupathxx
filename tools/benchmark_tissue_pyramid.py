"""Bounded whole-image tissue detection on the generated 205.5 MP pyramid."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import struct
import time

import numpy as np
import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, default=Path("build/fixtures/large-pyramid.tif"))
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/tissue-pyramid.json"))
    args = parser.parse_args()
    server = q.open_slide(str(args.fixture))
    metadata = server.metadata
    assert (metadata.width, metadata.height) == (100352, 2048)
    assert [(level.width, level.height, level.downsample) for level in metadata.levels] == [
        (100352 >> index, 2048 >> index, 2**index) for index in range(9)]
    doc = q.Document(server)
    doc.set_history_limits(0, 0)
    if args.gui:
        doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % 2048, i//2048, .5, .5))) for i in range(100000)])
    obj = doc.add_rectangle(q.Rect(0, 0, metadata.width, metadata.height))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    parameters = q.TissueParameters()
    parameters.level = 0
    try:
        q.TissueRequest(doc, obj, transform, parameters)
    except (ValueError, RuntimeError) as error:
        assert "work budget" in str(error)
    else:
        raise AssertionError("full-resolution allocation should exceed the default budget")
    parameters.level = -1
    start = time.perf_counter()
    request = q.TissueRequest(doc, obj, transform, parameters)
    capture_ms = (time.perf_counter()-start)*1000
    assert request.level == 3 and request.downsample == 8
    runs = []
    for _ in range(2):
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
        count = (100352//8)*(2048//8)
        assert result.samples == result.foreground_pixels == count and result.component_count == 1
        assert np.all(np.frombuffer(result.labels, dtype=np.uint32) == 1)
        component = result.components[0]
        assert (component.bounds.x, component.bounds.y, component.bounds.width, component.bounds.height) == (0, 0, 100352, 2048)
        assert (component.centroid.x, component.centroid.y) == (50176, 1024)
        assert result.retained_bytes <= request.estimated_work_bytes <= parameters.work_bytes
        runs.append(dict(submit_ms=submit_ms, worker_ms=status.elapsed_ms, max_poll_ms=max_poll))
    digest = hashlib.sha256(b"qpx.rgba-level-tiles/1")
    digest.update(struct.pack("<5Qd5Q", 100352, 2048, 3, 12544, 256, 8., 0, 0, 12544, 256, 256))
    for x in range(0, 12544, 256):
        digest.update(struct.pack("<6Q", x, 0, x*8, 0, 256, 256))
        digest.update(bytes([40, 80, 160, 255])*(256*256))
    assert result.input_sha256 == digest.hexdigest(), "fixture pixels differ from generate_large_fixture.py"
    output = dict(platform=platform.platform(), processor=platform.processor(),
                  source_pixels=metadata.width*metadata.height, level=result.level, downsample=result.downsample,
                  sampled_pixels=result.samples, sample_reduction=64, capture_ms=capture_ms,
                  estimated_work_bytes=request.estimated_work_bytes, retained_bytes=result.retained_bytes,
                  input_format=result.input_format, input_sha256=result.input_sha256, runs=runs)
    if args.gui:
        options = q.GuiOptions()
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
        assert report.tissue_level == 3 and report.tissue_downsample == 8
        assert (report.tissue_width, report.tissue_height) == (12544, 256)
        assert report.tissue_samples == report.tissue_foreground_pixels == result.samples
        assert report.tissue_input_sha256 == result.input_sha256 and len(report.selected_ids) == 100001
        assert report.tissue_gpu_bytes <= 32*1024*1024
        output.update(objects=doc.object_count, selected=len(report.selected_ids), visible=report.visible_objects,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      gui_worker_ms=report.tissue_ms, image_gpu_bytes=report.peak_gpu_bytes,
                      tissue_gpu_bytes=report.tissue_gpu_bytes, gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
