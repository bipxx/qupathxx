"""Measure installed native case aggregation with independently checked results.

Run after builds and desktop replays finish. Timings are observations, not a
supported hardware target or a comparison against QuPath.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import statistics
import time

import qupathxx as q
from qupathxx import _native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rows', type=int, nargs='+', default=[1000, 10000, 100000])
    parser.add_argument('--trials', type=int, default=5)
    args = parser.parse_args()
    if args.trials < 1 or any(n < 1 or n > 1000000 for n in args.rows):
        parser.error('require 1..1000000 rows and positive trial count')
    doc = q.Document(q.Image(1024, 1024, [128]*1024*1024*3))
    options = q.TmaGridOptions()
    options.rows = options.columns = 100
    options.origin = q.Point(5, 5)
    options.row_step, options.column_step = q.Point(0, 10), q.Point(10, 0)
    options.width = options.height = 6
    ids = doc.create_tma_grid(options)
    doc.set_tma_case_ids({identity: f'case-{i//3}' for i, identity in enumerate(ids)})
    doc.set_tma_missing(ids[::10], True)
    snapshot = doc.tma_grid()
    cores = snapshot.cores
    measurements = []
    for count in args.rows:
        stream = io.StringIO(newline='')
        writer = csv.writer(stream)
        writer.writerow(['source_object_id', 'source_object_revision', 'value', 'weight'])
        expected = {}
        for core in cores:
            key = core.tma_core.case_id
            stats = expected.setdefault(key, dict(cores=0, missing=0, rows=0, ignored=0, values=0, weights=0, weighted=0, measured=set()))
            stats['cores'] += 1
            stats['missing'] += core.tma_core.missing
        for i in range(count):
            core = cores[i % len(cores)]
            value, weight = i % 21 - 10, i % 4 + 1
            writer.writerow([core.id, core.revision, value, weight])
            stats = expected[core.tma_core.case_id]
            if core.tma_core.missing:
                stats['ignored'] += 1
            else:
                stats['rows'] += 1
                stats['values'] += value
                stats['weights'] += weight
                stats['weighted'] += value*weight
                stats['measured'].add(core.id)
        table = stream.getvalue()
        samples = []
        output_digest = None
        for trial in range(args.trials+1):
            start = time.perf_counter()
            output = q.tma_case_summary_csv(snapshot, table, 'value', 'weight')
            elapsed = (time.perf_counter()-start)*1000
            digest = hashlib.sha256(output.encode()).hexdigest()
            if output_digest is None:
                output_digest = digest
            assert digest == output_digest
            result = list(csv.DictReader(io.StringIO(output)))
            assert len(result) == len(expected)
            for row in result:
                stats = expected[row['case_id']]
                for column, value in [('cores', stats['cores']), ('missing_cores', stats['missing']), ('measurement_rows', stats['rows']), ('ignored_missing_rows', stats['ignored']), ('measured_cores', len(stats['measured']))]:
                    assert int(row[column]) == value
                if stats['rows']:
                    for column, value in [('value_sum', stats['values']), ('weight_sum', stats['weights']), ('weighted_sum', stats['weighted'])]:
                        assert float(row[column]) == value
                    for column, value in [('mean', stats['values']/stats['rows']), ('weighted_mean', stats['weighted']/stats['weights'])]:
                        assert abs(float(row[column])-value) <= 1e-12
                else:
                    assert row['mean'] == row['weighted_mean'] == row['value_sum'] == ''
            samples.append(dict(trial=trial, warmup=trial == 0, milliseconds=elapsed, output_sha256=digest))
        measurements.append(dict(rows=count, input_bytes=len(table.encode()), input_sha256=hashlib.sha256(table.encode()).hexdigest(),
                                 median_ms=statistics.median(s['milliseconds'] for s in samples if not s['warmup']), trials=samples))
    environment = q.build_environment()
    report = dict(schema='qpx.tma-case-profile/1', cores=len(ids), cases=len(expected),
                  independent_integer_statistics_verified=True, measurements=measurements,
                  native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                  environment={k: getattr(environment, k) for k in ['source_tree_sha256', 'git_revision', 'compiler', 'configuration', 'platform']},
                  scope='Synthetic case pooling only. Includes native CSV parsing, aggregation, hashing and formatting; excludes input creation and Python oracle. One explicit warmup per size. No peak-memory, hardware, QuPath, biological or speedup acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes((json.dumps(report, indent=2)+'\n').encode())
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
