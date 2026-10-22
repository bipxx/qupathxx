"""Installed object-workflow timings; excludes image reading and GUI rendering."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import sys
import time
import qupathxx as q
from qupathxx import _native


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rows', type=int, default=10000)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    if not 500 <= args.rows <= 100000:
        ap.error('--rows must be 500..100000')
    started = time.perf_counter()
    doc = q.Document(q.Image(64, 64, [80]*64*64*3))
    # History is separately tested with real undo/redo fixtures. This sample
    # measures one large publication without retaining a duplicate edit corpus.
    doc.set_history_limits(0, 0)
    edits = []
    for row in range(args.rows):
        side = float(2+row % 20)
        label = ('small' if side < 10 else 'large') if row < 400 else ''
        edits.append(q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(1, 1, side, side)),
                                                  f'Cell {row}', class_name=label, kind=q.ObjectKind.CELL))
    ids = doc.execute(edits, 'Prepare benchmark cells')
    del edits
    prepare_ms = (time.perf_counter()-started)*1000
    options = q.ObjectFeatureOptions()
    options.snapshot_bytes = 128*1024*1024
    started = time.perf_counter()
    training = q.ObjectFeatureRequest(doc, ids[:400], options)
    training_table = q.extract_object_features(training)
    parameters = q.ForestParameters()
    parameters.trees, parameters.max_depth = 32, 8
    model = q.train_object_forest(training_table, parameters)
    training_ms = (time.perf_counter()-started)*1000
    started = time.perf_counter()
    request = q.ObjectFeatureRequest(doc, ids[400:], options)
    capture_wall_ms = (time.perf_counter()-started)*1000
    started = time.perf_counter()
    result = q.classify_objects(request, model)
    classification_ms = (time.perf_counter()-started)*1000
    started = time.perf_counter()
    changed = q.apply_object_classification(doc, request, result)
    publication_ms = (time.perf_counter()-started)*1000
    assert len(changed) == args.rows-400
    assert all(doc.annotation(ids[row]).class_name == ('small' if 2+row % 20 < 10 else 'large')
               for row in range(400, args.rows))
    environment = q.build_environment()
    payload = dict(schema='qpx.object-classifier-benchmark/1', rows=args.rows,
                   prediction_rows=args.rows-400, training_rows=400, features=len(model.features),
                   prepare_ms=prepare_ms, training_ms=training_ms, capture_wall_ms=capture_wall_ms,
                   capture_native_ms=request.capture_ms, snapshot_bytes=request.snapshot_bytes,
                   snapshot_limit_bytes=options.snapshot_bytes, classification_ms=classification_ms,
                   publication_ms=publication_ms, changed_objects=len(changed),
                   history_enabled=False, model_sha256=model.sha256,
                   table_sha256=result.table.sha256, backend=model.backend,
                   native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                   python=sys.version, platform=platform.platform(),
                   environment={name:getattr(environment, name) for name in
                                ['app_version','source_tree_sha256','git_revision','compiler','compiler_flags','configuration','platform']},
                   fixture='Overlapping rectangles with repeated discrete sizes; labels follow side<10. No image reads, cell detection or biological accuracy evidence.',
                   timing_scope='One call per phase; capture includes Python ID conversion, classification includes native feature extraction and prediction; publication includes class edit and Python changed-ID construction. Preparation and history policy reported separately; no GUI/hardware budget claim.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2)+'\n', encoding='utf8')
    print(json.dumps({k:payload[k] for k in ['rows','capture_wall_ms','snapshot_bytes','classification_ms','publication_ms']}))


if __name__ == '__main__':
    main()
