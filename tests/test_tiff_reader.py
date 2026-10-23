"""Independent tifffile fixtures against the native quantitative reader."""
import gc
import subprocess
from pathlib import Path

import numpy as np
import pytest
import qupathxx as q
import tifffile

pytestmark = pytest.mark.skipif(not q.has_tiff, reason="optional native TIFF reader")


def request(w, h, *, c=1, z=0, t=0, x=0, y=0, level=0, first=0):
    r = q.RawRegionRequest()
    r.region = q.RegionRequest(x, y, level, w, h)
    r.channels, r.z, r.t, r.first_channel = c, z, t, first
    return r


@pytest.mark.parametrize('dtype', ['uint8','uint16','int8','int16','uint32','int32','uint64','int64','float32','float64'])
@pytest.mark.parametrize('layout', ['strip','tile','bigendian'])
def test_exact_native_dtype_and_owned_arrays(tmp_path, dtype, layout):
    values = np.arange(35*39, dtype=np.uint64).reshape(35,39)
    if dtype.startswith('int'): values = values.astype(np.int64)-500
    values = values.astype(dtype)
    if dtype == 'uint64': values[0,0] = 2**64-1
    if dtype == 'int64': values[0,0] = -(2**63)
    if dtype.startswith('float'):
        values /= 19
        values[0,:3] = [np.nan, np.inf, -np.inf]
    path = tmp_path/'samples.tif'
    opts = dict(tile=(16,16), compression='deflate') if layout=='tile' else dict(rowsperstrip=7)
    if layout=='bigendian': opts['byteorder']='>'
    tifffile.imwrite(path, values, photometric='minisblack', metadata=None, **opts)
    server = q.open_tiff(str(path))
    assert server.raw_metadata.channels == 1
    raw = server.read_raw(request(39,35))
    array, valid = raw.numpy(), raw.valid_numpy()
    assert array.dtype == np.dtype(dtype) and array.shape == (35,39,1)
    np.testing.assert_array_equal(array[...,0], values)
    assert valid.all() and not array.flags.writeable and not valid.flags.writeable
    copy = raw.numpy(copy=True)
    copy[2,2,0] = 0
    assert copy.flags.writeable and array[2,2,0] == values[2,2]
    server.close()
    del server, raw
    gc.collect()
    np.testing.assert_array_equal(array[...,0], values)


@pytest.mark.parametrize('planar', ['contig','separate'])
def test_rgb_channel_selection_and_preview_cache(tmp_path, planar):
    values = (np.arange(19*23*3).reshape(19,23,3)*37).astype('uint16')
    path=tmp_path/'rgb.tif'
    tifffile.imwrite(path, values if planar=='contig' else values.transpose(2,0,1),
                     photometric='rgb', planarconfig=planar, tile=(16,16), compression='deflate', metadata=None)
    s=q.open_tiff(str(path))
    np.testing.assert_array_equal(s.read_raw(request(23,19,c=2,first=1)).numpy(), values[...,1:])
    s.read_raw(request(23,19,c=3))
    decoded=s.tiff_read_stats.decoded_blocks
    v=s.tiff_view;v.channels=[2,1,0];v.lower=[100,100,100];v.upper=[10000,10000,10000]
    projected=s.with_tiff_view(v)
    display=projected.read_region(q.RegionRequest(0,0,0,23,19)).numpy()
    expected=np.floor(np.clip((values[...,::-1].astype(float)-100)/9900,0,1)*255+.5).astype('uint8')
    np.testing.assert_array_equal(display[...,:3],expected)
    assert projected.tiff_read_stats.decoded_blocks==decoded
    assert s.tiff_view.channels != projected.tiff_view.channels
    s.close()
    np.testing.assert_array_equal(projected.read_raw(request(23,19,c=3)).numpy(), values)
    assert projected.tiff_read_stats.decoded_blocks==decoded


