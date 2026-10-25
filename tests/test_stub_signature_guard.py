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
