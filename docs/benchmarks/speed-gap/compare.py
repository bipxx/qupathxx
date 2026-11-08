"""Fixed-workload speed-gap report; exact masks/CSV gate every native block."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import sys
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'candidate5-performance'))
from summarize import canonical


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def distribution(values):
    return dict(median_ms=statistics.median(values),min_ms=min(values),max_ms=max(values))


def main():
    parser=argparse.ArgumentParser();parser.add_argument('work',type=Path);args=parser.parse_args()
    work=args.work.resolve()
    names=['native-baseline','qupath-baseline','native-parallel','qupath-paired',
        'native-serial-control','native-parallel-control','native-lazy','qupath-lazy',
        'native-parallel-later','native-lazy-control','native-final','qupath-final',
        'native-final-control','qupath-final-control','native-binary','qupath-binary',
        'native-binary-control','native-selected','qupath-selected','native-selected-control',
        'native-baseline-late','qupath-selected-control']
    reports={name:read(work/name/'report.json') for name in names}
    blocks={}
    for name,report in reports.items():
        native=name.startswith('native');runs=report['runs'][1:]
        pipeline=['cell_capture','cell_detection','keep_cells','publish_cells'] if native else ['cell_detection_and_publication']
        blocks[name]=dict(source_tree=report.get('source_tree_sha256'),
            cell_pipeline=distribution([sum(r['stages'][k]['wall_ms'] for k in pipeline) for r in runs]),
            cell_pipeline_cpu=distribution([sum(r['stages'][k]['cpu_ms'] for k in pipeline) for r in runs]),
            workflow=distribution([r['workflow_ms'] for r in runs]),
            stages={k:distribution([r['stages'][k]['wall_ms'] for r in runs]) for k in runs[0]['stages']},
            process_resources=read(work/(name+'-resources.json')))
    # Byte identity across all native variants is stronger than partition equality.
    for name in names:
        if not name.startswith('native'):continue
        for i in range(4):
            for file in ['rgb.u8','nucleus.u32','body.u32','density.csv']:
                assert (work/name/str(i)/file).read_bytes()==(work/'native-baseline'/str(i)/file).read_bytes(),(name,i,file)
    exact=[]
    for native,reference in [('native-selected','qupath-selected'),('native-selected-control','qupath-selected-control')]:
        for i in range(4):
            n=work/native/str(i);r=work/reference/str(i)
            assert (n/'rgb.u8').read_bytes()==(r/'rgb.u8').read_bytes()
            a=[np.fromfile(n/(k+'.u32'),dtype='<u4').reshape(512,512) for k in ['nucleus','body']]
            b=[np.fromfile(r/(k+'.u32'),dtype='<u4').reshape(512,512) for k in ['nucleus','body']]
            assert all(np.array_equal(x,y) for x,y in zip(canonical(*a),canonical(*b)))
            nr=reports[native]['runs'][i];qr=reports[reference]['runs'][i]
            assert nr['cells']==qr['cells']==2237 and nr['area_mm2']==qr['area_mm2']
            assert (nr['pixel_width_um'],nr['pixel_height_um'])==(qr['pixel_width_um'],qr['pixel_height_um'])
            exact.append(dict(native=native,reference=reference,iteration=i,cells=2237,exact_partitions_rgb_area_calibration=True))
    original=read(work.parent/'contour-performance/final-contours.json')
    selected=read(work/'selected-contours.json');rejected=read(work/'rectangle-contours.json')
    old_hash={key:sorted({r[key]['geometry_sha256'] for r in original['runs']}) for key in ['nucleus','body']}
    for key in old_hash:
        assert old_hash[key]==sorted({r[key]['geometry_sha256'] for r in selected['runs']})
    rejected_hash={key:sorted({r[key]['geometry_sha256'] for r in rejected['runs']}) for key in old_hash}
    assert any(old_hash[key]!=rejected_hash[key] for key in old_hash)
    paired_ratios={native:blocks[native]['cell_pipeline']['median_ms']/blocks[reference]['cell_pipeline']['median_ms']
        for native,reference in [('native-baseline','qupath-baseline'),('native-selected','qupath-selected'),('native-selected-control','qupath-selected-control')]}
    result=dict(scope='Windows fixed512px ROI; each block one warmup+3 measured workflows; stage CPU and observed system load retained; no overall/save/reopen equivalent-work ratio',
        final_source=reports['native-selected']['source_revision'],final_tree=reports['native-selected']['source_tree_sha256'],
        blocks=blocks,paired_native_over_qupath_ratios=paired_ratios,exact_pairs=exact,
        exact_rgb_masks_csv_across_all_native_blocks=True,exact_ordered_geometry_preserved=True,
        original_geometry_hashes=old_hash,selected_geometry_hashes={key:sorted({r[key]['geometry_sha256'] for r in selected['runs']}) for key in old_hash},
        rejected_rectangle=dict(reason='Failed exact real-data ordered geometry identity despite small-pattern success; implementation discarded',geometry_hashes=rejected_hash),
        speed_parity_achieved=all(value<=1 for key,value in paired_ratios.items() if key!='native-baseline'),
        phase_probe=read(work/'phase-profile.json'),
        limits=['Substantial other-process CPU load and host drift; local observations, not statistical/platform/whole-slide guarantees.',
            'System counters include process startup/warmup/oracles; other-process estimate is not attribution to named apps.',
            'Native and reference saved data/statistics differ; no equivalent-work overall ratio.',
            'No new resource-only RAM comparison; lifetime RSS includes oracles.',
            'CellJob phase names are coarse: reading includes compatibility segmentation; diagnostic probe is not a kernel profile.',
            'Published candidate5 assets remain immutable; new optimizations are development-only.'])
    (work/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(ratios=paired_ratios,speed_parity_achieved=result['speed_parity_achieved'],exact_pairs=len(exact))))


if __name__=='__main__':
    main()
