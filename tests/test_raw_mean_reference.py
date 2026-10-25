"""The numeric oracle must preserve dtype bits and regenerate native vectors."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_raw_mean_oracle', ROOT/'tools/reference_raw_means.py')
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)
FIXTURE = ROOT/'tests/fixtures/raw-scientific-means-v1/reference.json'


def test_fraction_oracle_and_flat_native_vectors_regenerate_exactly():
    data = oracle.reference()
    assert json.loads(FIXTURE.read_text(encoding='utf-8')) == data
    assert (ROOT/'cpp/tests/raw_scientific_mean_reference.hpp').read_bytes() == oracle.native_header(data).encode('utf-8')
    assert len(data['cases']) == len({c['name'] for c in data['cases']}) == 2891
    assert {c['type'] for c in data['cases']} == set(oracle.TYPES)
    assert all(0 < sum(c['weights']) <= 1 << 28 for c in data['cases'])


def test_oracle_catches_early_integer_conversion_and_double_sum_overflow():
    cases = {c['name']: c for c in oracle.reference()['cases']}
    exact = cases['u64-no-input-double-rounding']
    wanted = struct.unpack('>d', bytes.fromhex(exact['expected_binary64']))[0]
    assert wanted == 9007199254740994
    early = (float(9007199254740993)+float(9007199254740994))/2
    assert wanted != early
    assert cases['i64-cancel-extremes']['expected_binary64'] == 'bfe0000000000000'
    assert cases['f64-negative-underflow-tie']['expected_binary64'] == '8000000000000000'
    assert cases['f64-maximum-no-float-overflow']['expected_binary64'] == '7fefffffffffffff'


def test_reference_cli_requires_new_outputs_and_preserves_existing_bytes(tmp_path):
    data, header = tmp_path/'data.json', tmp_path/'native.hpp'
    command = [sys.executable, '-I', str(ROOT/'tools/reference_raw_means.py'),
               '--output', str(data), '--native-header', str(header)]
    first = subprocess.run(command, capture_output=True)
    assert first.returncode == 0, first.stderr
    saved = data.read_bytes(), header.read_bytes()
    second = subprocess.run(command, capture_output=True)
    assert second.returncode == 2 and b'outputs must be new paths' in second.stderr
    assert (data.read_bytes(), header.read_bytes()) == saved
