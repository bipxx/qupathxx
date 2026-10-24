"""Shape-stage byte provenance and dependency-free native fixture derivation."""
import importlib.util
import json
from pathlib import Path
import shutil

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_shape_reference',ROOT/'tools/compare_native_shape.py')
comparison=importlib.util.module_from_spec(spec);spec.loader.exec_module(comparison)


def test_shape_capture_and_native_derivatives_are_exact():
    contract=comparison.verify_native()
    assert len(contract['cases'])==26
    assert {c['name'] for c in contract['cases']} >= {'blank','full','row','column','border','ring','diagonal','ndpi_context0','ndpi_context32','large_disks'}


@pytest.mark.parametrize('file',['contract.json','reference.json','runtime.log','ndpi_context0/mask.u8.gz','ndpi_context32/maxima.u8.gz'])
def test_shape_capture_rejects_changed_bytes(tmp_path,file):
    shutil.copytree(comparison.FIXTURE/'capture',tmp_path/'capture');path=tmp_path/'capture'/file
    path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises((ValueError,json.JSONDecodeError)):comparison.verify_capture(tmp_path/'capture')


@pytest.mark.parametrize('key',['upstream','generator_sha256','input_generator_sha256'])
def test_shape_capture_rejects_wrong_identity(tmp_path,key):
    shutil.copytree(comparison.FIXTURE/'capture',tmp_path/'capture');path=tmp_path/'capture/manifest.json'
    value=json.loads(path.read_text());value[key]='unrecognized';path.write_text(json.dumps(value))
    with pytest.raises(ValueError,match='identity differs'):comparison.verify_capture(tmp_path/'capture')


@pytest.mark.parametrize('file',['index.txt','ndpi_context0.input.u8','ndpi_context32.mask.u8','large_disks.maxima.u8'])
def test_native_fixture_rejects_changed_bytes(tmp_path,file):
    shutil.copytree(comparison.FIXTURE/'native',tmp_path/'native');path=tmp_path/'native'/file
    path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises(ValueError,match='bytes changed'):comparison.verify_native(target=tmp_path/'native')
