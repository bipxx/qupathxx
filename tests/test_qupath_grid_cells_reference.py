"""Independent pinned masks/features for the explicit-grid production recipe."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_grid_reference', ROOT/'tools/compare_qupath_grid_cells.py')
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
CAPTURE = comparison.FIXTURES/'capture'
CONTRACT = comparison.comparison_tool().read(comparison.FIXTURES/'contract.json')
NAMES = [case['name'] for case in CONTRACT['cases']]
EXACT = ['blank', 'isolated_three', 'touching_pair', 'gap_pair',
         'size_filters', 'gradient_noise', 'anisotropic_level0']
pytestmark = pytest.mark.skipif(not q.has_geos, reason='GEOS grid cell reference')


@pytest.fixture(scope='module')
def reports():
    return {context: comparison.compare(CAPTURE, context) for context in [0, 32]}


@pytest.mark.parametrize('name', EXACT)
def test_explicit_unit_grid_pinned_nucleus_body_and_features(reports, name):
    for report in reports.values():
        case = next(c for c in report['cases'] if c['case_name'] == name)
        assert case['algorithm'] == 'qpx.grid-log-watershed-bodies/1'
        assert case['grid_equivalent'] and case['independent_source_features_accepted']
        assert all(case['characterization_gates'].values())
        for metric in case['segmentation'].values():
            assert metric['reference_count'] == metric['native_count']
            assert metric['foreground_iou'] == metric['foreground_dice'] == 1
            assert not metric['unmatched_reference'] and not metric['unmatched_native']
            assert not metric['split_reference_labels'] and not metric['merged_native_labels']
            assert all(m['iou'] == 1 and m['centroid_error_px'] == 0 for m in metric['matches'])
        assert all(f['within_frozen_feature_tolerance'] for f in case['matched_feature_differences'])


@pytest.mark.parametrize('name', NAMES)
def test_grid_context_does_not_change_full_image_labels(reports, name):
    first, padded = [next(c for c in reports[k]['cases'] if c['case_name'] == name) for k in [0, 32]]
    assert first['label_sha256'] == padded['label_sha256']
    assert first['segmentation'] == padded['segmentation']
    assert first['matched_feature_differences'] == padded['matched_feature_differences']
    assert first['input_sha256'] != padded['input_sha256']


def test_roi_and_resampling_differences_remain_visible(reports):
    for report in reports.values():
        assert [c['case_name'] for c in report['cases']] == NAMES
        assert not report['omitted_anisotropic_physical_cases']
        assert report['state'] == 'characterized-partial'
        assert not report['segmentation_parity_accepted'] and not report['biological_validation']
        roi = next(c for c in report['cases'] if c['case_name'] == 'roi_cut_hole')
        assert roi['grid_equivalent']
        assert roi['segmentation']['nucleus']['foreground_iou'] < 1
        assert roi['segmentation']['body']['foreground_iou'] == 1
        resampled = next(c for c in report['cases'] if c['case_name'] == 'resampled_reference')
        assert not resampled['grid_equivalent']
        assert resampled['reference_requested_pixel_size_um'] == .75
        assert resampled['segmentation']['nucleus']['foreground_iou'] < 1
        assert resampled['segmentation']['body']['foreground_iou'] < 1


def test_isotropic_physical_recipe_matches_pixel_recipe(reports):
    physical = comparison.compare(CAPTURE, 32, 'micrometres')
    assert physical['omitted_anisotropic_physical_cases'] == ['anisotropic_level0']
    assert len(physical['cases']) == 8
    for case in physical['cases']:
        pixel = next(c for c in reports[32]['cases'] if c['case_name'] == case['case_name'])
        assert case['units'] == 'micrometres'
        assert case['label_sha256'] == pixel['label_sha256']
        assert case['segmentation'] == pixel['segmentation']
        assert case['matched_feature_differences'] == pixel['matched_feature_differences']


@pytest.mark.parametrize('units', ['micrometres', 'unknown'])
def test_comparison_rejects_incompatible_recipe(units):
    case = next(c for c in CONTRACT['cases'] if c['name'] == 'anisotropic_level0')
    with pytest.raises(ValueError, match='isotropic calibration|unknown comparison units'):
        comparison.compare_case(CAPTURE, CONTRACT, case, units=units)


@pytest.mark.parametrize('failure', ['none', 'extent', 'samples', 'origin', 'image', 'shape', 'escaped'])
def test_unit_projection_rejects_incompatible_grid_or_hidden_labels(failure):
    # These adversarial arrays ensure a comparison cannot hide context labels
    # or crop an incompatible grid into an apparently successful reference mask.
    args = dict(image_width=4, image_height=3,
                x=q.ScientificGridAxis(0, 4, 4, 1), y=q.ScientificGridAxis(0, 3, 3, 1))
    if failure == 'extent': args['x'] = q.ScientificGridAxis(0, 3, 4, 1)
    if failure == 'samples': args['x'] = q.ScientificGridAxis(0, 4, 2, 1)
    if failure == 'origin': args['x'] = q.ScientificGridAxis(1, 4, 4, 1)
    if failure == 'image': args['image_width'] = 5
    grid = None if failure == 'none' else q.ScientificGrid(**args)
    labels = np.zeros((5, 6), dtype=np.uint32)
    if failure == 'shape': labels = labels[:-1]
    if failure == 'escaped': labels[0, 0] = 1
    raster = SimpleNamespace(scientific_grid=grid, labels_numpy=lambda: labels)
    with pytest.raises(ValueError, match='declared unit grid|raster shape|escaped'):
        comparison.unit_canvas(raster, 4, 3)


def test_reference_roi_processing_extent_and_context_are_explicit(reports):
    case = next(c for c in CONTRACT['cases'] if c['name'] == 'roi_cut_hole')
    comparison.comparison_tool().verify_capture(CAPTURE)
    cropped = comparison.compare_case(CAPTURE, CONTRACT, case, 0, grid_scope='reference-roi')
    padded = comparison.compare_case(CAPTURE, CONTRACT, case, 32, grid_scope='reference-roi')
    full = next(c for c in reports[32]['cases'] if c['case_name'] == case['name'])
    assert cropped['grid'] == dict(origin=[32, 32], extent=[192, 128], samples=[192, 128], context=[0, 0])
    assert cropped['grid_scope'] == padded['grid_scope'] == 'reference-roi'
    assert cropped['independent_source_features_accepted']
    assert cropped['segmentation']['body']['foreground_iou'] == 1
    assert cropped['segmentation']['nucleus']['foreground_iou'] < 1
    # Matching the processing crop changes the discrepancy; it does not make
    # the distinct Gaussian boundary/arithmetic recipes equivalent.
    assert cropped['label_sha256']['nucleus'] != padded['label_sha256']['nucleus']
    assert padded['label_sha256'] == full['label_sha256']
    assert len({c['input_sha256'] for c in [cropped, padded, full]}) == 3


@pytest.mark.parametrize('bounds', [[-1, 0, 4, 3], [0, 0, 0, 3], [0, 0, 5, 3], [0., 0, 4, 3]])
def test_unit_projection_rejects_invalid_declared_core(bounds):
    with pytest.raises(ValueError, match='invalid comparison core bounds'):
        comparison.unit_canvas(None, 4, 3, bounds)
