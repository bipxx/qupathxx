"""Interleaved engine observations with exact outputs and balanced order."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-python', type=Path, required=True)
    parser.add_argument('--candidate-python', type=Path, required=True)
    parser.add_argument('--output-directory', type=Path, required=True)
    args = parser.parse_args()
    profiler = Path(__file__).resolve().with_name('profile_roi_sampling.py')
    spec = importlib.util.spec_from_file_location('qpx_roi_profiler', profiler)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    destination = args.output_directory
    destination.mkdir(parents=True, exist_ok=False)
    interpreters = dict(baseline=args.baseline_python, candidate=args.candidate_python)
    cases = []
    for shape, size in module.WORKLOADS:
        retained = []; excluded = None
        for index in range(9):
            order = ['candidate', 'baseline'] if (index//2) % 2 else ['baseline', 'candidate']
            pair = dict(engine_order=order, observations={})
            for engine in order:
                path = destination/f'{shape}-{size}-{index}-{engine}.json'
                command = [str(interpreters[engine]), '-I', str(profiler), '--child-output', str(path),
                           '--shape', shape, '--size', str(size)]
                if index % 2:
                    command.append('--winding-first')
                subprocess.run(command, check=True)
                pair['observations'][engine] = json.loads(path.read_text(encoding='utf-8'))
            a, b = [pair['observations'][engine] for engine in ['baseline', 'candidate']]
            assert a['exact_outputs'] == b['exact_outputs']
            assert a['script_sha256'] == b['script_sha256']
            if index:
                retained.append(pair)
            else:
                excluded = pair
        assert sum(p['engine_order'][0] == 'baseline' for p in retained) == 4
        combinations = Counter((p['engine_order'][0], p['observations']['baseline']['order'][0]) for p in retained)
        assert len(combinations) == 4 and set(combinations.values()) == {2}
        summary = {}
        for workflow in ['roi', 'tissue']:
            values = {engine: {arm: [statistics.median(p['observations'][engine]['request_and_analysis_ms'][workflow][arm])
                                     for p in retained] for arm in ['coverage', 'winding']}
                      for engine in interpreters}
            metrics = {}
            for engine in interpreters:
                for arm in ['coverage', 'winding']:
                    metrics[engine+'_'+arm] = values[engine][arm]
                metrics[engine+'_overhead'] = [w-c for w, c in zip(values[engine]['winding'], values[engine]['coverage'], strict=True)]
            metrics['paired_winding_change'] = [c-b for c,b in zip(values['candidate']['winding'], values['baseline']['winding'], strict=True)]
            metrics['paired_overhead_change'] = [c-b for c,b in zip(metrics['candidate_overhead'], metrics['baseline_overhead'], strict=True)]
            summary[workflow] = {key: dict(median=statistics.median(v), minimum=min(v), maximum=max(v)) for key,v in metrics.items()}
        cases.append(dict(shape=shape, size=size, summary_ms=summary, excluded_warmup=excluded, pairs=retained))
        print(shape, size, {key: value['paired_winding_change']['median'] for key,value in summary.items()}, flush=True)
    records = [p['observations'][engine] for c in cases for p in c['pairs'] for engine in interpreters]
    for engine in interpreters:
        own = [p['observations'][engine] for c in cases for p in c['pairs']]
        assert len({r['native_sha256'] for r in own}) == len({r['source_tree_sha256'] for r in own}) == 1
    assert len({r['script_sha256'] for r in records}) == 1
    report = dict(schema='qpx.roi-sampling-engine-comparison/1', performance_accepted=False,
                  protocol='Six workloads; one excluded paired warmup and eight retained engine pairs each. Engine order and within-process policy order cross-balanced twice per combination. Per child: both policies warmed, three timed calls each, ROI and tissue workflows. Exact numeric outputs required across policies and engines.',
                  limitations=['Shared workstation, warm synthetic dense RGB inputs only; no universal speedup or hardware acceptance.',
                               'Cold initialization, process RSS, slide decoding, biological and GPU/frame budgets remain outside this measurement.'],
                  profiler_sha256=hashlib.sha256(profiler.read_bytes()).hexdigest(),
                  comparison_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), cases=cases)
    module.write(destination/'summary.json', report)


if __name__ == '__main__':
    main()
