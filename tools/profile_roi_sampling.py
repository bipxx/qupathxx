"""Paired native ROI/tissue sampling observations; no hardware acceptance."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

WORKLOADS = [(shape, size) for shape in ['square-hole', 'complex'] for size in [256, 1024]] + [
    ('dense-triangle', 1024), ('edge-strip', 1024)]

def write(path, value):
    path.write_bytes((json.dumps(value, indent=2, allow_nan=False)+'\n').encode('utf-8'))


def child(args):
    import numpy as np
    import qupathxx as q
    from qupathxx import _native

    size = args.size
    y, x = np.mgrid[:size, :size]
    rgb = np.stack([(x+3*y)%241+1, (5*x+y)%241+1, (x+7*y)%241+1], axis=2).astype(np.uint8)
    doc = q.Document(q.Image.from_numpy(rgb))
    if args.shape == 'square-hole':
        def ring(a, b):
            return [q.Point(a, a), q.Point(b, a), q.Point(b, b), q.Point(a, b)]
        roi = q.Roi.polygon(ring(4, size-4), [ring(size//3, 2*size//3)])
    elif args.shape == 'dense-triangle':
        # Quarter-coordinate parity prevents pixel centers lying on the
        # diagonal, whose bounding box covers the complete broad-phase grid.
        roi = q.Roi.polygon([q.Point(4.25, 4.25), q.Point(size-4.25, 4.25),
                             q.Point(4.25, size-4.5)])
    elif args.shape == 'edge-strip':
        # Every sampled center lies on the included lower horizontal boundary.
        roi = q.Roi.polygon([q.Point(0, .5), q.Point(size, .5),
                             q.Point(size, 1.5), q.Point(0, 1.5)])
    else:
        # Integer sample centers do not intentionally lie on these edges.
        # Exact output equality below checks that assumption in every process.
        roi = q.Roi.polygon([q.Point(size*.5+.123+size*.45*math.cos(2*math.pi*i/4096),
                                     size*.5+.321+size*.41*math.sin(2*math.pi*i/4096))
                             for i in range(4096)])
    obj = doc.add_roi(roi)
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    policies = dict(coverage=q.RoiSamplingPolicy.GEOMETRY_COVERAGE,
                    winding=q.RoiSamplingPolicy.POLYGON_WINDING)
    order = ['winding', 'coverage'] if args.winding_first else ['coverage', 'winding']

    def roi_run(arm):
        return q.analyze(q.AnalysisRequest(doc, obj, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS,
                                          tile_size=256, sampling_policy=policies[arm]))

    def tissue_run(arm):
        p = q.TissueParameters()
        p.threshold = 0; p.level = 0; p.tile_size = 256; p.work_bytes = 256*1024*1024
        p.sampling_policy = policies[arm]
        return q.detect_tissue(q.TissueRequest(doc, obj, transform, p))

    timings = {}; identities = {}; budgets = {}
    for workflow, execute in [('roi', roi_run), ('tissue', tissue_run)]:
        for arm in order:
            execute(arm)
        values = {}; elapsed = {}
        for arm in order:
            elapsed[arm] = []
            for _ in range(3):
                start = time.perf_counter_ns()
                value = execute(arm)
                elapsed[arm].append((time.perf_counter_ns()-start)/1e6)
            if workflow == 'roi':
                values[arm] = dict(samples=value.samples, means=list(value.means))
            else:
                values[arm] = dict(samples=value.samples, foreground=value.foreground_pixels,
                                   components=value.component_count,
                                   labels_sha256=hashlib.sha256(value.labels).hexdigest())
                budgets[arm] = value.estimated_work_bytes
        assert values['coverage'] == values['winding'], (workflow, values)
        timings[workflow] = elapsed
        identities[workflow] = values['coverage']
    assert budgets['coverage'] == budgets['winding']
    return dict(schema='qpx.roi-sampling-observation/1', size=size, shape=args.shape, order=order,
                timed_repetitions=3, request_and_analysis_ms=timings, exact_outputs=identities,
                tissue_work_budget_bytes=256*1024*1024, estimated_tissue_work_bytes=budgets['coverage'],
                native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                source_tree_sha256=q.build_environment().source_tree_sha256,
                build_git_revision=q.build_environment().git_revision,
                script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                platform=platform.platform(), python=sys.version, performance_accepted=False)


def profile(destination):
    destination.mkdir(parents=True, exist_ok=False)
    cases = []
    for shape, size in WORKLOADS:
        samples = []
        for index in range(9):
            path = destination/f'{shape}-{size}-{index}.json'
            command = [sys.executable, '-I', str(Path(__file__).absolute()), '--child-output',
                       str(path), '--shape', shape, '--size', str(size)]
            if index % 2:
                command.append('--winding-first')
            subprocess.run(command, check=True)
            result = json.loads(path.read_text(encoding='utf-8'))
            if index:
                samples.append(result)
        assert len({s['source_tree_sha256'] for s in samples}) == 1
        assert len({s['native_sha256'] for s in samples}) == 1
        assert len({json.dumps(s['exact_outputs'], sort_keys=True) for s in samples}) == 1
        summary = {}
        for workflow in ['roi', 'tissue']:
            paired = {arm: [statistics.median(s['request_and_analysis_ms'][workflow][arm])
                            for s in samples] for arm in ['coverage', 'winding']}
            paired['winding_minus_coverage'] = [w-c for w, c in zip(paired['winding'], paired['coverage'], strict=True)]
            summary[workflow] = {arm: dict(median=statistics.median(values), minimum=min(values), maximum=max(values))
                                 for arm, values in paired.items()}
        cases.append(dict(shape=shape, size=size, summary_ms=summary, samples=samples))
    write(destination/'summary.json', dict(schema='qpx.roi-sampling-profile/1', performance_accepted=False,
          protocol='One excluded fresh-process warmup and eight retained paired fresh processes per case; four each first-arm order. Per workflow, each child warms both arms then times three request-and-analysis calls per arm. Summaries compare per-child medians; all raw times are retained.',
          limitations=['Synthetic dense RGB natural-grid ROIs only; no slide decoding, biological or GUI throughput claim.',
                       'Exact output equality is required. Edge-strip intentionally samples the included lower boundary; other cases avoid intentional boundary centers.',
                       'Work estimates are not RSS; system contention, allocation and library effects are uncontrolled.',
                       'No universal speedup, performance parity or hardware-budget acceptance.'], cases=cases))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--child-output', type=Path)
    parser.add_argument('--shape', choices=['square-hole', 'complex', 'dense-triangle', 'edge-strip'], default='square-hole')
    parser.add_argument('--size', type=int, choices=[256, 1024], default=256)
    parser.add_argument('--winding-first', action='store_true')
    parser.add_argument('--output-directory', type=Path)
    args = parser.parse_args()
    if args.child_output:
        write(args.child_output, child(args))
    elif args.output_directory:
        profile(args.output_directory)
    else:
        parser.error('provide --output-directory or --child-output')


if __name__ == '__main__':
    main()
