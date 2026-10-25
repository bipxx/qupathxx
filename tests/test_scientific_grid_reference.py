"""Math-only preparation; these tests do not qualify a native detector."""
from fractions import Fraction as F
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'tests/fixtures/scientific-grid-v1'
REFERENCE=json.loads((FOLDER/'reference.json').read_text(encoding='utf-8'))
spec=importlib.util.spec_from_file_location('qpx_scientific_grid_reference',ROOT/'tools/reference_scientific_grid.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)


def case(name):
    return next(c for c in REFERENCE['cases'] if c['name']==name)


def test_reference_contract_generator_and_data_are_byte_bound():
    manifest=json.loads((FOLDER/'manifest.json').read_text(encoding='utf-8'))
    for key in ['generator','contract']:
        assert hashlib.sha256((ROOT/manifest[key]).read_bytes()).hexdigest()==manifest[key+'_sha256']
    data=(FOLDER/'reference.json').read_bytes()
    assert len(data)==manifest['files']['reference.json']['bytes']
    assert hashlib.sha256(data).hexdigest()==manifest['files']['reference.json']['sha256']
    assert REFERENCE==oracle.reference()
    assert manifest['cases']==14 and manifest['rounding_probes']==8
    header=(ROOT/manifest['native_header']).read_bytes()
    assert hashlib.sha256(header).hexdigest()==manifest['native_header_sha256']
    assert header==oracle.native_header(REFERENCE).encode('utf-8')
    assert manifest['fraction_probes']==len(REFERENCE['fraction_probes'])==521


def test_identity_has_hand_calculated_centers_and_rgb():
    identity=case('identity')
    assert identity['x'][0]['center']=='5/2' and identity['y'][0]['center']=='7/2'
    assert identity['pixels'][0]['rgb']==['43/1','67/1','43/1']
    assert identity['read_bounds']==identity['clipped_read_bounds']==[2,3,3,2]
    assert identity['overlap_visits']==6


def test_exact_overlap_mass_and_source_coverage():
    for item in REFERENCE['cases']:
        for label,extent,samples in [('x',item['extent'][2],item['samples'][0]),('y',item['extent'][3],item['samples'][1])]:
            for sample in item[label]:
                assert sum(F(w) for _,w in sample['weights'])==F(extent,samples)
                assert [p for p,_ in sample['weights']]==list(range(sample['first'],sample['stop']))
                assert all(F(w)>0 for _,w in sample['weights'])
                assert struct.pack('>d',float(F(sample['center']))).hex()==sample['center_ieee754']


def test_enumerated_work_matches_independent_boundary_count_formula():
    for item in REFERENCE['cases']:
        visits=[]
        for origin,extent,samples,context in zip(item['extent'][:2],item['extent'][2:],item['samples'],item['context']):
            a,b=-context,samples+context
            touched=math.ceil(F(origin)+F(b*extent,samples))-math.floor(F(origin)+F(a*extent,samples))
            period=samples//math.gcd(extent,samples)
            integer_boundaries=(b-1)//period-a//period
            visits.append(touched+(b-a-1)-integer_boundaries)
        assert visits[0]*visits[1]==item['overlap_visits']


def test_context_retains_global_phase_instead_of_resizing_snapped_read_window():
    item=case('context-phase')
    assert item['read_bounds']==[0,-1,13,15]
    assert item['clipped_read_bounds']==[0,0,13,14]
    assert F(item['x'][0]['center'])==F(11,6)
    snapped_center=F(item['read_bounds'][0])+F(item['read_bounds'][2],2*(item['samples'][0]+2*item['context'][0]))
    assert snapped_center==F(13,10) and snapped_center!=F(item['x'][0]['center'])


def test_half_open_roi_includes_minimum_and_excludes_maximum_centers():
    item=case('half-open-roi')
    assert sum(p['eligible'] for p in item['pixels'])==6
    assert next(p for p in item['pixels'] if p['index']==[0,0])['eligible']
    assert not next(p for p in item['pixels'] if p['index']==[2,0])['eligible']
    assert not next(p for p in item['pixels'] if p['index']==[0,3])['eligible']


def test_nonopaque_and_missing_support_are_never_renormalized():
    partial,transparent=case('partial-alpha'),case('transparent-alpha')
    assert [p['valid_support'] for p in partial['pixels']]==[False,False,True,True]
    assert [p['valid_support'] for p in transparent['pixels']]==[False,False,True,True]
    assert all(p['rgb'] is None for p in partial['pixels'] if not p['valid_support'])
    missing=case('no-source-support')
    assert missing['clipped_read_bounds']==[4,4,0,0]
    assert all(not p['valid_support'] and not p['eligible'] and p['rgb'] is None for p in missing['pixels'])


def test_axis_calibration_is_not_replaced_by_an_average_or_unknown_zero():
    assert case('anisotropic-calibration')['effective_spacing_um']==['7/6','5/8']
    assert case('isotropic-physical-spacing')['effective_spacing_um']==['1/1','1/1']
    unknown=case('one-unknown-axis')
    assert unknown['effective_spacing_um']==[None,'5/8'] and not unknown['physical_parameters_admissible']
    assert case('anisotropic-calibration')['physical_parameters_admissible']


def test_fractional_rgb_information_would_be_lost_by_display_byte_rounding():
    values=[F(v) for p in case('fractional-reduction')['pixels'] for v in p['rgb']]
    assert any(v.denominator!=1 for v in values)
    assert any(F(round(float(v)))!=v for v in values)


def test_large_origin_probes_reject_product_first_double_rounding():
    for probe in REFERENCE['rounding_probes']:
        exact=F(probe['center'])
        assert probe['origin']+probe['extent']<=10**12
        assert float(exact).hex()==probe['center_binary64']
        assert probe['center_binary64']!=probe['incorrect_product_first_binary64']
        whole=exact.numerator//exact.denominator
        remainder=exact.numerator-whole*exact.denominator
        assert float(whole)+float(remainder)/exact.denominator==float(exact)
