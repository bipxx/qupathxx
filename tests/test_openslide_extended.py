"""Opt-in full vendor pyramid checks against the independent OpenSlide binding."""
import ctypes
import gc
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native

pytestmark=pytest.mark.skipif(not os.environ.get('QUPATHXX_APERIO_MULTILEVEL_FIXTURE'),reason='opt-in full Aperio pyramid')


@pytest.fixture(scope='module')
def vendor_slide():
    assert q.has_openslide, 'extended reader validation requires an OpenSlide-enabled build'
    path=Path(os.environ['QUPATHXX_APERIO_MULTILEVEL_FIXTURE']).resolve()
    manifest=json.loads((Path(__file__).parent/'fixtures/manifest.json').read_text(encoding='utf-8'))
    entry=next(item for item in manifest['downloads'] if item['filename']=='CMU-1.svs')
    with path.open('rb') as source:assert hashlib.file_digest(source,'sha256').hexdigest()==entry['sha256']
    assert path.stat().st_size==entry['bytes']
    directory=os.add_dll_directory(str(Path(_native.__file__).parent)) if sys.platform=='win32' else None
    try:
        import openslide as api
        assert api.__library_version__==q.build_environment().openslide_version
        yield path,api
    finally:
        if directory:directory.close()


def rounded_reference(reference,api,x,y,level,width,height):
    """Independent C buffer plus the declared nearest-integer straight-alpha policy.

    OpenSlide-Python's Image conversion truncates partial-alpha RGB values;
    reading the premultiplied C buffer avoids silently adopting that policy.
    """
    packed=np.empty((height,width),dtype='uint32')
    api.lowlevel._read_region(reference._osr,packed.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32)),x,y,level,width,height)
    alpha=(packed>>24).astype('uint8')
    premultiplied=np.stack([(packed>>16)&255,(packed>>8)&255,packed&255],axis=-1).astype('float64')
    straight=np.divide(premultiplied*255,alpha[:,:,None],out=np.zeros_like(premultiplied),where=alpha[:,:,None]!=0)
    output=np.empty((height,width,4),dtype='uint8')
    output[:,:,:3]=np.clip(np.floor(straight+.5),0,255).astype('uint8');output[:,:,3]=alpha
    return output


def test_real_metadata_and_calibrated_measurement(vendor_slide):
    path,api=vendor_slide
    with api.OpenSlide(str(path)) as reference:
        source=q.open_slide(str(path))
        try:
            metadata=source.metadata
            assert (metadata.width,metadata.height)==reference.dimensions==(46000,32914)
            assert [(level.width,level.height) for level in metadata.levels]==list(reference.level_dimensions)
            assert [level.downsample for level in metadata.levels]==list(reference.level_downsamples)
            assert len(metadata.levels)==3 and metadata.levels[1].downsample!=4
            assert metadata.vendor=='aperio'
            assert metadata.pixel_width_um==metadata.pixel_height_um==float(reference.properties['openslide.mpp-x'])
            with pytest.raises((ValueError,RuntimeError)):
                source.read_region(q.RegionRequest(0,0,0,metadata.width,metadata.height))
            document=q.Document(source);obj=document.add_rectangle(q.Rect(20000,15000,40,30))
            expected=np.asarray(reference.read_region((20000,15000),0,(40,30)))
            assert (expected[:,:,3]==255).all()
            assert document.mean_intensity(obj)==pytest.approx(float(expected[:,:,:3].mean()),rel=1e-12)
            assert document.area_um2(obj)==pytest.approx(40*30*metadata.pixel_width_um*metadata.pixel_height_um,rel=1e-12)
        finally:source.close()


@pytest.mark.parametrize('level',[0,1,2])
def test_real_pyramid_regions_padding_and_owned_buffers(vendor_slide,level):
    path,api=vendor_slide;source=q.open_slide(str(path));held=None
    try:
        with api.OpenSlide(str(path)) as reference:
            for x,y in [(0,0),(20000,15000),(22813,16129),(-23,-11),(45983,32895)]:
                expected=rounded_reference(reference,api,x,y,level,37,29)
                actual=source.read_region(q.RegionRequest(x,y,level,37,29)).numpy()
                np.testing.assert_array_equal(actual,expected)
                python_rgba=np.asarray(reference.read_region((x,y),level,(37,29)))
                np.testing.assert_array_equal(actual[:,:,3],python_rgba[:,:,3])
                delta=actual[:,:,:3].astype('int16')-python_rgba[:,:,:3].astype('int16')
                assert ((delta>=0)&(delta<=1)).all()
                assert not delta[(actual[:,:,3]==0)|(actual[:,:,3]==255)].any()
                held=actual
            expected_last=expected.copy()
        with pytest.raises(IndexError):source.read_region(q.RegionRequest(0,0,99,1,1))
        assert source.read_region(q.RegionRequest(20000,15000,level,1,1)).width==1
    finally:source.close()
    del source;gc.collect()
    np.testing.assert_array_equal(held,expected_last)
    assert not held.flags.writeable


@pytest.mark.parametrize('level',[0,1,2])
def test_installed_region_cli_on_real_pyramid(vendor_slide,tmp_path,level):
    path,api=vendor_slide;output=tmp_path/'region.pam'
    cli=Path(_native.__file__).with_name('qupathxx_region.exe' if sys.platform=='win32' else 'qupathxx_region')
    done=subprocess.run([str(cli),str(path),'22813','16129',str(level),'71','53',str(output)],capture_output=True,timeout=30)
    assert done.returncode==0,done.stderr.decode('utf-8',errors='replace')
    with api.OpenSlide(str(path)) as reference:
        expected=rounded_reference(reference,api,22813,16129,level,71,53).tobytes()
    assert output.read_bytes().split(b'ENDHDR\n',1)[1]==expected


