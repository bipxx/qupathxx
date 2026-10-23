"""Installed native TMA size limits, immutable snapshots and optional UI clipping."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import time

import qupathxx as q
from qupathxx import _native


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--capture', default='')
    args = parser.parse_args()
    doc = q.Document(q.Image(1024, 1024, [128]*1024*1024*3))
    options = q.TmaGridOptions()
    options.rows = options.columns = 100
    options.origin = q.Point(5, 5)
    options.row_step, options.column_step = q.Point(0, 10), q.Point(10, 0)
    options.width = options.height = 6
    start = time.perf_counter()
    ids = doc.create_tma_grid(options)
    create_ms = (time.perf_counter()-start)*1000
    start = time.perf_counter()
    held = doc.tma_grid()
    snapshot_ms = (time.perf_counter()-start)*1000
    start = time.perf_counter()
    doc.set_tma_missing(ids[::10], True)
    edit_ms = (time.perf_counter()-start)*1000
    snapshot = doc.tma_grid()
    start = time.perf_counter()
    text = q.tma_metadata_csv(snapshot)
    csv_ms = (time.perf_counter()-start)*1000
    rows = list(csv.DictReader(io.StringIO(text)))
    assert len(ids) == len(rows) == 10000 and len(doc.tma_core_ids()) == 9000
    assert sum(row['missing'] == '1' for row in rows) == 1000
    assert not any(core.tma_core.missing for core in held.cores)
    assert all(snapshot.core(i//100, i%100).id == ids[i] for i in range(10000))
    epoch = doc.tma_revision
    doc.add_rectangle(q.Rect(0, 0, 1, 1))
    assert doc.tma_revision == epoch
    gui = None
    if args.gui:
        ui = q.GuiOptions()
        ui.tma_open = True
        ui.width, ui.height = 1280, 1000
        report = q.show(doc, frame_limit=80, options=ui, capture_path=args.capture)
        assert not report.tma_error and report.tma_cores == 10000
        assert 0 < report.tma_catalog_max_step <= 512 and report.ui_overflow_items == 0
        gui = dict(frames=report.frames, catalog_max_step=report.tma_catalog_max_step,
                   ui_overflow=report.ui_overflow_items, cpu_frame_p95_ms=report.cpu_frame_p95_ms)
    environment = q.build_environment()
    result = dict(schema='qpx.tma-benchmark/1', cores=len(ids), missing=1000,
                  create_ms=create_ms, snapshot_ms=snapshot_ms, edit_1000_ms=edit_ms,
                  csv_ms=csv_ms, csv_bytes=len(text.encode()), snapshot_bytes=snapshot.payload_bytes,
                  immutable_snapshot_verified=True, unrelated_edit_kept_tma_revision=True,
                  gui=gui, native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                  environment={k: getattr(environment, k) for k in ['source_tree_sha256', 'git_revision', 'compiler', 'configuration', 'platform']},
                  scope='Synthetic 100x100 grid on a shared workstation. Timings are observations, not hardware acceptance. Snapshot bytes are accounted native payload, not measured RSS. No biological or QuPath parity acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