def test_padding_statistics_nonfinite_and_numpy_export(tmp_path):
    values=np.array([[1,np.nan,3],[np.inf,-7,9]],dtype='float64')
    path=tmp_path/'量化.tif';tifffile.imwrite(path,values,metadata=None)
    s=q.open_tiff(str(path));r=request(5,4,x=-1,y=-1)
    raw=s.read_raw(r);a=raw.numpy()[...,0];valid=raw.valid_numpy().astype(bool)
    assert valid.sum()==6 and np.count_nonzero(a[~valid])==0
    np.testing.assert_array_equal(a[1:3,1:4],values)
    stats=q.raw_region_statistics(s,r,tile_size=16)
    assert stats.pixels==20 and stats.valid_pixels==6
    assert stats.finite_samples==[4] and stats.nonfinite_samples==[2]
    assert stats.means==[1.5] and stats.minima==[-7] and stats.maxima==[9]
    output=tmp_path/'原始.npy';q.save_raw_numpy(raw,str(output))
    np.testing.assert_array_equal(np.load(output),raw.numpy())
    original=output.read_bytes()
    with pytest.raises((RuntimeError,ValueError)): q.save_raw_numpy(raw,str(output))
    assert output.read_bytes()==original
    assert not list(tmp_path.glob('*.tmp'))


@pytest.mark.parametrize('order',['ZCT','ZTC','CZT','CTZ','TZC','TCZ'])
def test_ome_plane_orders_and_calibration(tmp_path,order):
    # XML orders fastest to slowest; numpy axes are slowest to fastest.
    axes=order[::-1]+'YX';sizes={'T':2,'Z':3,'C':2,'Y':7,'X':11}
    values=np.arange(np.prod([sizes[k] for k in axes]),dtype='uint16').reshape([sizes[k] for k in axes])
    path=tmp_path/'planes.ome.tif'
    tifffile.imwrite(path,values,ome=True,photometric='minisblack',metadata={'axes':axes,
        'PhysicalSizeX':250,'PhysicalSizeXUnit':'nm','PhysicalSizeY':0.5,'PhysicalSizeYUnit':'µm',
        'PhysicalSizeZ':2,'TimeIncrement':50,'TimeIncrementUnit':'ms','Channel':{'Name':['A','B']}})
    s=q.open_tiff(str(path));m=s.raw_metadata
    assert (m.channels,m.z_planes,m.timepoints)==(2,3,2)
    assert m.dimension_order=='XY'+order and m.channel_names==['A','B']
    assert m.pixel_width_um==.25 and m.pixel_height_um==.5 and m.z_spacing_um==2 and m.time_interval_seconds==.05
    for t in range(2):
        for z in range(3):
            expected=np.stack([values[tuple({'T':t,'Z':z,'C':c}[k] for k in axes[:-2])] for c in range(2)],axis=-1)
            np.testing.assert_array_equal(s.read_raw(request(11,7,c=2,z=z,t=t)).numpy(),expected)


def test_subifd_integer_pyramid_and_plain_pages(tmp_path):
    path=tmp_path/'pyramid.tif';base=np.arange(32*48,dtype='uint16').reshape(32,48)
    with tifffile.TiffWriter(path,bigtiff=True) as writer:
        writer.write(base,subifds=2,metadata=None,tile=(16,16))
        writer.write(base[::2,::2],subfiletype=1,metadata=None)
        writer.write(base[::4,::4],subfiletype=1,metadata=None)
        writer.write(base+100,metadata=None)
    s=q.open_tiff(str(path));assert len(s.raw_metadata.levels)==3 and s.raw_metadata.series_count==2
    for level in range(3):
        expected=base[::2**level,::2**level]
        np.testing.assert_array_equal(s.read_raw(request(expected.shape[1],expected.shape[0],level=level)).numpy()[...,0],expected)
    opts=q.TiffOpenOptions();opts.view.series=1
    second=q.open_tiff(str(path),opts)
    np.testing.assert_array_equal(second.read_raw(request(48,32)).numpy()[...,0],base+100)
    with pytest.raises((ValueError,IndexError)):s.with_tiff_view(opts.view)


