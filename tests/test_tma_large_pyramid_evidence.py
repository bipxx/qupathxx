"""Fault controls for each independent large-pyramid acceptance criterion."""
import copy
import importlib.util
import json
from pathlib import Path

import pytest

spec=importlib.util.spec_from_file_location('large_tma_checker',Path(__file__).resolve().parents[1]/'tools/check_tma_large_pyramid.py')
checker=importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
C=json.loads(checker.CONTRACT.read_text(encoding='utf-8'))


def valid():
    return dict(engine=dict(revision='source',fingerprint='inputs'),image_width=C['width'],image_height=C['height'],
        plan=dict(level=3,level_downsample=64,width=480,height=352,source_width=480,source_height=352,source_pixels=168960),
        core_size=dict(width=512,height=384,width_pixels=2048,height_pixels=3072,pixel_width_um=.25,pixel_height_um=.125,calibration_from_reader=True),
        before=dict(decoded_blocks=0,decoded_bytes=0),
        after=dict(decoded_blocks=4,decoded_bytes=786432,cache_bytes=787456,peak_cache_bytes=787456),
        payload_bytes=2892,estimated_work_bytes=2787328,memory=dict(peak_working_set_bytes=33435648),
        cores=[dict(center=c['center'].copy(),observed=c['present'],missing=not c['present']) for c in C['cores']])


def test_complete_protocol_accepts():
    assert checker.assess(valid(),C,'source','inputs')==[]


@pytest.mark.parametrize('path,value',[
    (('engine','revision'),'other'),(('engine','fingerprint'),'other'),
    (('image_width',),960),(('image_height',),704),
    (('plan','level'),0),(('plan','level_downsample'),1),
    (('plan','source_width'),30720),(('plan','source_height'),22528),
    (('plan','width'),481),(('plan','height'),353),(('plan','source_pixels'),692060160),
    (('core_size','width_pixels'),32),(('core_size','pixel_height_um'),.25),
    (('core_size','calibration_from_reader'),False),
    (('before','decoded_blocks'),1),(('before','decoded_bytes'),1),
    (('after','decoded_blocks'),5),(('after','decoded_bytes'),2076180480),
    (('after','peak_cache_bytes'),1048577),(('after','cache_bytes'),1048577),
    (('estimated_work_bytes',),3145729),(('payload_bytes',),2787329),
    (('memory','peak_working_set_bytes'),536870913),
    (('memory','peak_working_set_bytes'),0),
    (('cores',0,'center'),[C['cores'][0]['center'][0]+65,C['cores'][0]['center'][1]]),
    (('cores',0,'center'),[float('nan'),0]),(('cores',0,'center'),[float('inf'),0]),
    (('cores',6,'missing'),False),(('cores',6,'observed'),True),
    (('cores',0,'observed'),False),
])
def test_wrong_observation_rejects(path,value):
    data=copy.deepcopy(valid());target=data
    for key in path[:-1]: target=target[key]
    target[path[-1]]=value
    assert checker.assess(data,C,'source','inputs')


def test_missing_or_extra_core_rejects():
    for cores in (valid()['cores'][:-1],valid()['cores']+[valid()['cores'][0]]):
        data=valid();data['cores']=cores
        assert checker.assess(data,C,'source','inputs')


def test_tolerance_boundary_is_inclusive_and_contract_unchanged():
    data=valid();data['cores'][0]['center'][0]+=64
    assert checker.assess(data,C,'source','inputs')==[]
    assert C['acceptance']['max_center_error_pixels']==64


@pytest.mark.parametrize('fault',['bytes','sha256','contract_sha256'])
def test_fixture_identity_rejects_changed_bytes_or_contract(tmp_path,fault):
    path=tmp_path/'fixture.tif';path.write_bytes(b'fixture')
    import hashlib
    manifest=dict(bytes=7,sha256=hashlib.sha256(b'fixture').hexdigest(),
        contract_sha256=hashlib.sha256(json.dumps(C,sort_keys=True,separators=(',',':')).encode()).hexdigest())
    sidecar=path.with_suffix('.tif.json')
    sidecar.write_text(json.dumps(manifest),encoding='utf-8')
    assert checker.fixture_identity(path,C)==manifest
    manifest[fault]=0 if fault=='bytes' else 'wrong'
    sidecar.write_text(json.dumps(manifest),encoding='utf-8')
    with pytest.raises(ValueError,match='Fixture bytes'):
        checker.fixture_identity(path,C)
