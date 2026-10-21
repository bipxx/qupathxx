"""Measure shared color edits and class rename on a large classified document."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import time

import qupathxx as q


def timed(action):
    start = time.perf_counter()
    result = action()
    return result, (time.perf_counter() - start) * 1000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--output", type=Path, default=Path("build/classes-benchmark.json"))
    args = parser.parse_args()
    if not 1 <= args.objects <= 200000:
        parser.error("objects must be between 1 and 200000")
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(128, 256 * 1024 * 1024)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1024, i // 1024, 1, 1),
                       f"Object {i}", class_name="Tumor") for i in range(args.objects)])
    doc.query_region(q.Rect(0, 0, 1024, 768))
    builds = doc.spatial_stats.builds
    revision = doc.annotation(ids[-1]).revision
    doc.clear_history()
    samples = []
    for i in range(100):
        _, elapsed = timed(lambda: doc.set_class("Tumor", 0xe02030 if i % 2 else 0x2040e0))
        samples.append(elapsed)
    color_payload = doc.history_bytes // 100
    assert doc.annotation(ids[-1]).revision == revision
    doc.clear_history()
    _, rename_ms = timed(lambda: doc.rename_class("Tumor", "Positive"))
    rename_payload = doc.history_bytes
    assert doc.class_count("Positive") == args.objects and doc.class_count("Tumor") == 0
    _, undo_ms = timed(doc.undo)
    assert doc.annotation(ids[-1]).class_name == "Tumor"
    _, redo_ms = timed(doc.redo)
    assert doc.annotation(ids[-1]).class_name == "Positive"
    doc.query_region(q.Rect(0, 0, 1024, 768))
    assert doc.spatial_stats.builds == builds and doc.spatial_stats.dirty_entries == 0
    result = {
        "platform": platform.platform(), "processor": platform.processor(),
        "object_count": args.objects,
        "color_median_ms": statistics.median(samples), "color_p95_ms": sorted(samples)[94],
        "color_history_payload_bytes": color_payload,
        "rename_ms": rename_ms, "rename_undo_ms": undo_ms, "rename_redo_ms": redo_ms,
        "rename_history_payload_bytes": rename_payload, "spatial_builds": builds,
        "scope": "Native API including nanobind. Shared color edits touch no objects. Rename scans all objects and journals assignments. Bounds index reused. No rendering, complex topology or analysis claim.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
