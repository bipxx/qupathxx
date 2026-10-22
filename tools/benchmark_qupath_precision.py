"""One-time foreign ellipse import cost; no per-frame or QuPath speed claim."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import platform
import time

import qupathxx as q
from qupathxx import _native


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--objects', type=int, default=1000)
    parser.add_argument('--output', type=Path, default=Path('build/qupath-precision-benchmark.json'))
    args = parser.parse_args()
    assert 1 <= args.objects <= 10000
    root = Path(__file__).resolve().parents[1]
    case = json.loads((root/'tests/fixtures/qupath-0.7.0/upstream/edge-cases.json').read_text(encoding='utf8'))['cases'][0]
    feature = json.loads(case['geojson'])['features'][0]
    coordinates = feature['geometry']['coordinates'][0]
    x, y = min(p[0] for p in coordinates), min(p[1] for p in coordinates)
    raw = [x, y, max(p[0] for p in coordinates)-x, max(p[1] for p in coordinates)-y]
    features = []
    for i in range(args.objects):
        item = copy.deepcopy(feature)
        item['id'] = f'00000000-0000-0000-0000-{i+1:012d}'
        item['properties']['name'] = f'Thin {i}'
        features.append(item)
    text = json.dumps(dict(type='FeatureCollection', features=features), separators=(',', ':'))
    result = dict(schema='qpx.qupath-precision-benchmark/1', platform=platform.platform(),
                  native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                  source_tree_sha256=q.build_environment().source_tree_sha256,
                  objects=args.objects, input_bytes=len(text.encode()), repetitions=5,
                  measurement_scope='one synchronous native import including parse/validation/publication; input/document creation and post-import checks excluded')
    for matching in [False, True]:
        samples = []
        for _ in range(5):
            doc = q.Document(q.Image(256, 256, [32]*256*256*3))
            doc.set_history_limits(0, 0)
            start = time.perf_counter()
            report = q.import_geojson(doc, text, assume_pixels=True, allow_losses=True, qupath_ellipse_precision=matching)
            samples.append((time.perf_counter()-start)*1000)
            assert doc.object_count == report.features == args.objects
            for obj in [doc.annotation(1), doc.annotation(args.objects)]:
                b = obj.geometry.bounds
                assert [b.x, b.y, b.width, b.height] == (case['decoded_geometry']['bounds'] if matching else raw)
            changed = sum(i.count for i in report.issues if i.code == 'qupath_ellipse_precision_changed')
            assert changed == (args.objects if matching else 0)
        result['matching' if matching else 'raw'] = dict(samples_ms=samples, median_ms=sorted(samples)[2], maximum_ms=max(samples))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
