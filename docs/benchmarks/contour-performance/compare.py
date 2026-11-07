"""Summarize the selected native A/B/A blocks; reuse frozen reference masks."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent/'candidate5-performance'))
from summarize import canonical


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def distribution(values):
    return dict(median_ms=statistics.median(values),min_ms=min(values),max_ms=max(values))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('work',type=Path)
    parser.add_argument('reference_work',type=Path)
    args=parser.parse_args()
    work=args.work.resolve();reference=args.reference_work.resolve()
    reports={name:read(work/name/'report.json') for name in ['native-before','native-after','native-control']}
    blocks={}
    for name,report in reports.items():
        runs=report['runs'][1:]
        blocks[name]=dict(source_tree=report['source_tree_sha256'],
            workflow=distribution([r['workflow_ms'] for r in runs]),
            stages={stage:distribution([r['stages'][stage]['wall_ms'] for r in runs]) for stage in runs[0]['stages']})
    pairs=[]
    for iteration in range(4):
        after=work/'native-after'/str(iteration)
        for name in ['native-before','native-control']:
            for file in ['rgb.u8','nucleus.u32','body.u32','density.csv']:
                assert (work/name/str(iteration)/file).read_bytes()==(after/file).read_bytes(),(name,iteration,file)
        a=[np.fromfile(after/(name+'.u32'),dtype='<u4').reshape(512,512) for name in ['nucleus','body']]
        ref=reference/'qupath-retry'/str(iteration)
        b=[np.fromfile(ref/(name+'.u32'),dtype='<u4').reshape(512,512) for name in ['nucleus','body']]
        assert (after/'rgb.u8').read_bytes()==(ref/'rgb.u8').read_bytes()
        assert all(np.array_equal(x,y) for x,y in zip(canonical(*a),canonical(*b)))
        rows=[report['runs'][iteration] for report in reports.values()]
        assert all(row['cells']==2237 for row in rows)
        assert len({(row['area_mm2'],row['pixel_width_um'],row['pixel_height_um'],tuple(row['rgb_sum'])) for row in rows})==1
        pairs.append(dict(iteration=iteration,cells=2237,exact_rgb_masks_and_csv_across_native=True,exact_qupath_partitions=True))
    probes={name:read(work/(name+'-contours.json')) for name in ['profile','candidate','final','control']}
    for key in ['nucleus','body']:
        hashes=[{r[key]['geometry_sha256'] for r in probe['runs']} for probe in probes.values()]
        assert all(value==hashes[0] for value in hashes)
    reductions={name:1-blocks['native-after']['workflow']['median_ms']/blocks[name]['workflow']['median_ms'] for name in ['native-before','native-control']}
    phases=[]
    for line in (work/'profile-contours.log').read_text(encoding='utf-8').splitlines():
        if line.startswith('CONTOUR_PROFILE '):
            phases.append({key:float(value) for key,value in re.findall(r'(\w+)=([\d.]+)',line)})
    result=dict(scope='One Windows ROI; one warmup and three measured native workflows per block; prior frozen QuPath masks reused, no new cross-engine time ratio',
        native_source=reports['native-after']['source_revision'],native_tree=reports['native-after']['source_tree_sha256'],
        blocks=blocks,workflow_time_reduction_vs_controls=reductions,exact_comparisons=pairs,
        contour_probes={name:{key:distribution([r[key]['wall_ms'] for r in probe['runs']]) for key in ['nucleus','body']} for name,probe in probes.items()},
        exact_ordered_geometry_preserved=True,instrumented_baseline_phases=phases,
        reference_report_sha256=hashlib.sha256((reference/'qupath-retry/report.json').read_bytes()).hexdigest(),
        caveats=['Host timing drift is substantial; percentages are local observations, not statistical or universal claims.',
            'Temporary phase instrumentation was removed from the candidate; the baseline source/probe is retained.',
            'No new resource-only RAM run or QuPath timing; previous memory/reference timings do not acquire this source identity.',
            'Candidate5 release files are unchanged; this is a development source optimization.'])
    (work/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(raw_tree=result['native_tree'],exact_pairs=len(pairs),reductions=reductions)))


if __name__=='__main__':
    main()