def test_cache_budget_and_source_mutation(tmp_path):
    path=tmp_path/'cached.tif';values=np.arange(64*64,dtype='uint16').reshape(64,64)
    tifffile.imwrite(path,values,tile=(16,16),metadata=None)
    opts=q.TiffOpenOptions();opts.cache_bytes=1024
    s=q.open_tiff(str(path),opts);r=request(64,64)
    s.read_raw(r);assert 0 < s.tiff_read_stats.cache_bytes <= 1024
    first=s.tiff_read_stats.decoded_blocks;s.read_raw(r)
    assert s.tiff_read_stats.decoded_blocks>first and s.tiff_read_stats.peak_cache_bytes<=1024
    with path.open('ab') as stream:stream.write(b'changed')
    with pytest.raises(RuntimeError,match='changed'):s.read_raw(r)


@pytest.mark.parametrize('failure',['orientation','bits','badxml','incomplete'])
def test_unsupported_or_invalid_metadata_is_explicit(tmp_path,failure):
    path=tmp_path/'bad.tif';kwargs={};values=np.arange(16,dtype='uint8').reshape(4,4)
    if failure=='orientation':kwargs['extratags']=[(274,'H',1,3,False)]
    if failure=='bits':values=values.astype(bool)
    if failure=='badxml':kwargs['description']='<OME><bad>'
    if failure=='incomplete':kwargs['description']='<OME><Image><Pixels SizeX="4" SizeY="4" SizeZ="2" SizeC="1" SizeT="1" DimensionOrder="XYZCT" Type="uint8"><Channel SamplesPerPixel="1"/><TiffData IFD="0" PlaneCount="1"/></Pixels></Image></OME>'
    tifffile.imwrite(path,values,metadata=None,**kwargs)
    with pytest.raises((RuntimeError,ValueError,IndexError)):q.open_tiff(str(path))


def test_roi_statistics_matches_independent_pixel_center_mask(tmp_path):
    values=np.arange(20*30,dtype='uint16').reshape(20,30)
    path=tmp_path/'roi.tif';tifffile.imwrite(path,values,metadata=None)
    server=q.open_tiff(str(path));roi=q.Roi.ellipse(q.Rect(3,4,17,11))
    yy,xx=np.mgrid[:20,:30];mask=((xx+.5-11.5)/8.5)**2+((yy+.5-9.5)/5.5)**2 <= 1
    stats=q.raw_region_statistics(server,request(30,20),tile_size=16,mask=roi)
    assert stats.valid_pixels==int(mask.sum())
    np.testing.assert_allclose(stats.means,[values[mask].mean()],rtol=1e-14)


def test_ome_grouped_rgb_and_explicit_shuffled_tiffdata(tmp_path):
    path=tmp_path/'rgb.ome.tif';rgb=np.arange(2*3*5*7*3,dtype='uint16').reshape(2,3,5,7,3)
    tifffile.imwrite(path,rgb,ome=True,photometric='rgb',metadata={'axes':'TZYXS'})
    server=q.open_tiff(str(path));assert server.raw_metadata.channels==3
    np.testing.assert_array_equal(server.read_raw(request(7,5,c=3,z=2,t=1)).numpy(),rgb[1,2])
    shuffled=tmp_path/'shuffle.ome.tif'
    xml='<OME><Image><Pixels SizeX="7" SizeY="5" SizeZ="2" SizeC="1" SizeT="1" DimensionOrder="XYZCT" Type="uint16"><Channel SamplesPerPixel="1"/><TiffData IFD="1" FirstZ="0" PlaneCount="1"/><TiffData IFD="0" FirstZ="1" PlaneCount="1"/></Pixels></Image></OME>'
    first=np.arange(35,dtype='uint16').reshape(5,7)
    with tifffile.TiffWriter(shuffled) as writer:
        writer.write(first,description=xml,metadata=None)
        writer.write(first+100,metadata=None)
    server=q.open_tiff(str(shuffled))
    np.testing.assert_array_equal(server.read_raw(request(7,5,z=0)).numpy()[...,0],first+100)
    np.testing.assert_array_equal(server.read_raw(request(7,5,z=1)).numpy()[...,0],first)


