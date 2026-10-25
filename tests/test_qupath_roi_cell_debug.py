"""Public plugin stages bind the clipped processing domain to ordinary labels."""
import gzip
import importlib.util
import json
from pathlib import Path
import shutil

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_roi_debug', ROOT/'tools/qupath_roi_cell_debug.py')
debug = importlib.util.module_from_spec(spec)
spec.loader.exec_module(debug)


def test_public_debug_stages_reproduce_ordinary_roi_capture():
    result = debug.verify()
    assert result['processing_bounds'] == [32, 32, 192, 128]
    assert result['ordinary_labels_exact'] and result['debug_nucleus_masks_exact']
    assert result['nuclei'] == 3 and result['optical_density_max_absolute_error'] < 1e-6
    assert result['stages'][-1]['positive_pixels'] == 772
    assert [(p['x'], p['y']) for p in result['boundary_probes']] == [(32, 68), (33, 68), (32, 81), (33, 81)]
    assert all(p['label'] == 1 and p['log_response'] > 0 for p in result['boundary_probes'])
    assert not result['biological_validation']


@pytest.mark.parametrize('name', ['debug-stack.tif', 'debug-stack.json', 'driver.log',
                                  'debug-reference.groovy', 'roi_cut_hole/nucleus.u32le.gz'])
def test_debug_capture_rejects_changed_bytes(tmp_path, name):
    folder = tmp_path/'capture'
    shutil.copytree(debug.FIXTURE, folder)
    path = folder/name
    path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises(ValueError, match='capture bytes changed'):
        debug.verify(folder)


@pytest.mark.parametrize('change', ['case', 'upstream', 'files', 'generator'])
def test_debug_capture_rejects_changed_identity(tmp_path, change):
    folder = tmp_path/'capture'
    shutil.copytree(debug.FIXTURE, folder)
    path = folder/'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    if change == 'case': manifest['case'] = 'isolated_three'
    elif change == 'upstream': manifest['upstream']['revision'] = 'unknown'
    elif change == 'files': manifest['files'].pop('debug-stack.tif')
    else: manifest['base_generator_sha256'] = 'unknown'
    path.write_bytes((json.dumps(manifest)+'\n').encode('utf-8'))
    with pytest.raises(ValueError, match='capture identity differs'):
        debug.verify(folder)


@pytest.mark.parametrize('change', ['script', 'contract', 'labels', 'metadata'])
def test_debug_capture_rejects_resealed_semantic_changes(tmp_path, change):
    folder = tmp_path/'capture'
    shutil.copytree(debug.FIXTURE, folder)
    if change == 'script':
        name = 'debug-reference.groovy'
        text = (folder/name).read_text(encoding='utf-8').replace('setDebugMode(true)', 'setDebugMode(false)')
        (folder/name).write_bytes(text.encode('utf-8'))
    elif change == 'contract':
        name = 'contract.json'
        value = json.loads((folder/name).read_text(encoding='utf-8'))
        value['native']['threshold'] += 1
        (folder/name).write_bytes(json.dumps(value).encode('utf-8'))
    elif change == 'labels':
        name = 'roi_cut_hole/nucleus.u32le.gz'
        raw = bytearray(gzip.decompress((folder/name).read_bytes()))
        raw[0] = 1
        (folder/name).write_bytes(gzip.compress(raw, mtime=0))
    else:
        name = 'debug-stack.json'
        value = json.loads((folder/name).read_text(encoding='utf-8'))
        value['width'] += 1
        (folder/name).write_bytes(json.dumps(value).encode('utf-8'))
    comparison, _, _ = debug.context()
    path = folder/'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    manifest['files'][name] = dict(bytes=(folder/name).stat().st_size, sha256=comparison.sha(folder/name))
    path.write_bytes(json.dumps(manifest).encode('utf-8'))
    with pytest.raises(ValueError, match='recipe differs|ordinary labels differ|processing extent/stages'):
        debug.verify(folder)
