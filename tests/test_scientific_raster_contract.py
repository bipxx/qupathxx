"""Independent little-endian vectors for one-read raster input identity."""
import hashlib
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
VECTOR=json.loads((ROOT/'tests/fixtures/scientific-raster-v1/canonical-input.json').read_text(encoding='utf-8'))


def test_decoded_snapshot_binds_dimensions_bounds_and_actual_rgba():
    vector=VECTOR['decoded'];h=hashlib.sha256(vector['format'].encode('ascii'))
    for value in vector['image']+vector['clipped_bounds']:h.update(struct.pack('<Q',value))
    rgba=bytes(vector['rgba_pixel'])*vector['pixel_count'];h.update(rgba)
    assert len(rgba)==vector['clipped_bounds'][2]*vector['clipped_bounds'][3]*4
    assert h.hexdigest()==vector['sha256']


def test_canonical_input_binds_grid_roi_transform_calibration_and_actual_domain():
    v=VECTOR['input'];h=hashlib.sha256(v['format'].encode('ascii'))
    def integer(value):h.update(struct.pack('<Q',value))
    def blob(value):integer(len(value));h.update(value)
    blob(VECTOR['decoded']['sha256'].encode('ascii'));blob(v['reader'].encode('utf-8'));integer(0)
    for value in v['grid_fields']+[v['kind'],v['sampling_policy']]:integer(value)
    geometry=b'QRO\x01\x00'+struct.pack('<dddd',*v['roi_rectangle'])
    blob(geometry)
    for value in v['white']+[x for row in v['stains'] for x in row]:h.update(struct.pack('<d',value))
    for value in v['calibration']:
        integer(int(value is not None))
        if value is not None:h.update(struct.pack('<d',value))
    h.update(bytes.fromhex(v['domain_hex']))
    assert h.hexdigest()==v['sha256']


def test_independent_vectors_are_exercised_by_the_native_integration_case():
    native=(ROOT/'cpp/tests/scientific_raster_tests.cpp').read_text(encoding='utf-8')
    assert VECTOR['decoded']['sha256'] in native and VECTOR['input']['sha256'] in native
