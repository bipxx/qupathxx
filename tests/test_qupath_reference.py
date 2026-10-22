"""Independent pinned QuPath outputs, including explicit unresolved differences."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

import numpy as np
import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT/'tests/fixtures/qupath-0.7.0'
spec = importlib.util.spec_from_file_location('qpx_compare_reference', ROOT/'tools/compare_qupath.py')
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
CONTRACT = comparison.read(FIXTURES/'contract.json')
TOL = CONTRACT['tolerances']
GEOMETRY = pytest.mark.skipif(not q.has_geos or not q.has_interchange, reason='GEOS/GeoJSON reference workflow')


def test_capture_identity_and_independent_runtime_provenance():
    manifest = comparison.verify_capture(FIXTURES, CONTRACT)
    assert manifest['upstream']['revision'] == '04ccfa4fb7d43e9b566393e08e83690b72248d44'
    assert manifest['upstream']['bundle_sha256'] == '5839743281d11eca1ac55c87ddcf92c89a30cba7ba2060f162036b0514b1e30c'
    assert manifest['upstream']['runtime_files'] == 496
    assert len(manifest['files']) == 12 and manifest['comparison_state'] == 'passed'
    for direction in ['upstream', 'roundtrip']:
        data = comparison.read(FIXTURES/direction/'measurements.json')
        assert data['qupath_version'] == '0.7.0' and data['java_version'] == '25.0.2'
        assert len(data['objects']) == 10
        assert len({str(uuid.UUID(o['id'])) for o in data['objects']}) == 10


def test_capture_mutation_is_rejected(tmp_path):
    target = tmp_path/'capture'
    shutil.copytree(FIXTURES, target)
    with (target/'upstream/roi-means.json').open('ab') as stream:
        stream.write(b' ')
    with pytest.raises(ValueError, match='capture changed'):
        comparison.verify_capture(target, CONTRACT)


@pytest.fixture(scope='module', params=['upstream', 'roundtrip'])
def native_reference(request):
    comparison.verify_capture(FIXTURES, CONTRACT)
    folder = FIXTURES/request.param
    doc, imported = comparison.document(folder, CONTRACT)
    return folder, doc, imported


def quadrature_length(roi, sx, sy):
    # Independent fixed-grid trapezoids, rather than native adaptive Simpson
    # or QuPath's Ramanujan approximation. Resolve fixture axis ratios <=200.
    bounds = roi.bounds
    t = np.linspace(0, np.pi/2, 65537)
    speed = np.hypot(bounds.width*sx/2*np.sin(t), bounds.height*sy/2*np.cos(t))
    return float(4*np.sum((speed[:-1]+speed[1:])*np.diff(t)/2))


@GEOMETRY
@pytest.mark.parametrize('index', range(10))
def test_upstream_geometry_classes_and_calibrated_features(native_reference, index):
    folder, doc, _ = native_reference
    expected = comparison.read(folder/'measurements.json')['objects'][index]
    obj = next(a for a in doc.annotations if a.name == expected['name'])
    assert obj.class_name == expected.get('classification', '')
    assert {q.ObjectKind.ANNOTATION: 'annotation', q.ObjectKind.DETECTION: 'detection', q.ObjectKind.CELL: 'cell'}[obj.kind] == expected['kind']
    if obj.class_name:
        assert doc.object_class(obj.class_name).color == expected['class_color'] & 0xffffff
    features = doc.shape_measurements(obj.id)
    for roi, measurement, data in [(obj.geometry, features.roi, expected['geometry']),
                                   (obj.nucleus, features.nucleus, expected.get('nucleus'))]:
        if data is None:
            assert roi is None and measurement is None
            continue
        assert roi is not None and measurement is not None
        bounds = roi.bounds
        assert [bounds.x, bounds.y, bounds.width, bounds.height] == pytest.approx(data['bounds'], abs=TOL['geometry_absolute'], rel=TOL['geometry_relative'])
        assert [measurement.area_px2, measurement.area_um2] == pytest.approx([data['area'], data['area_um2']], abs=TOL['geometry_absolute'], rel=TOL['geometry_relative'])
        for actual, wanted, sx, sy in [(measurement.length_px, data['length'], 1, 1),
                                      (measurement.length_um, data['length_um'], .5, .25)]:
            if roi.kind == q.RoiKind.ELLIPSE:
                assert actual == pytest.approx(wanted, abs=TOL['geometry_absolute'], rel=TOL['ellipse_length_relative'])
                assert actual == pytest.approx(quadrature_length(roi, sx, sy), abs=1e-8, rel=1e-10)
            else:
                assert actual == pytest.approx(wanted, abs=TOL['geometry_absolute'], rel=TOL['geometry_relative'])


@pytest.mark.parametrize('index', [0, 1])
def test_pinned_upstream_od_and_stain_values(index):
    data = comparison.read(FIXTURES/'upstream/colors.json')
    model = data['models'][index]
    pixels = np.asarray(data['pixels'], dtype=np.uint8)[None, :, :]
    assert pixels.shape == (1, 260, 3)
    assert set(pixels[0, 4:, 0].tolist()) == set(range(256))
    server = q.Document(q.Image.from_numpy(pixels)).server
    buffer = server.read_region(q.RegionRequest(0, 0, 0, 260, 1))
    transform = q.StainTransform(data['vectors'], model['white'])
    np.testing.assert_allclose(transform.stains, model['basis'], atol=TOL['basis_absolute'], rtol=0)
    expected = np.asarray(model['channels']).T
    actual = np.concatenate([transform.optical_densities(buffer).numpy().reshape(-1, 3),
                             transform.deconvolve(buffer).numpy().reshape(-1, 3)], axis=1)
    np.testing.assert_allclose(actual, expected, atol=TOL['float_absolute'], rtol=TOL['float_relative'])


@GEOMETRY
@pytest.mark.parametrize('index', range(9))
def test_actual_sampling_masks_match_upstream_centers(native_reference, index):
    folder, doc, _ = native_reference
    data = comparison.read(folder/'roi-means.json')
    region = data['regions'][index]
    obj = next(a for a in doc.annotations if a.name == region['name'])
    mask, count = comparison.sample_mask(doc, obj.id, q.StainTransform(data['vectors'], data['white']),
                                         region['compartment'] == 'nucleus', 256, 256)
    assert count == region['samples'] == sum(mask)
    assert hashlib.sha256(mask).hexdigest() == region['mask_sha256']


@GEOMETRY
@pytest.mark.parametrize('index', range(9))
def test_tiled_roi_jobs_match_independent_upstream_means(native_reference, index):
    folder, doc, _ = native_reference
    data = comparison.read(folder/'roi-means.json')
    region = data['regions'][index]
    obj = next(a for a in doc.annotations if a.name == region['name'])
    transform = q.StainTransform(data['vectors'], data['white'])
    for tile in [16, 64, 256]:
        for kind, key in [(q.AnalysisKind.OPTICAL_DENSITY_MEANS, 'od_means'), (q.AnalysisKind.STAIN_MEANS, 'stain_means')]:
            result = q.analyze(q.AnalysisRequest(doc, obj.id, transform, kind, region['compartment'] == 'nucleus', tile))
            assert result.samples == region['samples']
            assert result.means == pytest.approx(region[key], abs=TOL['float_absolute'], rel=TOL['float_relative'])


def test_upstream_serialization_loss_is_not_hidden_by_native_comparisons():
    data = comparison.read(FIXTURES/'upstream/roi-means.json')
    created = next(r for r in data['created_regions'] if r['name'] == 'Subpixel boundary')
    decoded = next(r for r in data['regions'] if r['name'] == 'Subpixel boundary')
    assert created['samples'] == 441 and decoded['samples'] == 400
    assert created['mask_sha256'] != decoded['mask_sha256']
    changes = comparison.serialization_changes(FIXTURES/'upstream')
    change = next(c for c in changes if c['name'] == 'Subpixel boundary')
    assert change['created_area'] == pytest.approx(20.002**2)
    assert change['decoded_area'] == 400
    assert change['bounds_max_absolute_error'] <= TOL['upstream_export_coordinate_absolute']


def test_numeric_ids_are_replaced_by_qupath_uuids_in_real_roundtrip():
    exported = comparison.read(FIXTURES/'native-export.geojson')['features']
    returned = comparison.read(FIXTURES/'roundtrip/objects.geojson')['features']
    assert [str(f['id']) for f in exported] == [str(i) for i in range(1, 11)]
    assert {f['properties']['name'] for f in exported} == {f['properties']['name'] for f in returned}
    assert len({str(uuid.UUID(f['id'])) for f in returned}) == 10


@GEOMETRY
def test_retained_edge_case_results_remain_explicit():
    result = comparison.compare_edges(FIXTURES/'upstream', CONTRACT)
    half = next(r for r in result if r['name'] == 'Half-pixel hole')
    assert half['state'] == 'passed' and half['mask_equal']
    assert half['native_samples'] == half['decoded_samples'] == 1874
    assert half['native_area'] == half['qupath_decoded_area'] == 1850
    thin = next(r for r in result if r['name'] == 'Very thin ellipse')
    # Keep its geometry disagreement visible even though its mask/means agree.
    assert thin['state'] == 'passed' and thin['mask_equal'] and thin['native_samples'] == thin['decoded_samples'] == 60
    assert thin['default_raw_bounds'] == pytest.approx([170,220.45,60,.1])
    assert any(i['code']=='qupath_ellipse_precision_changed' for i in thin['import_notices'])
    assert max(thin['length_relative_errors'].values()) < TOL['ellipse_length_relative']


@GEOMETRY
def test_very_thin_ellipse_matches_qupath_decoded_envelope():
    thin = next(r for r in comparison.compare_edges(FIXTURES/'upstream', CONTRACT) if r['name'] == 'Very thin ellipse')
    assert thin['native_bounds'] == pytest.approx(thin['qupath_decoded_bounds'], abs=TOL['geometry_absolute'], rel=TOL['geometry_relative'])
    assert thin['native_area'] == pytest.approx(thin['qupath_decoded_area'], abs=TOL['geometry_absolute'], rel=TOL['geometry_relative'])


@GEOMETRY
def test_offline_checker_reports_explicit_matching_policy(tmp_path):
    output = tmp_path/'offline'
    result = subprocess.run([sys.executable, '-I', str(ROOT/'tools/compare_qupath.py'), '--reference-root', str(FIXTURES), '--output', str(output)],
                            capture_output=True, timeout=60)
    assert result.returncode == 0, result.stdout.decode('utf8', errors='replace')
    report = comparison.read(output/'report.json')
    assert report['state'] == 'passed'
    assert report['unmet_gates'] == []
    assert report['retained_edge_cases'][0]['import_policy'] == 'qupath_ellipse_precision=True, allow_losses=True'
    assert report['original']['analysis_jobs'] == report['roundtrip']['analysis_jobs'] == 54
    assert report['original']['mask_mismatches'] == report['roundtrip']['mask_mismatches'] == 0


@GEOMETRY
@pytest.mark.skipif(os.name != 'nt' or os.environ.get('QUPATHXX_COMPARE_QUPATH') != '1', reason='opt-in pinned Windows QuPath runtime')
def test_live_pinned_runtime_regenerates_an_independent_roundtrip(tmp_path):
    output = tmp_path/'live'
    cache = os.environ.get('QUPATHXX_QUPATH_CACHE', str(ROOT/'build/upstream/qupath'))
    result = subprocess.run([sys.executable, '-I', str(ROOT/'tools/compare_qupath.py'), '--runtime-cache', cache, '--output', str(output)],
                            capture_output=True, timeout=300)
    assert result.returncode == 0, result.stdout.decode('utf8', errors='replace')
    manifest = comparison.verify_capture(output, CONTRACT)
    assert manifest['comparison_state'] == 'passed'
    report = comparison.read(output/'report.json')
    assert report['original']['objects'] == report['roundtrip']['objects'] == 10
    assert report['original']['roi_mean_max_absolute_error'] <= TOL['float_absolute']
    assert report['roundtrip']['roi_mean_max_absolute_error'] <= TOL['float_absolute']
