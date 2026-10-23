"""GEOS ELF verification supports the actual SDK lib and lib64 layouts."""
import importlib.util
from pathlib import Path

import pytest

spec=importlib.util.spec_from_file_location('qpx_linux_runtime_layout',Path(__file__).resolve().parents[1]/'tools/ci/check_linux_runtime.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)


@pytest.mark.parametrize('directory',['lib','lib64'])
def test_exact_c_api_runtime_directory_is_selected(tmp_path,directory):
    destination=tmp_path/directory;destination.mkdir()
    (destination/'libgeos_c.so.1').write_bytes(b'fixture')
    assert module.geos_runtime_directory(tmp_path)==destination


def test_missing_geos_runtime_is_rejected(tmp_path):
    (tmp_path/'lib').mkdir()
    (tmp_path/'lib/libgeos.so.3.15.0').write_bytes(b'fixture')
    with pytest.raises(ValueError,match='exactly one C API runtime'):
        module.geos_runtime_directory(tmp_path)


def test_ambiguous_geos_runtime_is_rejected(tmp_path):
    for directory in ['lib','lib64']:
        destination=tmp_path/directory;destination.mkdir()
        (destination/'libgeos_c.so.1').write_bytes(b'fixture')
    with pytest.raises(ValueError,match='exactly one C API runtime'):
        module.geos_runtime_directory(tmp_path)
