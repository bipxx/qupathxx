"""Measure the installed native forest engine on a reproducible numeric fixture.

No image-reading, feature-extraction, GUI or biological accuracy claim is made.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import sys
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    rng = np.random.default_rng(671)
    train = rng.normal(size=(10000, 8)).astype(np.float32)
    labels = np.digitize(train[:, 0]+train[:, 1]*.5, [-.5, .5]).astype(np.uint32)
    schema = [q.ClassifierFeature(f'f{i}', '1') for i in range(8)]
    parameters = q.ForestParameters()
    parameters.trees, parameters.max_depth, parameters.seed = 64, 10, 671
    started = time.perf_counter()
    model = q.train_forest(train, labels, schema, ['low', 'middle', 'high'], parameters)
    training_ms = (time.perf_counter()-started)*1000
    encoded = model.to_bytes()
    started = time.perf_counter()
    loaded = q.ForestModel.from_bytes(encoded)
    load_ms = (time.perf_counter()-started)*1000
    test = rng.normal(size=(80000, 8)).astype(np.float32)
    expected = np.digitize(test[:, 0]+test[:, 1]*.5, [-.5, .5])
    timings = []
    for _ in range(5):
        started = time.perf_counter()
        result = loaded.predict(test, schema)
        timings.append((time.perf_counter()-started)*1000)
    predicted = np.array([r.class_index for r in result])
    payload = dict(schema='qpx.classifier-benchmark/1', python=sys.version,
                   platform=platform.platform(), numpy=np.__version__,
                   backend=model.backend, training_rows=len(train), prediction_rows=len(test),
                   features=8, classes=3, trees=model.tree_count, nodes=model.node_count,
                   max_depth=parameters.max_depth, seed=parameters.seed,
                   training_ms=training_ms, reload_ms=load_ms, prediction_ms=timings,
                   median_prediction_ms=statistics.median(timings),
                   fixture_agreement=float(np.mean(predicted==expected)),
                   model_bytes=len(encoded), model_sha256=model.sha256,
                   training_sha256=model.training_sha256,
                   input_sha256=hashlib.sha256(test.tobytes()).hexdigest(),
                   native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                   includes='Native traversal, input snapshot and Python result construction; five sequential calls, no warm-up discarded',
                   excludes='Image access, object features, document edits, GUI and biological validation')
    environment = q.build_environment()
    payload['environment'] = {name: getattr(environment, name) for name in
                              ['app_version', 'source_tree_sha256', 'git_revision', 'compiler', 'compiler_flags', 'configuration', 'platform']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2)+'\n', encoding='utf8')
    print(json.dumps({k:payload[k] for k in ['training_ms', 'median_prediction_ms', 'fixture_agreement', 'model_bytes', 'model_sha256']}))


if __name__ == '__main__':
    main()
