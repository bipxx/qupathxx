"""Installed float64 RGB -> OD/stains, owned views and captured grid/basis."""
import gc
import json
from pathlib import Path
import struct
import numpy as np
import pytest
import qupathxx as q

REFERENCE=json.loads((Path(__file__).parent/'fixtures/scientific-channels-v1/reference.json').read_text(encoding='utf-8'))
CASES=[v for v in REFERENCE['cases'] if all(o[-1]==255 for o in v['overrides'])]


def input_grid(case):
    iw,ih=case['image'];y,x=np.indices((ih,iw))
    rgb=np.stack(((17*x+3*y)%256,(5*x+19*y)%256,(11*x+7*y)%256),axis=-1).astype(np.uint8)
    for px,py,r,g,b,a in case['overrides']:
        assert a==255;rgb[py,px]=[r,g,b]
    server=q.dense_image_server(q.Image.from_numpy(rgb))
    px,py,w,h=case['extent'];nx,ny=case['samples'];cx,cy=case['context']
    grid=q.ScientificGrid(iw,ih,q.ScientificGridAxis(px,w,nx,cx),q.ScientificGridAxis(py,h,ny,cy))
    return server,q.read_scientific_rgb(server,grid)


def numbers(values):
    return np.array([struct.unpack('>d',bytes.fromhex(v))[0] for v in values],dtype=np.float64)


@pytest.mark.parametrize('case',CASES,ids=lambda v:v['name'])
def test_fractional_means_flow_to_decimal_od_and_signed_stain_reference(case):
    server,source=input_grid(case);transform=q.StainTransform(case['raw_stains'],case['white'])
    charge=q.scientific_channel_work_bytes(source)
    assert charge==q.scientific_channel_work_bytes(source.grid)
    od=q.scientific_optical_densities(source,transform,work_bytes=charge)
    stains=q.scientific_deconvolve(source,transform,work_bytes=charge)
    assert od.mode==q.ScientificChannelMode.OPTICAL_DENSITY and stains.mode==q.ScientificChannelMode.STAINS
    assert od.algorithm=='qpx.grid-rgb-od/1' and stains.algorithm=='qpx.grid-rgb-stains/1'
    for result,key in [(od,'od_bits'),(stains,'stain_bits')]:
        values=result.numpy();valid=result.valid_numpy()
        assert values.dtype==np.float64 and valid.dtype==np.uint8
        assert not values.flags.writeable and not valid.flags.writeable
        np.testing.assert_allclose(values.ravel(),numbers(case[key]),atol=2e-12,rtol=2e-12)
        np.testing.assert_array_equal(valid.ravel(),case['valid'])
        assert result.white==transform.white and result.stains==transform.stains
        assert result.estimated_work_bytes==charge and result.grid.x.origin==case['extent'][0]
    values=stains.numpy();valid=stains.valid_numpy();expected=numbers(case['stain_bits'])
    server.close();del source,server,transform,stains,od,result;gc.collect()
    np.testing.assert_allclose(values.ravel(),expected,atol=2e-12,rtol=2e-12)
    assert valid.size==len(case['valid'])


def test_mean_precedes_the_logarithm_and_never_passes_through_byte_rounding():
    case=next(v for v in CASES if v['name']=='mean-before-od-identity')
    _,source=input_grid(case);transform=q.StainTransform(case['raw_stains'])
    actual=q.scientific_optical_densities(source,transform).numpy()[0,0,0]
    assert abs(actual-q.optical_density(127.5))<1e-15
    assert abs(actual-q.optical_density(128))>1e-4
    assert abs(actual-(q.optical_density(0)+q.optical_density(255))/2)>0.5


def test_mutable_copies_are_isolated_and_budget_boundaries_are_explicit():
    case=CASES[0];_,source=input_grid(case);transform=q.StainTransform(case['raw_stains'])
    charge=q.scientific_channel_work_bytes(source);result=q.scientific_deconvolve(source,transform,work_bytes=charge)
    original=result.numpy().copy();copy=result.numpy(copy=True);copy[:]=99
    valid=result.valid_numpy(copy=True);valid[:]=0
    np.testing.assert_array_equal(result.numpy(),original)
    assert result.valid_numpy().any()
    with pytest.raises(ValueError):result.numpy().setflags(write=True)
    with pytest.raises(ValueError):result.valid_numpy().setflags(write=True)
    with pytest.raises(ValueError):q.scientific_deconvolve(source,transform,work_bytes=charge-1)
    for budget in [0,2**30+1]:
        with pytest.raises(ValueError):q.scientific_optical_densities(source,transform,work_bytes=budget)
    with pytest.raises(AttributeError):result.mode=q.ScientificChannelMode.OPTICAL_DENSITY
    with pytest.raises(TypeError):q.ScientificChannels()
