"""Decimal transform expectations are independent of native LUT/cofactors."""
from decimal import Decimal,localcontext
from fractions import Fraction as F
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1];FOLDER=ROOT/'tests/fixtures/scientific-channels-v1'
REFERENCE=json.loads((FOLDER/'reference.json').read_text(encoding='utf-8'))
spec=importlib.util.spec_from_file_location('qpx_channel_reference',ROOT/'tools/reference_scientific_channels.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)


def number(bits):
    return struct.unpack('>d',bytes.fromhex(bits))[0]


def test_generator_sampler_contract_and_cpp_expectations_are_byte_bound():
    manifest=json.loads((FOLDER/'manifest.json').read_text(encoding='utf-8'))
    for key in ['generator','contract','native_header']:
        assert hashlib.sha256((ROOT/manifest[key]).read_bytes()).hexdigest()==manifest[key+'_sha256']
    raw=(FOLDER/'reference.json').read_bytes();assert len(raw)==manifest['files']['reference.json']['bytes']
    assert hashlib.sha256(raw).hexdigest()==manifest['files']['reference.json']['sha256']
    assert hashlib.sha256((ROOT/'tests/fixtures/scientific-grid-v1/reference.json').read_bytes()).hexdigest()==REFERENCE['sampler_reference_sha256']
    assert REFERENCE==oracle.reference() and manifest['cases']==len(REFERENCE['cases'])==44
    assert (ROOT/manifest['native_header']).read_bytes()==oracle.native_header(REFERENCE).encode('utf-8')


def test_decimal_inverse_solves_the_original_columns():
    with localcontext() as ctx:
        ctx.prec=60
        for profile in oracle.profiles():
            rows=oracle.inverse(profile['basis'])
            for r in range(3):
                for c in range(3):
                    value=sum(Decimal.from_float(float(profile['basis'][k][r]))*rows[k][c] for k in range(3))
                    assert abs(value-Decimal(int(r==c)))<Decimal('1e-57')


def test_reference_retains_signed_channels_and_zero_validity_separately():
    signed=next(v for v in REFERENCE['cases'] if v['name']=='identity-signed-residual')
    assert any(number(v)<0 for v in signed['stain_bits'])
    invalid=next(v for v in REFERENCE['cases'] if v['name']=='invalid-override-identity')
    assert invalid['valid']==[0] and all(number(v)==0 for v in invalid['od_bits']+invalid['stain_bits'])
    clamp=next(v for v in REFERENCE['cases'] if v['name']=='subunit-white-clamp-identity')
    assert number(clamp['od_bits'][0])==number(clamp['od_bits'][1]) and number(clamp['od_bits'][2])==0


def test_capture_never_overwrites_an_existing_output(tmp_path):
    output=tmp_path/'old.json';output.write_bytes(b'keep');header=tmp_path/'new.hpp'
    result=subprocess.run([sys.executable,ROOT/'tools/reference_scientific_channels.py','--output',output,'--native-header',header],capture_output=True)
    assert result.returncode!=0 and output.read_bytes()==b'keep' and not header.exists()
