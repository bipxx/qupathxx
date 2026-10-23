"""Installed SDK layout contracts, including the observed manylinux lib64 case."""
import importlib.util
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_opencv_sdk', ROOT/'tools/build_opencv_sdk.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def config(folder, complete=True):
    folder.mkdir(parents=True,exist_ok=True)
    (folder/'OpenCVConfig.cmake').write_text('# fixture\n',encoding='utf-8')
    if complete:
        (folder/'OpenCVConfig-version.cmake').write_text('# fixture\n',encoding='utf-8')


@pytest.mark.parametrize('relative',['.','lib/cmake/opencv4','lib64/cmake/opencv4'])
def test_supported_installed_config_locations(tmp_path, relative):
    expected=tmp_path/relative
    config(expected)
    assert sdk.cmake_config_directory(tmp_path)==expected


@pytest.mark.parametrize('incomplete',[False,True])
def test_missing_or_incomplete_config_is_rejected(tmp_path, incomplete):
    if incomplete:
        config(tmp_path/'lib64/cmake/opencv4',complete=False)
    with pytest.raises(ValueError,match='exactly one complete'):
        sdk.cmake_config_directory(tmp_path)


def test_ambiguous_sdk_configs_are_rejected(tmp_path):
    config(tmp_path/'lib/cmake/opencv4')
    config(tmp_path/'lib64/cmake/opencv4')
    with pytest.raises(ValueError,match='exactly one complete'):
        sdk.cmake_config_directory(tmp_path)
