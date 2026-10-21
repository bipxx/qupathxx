"""Measure deterministic native interchange on classified unit rectangles."""
import argparse
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def timed(action):
    start = time.perf_counter()
    result = action()
    return result, (time.perf_counter() - start) * 1000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--output", type=Path, default=Path("build/interchange-benchmark.json"))
    args = parser.parse_args()
    source = q.Document(q.Image.demo())
    source.set_history_limits(0, 0)
    ids = source.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f"Object {i}", class_name="Tumor") for i in range(args.objects)])
    source.set_class("Tumor", 0x123456)
    exported, export_ms = timed(lambda: q.export_geojson(source))
    csv, csv_ms = timed(lambda: q.measurements_csv(source))
    target = q.Document(q.Image.demo())
    target.set_history_limits(0, 0)
    imported, import_ms = timed(lambda: q.import_geojson(target, exported.text))
    assert imported.imported_ids == ids and target.class_count("Tumor") == args.objects
    assert target.object_class("Tumor").color == 0x123456
    assert target.annotation(ids[-1]).name == f"Object {args.objects-1}"
    assert target.annotation(ids[-1]).geometry.equals(source.annotation(ids[-1]).geometry)
    result = dict(platform=platform.platform(), processor=platform.processor(), objects=args.objects,
                  geojson_export_ms=export_ms, geojson_import_ms=import_ms, csv_export_ms=csv_ms,
                  geojson_bytes=len(exported.text.encode()), csv_bytes=len(csv.encode()),
                  scope="One dense image; classified unit rectangles, no hierarchy or intensity reads. Native serialization/parsing plus Python result conversion; construction and filesystem IO excluded. History disabled. No complex-shape or QuPath runtime parity claim.")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
