"""Measure validated editing and overlays on a nontrivial polygon fixture."""
import argparse
import json
import math
from pathlib import Path
import platform
import statistics
import time

import qupathxx as q


def timed(action):
    start = time.perf_counter()
    value = action()
    return value, (time.perf_counter() - start) * 1000


def polygon(cx, count):
    points = []
    for i in range(count):
        angle = 2*math.pi*i/count
        radius = 100 + 10*math.cos(8*angle)
        points.append(q.Point(cx + radius*math.cos(angle), 200 + radius*math.sin(angle)))
    return q.Roi.polygon(points)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/geometry-benchmark.json"))
    args = parser.parse_args()
    left, right = polygon(200, 4096), polygon(250, 4096)
    union_samples = []
    for _ in range(20):
        result, ms = timed(lambda: left.boolean_operation(right, q.BooleanOp.UNION))
        union_samples.append(ms)
    intersection = left.boolean_operation(right, q.BooleanOp.INTERSECTION)
    assert math.isclose(result.area + intersection.area, left.area + right.area, rel_tol=1e-12, abs_tol=1e-9)
    doc = q.Document(q.Image.demo())
    obj = doc.add_roi(left)
    doc.clear_history()
    edits = []
    for i in range(20):
        _, ms = timed(lambda: doc.edit_vertex(obj, 0, 0, q.Point(311 if i % 2 == 0 else 310, 200)))
        edits.append(ms)
    assert doc.annotation(obj).geometry.equals(left)
    record = {
        "platform": platform.platform(), "processor": platform.processor(), "geos_version": q.geos_version,
        "input_vertices_each": 4096, "union_result_vertices": sum(map(len, result.paths)),
        "union_median_ms": statistics.median(union_samples), "union_p95_ms": sorted(union_samples)[18],
        "edit_median_ms": statistics.median(edits), "edit_p95_ms": sorted(edits)[18],
        "edit_history_payload_bytes": doc.history_bytes // 20,
        "scope": "Two overlapping 4096-vertex eight-lobed radial polygons. Boolean union includes input parsing and validated output construction; vertex edits include native validation/journaling and nanobind. No vendor or QuPath parity claim.",
    }
    if args.gui:
        options = q.GuiOptions()
        options.width, options.height = 2560, 1440
        view = q.Viewport()
        view.zoom = 2
        options.start_view = view
        events = []
        for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (8, q.GuiInputKind.VERTEX_KEY)]:
            event = q.GuiInput()
            event.frame, event.kind = frame, kind
            events.append(event)
        options.inputs = events
        report = q.show(doc, frame_limit=180, options=options)
        assert report.selected_ids == [obj]
        record["viewer"] = {"framebuffer": [report.framebuffer_width, report.framebuffer_height],
                            "cpu_frame_p95_ms": report.cpu_frame_p95_ms,
                            "gl_renderer": report.gl_renderer, "gl_version": report.gl_version,
                            "vertices_with_handles": 4096, "peak_gpu_payload_bytes": report.peak_gpu_bytes}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
