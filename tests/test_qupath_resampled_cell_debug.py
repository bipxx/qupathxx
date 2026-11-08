"""Independent processed pixels, nominal coordinates and captured-input detector."""
import copy
from fractions import Fraction
import gzip
import importlib.util
import json
from pathlib import Path
import shutil

import numpy as np
from PIL import Image
import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_resampled_debug', ROOT/'tools/qupath_resampled_cell_debug.py')
debug = importlib.util.module_from_spec(spec)
spec.loader.exec_module(debug)


def test_public_processed_input_mapping_and_rounding_controls():
    report = debug.verify()
    assert report['source_dimensions'] == [256, 192]
    assert report['processing_dimensions'] == [171, 128]
    assert report['nominal_step'] == [3, 2] and report['realized_x_step'] == [256, 171]
    assert report['ordinary_labels_and_geometry_exact']
    assert report['optical_density_max_absolute_error'] < 1e-6
    assert report['processing_nucleus_pixels'] == 379 and report['processing_body_pixels'] == 825
    assert report['nominal_bilinear_half_up_different_components'] == 0
    assert report['realized_width_bilinear_different_components'] > 0
    assert report['ties_even_bilinear_different_components'] > 0
    assert not report['model_generalized'] and not report['sampler_equivalence_accepted']
    assert not report['biological_validation']


@pytest.mark.skipif(not q.has_geos, reason='GEOS captured processing cell detector')
def test_native_captured_processing_masks_features_and_authority():
    report = debug.compare_native()
    for metric in report['segmentation'].values():
        assert metric['reference_count'] == metric['native_count'] == 3
        assert metric['foreground_iou'] == metric['foreground_dice'] == 1
        assert not metric['unmatched_reference'] and not metric['unmatched_native']
        assert not metric['split_reference_labels'] and not metric['merged_native_labels']
        assert all(m['iou'] == 1 and m['centroid_error_px'] == 0 for m in metric['matches'])
    assert len(report['feature_checks']) == 6
    assert all(c[k] for c in report['feature_checks']
               for k in ['sample_count_exact', 'physical_area_exact', 'rgb_accepted', 'od_accepted'])
    assert report['algorithm'] == 'qpx.grid-log-watershed-bodies/1'
    assert report['native_processing_parameters']['calibration_um'] == [.75, .75]
    assert report['original_source_area_different_components'] > 0
    assert report['original_source_area_max_absolute_rgb_difference'] > 1
    assert not report['original_source_replay_authority']
    assert not report['sampler_equivalence_accepted'] and not report['segmentation_parity_accepted']


@pytest.mark.parametrize('name', ['debug-stack.tif', 'debug-reference.groovy', 'driver.log',
                                'resampled_reference/processed-input.png',
                                'resampled_reference/processed-input.json'])
def test_resampled_capture_rejects_changed_bytes(tmp_path, name):
    folder = tmp_path/'capture'
    shutil.copytree(debug.FIXTURE, folder)
    path = folder/name
    path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises(ValueError, match='capture bytes changed'):
        debug.verify(folder)


@pytest.mark.parametrize('change', ['schema', 'case', 'upstream', 'files'])
def test_resampled_capture_rejects_mixed_identity(tmp_path, change):
    folder = tmp_path/'capture'
    shutil.copytree(debug.FIXTURE, folder)
    path = folder/'manifest.json'
    value = json.loads(path.read_text(encoding='utf-8'))
    if change == 'schema': value['schema'] = 'qpx.qupath-roi-cell-debug/1'
    elif change == 'case': value['case'] = 'roi_cut_hole'
    elif change == 'upstream': value['upstream']['revision'] = 'unknown'
    else: value['files'].pop('resampled_reference/processed-input.png')
    path.write_bytes(json.dumps(value).encode('utf-8'))
    with pytest.raises(ValueError, match='capture identity differs'):
        debug.verify(folder)


@pytest.mark.parametrize('change', ['step', 'pixels', 'alpha', 'script', 'geometry', 'labels'])
def test_resampled_capture_rejects_resealed_semantic_changes(tmp_path, change):
    folder = tmp_path/'capture'
    shutil.copytree(debug.FIXTURE, folder)
    if change == 'step':
        name = 'resampled_reference/processed-input.json'
        value = json.loads((folder/name).read_text(encoding='utf-8'))
        value['downsample'] = 256/171
        (folder/name).write_bytes(json.dumps(value).encode('utf-8'))
    elif change in ('pixels', 'alpha'):
        name = 'resampled_reference/processed-input.png'
        with Image.open(folder/name) as image:
            pixels = np.asarray(image).copy()
            if change == 'pixels':
                pixels[0, 0, 0] -= 1
                changed = Image.fromarray(pixels)
            else:
                changed = image.convert('RGBA')
        changed.save(folder/name)
    elif change == 'script':
        name = 'debug-reference.groovy'
        text = (folder/name).read_text(encoding='utf-8').replace('setDebugMode(true)', 'setDebugMode(false)')
        (folder/name).write_bytes(text.encode('utf-8'))
    elif change == 'geometry':
        name = 'resampled_reference/cells.geojson'
        value = json.loads((folder/name).read_text(encoding='utf-8'))
        value['features'][0]['nucleusGeometry']['coordinates'][0][0][0] += .25
        (folder/name).write_bytes(json.dumps(value).encode('utf-8'))
    else:
        name = 'resampled_reference/nucleus.u32le.gz'
        data = bytearray(gzip.decompress((folder/name).read_bytes()))
        data[0] = 1
        (folder/name).write_bytes(gzip.compress(data, mtime=0))
    comparison, _, _ = debug.common().context(debug.CASE)
    path = folder/'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    manifest['files'][name] = dict(bytes=(folder/name).stat().st_size, sha256=comparison.sha(folder/name))
    path.write_bytes(json.dumps(manifest).encode('utf-8'))
    with pytest.raises(ValueError, match='input request/mapping|optical densities|RGB input format|recipe differs|exported geometry differs|ordinary labels differ'):
        debug.verify(folder)


@pytest.mark.parametrize('change', ['realized_step', 'off_lattice', 'open', 'diagonal', 'overlap', 'label'])
def test_processing_geometry_rejects_wrong_lattice_or_hidden_topology(change):
    comparison, _, _ = debug.common().context(debug.CASE)
    features = copy.deepcopy(comparison.read(debug.FIXTURE/debug.CASE/'cells.geojson')['features'])
    step = 1.5
    ring = features[0]['geometry']['coordinates'][0]
    if change == 'realized_step': step = 256/171
    elif change == 'off_lattice': ring[1][0] += .125
    elif change == 'open': ring.pop()
    elif change == 'diagonal':
        ring[1][0] += 1.5
        ring[1][1] += 1.5
    elif change == 'overlap': features[1]['geometry'] = copy.deepcopy(features[0]['geometry'])
    else: features[1]['properties']['name'] = 'cell_1'
    with pytest.raises(ValueError, match='integer lattice|rectilinear|overlaps|labels/type'):
        debug.processing_labels(features, 'geometry', 171, 128, step_x=step)


@pytest.mark.parametrize('step', [Fraction(256, 171), Fraction(1), Fraction(0)])
def test_nominal_projection_does_not_infer_geometry_from_array_shape(step):
    with pytest.raises(ValueError, match='nominal projection contract'):
        debug.project_nominal(np.zeros((128, 171), np.uint32), step=step)
