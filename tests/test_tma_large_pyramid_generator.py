"""Independent scalar checks of the streamed fixture, without native detection."""
import importlib.util
import json
from pathlib import Path

import numpy as np
import pytest
import tifffile

spec=importlib.util.spec_from_file_location('large_tma_fixture',Path(__file__).resolve().parents[1]/'tools/reference_tma_large_pyramid.py')
generator=importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def scene():
    return dict(width=37,height=29,tile_size=16,downsamples=[1,4],
                core_width=12,core_height=18,pixel_width_um=.25,pixel_height_um=.125,
                cores=[dict(center=[11,13],present=True),dict(center=[28,13],present=False)])


@pytest.mark.parametrize('scale',[1,4])
def test_streamed_pixels_match_independent_scalar_ellipse_and_white_padding(scale):
    contract=scene()
    width=(contract['width']+scale-1)//scale
    height=(contract['height']+scale-1)//scale
    actual=list(generator.tiles(contract,scale))
    assert len(actual)==((width+15)//16)*((height+15)//16)
    for index,tile in enumerate(actual):
        top=(index//((width+15)//16))*16
        left=(index%((width+15)//16))*16
        assert tile.flags.owndata
        for y in range(16):
            for x in range(16):
                inside=False
                if left+x<width and top+y<height:
                    dx=scale*(2*(left+x)+1)-22
                    dy=scale*(2*(top+y)+1)-26
                    inside=dx*dx*18**2+dy*dy*12**2<=12**2*18**2
                assert tuple(tile[y,x]) == ((0,0,0) if inside else (255,255,255))
    if len(actual)>1:
        saved=actual[0].copy()
        actual[-1].fill(123)
        np.testing.assert_array_equal(actual[0],saved)


def test_written_levels_calibration_and_existing_output_refusal(tmp_path):
    path=tmp_path/'scene.ome.tif'
    manifest=generator.generate(path,scene())
    assert manifest['bytes']==path.stat().st_size
    with tifffile.TiffFile(path) as source:
        assert source.is_bigtiff and source.is_ome
        assert [level.shape for level in source.series[0].levels]==[(29,37,3),(8,10,3)]
        assert 'PhysicalSizeX="0.25"' in source.ome_metadata
        assert 'PhysicalSizeY="0.125"' in source.ome_metadata
        for level,scale in zip(source.series[0].levels,[1,4],strict=True):
            stored=list(generator.tiles(scene(),scale))
            columns=(level.shape[1]+15)//16
            assembled=np.concatenate([
                np.concatenate(stored[row:row+columns],axis=1)
                for row in range(0,len(stored),columns)],axis=0)
            np.testing.assert_array_equal(level.asarray(),assembled[:level.shape[0],:level.shape[1]])
    with pytest.raises(FileExistsError):
        generator.generate(path,scene())


def test_preregistered_scene_has_large_base_and_bounded_coarse_level():
    contract=json.loads(generator.CONTRACT.read_text(encoding='utf-8'))
    assert contract['width']*contract['height']*3>2_000_000_000
    assert contract['width']//64==480 and contract['height']//64==352
    assert [i for i,c in enumerate(contract['cores']) if not c['present']]==[6]
