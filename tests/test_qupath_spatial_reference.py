"""Frozen independent spatial reference and checks for misleading oracle results."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/qupath-spatial-0.7.0'
spec = importlib.util.spec_from_file_location('qpx_spatial_capture', ROOT / 'tools/capture_qupath_spatial.py')
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


def test_spatial_reference_identity_and_analytic_expectations():
    manifest = capture.read(FIXTURES / 'manifest.json')
    for name, identity in manifest['files'].items():
        path = ROOT / name
        assert path.stat().st_size == identity['bytes']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == identity['sha256']
    contract = capture.read(FIXTURES / 'contract.json')
    reference = capture.read(FIXTURES / 'reference.json')
    assert len(contract['cases']) == 26
    capture.validate_reference(contract, reference)


@pytest.mark.parametrize('fault', [
    'missing_case', 'duplicate_case', 'reordered', 'wrong_source', 'wrong_version',
    'zero_distances', 'isotropic_distance', 'body_instead_of_nucleus',
    'bounding_box_centroid', 'missing_is_zero', 'cross_plane_distance',
    'float_rounding_in_general', 'missing_indexed_target', 'nan_distance', 'wrong_storage',
    'storage_rounding_drift',
])
def test_spatial_reference_rejects_contract_faults(fault):
    contract = capture.read(FIXTURES / 'contract.json')
    reference = copy.deepcopy(capture.read(FIXTURES / 'reference.json'))
    rows = {row['name']: row for row in reference['cases']}
    if fault == 'missing_case':
        reference['cases'].pop()
    elif fault == 'duplicate_case':
        reference['cases'].append(copy.deepcopy(reference['cases'][-1]))
    elif fault == 'reordered':
        reference['cases'].reverse()
    elif fault == 'wrong_source':
        reference['upstream_revision'] = '0' * 40
    elif fault == 'wrong_version':
        reference['qupath_version'] = '0.0.0'
    elif fault == 'zero_distances':
        for row in rows.values():
            if row['measured']:
                row['distance'] = 0
    elif fault == 'isotropic_distance':
        rows['anisotropic_scale']['distance'] = 5
    elif fault == 'body_instead_of_nucleus':
        rows['source_nucleus_preferred']['source_centroid'] = [50, 50]
    elif fault == 'bounding_box_centroid':
        rows['source_triangle_area_centroid']['source_centroid'] = [3, 3]
    elif fault == 'missing_is_zero':
        rows['empty_targets'].update(measured=True, distance=0)
    elif fault == 'cross_plane_distance':
        rows['different_z'].update(measured=True, distance=5)
    elif fault == 'float_rounding_in_general':
        rows['anisotropic_scale']['distance'] = rows['default_float_anisotropic_scale']['distance']
    elif fault == 'missing_indexed_target':
        rows['indexed_target_branch']['target_centroids'].pop()
    elif fault == 'nan_distance':
        rows['pixel_3_4_5']['distance'] = float('nan')
    elif fault == 'wrong_storage':
        rows['anisotropic_scale']['measurement_storage'] = rows['default_float_anisotropic_scale']['measurement_storage']
    elif fault == 'storage_rounding_drift':
        rows['explicit_double_anisotropic_scale']['distance'] += 1e-10
    with pytest.raises(ValueError):
        capture.validate_reference(contract, reference)
