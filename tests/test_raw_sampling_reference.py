"""Independent typed footprint, validity and canonical provenance fixtures."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_raw_sampling_reference', ROOT/'tools/reference_raw_sampling.py')
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)
FIXTURE = ROOT/'tests/fixtures/raw-scientific-sampling-v1/reference.json'


def test_all_dtype_fraction_overlaps_hashes_and_native_table_regenerate():
    data = reference.reference()
    assert json.loads(FIXTURE.read_text(encoding='utf-8')) == data
    assert (ROOT/'cpp/tests/raw_scientific_sampling_reference.hpp').read_bytes() == reference.native_header(data).encode('utf-8')
    assert len(data['cases']) == len({c['name'] for c in data['cases']}) == 94
    assert sum(len(c['reasons']) for c in data['cases']) == 3732
    assert {c['type'] for c in data['cases']} == set(reference.TYPES)
    cases = {c['name']: c for c in data['cases']}
    assert cases['f64-nan-payload']['reasons'] == [0, 3, 0]
    assert cases['f64-invalid-before-nan']['reasons'] == [2, 2, 2]
    assert cases['f64-unselected-nan']['reasons'] == [0, 0]
    assert cases['f64-signed-underflow']['expected_binary64'] == ['8000000000000000']


def test_input_identity_keeps_bits_hidden_by_rounded_means():
    data = reference.reference()
    case = next(c for c in data['cases'] if c['name'] == 'u64-integer-pair')
    raw = [int(v, 16) for v in data['banks']['u64']]
    for index, value in case['overrides']: raw[index] = int(value, 16)
    before = reference.expected(case, raw, set())
    old_hash = reference.identities(case, raw, set())
    raw[3] ^= 1
    assert reference.expected(case, raw, set()) == before
    assert reference.identities(case, raw, set()) != old_hash
    assert before[0] == ['4340000000000001']


def test_sampling_reference_cli_preserves_existing_outputs(tmp_path):
    data, header = tmp_path/'data.json', tmp_path/'native.hpp'
    command = [sys.executable, '-I', str(ROOT/'tools/reference_raw_sampling.py'), '--output', str(data), '--native-header', str(header)]
    result = subprocess.run(command, capture_output=True)
    assert result.returncode == 0, result.stderr
    saved = data.read_bytes(), header.read_bytes()
    result = subprocess.run(command, capture_output=True)
    assert result.returncode == 2 and b'outputs must be new paths' in result.stderr
    assert (data.read_bytes(), header.read_bytes()) == saved
