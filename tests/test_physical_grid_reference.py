"""Independent numerical contract; no native detector or biology acceptance."""
from fractions import Fraction as F
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'tests/fixtures/physical-grid-v1'
REFERENCE=json.loads((FOLDER/'reference.json').read_text(encoding='utf-8'))
spec=importlib.util.spec_from_file_location('qpx_physical_oracle',ROOT/'tools/reference_physical_grid.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)


def number(bits):
    return struct.unpack('>d',bytes.fromhex(bits))[0]


def test_reference_generator_contract_and_native_expectations_are_byte_bound():
    manifest=json.loads((FOLDER/'manifest.json').read_text(encoding='utf-8'))
    for key in ['generator','contract','native_header']:
        assert hashlib.sha256((ROOT/manifest[key]).read_bytes()).hexdigest()==manifest[key+'_sha256']
    raw=(FOLDER/'reference.json').read_bytes()
    assert len(raw)==manifest['files']['reference.json']['bytes']
    assert hashlib.sha256(raw).hexdigest()==manifest['files']['reference.json']['sha256']
    assert REFERENCE==oracle.reference()
    assert (ROOT/manifest['native_header']).read_bytes()==oracle.native_header(REFERENCE).encode('utf-8')
    assert (manifest['distance_cases'],manifest['gaussian_cases'],manifest['wide_comparisons'])==(36,9,517)


def test_anisotropic_metric_changes_the_nearest_site_instead_of_using_a_mean():
    case=next(v for v in REFERENCE['distance'] if v['name']=='anisotropic-owner')
    assert case['nearest'][0]==10 and number(case['squared_bits'][0])==1
    assert min(((i%5)**2+(i//5)**2,i) for i in case['sites'])[1]==2


def test_nearest_site_is_preserved_when_rounded_total_costs_are_indistinguishable():
    case=next(v for v in REFERENCE['distance'] if v['name']=='extreme-x-metric-owner')
    assert len({case['squared_bits'][y*3+1] for y in range(8)})==1
    assert [case['nearest'][y*3+1] for y in range(8)]==[y*3 for y in range(8)]


def test_no_sites_and_virtual_outside_have_distinct_explicit_meanings():
    case=next(v for v in REFERENCE['distance'] if v['name']=='none')
    assert all(number(v)==float('inf') for v in case['squared_bits'])
    assert set(case['nearest'])=={2**32-1}
    outside=next(v for v in REFERENCE['distance'] if v['outside'] and v['width']==v['height']==1 and F(v['sx'])==F(1,2))
    assert number(outside['squared_bits'][0])==.25


def test_normalized_gaussian_preserves_constants_without_publishing_invalid_centers():
    case=next(v for v in REFERENCE['gaussian'] if v['name']=='constant-with-holes')
    assert [number(v) for v in case['expected_bits']]==[1.75 if v else 0 for v in case['valid']]
    zero=next(v for v in REFERENCE['gaussian'] if v['name']=='zero')
    assert [number(v) for v in zero['expected_bits']]==[float(F(v)) if valid else 0 for v,valid in zip(zero['values'],zero['valid'])]
    assert REFERENCE['gaussian_absolute_tolerance']==REFERENCE['gaussian_relative_tolerance']==2e-12


def test_capture_rejects_either_existing_output_before_creating_the_other(tmp_path):
    header=tmp_path/'existing.hpp';header.write_bytes(b'keep')
    output=tmp_path/'new.json'
    result=subprocess.run([sys.executable,ROOT/'tools/reference_physical_grid.py','--output',output,'--native-header',header],capture_output=True)
    assert result.returncode!=0 and not output.exists() and header.read_bytes()==b'keep'
