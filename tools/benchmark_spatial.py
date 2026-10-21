"""Measure 100k-object broad-phase queries and optional real viewport culling."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--select-all", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/spatial-benchmark.json"))
    args = parser.parse_args()
    if args.select_all and not args.gui:
        parser.error("--select-all requires --gui")
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(128, 256 * 1024 * 1024)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f"Object {i}") for i in range(100000)])
    start = time.perf_counter()
    assert len(doc.query_region(q.Rect(0, 0, 1024, 768))) == 100000
    build_ms = (time.perf_counter() - start) * 1000
    samples = []
    for i in range(1000):
        start = time.perf_counter()
        hits = doc.query_region(q.Rect(100 + i % 700, 20 + i % 50, 10, 10))
        samples.append((time.perf_counter() - start) * 1000)
        assert len(hits) == 144  # Inclusive AABB boundaries include neighboring boxes.
    builds = doc.spatial_stats.builds
    doc.update_rectangle(ids[0], q.Rect(1000, 700, 1, 1))
    assert not doc.pick(q.Point(.5, .5))
    assert doc.pick(q.Point(1000.5, 700.5)) == [ids[0]]
    assert doc.spatial_stats.builds == builds
    doc.undo()
    result = {
        "platform": platform.platform(), "processor": platform.processor(),
        "geos_version": q.geos_version,
        "objects": doc.object_count, "first_build_and_full_query_ms": build_ms,
        "query_median_ms": statistics.median(samples), "query_p95_ms": sorted(samples)[949],
        "queries": len(samples), "hits_per_query": 144, "builds_after_edit_and_undo": doc.spatial_stats.builds,
        "scope": "Rectangular grid, bounds-only queries including nanobind return conversion. First query includes lazy BVH construction and full-result conversion. Single edit uses a delta overlay.",
    }
    if args.gui:
        options = q.GuiOptions()
        options.width, options.height = 2560, 1440
        view = q.Viewport()
        view.zoom, view.origin = 64, q.Point(-32000, -3000)
        options.start_view = view
        if args.select_all:
            event = q.GuiInput()
            event.frame, event.kind = 4, q.GuiInputKind.SELECT_ALL_KEY
            options.inputs = [event]
        report = q.show(doc, frame_limit=180, options=options)
        result["viewer"] = {
            "framebuffer": [report.framebuffer_width, report.framebuffer_height],
            "cpu_frame_p95_ms": report.cpu_frame_p95_ms, "visible_objects": report.visible_objects,
            "peak_gpu_payload_bytes": report.peak_gpu_bytes, "pending_tiles": report.pending_tiles,
            "gl_renderer": report.gl_renderer,
            "gl_version": report.gl_version,
            "selected_objects": len(report.selected_ids),
            "scope": "Cached narrow viewport of 100000 stored rectangles; no claim for fitting all overlays simultaneously.",
        }
        assert 0 < report.visible_objects < 2000 and report.pending_tiles == 0
        assert len(report.selected_ids) == (100000 if args.select_all else 0)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
