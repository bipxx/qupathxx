"""JPEG TIFF decoded-sample comparisons, independent of the native codec build."""
import importlib.util
import os
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image
import pytest
import qupathxx as q

spec=importlib.util.spec_from_file_location('jpeg_fixture',Path(__file__).resolve().parents[1]/'tools/reference_jpeg_tiff.py')
fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
pytestmark=pytest.mark.skipif(not q.has_tiff,reason='optional native TIFF reader')


def request(channels=1,*,outside=False):
    r=q.RawRegionRequest();r.channels=channels
    r.region=q.RegionRequest(-2 if outside else 0,-3 if outside else 0,0,44 if outside else 39,40 if outside else 35)
    return r


@pytest.mark.parametrize('rgb',[False,True])
@pytest.mark.parametrize('tiled',[False,True])
@pytest.mark.parametrize('tables',[False,True])
def test_scalar_fixture_matches_independent_tiff_decode(tmp_path,rgb,tiled,tables):
    path=tmp_path/'jpeg.tif';expected,_=fixture.generate(path,rgb=rgb,tiled=tiled,tables=tables)
    with Image.open(path) as image:np.testing.assert_array_equal(np.asarray(image),expected)


@pytest.mark.parametrize('rgb',[False,True])
@pytest.mark.parametrize('tiled',[False,True])
@pytest.mark.parametrize('tables',[False,True])
def test_jpeg_raw_preview_and_owned_cache(tmp_path,rgb,tiled,tables):
    path=tmp_path/'jpeg.ome.tif';expected,_=fixture.generate(path,rgb=rgb,tiled=tiled,tables=tables)
    expected=expected if rgb else expected[...,None]
    options=q.TiffOpenOptions();options.cache_bytes=1024*1024
    server=q.open_tiff(str(path),options);meta=server.raw_metadata
    assert (meta.width,meta.height,meta.channels,meta.pixel_width_um,meta.pixel_height_um)==(39,35,expected.shape[-1],.25,.5)
    raw=server.read_raw(request(expected.shape[-1]));owned=raw.numpy()
    np.testing.assert_array_equal(owned,expected);assert raw.valid_numpy().all()
    decoded=server.tiff_read_stats.decoded_blocks
    preview=server.read_region(q.RegionRequest(0,0,0,39,35)).numpy()
    np.testing.assert_array_equal(preview[...,:3],expected if rgb else np.repeat(expected,3,axis=-1))
    assert (preview[...,3]==255).all() and server.tiff_read_stats.decoded_blocks==decoded
    padded=server.read_raw(request(expected.shape[-1],outside=True));a=padded.numpy();valid=padded.valid_numpy().astype(bool)
    assert valid.sum()==39*35 and not a[~valid].any()
    np.testing.assert_array_equal(a[3:38,2:41],expected)
    server.close();del raw,server
    np.testing.assert_array_equal(owned,expected);assert not owned.flags.writeable


def test_jpeg_big_endian_white_keeps_raw_samples(tmp_path):
    path=tmp_path/'white.tif';expected,_=fixture.generate(path,tables=True,byteorder='>',white=True)
    server=q.open_tiff(str(path))
    np.testing.assert_array_equal(server.read_raw(request()).numpy()[...,0],expected)
    preview=server.read_region(q.RegionRequest(0,0,0,39,35)).numpy()
    np.testing.assert_array_equal(preview[...,0],255-expected);server.close()


@pytest.mark.parametrize('failure',['jpeg_header','decode_budget'])
def test_jpeg_decode_failure_and_budget_are_explicit(tmp_path,failure):
    path=tmp_path/'bad.tif';_,offsets=fixture.generate(path,tiled=True)
    options=q.TiffOpenOptions()
    if failure=='jpeg_header':
        data=bytearray(path.read_bytes());data[offsets[0]:offsets[0]+2]=b'\0\0';path.write_bytes(data)
    else:options.decode_bytes=128
    with pytest.raises((RuntimeError,ValueError)):
        server=q.open_tiff(str(path),options);server.read_raw(request())


@pytest.mark.skipif(not q.has_project,reason='project storage')
@pytest.mark.parametrize('rgb',[False,True])
def test_jpeg_project_analysis_replay_and_region_cli(tmp_path,rgb):
    path=tmp_path/'jpeg.tif';expected,_=fixture.generate(path,rgb=rgb,tiled=True,tables=True)
    project=q.Project();image=project.add_tiff_image(str(path));document=project.document(image)
    obj=document.add_rectangle(q.Rect(0,0,39,35),'JPEG decoded values')
    capture=q.RawAnalysisRequest(document,request(3 if rgb else 1),obj,tile_size=16)
    result=q.analyze_raw(capture);record=document.keep_raw_analysis(capture,result)
    saved=tmp_path/'jpeg.qpxproj';project.save(str(saved))
    restored=q.Project.open(str(saved));current=restored.document(image)
    replay=q.RawAnalysisRequest.replay(current,record);actual=q.analyze_raw(replay)
    assert current.verify_raw_analysis(record,replay,actual) and actual.input_sha256==result.input_sha256
    expected=expected if rgb else expected[...,None]
    np.testing.assert_allclose(result.statistics.means,expected.mean(axis=(0,1)),rtol=2e-15,atol=0)
    import qupathxx._native as native
    executable=Path(native.__file__).parent/('qupathxx_region.exe' if os.name=='nt' else 'qupathxx_region')
    output=tmp_path/'raw.npy'
    done=subprocess.run([str(executable),'--raw',str(path),str(output),'--channels',str(expected.shape[-1]),'--width','39','--height','35'],capture_output=True,text=True,timeout=30)
    assert done.returncode==0,done.stderr+done.stdout
    np.testing.assert_array_equal(np.load(output),expected)


def test_jpeg_package_contains_decoder_notices():
    import qupathxx._native as native
    folder=Path(native.__file__).parent/'licenses/tiff-jpeg'
    assert 'Independent JPEG Group' in (folder/'NOTICE.txt').read_text()
    assert 'libjpeg-turbo' in (folder/'LICENSE.md').read_text()
    assert 'Thomas G. Lane' in (folder/'README.ijg').read_text()