def test_parallel_native_views_and_snapshot_options(tmp_path):
    from concurrent.futures import ThreadPoolExecutor
    values=np.arange(64*80,dtype='uint16').reshape(64,80)
    path=tmp_path/'parallel.tif';tifffile.imwrite(path,values,tile=(16,16),compression='deflate',metadata=None)
    server=q.open_tiff(str(path));server.read_raw(request(80,64));decoded=server.tiff_read_stats.decoded_blocks
    def worker(i):
        v=server.tiff_view;v.upper=[1000+i]*3
        view=server.with_tiff_view(v)
        raw=view.read_raw(request(80,64));view.read_region(q.RegionRequest(0,0,0,80,64));view.close()
        return raw.numpy()
    with ThreadPoolExecutor(max_workers=4) as pool:
        arrays=list(pool.map(worker,range(12)))
    for array in arrays:np.testing.assert_array_equal(array[...,0],values)
    assert server.tiff_read_stats.decoded_blocks==decoded
    view=server.tiff_view;view.upper=[1,1,1]
    opts=server.tiff_options;opts.cache_bytes=0;opts.view.upper=[2,2,2]
    assert server.tiff_view.upper==[65535,65535,65535] and server.tiff_options.cache_bytes>0


def test_decode_budget_and_invalid_requests_fail_before_large_output(tmp_path):
    path=tmp_path/'budget.tif';tifffile.imwrite(path,np.ones((512,512),dtype='uint16'),rowsperstrip=512,compression='deflate',metadata=None)
    opts=q.TiffOpenOptions();opts.decode_bytes=65536;opts.cache_bytes=0
    server=q.open_tiff(str(path),opts)
    with pytest.raises((RuntimeError,ValueError)):server.read_raw(request(1,1))
    assert server.tiff_read_stats.decoded_blocks==server.tiff_read_stats.cache_bytes==0
    with pytest.raises((RuntimeError,ValueError)):server.read_raw(request(100000,100000))
    with pytest.raises((ValueError,IndexError)):server.read_raw(request(1,1,c=2))
    for field,value in [('decode_bytes',0),('decode_bytes',256*1024*1024+1),('cache_bytes',256*1024*1024+1)]:
        bad=q.TiffOpenOptions();setattr(bad,field,value)
        with pytest.raises((RuntimeError,ValueError)):q.open_tiff(str(path),bad)


@pytest.mark.parametrize('base_mode',['assocalpha','unassalpha'])
def test_preview_uses_each_pyramid_levels_own_alpha_mode(tmp_path,base_mode):
    source=tmp_path/'alpha.tif'
    straight=np.array([32768,16384,8192,32768],dtype='uint16')
    associated=np.array([16384,8192,4096,32768],dtype='uint16')
    samples={'assocalpha':associated,'unassalpha':straight}
    other='unassalpha' if base_mode=='assocalpha' else 'assocalpha'
    with tifffile.TiffWriter(source) as writer:
        writer.write(np.tile(samples[base_mode],(16,16,1)),photometric='rgb',extrasamples=base_mode,subifds=1,metadata=None)
        writer.write(np.tile(samples[other],(8,8,1)),photometric='rgb',extrasamples=other,subfiletype=1,metadata=None)
    server=q.open_tiff(str(source))
    for level,mode in [(0,base_mode),(1,other)]:
        np.testing.assert_array_equal(server.read_region(q.RegionRequest(0,0,level,1,1)).numpy()[0,0],[128,64,32,128])
        np.testing.assert_array_equal(server.read_raw(request(1,1,c=4,level=level)).numpy()[0,0],samples[mode])
