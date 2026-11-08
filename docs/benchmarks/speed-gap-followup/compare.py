"""Exact-output gates and ordinary/settled timing, kept as distinct protocols."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'candidate5-performance'))
from summarize import canonical


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def dist(values):
    return dict(median_ms=statistics.median(values),min_ms=min(values),max_ms=max(values),samples_ms=values)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('work',type=Path);args=parser.parse_args();work=args.work.resolve()
    names=['native-final','qupath-final','native-control','qupath-control',
        'native-steady','qupath-steady','native-accepted-steady','qupath-accepted-steady',
        'native-delivery','native-delivery-steady','qupath-delivery-steady']
    reports={name:read(work/name/'report.json') for name in names};blocks={}
    for name,report in reports.items():
        rows=[r for r in report['runs'] if not r['warmup']]
        pipeline=['cell_capture','cell_detection','keep_cells','publish_cells'] if name.startswith('native') else ['cell_detection_and_publication']
        blocks[name]=dict(source_tree=report.get('source_tree_sha256'),measured_runs=len(rows),
            cell_pipeline=dist([sum(r['stages'][k]['wall_ms'] for k in pipeline) for r in rows]),
            process_resources=read(work/(name+'-resources.json')))
    exact=[]
    for i in range(4):
        native=work/'native-delivery'/str(i);baseline=work.parent/'speed-gap/native-selected'/str(i)
        for file in ['rgb.u8','nucleus.u32','body.u32','density.csv']:
            assert (native/file).read_bytes()==(baseline/file).read_bytes(),(i,file)
        for ref_name in ['qupath-final','qupath-control']:
            reference=work/ref_name/str(i)
            assert (native/'rgb.u8').read_bytes()==(reference/'rgb.u8').read_bytes()
            a=[np.fromfile(native/(k+'.u32'),dtype='<u4').reshape(512,512) for k in ['nucleus','body']]
            b=[np.fromfile(reference/(k+'.u32'),dtype='<u4').reshape(512,512) for k in ['nucleus','body']]
            assert all(np.array_equal(x,y) for x,y in zip(canonical(*a),canonical(*b)))
            n=reports['native-delivery']['runs'][i];r=reports[ref_name]['runs'][i]
            assert n['cells']==r['cells']==2237 and n['area_mm2']==r['area_mm2']
            assert (n['pixel_width_um'],n['pixel_height_um'])==(r['pixel_width_um'],r['pixel_height_um'])
            exact.append(dict(iteration=i,reference=ref_name,cells=2237,exact_masks_rgb_area_calibration=True))
    for name in ['native-delivery-steady','qupath-delivery-steady']:
        rows=reports[name]['runs'];assert len(rows)==12 and [r['warmup'] for r in rows]==[True]*6+[False]*6
        assert not list((work/name).rglob('*.u32')) and not list((work/name).rglob('rgb.u8'))
        for row in rows:
            baseline=reports['native-delivery']['runs'][0]
            assert row['cells']==baseline['cells']==2237 and row['area_mm2']==baseline['area_mm2']
            assert row['rgb_sum']==baseline['rgb_sum']
            assert (row['pixel_width_um'],row['pixel_height_um'])==(baseline['pixel_width_um'],baseline['pixel_height_um'])
    sys.path.insert(0,str(work/'runtime-delivery'));import qupathxx as q
    env=q.build_environment();assert env.source_tree_sha256==reports['native-delivery']['source_tree_sha256']
    project=q.Project.open(str(work/'native-delivery/0/study.qpxproj'));result=project.document(1).cell_record(3).result
    geometry={key:hashlib.sha256(json.dumps([[c.label,c.pixels,c.geometry.wkt] for c in contours.components],separators=(',',':')).encode()).hexdigest()
        for key,contours in [('nucleus',result.nucleus_contours),('body',result.cell_contours)]}
    assert geometry==read(work/'detector-before.json')['runs'][0]['geometry']
    probes={name:read(work/(name+'.json')) for name in ['detector-before','detector-after','detector-grain','detector-integer']}
    for probe in probes.values():
        for row in probe['runs']:
            assert row['geometry']==geometry and row['masks']==probes['detector-before']['runs'][0]['masks']
    phases=[]
    for line in (work/'profile-run.log').read_text(encoding='utf-8').splitlines():
        if '_PROFILE ' in line:phases.append(dict(stage=line.split()[0],values={k:float(v) for k,v in re.findall(r'(\w+)=([\d.]+)',line)}))
    ratio=lambda n,r:blocks[n]['cell_pipeline']['median_ms']/blocks[r]['cell_pipeline']['median_ms']
    output=dict(scope='Fixed512px CMU-1 ROI on Windows; exact correctness workflow separate from six-warmup/six-measured oracle-free timing',
        source_revision=env.git_revision,source_tree=env.source_tree_sha256,compiler_inputs=523,blocks=blocks,exact_pairs=exact,
        exact_native_masks_rgb_csv_and_ordered_geometry=True,geometry_hashes=geometry,
        ordinary_ratios=[ratio('native-final','qupath-final'),ratio('native-control','qupath-control')],
        steady_ratio_before_integer_proof=ratio('native-steady','qupath-steady'),
        steady_ratio_integer_proof=ratio('native-accepted-steady','qupath-accepted-steady'),
        primary_steady_ratio=ratio('native-delivery-steady','qupath-delivery-steady'),
        strict_median_speed_parity=ratio('native-delivery-steady','qupath-delivery-steady')<=1,
        precise_profile=phases,covers_probe=read(work/'covers-probe.json'),
        detector_probe_medians_ms={name:statistics.median(r['wall_ms'] for r in probe['runs'][1:]) for name,probe in probes.items()},
        limits=['One ROI/host; no whole-slide or universal performance claim.',
            'One-warmup apparent parity was premature; longer warmup and no intersample mask oracles change the reference baseline.',
            'Host load and JVM activity still vary; no statistical equivalence or stable platform superiority is established.',
            'Different statistics/archives prohibit an overall/save/reopen equivalent-work ratio.',
            'No new resource-only RAM claim; process counters include startup/warmup.',
            'Published candidate5 remains unchanged.'])
    (work/'summary.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(primary_steady_ratio=output['primary_steady_ratio'],strict_median_speed_parity=output['strict_median_speed_parity'],exact_pairs=len(exact))))


if __name__=='__main__':
    main()