def test_real_scheduler_cache_generation_and_pending_close(vendor_slide):
    path,_=vendor_slide;source=q.open_slide(str(path));options=q.SchedulerOptions()
    options.cache_bytes=8*1024*1024;options.work_bytes=4*1024*1024;options.max_requests=32;options.workers=2
    scheduler=q.TileScheduler(source,options)
    try:
        region=q.RegionRequest(20000,15000,1,257,193)
        first=scheduler.submit(region);assert scheduler.wait_idle(15)
        result=scheduler.take(first);assert result.status==q.TileStatus.READY
        expected=result.pixels.data;reads=scheduler.stats.reads
        second=scheduler.submit(region);assert scheduler.wait_idle(15)
        cached=scheduler.take(second);assert cached.status==q.TileStatus.READY and cached.pixels.data==expected
        assert scheduler.stats.reads==reads and scheduler.stats.cache_hits>=1
        pending=[scheduler.submit(q.RegionRequest(20000+240*i,15000,0,257,193)) for i in range(16)]
        scheduler.advance_generation();source.close();assert scheduler.wait_idle(15)
        assert all(scheduler.take(ticket).status==q.TileStatus.STALE for ticket in pending)
        assert scheduler.stats.cache_bytes<=options.cache_bytes and scheduler.stats.reserved_bytes==0
    finally:scheduler.shutdown();source.close()
    assert scheduler.stats.stopped
    reopened=q.open_slide(str(path))
    try:assert reopened.read_region(region).data==expected
    finally:reopened.close()


def test_complete_real_overview_against_independent_global_area_mean(vendor_slide):
    path,api=vendor_slide;source=q.open_slide(str(path));options=q.ThumbnailOptions()
    options.max_width=options.max_height=1;options.source_pixel_limit=8*1024*1024
    try:
        with pytest.raises((ValueError,RuntimeError),match='source pixel limit'):
            q.read_thumbnail(source)
        thumbnail=q.read_thumbnail(source,options);plan=thumbnail.plan
        assert plan.level==2 and plan.source_pixels==2875*2057
        with api.OpenSlide(str(path)) as reference:
            pixels=rounded_reference(reference,api,0,0,2,2875,2057)
        alpha=pixels[:,:,3].astype('uint64');alpha_sum=int(alpha.sum());count=2875*2057
        expected_alpha=(alpha_sum+count//2)//count
        expected=[(int((pixels[:,:,channel].astype('uint64')*alpha).sum())+alpha_sum//2)//alpha_sum if expected_alpha else 0 for channel in range(3)]+[expected_alpha]
        np.testing.assert_array_equal(thumbnail.pixels.numpy()[0,0],expected)
        too_small=q.ThumbnailOptions();too_small.source_pixel_limit=1
        with pytest.raises((ValueError,RuntimeError)):q.read_thumbnail(source,too_small)
    finally:source.close()


def test_truncated_vendor_container_is_rejected(vendor_slide,tmp_path):
    path,_=vendor_slide;broken=tmp_path/'truncated.svs'
    with path.open('rb') as source:broken.write_bytes(source.read(8192))
    with pytest.raises(RuntimeError):q.open_slide(str(broken))


def test_damaged_jpeg_tile_reports_latched_error_and_other_handle_survives(vendor_slide,tmp_path):
    import tifffile
    path,api=vendor_slide;broken=tmp_path/'damaged.svs';shutil.copyfile(path,broken)
    with tifffile.TiffFile(path) as image:
        offset=int(image.pages[0].dataoffsets[0]);count=int(image.pages[0].databytecounts[0])
    with broken.open('r+b') as output:output.seek(offset);output.write(bytes(min(count,64)))
    damaged=q.open_slide(str(broken));healthy=q.open_slide(str(path))
    try:
        with pytest.raises(RuntimeError):damaged.read_region(q.RegionRequest(0,0,0,32,24))
        with pytest.raises(RuntimeError):damaged.read_region(q.RegionRequest(20000,15000,0,32,24))
        with api.OpenSlide(str(path)) as reference:assert healthy.read_region(q.RegionRequest(20000,15000,0,32,24)).data==reference.read_region((20000,15000),0,(32,24)).tobytes()
    finally:damaged.close();healthy.close()


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in native GUI')
def test_real_vendor_desktop_zoom_settles_with_bounded_unavailable_overview(vendor_slide,tmp_path):
    path,_=vendor_slide;source=q.open_slide(str(path))
    try:
        options=q.GuiOptions();options.settle_tiles=True
        fitted=q.show(q.Document(source),frame_limit=120,options=options)
        event=q.GuiInput();event.frame=5;event.kind=q.GuiInputKind.WHEEL
        event.x=event.y=.5;event.wheel=12;options.inputs=[event]
        zoomed=q.show(q.Document(source),frame_limit=120,capture_path=str(tmp_path/'vendor-zoom.ppm'),options=options)
        assert zoomed.final_zoom>fitted.final_zoom and zoomed.generations>fitted.generations
        for report in [fitted,zoomed]:
            assert report.uploaded_tiles>0 and report.pending_tiles==0 and report.tiles_settled
            assert report.peak_gpu_bytes<=64*1024*1024
            assert report.overview_state=='failed' and 'source pixel limit' in report.overview_error
            assert report.overview_result is None and report.overview_gpu_bytes==0
    finally:source.close()
