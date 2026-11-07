"""Report the fixed workload; paired detector ratios require identical partitions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import numpy as np


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def stages(report):
    measured=report['runs'][1:]
    return {name:dict(median_ms=statistics.median(r['stages'][name]['wall_ms'] for r in measured),
        min_ms=min(r['stages'][name]['wall_ms'] for r in measured),
        max_ms=max(r['stages'][name]['wall_ms'] for r in measured)) for name in measured[0]['stages']}


def canonical(nucleus,body):
    labels,first=np.unique(nucleus,return_index=True)
    order=labels[np.argsort(first)]
    order=order[order!=0]
    lut=np.zeros(int(max(nucleus.max(),body.max()))+1,dtype=np.uint32)
    lut[order]=np.arange(1,len(order)+1,dtype=np.uint32)
    assert np.all(lut[body[body!=0]]!=0)
    return lut[nucleus],lut[body]


def main():
    parser=argparse.ArgumentParser();parser.add_argument('work',type=Path);args=parser.parse_args()
    work=args.work.resolve();native=load(work/'native-geometry/report.json');reference=load(work/'qupath-retry/report.json')
    pairs=[]
    for iteration in range(4):
        a=work/'native-geometry'/str(iteration);b=work/'qupath-retry'/str(iteration)
        assert (a/'rgb.u8').read_bytes()==(b/'rgb.u8').read_bytes()
        native_masks=[np.fromfile(a/(name+'.u32'),dtype='<u4').reshape(512,512) for name in ['nucleus','body']]
        ref_masks=[np.fromfile(b/(name+'.u32'),dtype='<u4').reshape(512,512) for name in ['nucleus','body']]
        nc=canonical(*native_masks);rc=canonical(*ref_masks)
        comparison={}
        for name,n,r,cn,cr in zip(['nucleus','body'],native_masks,ref_masks,nc,rc):
            foreground=(n!=0)!=(r!=0)
            yy,xx=np.nonzero(foreground)
            border=np.minimum.reduce([xx,yy,511-xx,511-yy]) if len(xx) else np.empty(0)
            comparison[name]=dict(partition_equal=bool(np.array_equal(cn,cr)),
                foreground_mismatched_pixels=int(foreground.sum()),
                mismatches_within32px_border=int(np.count_nonzero(border<32)),
                foreground_iou=float(np.count_nonzero((n!=0)&(r!=0))/max(1,np.count_nonzero((n!=0)|(r!=0)))))
        nr=native['runs'][iteration];qr=reference['runs'][iteration]
        assert nr['rgb_sum']==qr['rgb_sum'] and nr['pixel_width_um']==qr['pixel_width_um'] and nr['pixel_height_um']==qr['pixel_height_um']
        pairs.append(dict(iteration=iteration,native_cells=nr['cells'],qupath_cells=qr['cells'],
            cell_counts_equal=nr['cells']==qr['cells'],area_equal=nr['area_mm2']==qr['area_mm2'],
            density_relative_difference=abs(nr['density']-qr['density'])/qr['density'],masks=comparison))
    exact=all(p['cell_counts_equal'] and p['area_equal'] and all(v['partition_equal'] for v in p['masks'].values()) for p in pairs)
    ns=stages(native);qs=stages(reference)
    ng=statistics.median(sum(r['stages'][name]['wall_ms'] for name in ['cell_capture','cell_detection','keep_cells','publish_cells']) for r in native['runs'][1:])
    qg=qs['cell_detection_and_publication']['median_ms']
    resources={}
    for key,name in [('native','native-resource'),('qupath','qupath-resource')]:
        raw=load(work/(name+'-resources.json'))
        assert raw['exit_code']==0 and not list((work/name).rglob('*.u32'))
        resources[key]={field:raw[field] for field in ['engine_pid','launcher_pid','process_wall_ms','peak_working_set_bytes','memory_samples','exit_code']}
        resources[key]['scope']='One full workflow plus actual engine process startup, without untimed mask oracles; OS high-water working set sampled25ms'
    optimization={}
    for name in ['native-matched','native-geometry','native-control']:
        runs=load(work/name/'report.json')['runs'][1:]
        optimization[name]=dict(workflow_median_ms=statistics.median(r['workflow_ms'] for r in runs),
            min_ms=min(r['workflow_ms'] for r in runs),max_ms=max(r['workflow_ms'] for r in runs))
    reduction=1-optimization['native-geometry']['workflow_median_ms']/optimization['native-control']['workflow_median_ms']
    micro_before=load(work/'contours-before.json');micro_after=load(work/'contours-after.json')
    for name in ['nucleus','body']:
        assert {r[name]['geometry_sha256'] for r in micro_before['runs']}=={r[name]['geometry_sha256'] for r in micro_after['runs']}
    for iteration in range(4):
        for name in ['density.csv','rgb.u8','nucleus.u32','body.u32']:
            assert (work/'native-matched'/str(iteration)/name).read_bytes()==(work/'native-geometry'/str(iteration)/name).read_bytes()
    result=dict(scope='One warmup +3 measured workflows, warm filesystem cache;512px ROI on whole-slide CMU-1; not whole-slide segmentation',
        host=dict(platform=platform.platform(),logical_processors=os.cpu_count(),processor=platform.processor()),
        native_engine=native['engine'],native_source=native['source_revision'],native_tree=native['source_tree_sha256'],
        qupath_version=reference['version'],inputs=load(work/'inputs.json'),observed_qupath_requests=load(work/'qupath-requests.json'),
        comparison=pairs,exact_partition_gate_passed=exact,native_stages=ns,qupath_stages=qs,
        native_cell_pipeline_median_ms=ng,qupath_cell_pipeline_median_ms=qg,
        paired_cell_speed_ratio_qupath_over_native=qg/ng if exact else None,
        native_workflow_median_ms=statistics.median(r['workflow_ms'] for r in native['runs'][1:]),
        qupath_workflow_median_ms=statistics.median(r['workflow_ms'] for r in reference['runs'][1:]),
        resources=resources,project_bytes=dict(native=native['runs'][-1]['project_bytes'],qupath=reference['runs'][-1]['project_bytes']),
        optimization_blocks=optimization,observed_workflow_time_reduction_vs_later_control=reduction,
        exact_geometry_wkt_and_csv_preserved=True,contour_micro_before=micro_before,contour_micro_after=micro_after,
        archive_fix=load(work/'candidate5-recovery.json'),contour_probe=load(work/'contour-breakdown.json'),
        reopen_probe=load(work/'reopen-breakdown.json'),
        caveats=['Native OD+RGB statistics differ from the scripted QuPath RGB reduction; no statistics speed ratio.',
            'Native QPX retains masks/statistics/identities; QPData stores image data/hierarchy. No save/reopen or overall equivalent-work speed ratio.',
            'Reported RAM is one resource-only full workflow including host startup; it is not a per-kernel or total machine memory cap.',
            'Isolated contour probes did not show a consistent gain; the optimization claim is limited to observed full-workflow A/B/A blocks.',
            'Candidate5 failed the real archived-cell reopen; successful measurements use the development decoder fix plus direct polygon construction.'])
    (work/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({key:result[key] for key in ['exact_partition_gate_passed','native_cell_pipeline_median_ms','qupath_cell_pipeline_median_ms','paired_cell_speed_ratio_qupath_over_native','native_workflow_median_ms','qupath_workflow_median_ms']}))


if __name__=='__main__':main()
