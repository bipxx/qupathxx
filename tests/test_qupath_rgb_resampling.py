"""Independent public-API RGB capture constrains future compatibility samplers."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil

import numpy as np
from PIL import Image
import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_rgb_capture', ROOT / 'tools/qupath_rgb_resampling.py')
rgb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rgb)
CONTRACT = rgb.read(rgb.FIXTURE / 'contract.json')
IDENTITIES = [(case['name'], request['name']) for case in CONTRACT['cases'] for request in case['requests']]


@pytest.fixture(scope='module')
def characterized():
    report = rgb.compare(native=True)
    assert len(report['cases']) == 36
    assert not report['sampler_equivalence_accepted'] and not report['biological_validation']
    assert not report['native_sampling_changed'] and not report['general_speed_claim']
    return {(v['case_name'], v['request_name']): v for v in report['cases']}


@pytest.mark.parametrize('identity', IDENTITIES, ids=lambda v: '-'.join(v))
def test_capture_shape_request_identity_and_declared_models(characterized, identity):
    row = characterized[identity]
    assert [row['width'], row['height']] == row['shape_round_half_up']
    assert row['server_class'] == 'qupath.lib.images.servers.WrappedBufferedImageServer'
    assert row['buffered_image_type'] == 1
    assert set(row['models']) == {mode + '_' + kernel for mode in ('nominal', 'realized')
                                 for kernel in ('nearest', 'half_up', 'ties_even')}
    assert all(v['changed_components'] >= 0 and 0 <= v['maximum_error'] <= 255 for v in row['models'].values())


@pytest.mark.parametrize('name', ['gradient', 'checker', 'noise', 'cells'])
def test_identity_pixels_match_both_native_area_and_captured_input(characterized, name):
    row = characterized[name, 'full_1']
    assert all(v['changed_components'] == 0 for v in row['models'].values())
    assert row['native_rgba8_area'] == dict(changed_components=0, maximum_error=0)


@pytest.mark.parametrize('name', ['gradient', 'checker', 'noise', 'cells'])
def test_enlargement_is_nominal_nearest_not_general_bilinear(characterized, name):
    row = characterized[name, 'full_0_75']
    assert row['models']['nominal_nearest']['changed_components'] == 0
    assert row['models']['nominal_half_up']['changed_components'] > 0
    assert row['models']['realized_nearest']['changed_components'] > 0


@pytest.mark.parametrize('name', ['gradient', 'checker', 'noise', 'cells'])
@pytest.mark.parametrize('request_name', ['full_1_25', 'full_1_5', 'full_3_75', 'crop_1_5'])
def test_exact_dyadic_reduction_candidates_on_captured_inputs(characterized, name, request_name):
    assert characterized[name, request_name]['models']['nominal_half_up']['changed_components'] == 0


@pytest.mark.parametrize('name', ['gradient', 'checker', 'noise'])
def test_odd_integer_reduction_has_black_destination_edge(characterized, name):
    row = characterized[name, 'full_2']
    assert [row['width'], row['height']] == [9, 7]
    pixels = np.asarray(Image.open(rgb.FIXTURE / name / 'full_2.png'))
    assert np.all(pixels[-1] == 0) and np.all(pixels[:, -1] == 0)
    assert row['models']['nominal_half_up']['changed_components'] > 0
    assert row['native_rgba8_area']['changed_components'] > 0


@pytest.mark.parametrize('name', ['gradient', 'checker', 'noise', 'cells'])
@pytest.mark.parametrize('request_name', ['full_2_3', 'crop_2_3'])
def test_nondyadic_reduction_keeps_one_byte_model_discrepancies(characterized, name, request_name):
    model = characterized[name, request_name]['models']['nominal_half_up']
    assert model['changed_components'] > 0 and model['maximum_error'] == 1


@pytest.mark.parametrize('name', ['contract.json', 'reference.json', 'noise/input.png',
                                'cells/full_1_5.png', 'gradient/full_2.png'])
def test_capture_payload_mutation_is_rejected(tmp_path, name):
    folder = tmp_path / 'capture'
    shutil.copytree(rgb.FIXTURE, folder)
    with (folder / name).open('ab') as stream:
        stream.write(b'!')
    with pytest.raises(ValueError, match='capture bytes changed'):
        rgb.verify_capture(folder)


@pytest.mark.parametrize('change', ['schema', 'upstream', 'generator', 'inventory', 'extra_file'])
def test_mixed_capture_identity_is_rejected(tmp_path, change):
    folder = tmp_path / 'capture'
    shutil.copytree(rgb.FIXTURE, folder)
    path = folder / 'manifest.json'
    value = rgb.read(path)
    if change == 'schema': value['schema'] = 'other'
    elif change == 'upstream': value['upstream']['revision'] = '0' * 40
    elif change == 'generator': value['generator_sha256'] = '0' * 64
    elif change == 'inventory': value['files'].pop('reference.json')
    else: (folder / 'unregistered.png').write_bytes(b'not a capture')
    path.write_text(json.dumps(value), encoding='utf-8')
    with pytest.raises(ValueError, match='capture identity differs'):
        rgb.verify_capture(folder)


def test_rehashed_false_reference_metadata_is_rejected(tmp_path):
    folder = tmp_path / 'capture'
    shutil.copytree(rgb.FIXTURE, folder)
    reference = rgb.read(folder / 'reference.json')
    reference['java_version'] = 'different-runtime'
    data = json.dumps(reference).encode()
    (folder / 'reference.json').write_bytes(data)
    manifest = rgb.read(folder / 'manifest.json')
    manifest['files']['reference.json'] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    (folder / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
    with pytest.raises(ValueError, match='reference identities differ'):
        rgb.verify_capture(folder)
