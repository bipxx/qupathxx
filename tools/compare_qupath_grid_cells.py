"""Characterize explicit unit-grid cells against immutable QuPath captures.

The requested-resampling case deliberately retains its grid mismatch. No
reference labels, detector parameters or acceptance thresholds are adjusted.
"""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/qupath-cells-0.7.0'


def comparison_tool():
    path = ROOT / 'tools/compare_qupath_cells.py'
    spec = importlib.util.spec_from_file_location('qpx_grid_cell_comparison', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def unit_canvas(raster, width, height, core_bounds=None):
    """Project a declared unit core; reject shifted grids and hidden labels."""
    import numpy as np
    x, y, w, h = core_bounds if core_bounds is not None else (0, 0, width, height)
    if (any(type(v) is not int for v in (x, y, w, h)) or min(x, y) < 0
            or min(w, h) <= 0 or x+w > width or y+h > height):
        raise ValueError('invalid comparison core bounds')
    grid = raster.scientific_grid
    if (grid is None or (grid.image_width, grid.image_height) != (width, height)
            or (grid.x.origin, grid.y.origin) != (x, y)
            or (grid.x.extent, grid.y.extent) != (w, h)
            or (grid.x.samples, grid.y.samples) != (w, h)):
        raise ValueError('comparison requires the declared unit grid')
    labels = raster.labels_numpy()
    cx, cy = grid.x.context, grid.y.context
    if labels.shape != (h + 2 * cy, w + 2 * cx):
        raise ValueError('unit-grid raster shape differs')
    retained = labels.copy()
    retained[cy:cy + h, cx:cx + w] = 0
    if np.any(retained):
        raise ValueError('cell labels escaped the grid core')
    canvas = np.zeros((height, width), dtype=np.uint32)
    canvas[y:y+h, x:x+w] = labels[cy:cy+h, cx:cx+w]
    return canvas


def compare_case(folder, contract, case, context=32, units='pixels', grid_scope='full-image'):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    if units not in ('pixels', 'micrometres'):
        raise ValueError('unknown comparison units')
    if grid_scope not in ('full-image', 'reference-roi'):
        raise ValueError('unknown comparison grid scope')
    # Equal physical conversion is possible only on an isotropic unit grid.
    # Do not replace the native per-axis metric with QuPath's averaged axis.
    px, py = case['pixel_size_um']
    if units == 'micrometres' and px != py:
        raise ValueError('equivalent physical recipe requires isotropic calibration')
    comparison = comparison_tool()
    path = Path(folder) / case['name']
    width, height = contract['width'], contract['height']
    rgb = np.asarray(Image.open(path / 'input.png').convert('RGB'))
    phantom = comparison.load_tool('qpx_grid_phantom', comparison.PHANTOMS)
    np.testing.assert_array_equal(rgb, phantom.pixels(contract, case))
    doc = q.Document(q.Image.from_numpy(rgb))
    doc.set_calibration(px, py)
    bounds = case.get('roi', [0, 0, width, height])
    if 'hole' in case:
        def ring(b):
            x, y, w, h = b
            return [q.Point(x, y), q.Point(x+w, y), q.Point(x+w, y+h), q.Point(x, y+h)]
        target = doc.add_roi(q.Roi.polygon(ring(bounds), [ring(case['hole'])]))
    else:
        target = doc.add_rectangle(q.Rect(*bounds))
    transform = q.StainTransform(contract['vectors'], contract['white'])
    core = bounds if grid_scope == 'reference-roi' else [0, 0, width, height]
    if any(type(v) is not int for v in core):
        raise ValueError('reference ROI unit-grid comparison requires integer bounds')
    x, y, w, h = core
    grid = q.ScientificGrid(width, height,
        q.ScientificGridAxis(x, w, w, context),
        q.ScientificGridAxis(y, h, h, context))
    native = contract['native']
    parameters = q.GridCellParameters()
    parameters.units = q.GridCellUnits.LEVEL_ZERO_PIXELS if units == 'pixels' else q.GridCellUnits.MICROMETRES
    scale = 1 if units == 'pixels' else px
    parameters.threshold = native['threshold']
    parameters.sigma = native['sigma'] * scale
    parameters.cell_expansion = native['cell_expansion'] * scale
    parameters.min_nucleus_area = native['min_nucleus_pixels'] * scale**2
    parameters.max_nucleus_area = native['max_nucleus_pixels'] * scale**2
    # Grid LoG uses its existing zero peak-radius/spacing defaults, not the
    # legacy distance-watershed controls from the captured native recipe.
    request = q.CellRequest(doc, target, transform, grid, parameters)
    began = time.perf_counter()
    result = q.detect_cells(request)
    elapsed = (time.perf_counter() - began) * 1000
    reference = comparison.read(path / 'reference.json')
    if reference['qupath_version'] != '0.7.0' or (reference['width'], reference['height']) != (width, height):
        raise ValueError('unexpected reference runtime/dimensions')
    masks = {name: unit_canvas(getattr(result, attr), width, height, core)
             for name, attr in [('nucleus', 'nuclei'), ('body', 'cells')]}
    expected = {name: np.frombuffer(gzip.decompress((path / (name+'.u32le.gz')).read_bytes()),
                    dtype='<u4').reshape(height, width) for name in masks}
    metrics = {name: comparison.metrics(expected[name], mask, contract['comparison'])
               for name, mask in masks.items()}
    regions = {(r['compartment'], r['label']): r for r in reference['regions']}
    features = {f.label: f for f in result.features}
    tolerance = contract['comparison']
    differences = []
    # Independent arithmetic on actual source pixels validates features even
    # where detector masks differ. Upstream feature differences remain separate.
    od = np.maximum(0, np.log10(np.asarray(contract['white']) / np.maximum(rgb.astype(float), 1)))
    for compartment, mask in masks.items():
        for label, feature in features.items():
            selected = mask == label
            prefix = 'nucleus' if compartment == 'nucleus' else 'cell'
            if int(selected.sum()) != getattr(feature, prefix+'_pixels'):
                raise ValueError('native feature sample count differs from labels')
            for suffix, source in [('rgb', rgb), ('channels', od)]:
                actual = np.asarray(getattr(feature, prefix+'_'+suffix))
                if not np.allclose(actual, source[selected].mean(axis=0),
                                   atol=tolerance['feature_absolute'], rtol=tolerance['feature_relative']):
                    raise ValueError('native feature differs from independent source arithmetic')
        for match in metrics[compartment]['matches']:
            feature = features[match['native_label']]
            region = regions[compartment, match['reference_label']]
            prefix = 'nucleus' if compartment == 'nucleus' else 'cell'
            actual_rgb = np.asarray(getattr(feature, prefix+'_rgb'))
            actual_od = np.asarray(getattr(feature, prefix+'_channels'))
            differences.append(dict(compartment=compartment,
                reference_label=match['reference_label'], native_label=match['native_label'],
                rgb_max_absolute_error=float(np.max(np.abs(actual_rgb-region['rgb_mean']))),
                od_max_absolute_error=float(np.max(np.abs(actual_od-region['od_mean']))),
                within_frozen_feature_tolerance=bool(
                    np.allclose(actual_rgb, region['rgb_mean'], atol=tolerance['feature_absolute'], rtol=tolerance['feature_relative'])
                    and np.allclose(actual_od, region['od_mean'], atol=tolerance['feature_absolute'], rtol=tolerance['feature_relative']))))
    gates = {}
    if 'baseline_count' in case:
        gates['baseline_count'] = result.cell_count == reference['count'] == case['baseline_count']
    if case['name'] == 'isolated_three':
        matches = metrics['nucleus']['matches']
        gates['isolated_nucleus_iou'] = len(matches) == 3 and all(m['iou'] >= tolerance['isolated_nucleus_iou_min'] for m in matches)
        gates['isolated_centroid'] = len(matches) == 3 and all(m['centroid_error_px'] <= tolerance['isolated_centroid_error_max_px'] for m in matches)
    return dict(case_name=case['name'], algorithm=result.algorithm, units=units,
        grid_scope=grid_scope,
        grid=dict(origin=[x, y], extent=[w, h], samples=[w, h], context=[context, context]),
        parameters=dict(threshold=parameters.threshold, sigma=parameters.sigma,
            log_prominence=parameters.log_prominence, min_peak_radius=parameters.min_peak_radius,
            min_seed_distance=parameters.min_seed_distance, cell_expansion=parameters.cell_expansion,
            min_nucleus_area=parameters.min_nucleus_area, max_nucleus_area=parameters.max_nucleus_area,
            sampling_policy=parameters.sampling_policy.name),
        grid_equivalent=case.get('grid_equivalent', True),
        reference_requested_pixel_size_um=reference['parameters']['requestedPixelSizeMicrons'],
        segmentation=metrics, matched_feature_differences=differences,
        independent_source_features_accepted=True, characterization_gates=gates,
        label_sha256={name: hashlib.sha256(mask.astype('<u4').tobytes()).hexdigest() for name, mask in masks.items()},
        input_sha256=result.nuclei.input_sha256, retained_bytes=result.retained_bytes,
        estimated_work_bytes=request.estimated_work_bytes,
        elapsed_ms=elapsed)


def compare(folder, context=32, units='pixels', grid_scope='full-image'):
    import qupathxx as q
    import qupathxx._native as native
    comparison = comparison_tool()
    contract = comparison.verify_capture(folder)
    selected = [case for case in contract['cases']
                if units == 'pixels' or case['pixel_size_um'][0] == case['pixel_size_um'][1]]
    cases = [compare_case(folder, contract, case, context, units, grid_scope) for case in selected]
    return dict(schema='qpx.qupath-grid-cells-comparison/1', state='characterized-partial',
        algorithm='qpx.grid-log-watershed-bodies/1', units=units, grid_scope=grid_scope,
        build_git_revision=q.build_environment().git_revision,
        source_tree_sha256=q.build_environment().source_tree_sha256,
        native_sha256=comparison.sha(native.__file__),
        capture_manifest_sha256=comparison.sha(Path(folder)/'capture-manifest.json'),
        comparison_tool_sha256=comparison.sha(__file__),
        shared_comparison_tool_sha256=comparison.sha(comparison.__file__),
        omitted_anisotropic_physical_cases=[case['name'] for case in contract['cases'] if case not in selected],
        segmentation_parity_accepted=False, biological_validation=False,
        cases=cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, default=FIXTURES/'capture')
    parser.add_argument('--context', type=int, default=32)
    parser.add_argument('--units', choices=['pixels', 'micrometres'], default='pixels')
    parser.add_argument('--grid-scope', choices=['full-image', 'reference-roi'], default='full-image')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = compare(args.reference, args.context, args.units, args.grid_scope)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes((json.dumps(report, indent=2, allow_nan=False)+'\n').encode('utf-8'))
    print(json.dumps(dict(state=report['state'], cases=len(report['cases']), report=str(args.report))))


if __name__ == '__main__':
    main()
