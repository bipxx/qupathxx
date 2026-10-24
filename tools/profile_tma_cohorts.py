"""Profile captured cohort pooling with independently checked integer statistics.

Run after native builds and desktop replays finish. Every trial is retained;
these in-memory timings are not hardware, QuPath or biological acceptance.
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
    parser.add_argument('--grids', type=int, default=4)
    parser.add_argument('--rows', type=int, nargs='+', default=[1000, 10000, 100000])
    parser.add_argument('--trials', type=int, default=5)
    args = parser.parse_args()
    if not __debug__ or not 2 <= args.grids <= 32 or not 1 <= args.trials <= 20 or any(not 1 <= n <= 1000000 for n in args.rows):
        parser.error('require assertions, 2..32 grids, 1..20 trials and 1..1000000 rows')
    documents, snapshots, entries = [], {}, []
    for image in range(1, args.grids+1):
        doc = q.Document(q.Image(400, 250, [128]*400*250*3))
        options = q.TmaGridOptions()
        options.rows, options.columns = 25, 40
        options.name = f'grid-{image}'
        options.origin = q.Point(5, 5)
        options.row_step, options.column_step = q.Point(0, 10), q.Point(10, 0)
        options.width = options.height = 6
        ids = doc.create_tma_grid(options)
        doc.set_tma_case_ids({identity: f'case-{i//3}' for i, identity in enumerate(ids) if i % 17})
        doc.set_tma_missing(ids[::10], True)
        doc.remove_annotation(ids[-1])
        documents.append(doc)
        snapshots[image] = doc.tma_grid()
        entries.extend((image, core) for core in snapshots[image].cores)
    manifest = q.tma_cohort_manifest_csv(snapshots)
    manifest_hash = hashlib.sha256(manifest.encode('utf-8')).hexdigest()
    source_rows = list(csv.DictReader(io.StringIO(manifest)))
    assert len(source_rows) == args.grids
    for source in source_rows:
        grid = snapshots[int(source['image_id'])]
        assert source['metadata_csv_sha256'] == hashlib.sha256(q.tma_metadata_csv(grid).encode('utf-8')).hexdigest()
        assert int(source['grid_slots']) == 1000 and int(source['grid_vacant_slots']) == 1
    measurements = []
    for count in args.rows:
        expected = {}
        for image, core in entries:
            case = core.tma_core.case_id
            key = (case, 0, 0) if case else ('', image, core.id)
            stats = expected.setdefault(key, dict(cores=0, missing=0, rows=0, ignored=0, values=0, weights=0, weighted=0,
                                                  measured=set(), grids=set(), minimum=None, maximum=None))
            stats['cores'] += 1
            stats['missing'] += core.tma_core.missing
            stats['grids'].add(image)
        stream = io.StringIO(newline='')
        writer = csv.writer(stream)
        writer.writerow(['image_id', 'source_object_id', 'source_object_revision', 'value', 'weight'])
        for i in range(count):
            image, core = entries[i % len(entries)]
            value, weight = i % 21 - 10, i % 4
            writer.writerow([image, core.id, core.revision, value, weight])
            case = core.tma_core.case_id
            stats = expected[(case, 0, 0) if case else ('', image, core.id)]
            if core.tma_core.missing:
                stats['ignored'] += 1
            else:
                stats['rows'] += 1
                stats['values'] += value
                stats['weights'] += weight
                stats['weighted'] += value*weight
                stats['measured'].add((image, core.id))
                stats['minimum'] = value if stats['minimum'] is None else min(value, stats['minimum'])
                stats['maximum'] = value if stats['maximum'] is None else max(value, stats['maximum'])
        table = stream.getvalue()
        input_hash = hashlib.sha256(table.encode('utf-8')).hexdigest()
        samples, output_hash = [], None
        for trial in range(args.trials+1):
            start = time.perf_counter()
            output = q.tma_cohort_case_summary_csv(snapshots, table, 'value', 'weight')
            elapsed = (time.perf_counter()-start)*1000
            digest = hashlib.sha256(output.encode('utf-8')).hexdigest()
            if output_hash is None:
                output_hash = digest
            assert digest == output_hash
            actual = list(csv.DictReader(io.StringIO(output)))
            assert len(actual) == len(expected)
            for row in actual:
                key = (row['case_id'], int(row['unassigned_image_id'] or 0), int(row['unassigned_core_id'] or 0))
                stats = expected[key]
                assert row['source_manifest_sha256'] == manifest_hash and row['input_table_sha256'] == input_hash
                for column, value in [('cores', stats['cores']), ('missing_cores', stats['missing']),
                                      ('present_cores', stats['cores']-stats['missing']), ('measurement_rows', stats['rows']),
                                      ('ignored_missing_rows', stats['ignored']), ('measured_cores', len(stats['measured'])),
                                      ('grids', len(stats['grids'])), ('cohort_grids', args.grids),
                                      ('cohort_slots', args.grids*1000), ('cohort_vacant_slots', args.grids)]:
                    assert int(row[column]) == value
                if stats['rows']:
                    for column, value in [('value_sum', stats['values']), ('weight_sum', stats['weights']),
                                          ('weighted_sum', stats['weighted']), ('minimum', stats['minimum']), ('maximum', stats['maximum'])]:
                        assert float(row[column]) == value
                    assert abs(float(row['mean'])-stats['values']/stats['rows']) <= 1e-12
                    if stats['weights']:
                        assert abs(float(row['weighted_mean'])-stats['weighted']/stats['weights']) <= 1e-12
                    else:
                        assert row['weighted_mean'] == ''
                else:
                    assert row['value_sum'] == row['mean'] == row['weighted_mean'] == row['minimum'] == row['maximum'] == ''
            samples.append(dict(trial=trial, warmup=trial == 0, milliseconds=elapsed, output_sha256=digest))
        measurements.append(dict(rows=count, input_bytes=len(table.encode('utf-8')), input_sha256=input_hash,
                                 median_ms=statistics.median(sample['milliseconds'] for sample in samples if not sample['warmup']), trials=samples))
    environment = q.build_environment()
    report = dict(schema='qpx.tma-cohort-profile/1', grids=args.grids, cores=len(entries), cases=len(expected),
                  independent_integer_statistics_verified=True, source_manifest_sha256=manifest_hash,
                  tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), measurements=measurements,
                  native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                  environment={key: getattr(environment, key) for key in ['source_tree_sha256', 'git_revision', 'compiler', 'configuration', 'platform']},
                  performance_accepted=False, release_ready=False,
                  scope='Captured multi-grid raw-row pooling with colliding local IDs, shared cases, isolated unassigned cores, missing cores, vacancies and zero weights. Native CSV parsing, statistics, manifest creation, hashing and formatting are timed; Python fixture/oracle work is excluded. All observations include explicit warmups. No peak-memory, hardware-wide speedup, QuPath or biological acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes((json.dumps(report, indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(grids=args.grids, cores=len(entries), measurements=measurements)))


if __name__ == '__main__':
    main()
