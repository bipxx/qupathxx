"""Capture pinned QuPath cells or characterize an immutable captured corpus.

The two detectors intentionally use different algorithms. Passing measurement
checks does not establish segmentation parity or biological accuracy.
"""
import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/qupath-cells-0.7.0'
GENERATOR = ROOT / 'tools/upstream/qupath_cells_reference.groovy'
PHANTOMS = ROOT / 'tools/qupath_cells_fixture.py'


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load_tool(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def files(contract):
    return ['contract.json'] + [f'{case["name"]}/{name}' for case in contract['cases']
        for name in ['input.png', 'cells.geojson', 'reference.json', 'nucleus.u32le.gz', 'body.u32le.gz',
                     'nucleus-decoded.u32le.gz', 'body-decoded.u32le.gz']]


def write_manifest(folder):
    folder = Path(folder)
    contract = read(FIXTURES / 'contract.json')
    upstream = read((FIXTURES / contract['upstream_contract']).resolve())['upstream']
    manifest = dict(schema='qpx.qupath-cells-capture/1',
        captured_utc=datetime.now(timezone.utc).isoformat(), upstream=upstream,
        contract_sha256=sha(FIXTURES / 'contract.json'),
        generator_sha256=sha(GENERATOR), phantom_generator_sha256=sha(PHANTOMS),
        files={name: dict(bytes=(folder / name).stat().st_size, sha256=sha(folder / name))
               for name in files(contract)})
    (folder / 'capture-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')


def verify_capture(folder):
    folder = Path(folder).resolve()
    contract = read(FIXTURES / 'contract.json')
    manifest = read(folder / 'capture-manifest.json')
    upstream = read((FIXTURES / contract['upstream_contract']).resolve())['upstream']
    if (manifest['schema'] != 'qpx.qupath-cells-capture/1' or manifest['upstream'] != upstream
            or manifest['contract_sha256'] != sha(FIXTURES / 'contract.json')
            or manifest['generator_sha256'] != sha(GENERATOR)
            or manifest['phantom_generator_sha256'] != sha(PHANTOMS)
            or set(manifest['files']) != set(files(contract))):
        raise ValueError('cell capture identity differs')
    for name, identity in manifest['files'].items():
        path = (folder / name).resolve()
        if (not path.is_relative_to(folder) or path.stat().st_size != identity['bytes']
                or sha(path) != identity['sha256']):
            raise ValueError('cell capture changed: '+name)
    if read(folder / 'contract.json') != contract:
        raise ValueError('captured contract differs')
    return contract


def capture(folder):
    folder = Path(folder).resolve()
    runtime = load_tool('qpx_fetch_cells_runtime', ROOT / 'tools/fetch_qupath.py').fetch(
        ROOT / 'build/upstream/qupath')
    load_tool('qpx_cells_phantoms', PHANTOMS).generate(folder)
    process = subprocess.run([str(runtime), 'script', '--args', str(folder), str(GENERATOR)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
    (folder / 'qupath.log').write_bytes(process.stdout)
    process.check_returncode()
    write_manifest(folder)
    verify_capture(folder)


def metrics(reference, native, tolerance):
    """Label-independent assignment plus explicit many-to-many relations."""
    import numpy as np
    from scipy.optimize import linear_sum_assignment
    reference, native = np.asarray(reference), np.asarray(native)
    if reference.shape != native.shape or reference.ndim != 2:
        raise ValueError('label canvases must have equal 2D shape')
    rids, nids = np.unique(reference[reference > 0]), np.unique(native[native > 0])
    ra = np.array([(reference == i).sum() for i in rids])
    na = np.array([(native == i).sum() for i in nids])
    overlap = np.array([[( (reference == r) & (native == n)).sum() for n in nids]
                        for r in rids], dtype=np.int64).reshape(len(rids), len(nids))
    union = ra[:, None]+na[None, :]-overlap
    iou = np.divide(overlap, union, out=np.zeros_like(overlap, dtype=float), where=union > 0)
    rows, cols = linear_sum_assignment(-iou)
    matches = []
    for r, n in zip(rows, cols):
        if iou[r, n] < tolerance['match_min_iou']:
            continue
        ry, rx = np.where(reference == rids[r]); ny, nx = np.where(native == nids[n])
        matches.append(dict(reference_label=int(rids[r]), native_label=int(nids[n]),
            iou=float(iou[r, n]), centroid_error_px=float(np.hypot(rx.mean()-nx.mean(), ry.mean()-ny.mean())),
            reference_pixels=int(ra[r]), native_pixels=int(na[n])))
    # A material overlap relative to the smaller object exposes splits/merges.
    denominator = np.minimum(ra[:, None], na[None, :])
    relation = (overlap >= tolerance['relation_min_pixels']) & (
        overlap >= tolerance['relation_min_fraction']*denominator)
    fg_intersection = int(((reference > 0) & (native > 0)).sum())
    fg_union = int(((reference > 0) | (native > 0)).sum())
    total = int((reference > 0).sum()+(native > 0).sum())
    return dict(reference_count=len(rids), native_count=len(nids), matches=matches,
        unmatched_reference=len(rids)-len(matches), unmatched_native=len(nids)-len(matches),
        split_reference_labels=[int(rids[i]) for i in np.flatnonzero(relation.sum(axis=1) > 1)],
        merged_native_labels=[int(nids[i]) for i in np.flatnonzero(relation.sum(axis=0) > 1)],
        foreground_iou=fg_intersection/fg_union if fg_union else 1.,
        foreground_dice=2*fg_intersection/total if total else 1.)


def canvas(result, width, height):
    import numpy as np
    output = np.zeros((height, width), dtype=np.uint32)
    x, y = int(result.level_bounds.x), int(result.level_bounds.y)
    output[y:y+result.height, x:x+result.width] = result.labels_numpy()
    return output


def on_polygon_boundary(roi, x, y):
    """Independent segment test diagnoses membership differences; no repair."""
    for path in roi.paths:
        for a, b in zip(path, path[1:]+path[:1]):
            cross = (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x)
            if (abs(cross) <= 1e-10 and min(a.x,b.x)-1e-10 <= x <= max(a.x,b.x)+1e-10
                    and min(a.y,b.y)-1e-10 <= y <= max(a.y,b.y)+1e-10):
                return True
    return False


def compare_case(folder, contract, case, cell_algorithm="distance"):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    folder = Path(folder) / case['name']
    width, height = contract['width'], contract['height']
    rgb = np.array(Image.open(folder / 'input.png').convert('RGB'))
    reference = read(folder / 'reference.json')
    if reference['qupath_version'] != '0.7.0' or (reference['width'], reference['height']) != (width, height):
        raise ValueError('unexpected reference runtime/dimensions')
    phantom = load_tool('qpx_cells_pixels', PHANTOMS).pixels(contract, case)
    np.testing.assert_array_equal(rgb, phantom)
    refs = {name: np.frombuffer(gzip.decompress((folder / (name+'.u32le.gz')).read_bytes()),
                               dtype='<u4').reshape(height, width) for name in ['nucleus', 'body']}
    transform = q.StainTransform(contract['vectors'], contract['white'])
    doc = q.Document(q.Image.from_numpy(rgb)); doc.set_calibration(*case['pixel_size_um'])
    bounds = case.get('roi', [0, 0, width, height])
    if 'hole' in case:
        def ring(b):
            x, y, w, h = b
            return [q.Point(x,y), q.Point(x+w,y), q.Point(x+w,y+h), q.Point(x,y+h)]
        target = doc.add_roi(q.Roi.polygon(ring(bounds), [ring(case['hole'])]))
    else:
        target = doc.add_rectangle(q.Rect(*bounds))
    results, timings = [], []
    for tile in contract['tile_sizes']:
        params = q.CellParameters()
        for key, value in contract['native'].items():
            setattr(params, key, value)
        params.tile_size = tile
        if cell_algorithm not in ['distance', 'log', 'log-bodies']:
            raise ValueError('unknown cell algorithm')
        if cell_algorithm == 'log':
            params.algorithm = q.CellAlgorithm.LOG_WATERSHED
        elif cell_algorithm == 'log-bodies':
            params.algorithm = q.CellAlgorithm.LOG_WATERSHED_BODIES
        started = time.perf_counter()
        result = q.detect_cells(q.CellRequest(doc, target, transform, params))
        timings.append(dict(tile_size=tile, elapsed_ms=(time.perf_counter()-started)*1000,
                            retained_bytes=result.retained_bytes))
        results.append(result)
    native_masks = {name: canvas(getattr(results[0], attr), width, height)
                    for name, attr in [('nucleus','nuclei'),('body','cells')]}
    for result in results[1:]:
        for name, attr in [('nucleus','nuclei'),('body','cells')]:
            np.testing.assert_array_equal(native_masks[name], canvas(getattr(result, attr), width, height))
        for actual, wanted in zip(result.features, results[0].features, strict=True):
            for key in ['nucleus_pixels','cell_pixels','nucleus_rgb','cell_rgb','nucleus_channels','cell_channels']:
                np.testing.assert_array_equal(getattr(actual,key), getattr(wanted,key))
    tolerance = contract['comparison']
    segmentation = {name: metrics(refs[name], native_masks[name], tolerance) for name in refs}
    # Measurement validation on identical QuPath geometry is independent of detection.
    shared = q.Document(q.Image.from_numpy(rgb)); shared.set_calibration(*case['pixel_size_um'])
    imported = q.import_geojson(shared, (folder/'cells.geojson').read_text(encoding='utf-8'), assume_pixels=True)
    objects = {obj.name: obj for obj in shared.annotations}
    if len(objects) != reference['count']:
        raise ValueError('reference cell interchange lost objects')
    shared_regions = []
    serialization_changes = {}
    for name in ['nucleus','body']:
        decoded = np.frombuffer(gzip.decompress((folder / (name+'-decoded.u32le.gz')).read_bytes()),
                                dtype='<u4').reshape(height, width)
        serialization_changes[name] = int(np.count_nonzero(refs[name] != decoded))
    winding_regions = []
    for policy, region_reports in [(q.RoiSamplingPolicy.GEOMETRY_COVERAGE, shared_regions),
                                   (q.RoiSamplingPolicy.POLYGON_WINDING, winding_regions)]:
        for region in reference['decoded_regions']:
            obj = objects['cell_'+str(region['label'])]
            nucleus = region['compartment'] == 'nucleus'
            sample = q.TissueParameters(); sample.threshold=0; sample.level=0; sample.sampling_policy=policy
            sampled = q.detect_tissue(q.TissueRequest(shared,obj.id,transform,sample,nucleus))
            mask = canvas(sampled,width,height)>0
            expected_mask = np.frombuffer(gzip.decompress((folder / (region['compartment']+'-decoded.u32le.gz')).read_bytes()),
                                          dtype='<u4').reshape(height,width) == region['label']
            mismatch = int(np.count_nonzero(mask != expected_mask))
            centers = np.argwhere(mask != expected_mask).tolist()
            roi = obj.nucleus if nucleus else obj.geometry
            boundary_only = all(on_polygon_boundary(roi,x+.5,y+.5) for y,x in centers)
            errors, count_errors, means_pass = [], [], True
            for tile in contract['tile_sizes']:
                for kind, key in [(q.AnalysisKind.OPTICAL_DENSITY_MEANS,'od_mean'),
                                  (q.AnalysisKind.STAIN_MEANS,'stain_mean')]:
                    result = q.analyze(q.AnalysisRequest(shared,obj.id,transform,kind,nucleus,tile,policy))
                    count_errors.append(abs(result.samples-region['pixels']))
                    wanted = np.asarray(region[key]); actual = np.asarray(result.means)
                    errors.append(float(np.max(np.abs(actual-wanted))))
                    means_pass = means_pass and bool(np.allclose(actual,wanted,
                        atol=tolerance['feature_absolute'],rtol=tolerance['feature_relative']))
            region_reports.append(dict(label=region['label'],compartment=region['compartment'],
                mask_mismatches=mismatch,sample_count_error=max(count_errors),max_mean_absolute_error=max(errors),
                passed=means_pass and mismatch==0 and max(count_errors)<=tolerance['sampling_count_error'],
                mismatched_centers_yx=centers,mismatches_only_on_polygon_boundary=boundary_only,
                native_only_centers=int(np.count_nonzero(mask & ~expected_mask)),
                reference_only_centers=int(np.count_nonzero(expected_mask & ~mask))))
    differences = []
    feature_by_label = {f.label:f for f in results[0].features}
    for name in ['nucleus','body']:
        for match in segmentation[name]['matches']:
            region = next(r for r in reference['regions'] if r['compartment']==name and r['label']==match['reference_label'])
            feature = feature_by_label[match['native_label']]
            rgb_mean = getattr(feature,'nucleus_rgb' if name=='nucleus' else 'cell_rgb')
            channels = getattr(feature,'nucleus_channels' if name=='nucleus' else 'cell_channels')
            differences.append(dict(compartment=name,reference_label=region['label'],native_label=feature.label,
                rgb_max_absolute_difference=float(np.max(np.abs(np.asarray(rgb_mean)-region['rgb_mean']))),
                stain_max_absolute_difference=float(np.max(np.abs(np.asarray(channels)-region['stain_mean'])))))
    gates = {}
    if 'baseline_count' in case:
        gates['baseline_count'] = results[0].cell_count == reference['count'] == case['baseline_count']
    if case['name'] == 'isolated_three':
        matches = segmentation['nucleus']['matches']
        gates['isolated_nucleus_iou'] = len(matches)==3 and all(m['iou']>=tolerance['isolated_nucleus_iou_min'] for m in matches)
        gates['isolated_centroid'] = len(matches)==3 and all(m['centroid_error_px']<=tolerance['isolated_centroid_error_max_px'] for m in matches)
    return dict(case_name=case['name'],grid_equivalent=case.get('grid_equivalent',True),
        segmentation=segmentation,reference_collisions=reference['collisions'],
        tile_invariant=True,shared_roi_regions=shared_regions,shared_roi_regions_winding=winding_regions,matched_feature_differences=differences,
        serialization_changed_label_pixels=serialization_changes,
        characterization_gates=gates,timings=timings)


def compare(folder, cell_algorithm="distance"):
    import qupathxx as q
    import qupathxx._native as native
    contract = verify_capture(folder)
    cases = [compare_case(folder, contract, case, cell_algorithm) for case in contract['cases']]
    return dict(schema='qpx.qupath-cells-comparison/1',state='characterized-partial',
        cell_algorithm=cell_algorithm,build_git_revision=q.build_environment().git_revision,
        segmentation_parity_accepted=False,biological_validation=False,
        explicit_winding_sampling_accepted=all(r['passed'] for case in cases for r in case['shared_roi_regions_winding']),
        explicit_winding_compartments=sum(len(case['shared_roi_regions_winding']) for case in cases),
        native_sha256=sha(native.__file__),source_tree_sha256=q.build_environment().source_tree_sha256,
        capture_manifest_sha256=sha(Path(folder)/'capture-manifest.json'),
        comparison_tool_sha256=sha(__file__),cases=cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, help='New output directory; runs hash-verified upstream runtime')
    parser.add_argument('--reference', type=Path, default=FIXTURES/'capture')
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--cell-algorithm', choices=['distance','log','log-bodies'], default='distance')
    args = parser.parse_args()
    folder = args.capture or args.reference
    if args.capture:
        capture(folder)
    report = compare(folder, args.cell_algorithm)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode('utf-8'))
    print(json.dumps(dict(state=report['state'],cases=len(report['cases']),report=str(args.report))))


if __name__ == '__main__':
    main()
