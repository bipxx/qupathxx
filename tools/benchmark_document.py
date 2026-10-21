"""Measure batch import/history and local edits against a large native document."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import time

import qupathxx as q


def timed(action):
    start = time.perf_counter()
    value = action()
    return value, (time.perf_counter() - start) * 1000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--output", type=Path, default=Path("build/document-benchmark.json"))
    args = parser.parse_args()
    if not 1 <= args.objects <= 200000:
        parser.error("object count must be between 1 and 200000 for this benchmark")
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(128, 256 * 1024 * 1024)
    edits = [q.ObjectEdit.create(q.Rect(i % 1024, (i // 1024) % 768, 1, 1), f"Object {i}") for i in range(args.objects)]
    ids, create_ms = timed(lambda: doc.execute(edits, "Import objects"))
    del edits
    baseline_bytes = doc.history_bytes
    _, undo_ms = timed(doc.undo)
    assert doc.object_count == 0
    _, redo_ms = timed(doc.redo)
    assert doc.object_count == args.objects
    samples = []
    deltas = []
    for index in range(100):
        _, elapsed = timed(lambda: doc.rename_annotation(ids[-1], f"Renamed {index}"))
        samples.append(elapsed)
        deltas.append(doc.history_bytes - baseline_bytes)
        assert doc.undo()
        assert doc.annotation(ids[-1]).name == f"Object {args.objects - 1}"
    result = {
        "platform": platform.platform(), "processor": platform.processor(),
        "object_count": args.objects, "create_batch_ms": create_ms,
        "undo_batch_ms": undo_ms, "redo_batch_ms": redo_ms,
        "single_rename_median_ms": statistics.median(samples),
        "single_rename_p95_ms": sorted(samples)[94],
        "single_rename_history_payload_bytes": max(deltas),
        "batch_history_payload_bytes": baseline_bytes,
        "scope": "Native document API including nanobind call/return overhead. Descriptor construction excluded. History payload excludes tree/allocator overhead. No GUI or spatial-index performance claim.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
