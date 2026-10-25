"""Reject public stub annotations that still describe unbound C++ values."""
import importlib.util
from pathlib import Path
import pytest

spec=importlib.util.spec_from_file_location('qpx_stub_generator',Path(__file__).resolve().parents[1]/'tools/generate_stubs.py')
generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)


@pytest.mark.parametrize('signature',[
    '    def digest(self) -> "std::basic_string<char>": ...',
    '    def digest(self) -> "std::__cxx11::basic_string<char>": ...',
    '    def grid(self) -> "std::optional<qpx::ScientificGrid>": ...',
    '    def read(self, value: "qpx::Image") -> object: ...',
])
def test_unbound_cpp_signatures_reject(signature):
    with pytest.raises(ValueError,match=r'unbound C\+\+ type'):
        generator.validate_native_signatures(signature)


@pytest.mark.parametrize('signature',[
    '    def digest(self) -> str: ...',
    '    def grid(self) -> ScientificGrid | None: ...',
    '    def read(self, value: Image) -> object: ...',
    '    """Documentation may discuss std::optional without exposing it."""',
])
def test_bound_python_signatures_pass(signature):
    generator.validate_native_signatures(signature)


def test_numpy_annotations_use_private_module_alias_without_rewriting_imports():
    text='import numpy\nfrom numpy.typing import NDArray\nclass Buffer:\n    def numpy(self) -> NDArray[numpy.float64]: ...\n    def valid_numpy(self) -> NDArray[numpy.uint8]: ...\n'
    refined=generator.numpy_annotation_alias(text)
    assert 'import numpy as _numpy\n' in refined and 'from numpy.typing import NDArray\n' in refined
    assert 'NDArray[_numpy.float64]' in refined and 'NDArray[_numpy.uint8]' in refined
    assert generator.numpy_annotation_alias(refined)==refined
