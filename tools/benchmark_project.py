"""Measure native transactional persistence for 100000 objects."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def timed(action):
    start = time.perf_counter()
    value = action()
    return value, (time.perf_counter() - start) * 1000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("build/project-benchmark.json"))
    args = parser.parse_args()
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f"Object {i}", class_name="Tumor") for i in range(100000)])
    image = project.add_document(doc, "Benchmark")
    # Unique destination avoids accidentally replacing an unrelated prior run.
    path = Path("build") / f"project-benchmark-{project.identity}.qpxproj"
    _, first_ms = timed(lambda: project.save(str(path)))
    doc.rename_annotation(ids[-1], "Saved update")
    _, resave_ms = timed(lambda: project.save(str(path)))
    loaded, load_ms = timed(lambda: q.Project.open(str(path)))
    restored = loaded.document(image)
    assert restored.object_count == 100000 and restored.class_count("Tumor") == 100000
    assert restored.annotation(ids[-1]).name == "Saved update" and not loaded.dirty
    assert restored.annotation(ids[0]).geometry.equals(doc.annotation(ids[0]).geometry)
    result = {
        "platform": platform.platform(), "processor": platform.processor(), "sqlite_version": q.sqlite_version,
        "objects": 100000, "embedded_rgb_bytes": 1024*768*3,
        "initial_save_ms": first_ms, "full_resave_ms": resave_ms, "validated_load_ms": load_ms,
        "file_bytes": path.stat().st_size, "generation": loaded.generation,
        "scope": "One embedded dense image and 100000 classified unit rectangles. Saves use a full SQLite transaction with synchronous=FULL; resave rewrites canonical rows. Load decodes and validates all objects/hierarchy. Input construction excluded. Local filesystem only; no cold-storage, autosave or vendor-workflow claim.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    path.unlink()


if __name__ == "__main__":
    main()
