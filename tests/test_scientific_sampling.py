"""Installed owning bindings and float64 RGB sampling; no detector acceptance."""
from fractions import Fraction
import gc
import json
from pathlib import Path
import struct

import numpy as np
import pytest
import qupathxx as q

REFERENCE=json.loads((Path(__file__).parent/'fixtures/scientific-grid-v1/reference.json').read_text(encoding='utf-8'))


def plan(case):
    x,y,w,h=case['extent'];nx,ny=case['samples'];cx,cy=case['context']
    return q.ScientificGrid(*case['image'],q.ScientificGridAxis(x,w,nx,cx),q.ScientificGridAxis(y,h,ny,cy))


def bounds(value):
    return [value.x,value.y,value.width,value.height]


@pytest.mark.parametrize('case',REFERENCE['cases'],ids=lambda case:case['name'])
def test_owning_grid_geometry_matches_exact_reference(case):
    grid=plan(case)
    assert bounds(grid.read_bounds)==case['read_bounds']
    assert bounds(grid.clipped_read_bounds)==case['clipped_read_bounds']
    assert grid.overlap_visits==case['overlap_visits']
    assert (grid.image_width,grid.image_height)==tuple(case['image'])
    for name in ['x','y']:
        axis=getattr(grid,name)
        for sample in case[name]:
            center=axis.center(sample['index'])
            assert Fraction(center.numerator,center.denominator)==Fraction(sample['center'])
            assert struct.pack('>d',float(center)).hex()==sample['center_ieee754']
    axis=grid.x;read=grid.read_bounds;del grid;gc.collect()
    assert axis.samples==case['samples'][0] and bounds(read)==case['read_bounds']
    with pytest.raises(AttributeError):axis.origin=99
    with pytest.raises(AttributeError):read.width=99


def test_exact_fraction_conversion_includes_large_and_negative_near_zero_values():
    for item in REFERENCE['fraction_probes']:
        value=q.GridFraction(item['n'],item['d'])
        assert struct.pack('>d',float(value)).hex()==item['bits']
    assert q.GridFraction(-1,3).floor()==-1 and q.GridFraction(-1,3).ceil()==0


CASES=[case for case in REFERENCE['cases'] if not case['alpha'] and max(case['image'])<=20]


@pytest.mark.parametrize('case',CASES,ids=lambda case:case['name'])
def test_native_rgb_means_and_support_match_independent_fraction_oracle(case):
    w,h=case['image'];y,x=np.indices((h,w))
    rgb=np.stack(((17*x+3*y)%256,(5*x+19*y)%256,(11*x+7*y)%256),axis=-1).astype(np.uint8)
    server=q.dense_image_server(q.Image.from_numpy(rgb))
    grid=plan(case);result=q.read_scientific_rgb(server,grid)
    values=result.numpy();valid=result.valid_numpy()
    assert values.shape==(grid.height,grid.width,3) and values.dtype==np.float64
    assert valid.shape==(grid.height,grid.width) and valid.dtype==np.uint8
    assert not values.flags.writeable and not valid.flags.writeable
    expected=np.array([[float(Fraction(v)) for v in pixel['rgb']] if pixel['rgb'] else [0.,0.,0.]
                       for pixel in case['pixels']],dtype=np.float64).reshape(values.shape)
    np.testing.assert_array_equal(values.view(np.uint64),expected.view(np.uint64))
    np.testing.assert_array_equal(valid.ravel(),[p['valid_support'] for p in case['pixels']])
    # Views own the immutable native result independently of the server/grid wrappers.
    server.close();del result,server,grid;gc.collect()
    np.testing.assert_array_equal(values,expected)


def test_explicit_array_copies_are_mutable_and_do_not_modify_native_result():
    server=q.dense_image_server(q.Image.from_numpy(np.full((2,2,3),42,dtype=np.uint8)))
    result=q.read_scientific_rgb(server,q.ScientificGrid(2,2,q.ScientificGridAxis(0,2,1),q.ScientificGridAxis(0,2,1)))
    rgb=result.numpy(copy=True);valid=result.valid_numpy(copy=True)
    assert rgb.flags.writeable and valid.flags.writeable
    rgb[:]=0;valid[:]=0
    assert np.all(result.numpy()==42) and np.all(result.valid_numpy()==1)
    with pytest.raises(ValueError):result.numpy().setflags(write=True)
    with pytest.raises(ValueError):result.valid_numpy().setflags(write=True)
    with pytest.raises(AttributeError):result.grid.x.origin=9
    with pytest.raises(TypeError):q.ScientificRgbGrid()


def test_mismatched_or_closed_reader_and_null_server_reject():
    server=q.dense_image_server(q.Image.from_numpy(np.zeros((2,2,3),dtype=np.uint8)))
    axis=q.ScientificGridAxis(0,1,1)
    with pytest.raises(ValueError):q.read_scientific_rgb(server,q.ScientificGrid(3,2,axis,axis))
    grid=q.ScientificGrid(2,2,axis,axis)
    with pytest.raises((TypeError,ValueError)):q.read_scientific_rgb(None,grid)
    server.close()
    with pytest.raises(RuntimeError):q.read_scientific_rgb(server,grid)


@pytest.mark.parametrize('args',[(0,0,1,0),(0,1,0,0),(0,1,1,-1),(0,1,1,4097),(10**12,1,1,0)])
def test_invalid_axis_rejects_before_reader_work(args):
    with pytest.raises(ValueError):q.ScientificGridAxis(*args)
