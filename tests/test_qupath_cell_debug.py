"""The plugin's public debug mode must reproduce its ordinary output exactly."""
import importlib.util
import json
from pathlib import Path
import shutil

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_debug_verifier',ROOT/'tools/verify_qupath_cell_debug.py')
verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)


def test_debug_capture_reproduces_all_506_ordinary_nuclei():
    report=verifier.verify()
    assert report['nuclei']==506 and report['nucleus_masks_exact']
    assert report['optical_density_max_abs_error']<1e-6


@pytest.mark.parametrize('name',['debug-stack.tif','debug-stack.json','driver.log'])
def test_debug_capture_rejects_changed_bytes(tmp_path,name):
    shutil.copytree(verifier.FIXTURE/'debug',tmp_path/'debug')
    path=tmp_path/'debug'/name;path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises(ValueError,match='bytes changed'):verifier.verify(tmp_path/'debug')


def test_debug_capture_rejects_generator_identity_change(tmp_path):
    shutil.copytree(verifier.FIXTURE/'debug',tmp_path/'debug')
    path=tmp_path/'debug/manifest.json';data=json.loads(path.read_text());data['debug_generator_sha256']='unrecognized'
    path.write_text(json.dumps(data))
    with pytest.raises(ValueError,match='identity differs'):verifier.verify(tmp_path/'debug')
