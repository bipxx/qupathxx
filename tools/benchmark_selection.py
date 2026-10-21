"""Selection scalability: cached membership, region selection and group deletion."""
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
    parser.add_argument("--output", type=Path, default=Path("build/selection-benchmark.json"))
    args = parser.parse_args()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(128, 256 * 1024 * 1024)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1)) for i in range(100000)])
    selection = q.ObjectSelection(doc)
    _, select_all_ms = timed(selection.select_all)
    assert selection.size == 100000
    doc.clear_history()
    # Color-only revisions must not cause a 100000-ID pruning pass per query.
    doc.set_class("Tumor", 0xe02030)
    samples = []
    for _ in range(1000):
        _, elapsed = timed(lambda: (selection.size, selection.primary, selection.contains(ids[-1])))
        samples.append(elapsed)
    _, window_ms = timed(lambda: selection.select_region(q.Rect(10, 10, 10, 10)))
    assert selection.size == 100
    chosen = selection.ids
    _, classify_ms = timed(lambda: doc.classify_objects(chosen, "Tumor"))
    assert doc.class_count("Tumor") == 100 and selection.size == 100
    _, delete_ms = timed(lambda: doc.remove_objects(chosen))
    _, prune_ms = timed(lambda: selection.size)
    assert selection.size == 0 and doc.object_count == 99900
    _, undo_ms = timed(doc.undo)
    assert doc.object_count == 100000 and doc.class_count("Tumor") == 100 and selection.size == 0
    result = {
        "platform": platform.platform(), "processor": platform.processor(),
        "objects": 100000, "select_all_ms": select_all_ms,
        "cached_access_median_ms": statistics.median(samples), "cached_access_p95_ms": sorted(samples)[949],
        "window_selection_ms": window_ms, "window_objects": len(chosen),
        "classify_ms": classify_ms, "delete_ms": delete_ms, "prune_ms": prune_ms, "delete_undo_ms": undo_ms,
        "scope": "Native selection API including nanobind; cached access combines size, primary and one membership check after a color edit. Window query includes initial spatial-tree build. Bulk actions touch 100 selected roots. No GUI/all-overlay rendering claim.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
