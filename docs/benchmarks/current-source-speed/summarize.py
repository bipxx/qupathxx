"""Check the one captured output pair and summarize comparable processing stages."""
import hashlib
import importlib.util
import json
from pathlib import Path
import statistics
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    work = Path(sys.argv[1]).resolve()
    native, reference = load(work / 'native/report.json'), load(work / 'qupath/report.json')
    baseline = load(ROOT / 'docs/benchmarks/cell-kernel-optimization/geometry-check.json')
    # Reuse the accepted independent first-nucleus-pixel label normalization.
    path = ROOT / 'docs/benchmarks/candidate5-performance/summarize.py'
    spec = importlib.util.spec_from_file_location('partition_oracle', path)
    oracle = importlib.util.module_from_spec(spec); spec.loader.exec_module(oracle)
    n, r = work / 'native/11', work / 'qupath/29'
    assert (n / 'rgb.u8').read_bytes() == (r / 'rgb.u8').read_bytes()
    nm = [np.fromfile(n / (name + '.u32'), dtype='<u4').reshape(512, 512) for name in ('nucleus', 'body')]
    rm = [np.fromfile(r / (name + '.u32'), dtype='<u4').reshape(512, 512) for name in ('nucleus', 'body')]
    assert all(np.array_equal(a, b) for a, b in zip(oracle.canonical(*nm), oracle.canonical(*rm)))
    expected = baseline['runs'][-1]['masks']
    observed = {name: hashlib.sha256((n / (name + '.u32')).read_bytes()).hexdigest() for name in ('nucleus', 'body')}
    assert observed == expected
    allruns = native['runs'] + reference['runs']
    assert all(row['cells'] == 2237 for row in allruns)
    assert all(row['pixel_width_um'] == .499 and row['pixel_height_um'] == .499 for row in allruns)
    assert all(row['area_mm2'] == allruns[0]['area_mm2'] for row in allruns)
    assert native['runs'][-1]['rgb_sum'] == reference['runs'][-1]['rgb_sum']
    result = dict(source_tree=native['source_tree_sha256'], qupath=reference['version'],
                  roi=native['roi'], context=native['context'], cells=2237,
                  exact_rgb=True, exact_cross_engine_partitions=True, exact_retained_native_masks=True,
                  native_mask_hashes=observed, calibration_um=[.499, .499], engines={})
    for key, report, names in [('native', native, ['cell_capture','cell_detection','keep_cells','publish_cells']),
                               ('qupath', reference, ['cell_detection_and_publication'])]:
        runs = [row for row in report['runs'] if not row['warmup']]
        stages = {name: {metric: statistics.median(row['stages'][name][metric] for row in runs)
                        for metric in ('wall_ms', 'cpu_ms')} for name in runs[0]['stages']}
        pipeline = {}
        for metric in ('wall_ms','cpu_ms'):
            values = [sum(row['stages'][name][metric] for name in names) for row in runs]
            pipeline[metric] = dict(median=statistics.median(values), minimum=min(values), maximum=max(values), samples=values)
        resource = load(work / (key + '-resources.json'))
        assert resource['exit_code'] == 0
        result['engines'][key] = dict(pipeline=pipeline, stages=stages, resource=resource,
                                     final_single_archive_stages={name: value for name,value in report['runs'][-1]['stages'].items()
                                                                 if name not in runs[0]['stages']},
                                     measured_runs=len(runs), warmups=sum(row['warmup'] for row in report['runs']))
    a = result['engines']['native']['pipeline']['wall_ms']['median']
    b = result['engines']['qupath']['pipeline']['wall_ms']['median']
    result['observed_native_overhead_fraction'] = a / b - 1
    result['cell_wall_difference_ms'] = a - b
    result['limits'] = ['Six alternating nonconcurrent measured pairs after separate warmups; lifetime resources include gate waits; one ROI, no general speed claim.',
                        'Process CPU covers all engine threads and can include JVM JIT/GC; Windows counters are quantized.',
                        'Native OD/RGB statistics and scripted reference RGB reduction differ; no statistics-kernel ratio.',
                        'QPX and QPData retain different content; one save/reopen each is descriptive, not an equivalent-work or cold-disk benchmark.',
                        'No code optimization is established by this timing-only check; unchanged science qualification is reused.']
    (work / 'summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: result[key] for key in ('exact_rgb','exact_cross_engine_partitions','exact_retained_native_masks','cell_wall_difference_ms','observed_native_overhead_fraction')},indent=2))


if __name__ == '__main__':
    main()
