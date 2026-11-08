"""Independent full-result identity and disclosed native before/after timings."""
import hashlib
import json
from pathlib import Path
import statistics
import sys
import qupathxx as q


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    work = Path(sys.argv[1]).resolve()
    comparisons = []
    baseline = work / 'baseline-before/11'
    geometry = []
    fields = ('label','nucleus_pixels','cell_pixels','nucleus_rgb','cell_rgb','cytoplasm_rgb',
              'nucleus_channels','cell_channels','cytoplasm_channels')
    feature_hashes = []
    for name in ('baseline-before', 'candidate-final', 'baseline-after'):
        output = work / name / '11'
        for member in ('rgb.u8','nucleus.u32','body.u32','density.csv'):
            assert (output/member).read_bytes() == (baseline/member).read_bytes(), (name,member)
        project=q.Project.open_without_journal_recovery(str(output/'study.qpxproj'))
        document=project.document(1)
        result=document.cell_record(3).result
        assert result.cell_count == 2237
        geometry.append({kind: sha(json.dumps([[part.label,part.pixels,part.geometry.wkt] for part in contours.components],separators=(',',':')).encode())
                         for kind,contours in [('nucleus',result.nucleus_contours),('body',result.cell_contours)]})
        feature_hashes.append(sha(json.dumps([[getattr(feature,field) for field in fields] for feature in result.features],separators=(',',':')).encode()))
        if name == 'candidate-final':
            replay=q.CellRequest.replay(document,3)
            recomputed=q.detect_cells(replay)
            assert document.verify_cells(3,replay,recomputed)
            assert recomputed.nuclei.input_sha256 == result.nuclei.input_sha256
            assert recomputed.cells.input_sha256 == result.cells.input_sha256
        report=json.loads((work/name/'report.json').read_text(encoding='utf-8'))
        measured=[row for row in report['runs'] if not row['warmup']]
        stages=('cell_capture','cell_detection','keep_cells','publish_cells')
        values=[sum(row['stages'][stage]['wall_ms'] for stage in stages) for row in measured]
        cpu=[sum(row['stages'][stage]['cpu_ms'] for stage in stages) for row in measured]
        resources=json.loads((work/(name+'-resources.json')).read_text(encoding='utf-8'))
        comparisons.append(dict(name=name,source_tree=report['source_tree_sha256'],wall_samples_ms=values,
                                wall_median_ms=statistics.median(values),cpu_samples_ms=cpu,cpu_median_ms=statistics.median(cpu),
                                other_cpu_fraction=resources['other_process_cpu_fraction']))
        document.server.close()
    assert geometry[0] == geometry[1] == geometry[2]
    assert feature_hashes[0] == feature_hashes[1] == feature_hashes[2]
    value=dict(comparisons=comparisons,cells=2237,exact_rgb_masks_density_csv=True,
               exact_ordered_geometry=True,exact_features=True,archive_reopen_verify=True,
               geometry=geometry[0],feature_sha256=feature_hashes[0],
               general_end_to_end_speedup_proven=False,
               limits='Sequential A/B/A block under substantial other-process load; one ROI, no general speed or parity claim.')
    (work/'comparison.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(value,indent=2))


if __name__ == '__main__':
    main()
