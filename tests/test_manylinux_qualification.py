"""ABI requirements must be satisfied by the declared system policy or wheel."""
import importlib.util
from pathlib import Path

import pytest

spec=importlib.util.spec_from_file_location('qpx_manylinux_qualification',Path(__file__).resolve().parents[1]/'tools/ci/qualify_manylinux_wheel.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
check=module.requirement_errors
system={'libc.so.6','libstdc++.so.6','libgcc_s.so.1'}
allowed={'GLIBC_2.2.5','GLIBC_2.28','GLIBCXX_3.4.24','CXXABI_1.3.11','GCC_3.0'}


def test_system_and_bundled_requirements_are_satisfied_together():
    assert check(['libc.so.6','libgeos_c.so.1'],{'libc.so.6':['GLIBC_2.28']},{'libgeos_c.so.1'},system,allowed)==[]


@pytest.mark.parametrize('library,version',[('libc.so.6','GLIBC_2.29'),('libstdc++.so.6','GLIBCXX_3.4.25'),('libstdc++.so.6','CXXABI_1.3.13'),('libc.so.6','GLIBC_PRIVATE')])
def test_new_or_private_system_versions_are_not_qualified(library,version):
    assert check([library],{library:[version]},set(),system,allowed)==[library+': unsupported version '+version]


def test_unbundled_sdk_dependency_is_not_treated_as_a_system_library():
    assert check(['libgeos_c.so.1'],{},set(),system,allowed)==['unbundled dependency: libgeos_c.so.1']


def test_private_library_versions_do_not_bypass_missing_dependency_check():
    assert check(['libprivate.so.1'],{'libprivate.so.1':['PRIVATE_4.0']},set(),system,allowed)==['unbundled dependency: libprivate.so.1']


def test_bundled_private_library_versions_are_not_compared_with_system_versions():
    assert check(['libprivate.so.1'],{'libprivate.so.1':['PRIVATE_4.0']},{'libprivate.so.1'},system,allowed)==[]
