"""Independent numeric and canonical-byte references for native raw analysis."""
import gc
import hashlib
import struct
import threading

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_tiff_reader import request

pytestmark = pytest.mark.skipif(not q.has_tiff, reason='quantitative TIFF reader')
TYPE_IDS = {'uint8':0,'uint16':1,'int8':2,'int16':3,'uint32':4,'int32':5,'uint64':6,'int64':7,'float32':8,'float64':9}


def scene(tmp_path):
    values=np.arange(2*3*2*16*24,dtype='uint16').reshape(2,3,2,16,24)
    source=tmp_path/'channels.ome.tif'
    tifffile.imwrite(source,values,ome=True,photometric='minisblack',metadata={
        'axes':'TZCYX','Channel':{'Name':['DAPI','Marker']},
        'PhysicalSizeX':.25,'PhysicalSizeY':.5,'PhysicalSizeZ':2,
        'TimeIncrement':50,'TimeIncrementUnit':'ms'})
    return q.Document(q.open_tiff(str(source))),values,source


def canonical_digest(values, region, *, tile=16, series=0, shape=None, downsample=1, names=None, calibration=(None,None,None,None)):
    """Published v1 byte grammar; all samples come from independent NumPy arrays."""
    # values is canonical T Z C Y X, independent of the TIFF's IFD order.
    t_count,z_count,c_count,height,width=values.shape
    x,y,w,h,level,first,channels,z,t=region
    base_height,base_width=shape or (height,width)
    digest=hashlib.sha256()
    def integer(value):digest.update(struct.pack('<Q',int(value)%(1<<64)))
    def text(value):
        data=value.encode('utf-8');integer(len(data));digest.update(data)
    def number(value):
        integer(value is not None)
        if value is not None:digest.update(struct.pack('<d',value))
    text('qpx.raw-roi-input/1')
    for value in [base_width,base_height,c_count,z_count,t_count,series,TYPE_IDS[values.dtype.name],x,y,w,h,level,first,channels,z,t,tile]:integer(value)
    number(float(downsample))
    for value in calibration:number(value)
    for name in (names or [f'Channel {i+1}' for i in range(c_count)])[first:first+channels]:text(name)
    integer(0)  # Manual rectangle has no additional ROI mask.
    for row in range(0,h,tile):
        for col in range(0,w,tile):
            tw,th=min(tile,w-col),min(tile,h-row)
            for value in [x+col*downsample,y+row*downsample,tw,th]:integer(value)
            data=np.zeros((th,tw,channels),dtype=values.dtype.newbyteorder('<'))
            valid=np.zeros((th,tw),dtype='uint8')
            for yy in range(th):
                for xx in range(tw):
                    sx,sy=x//downsample+col+xx,y//downsample+row+yy
                    if 0<=sx<width and 0<=sy<height:
                        data[yy,xx]=values[t,z,first:first+channels,sy,sx]
                        valid[yy,xx]=1
            digest.update(valid.tobytes())
            digest.update(data.tobytes())
    return digest.hexdigest()


@pytest.mark.parametrize('dtype',list(TYPE_IDS))
def test_all_dtypes_match_independent_statistics_and_exact_digest(tmp_path,dtype):
    values=np.arange(19*23,dtype='uint64').reshape(19,23).astype(dtype)
    if dtype=='uint64':values[0,0]=2**64-1
    if dtype=='int64':values[0,0]=-(2**63)
    if dtype.startswith('float'):
        values/=19
        values[0,:3]=[np.nan,np.inf,-np.inf]
    source=tmp_path/'samples.tif'
    tifffile.imwrite(source,values,byteorder='>',tile=(16,16),compression='deflate',metadata=None)
    doc=q.Document(q.open_tiff(str(source)))
    region=request(23,19)
    capture=q.RawAnalysisRequest(doc,region,tile_size=16)
    result=q.analyze_raw(capture)
    expected=canonical_digest(values[None,None,None],(0,0,23,19,0,0,1,0,0))
    assert result.input_sha256==expected
    fast=q.raw_analysis_input_digest(capture)
    assert fast.input_sha256==expected and fast.tiles==result.tiles==4
    assert fast.raw_bytes==19*23*(values.dtype.itemsize+1)
    finite=values[np.isfinite(values)].astype('float64')
    stats=result.statistics
    assert stats.valid_pixels==19*23 and stats.finite_samples==[len(finite)]
    assert stats.nonfinite_samples==[19*23-len(finite)]
    np.testing.assert_allclose(stats.means,[finite.mean()],rtol=1e-12,atol=1e-12)
    assert stats.minima==[float(finite.min())] and stats.maxima==[float(finite.max())]
    assert result.source.primary.bytes>0 and result.source.scope=='single_file'
    assert result.metadata.channel_names==['Channel 1']


def test_axes_padding_calibration_and_display_are_independent(tmp_path):
    doc,values,_=scene(tmp_path)
    raw=request(6,5,c=2,z=1,t=1,x=-1,y=-1)
    captured=q.RawAnalysisRequest(doc,raw,tile_size=16)
    raw.z=0;raw.channels=1
    result=q.analyze_raw(captured)
    expected=canonical_digest(values,(-1,-1,6,5,0,0,2,1,1),names=['DAPI','Marker'],calibration=(.25,.5,2.,.05))
    assert result.input_sha256==expected and result.statistics.valid_pixels==20
    np.testing.assert_allclose(result.statistics.means,values[1,1,:,:4,:5].mean(axis=(1,2)))
    view=doc.server.tiff_view;view.z=2;view.t=0;view.channels=[1,0,-1];view.lower=[1000]*3;view.upper=[5000]*3
    doc.set_tiff_view(view)
    assert captured.matches(doc)
    again=q.analyze_raw(q.RawAnalysisRequest(doc,captured.region,tile_size=16))
    assert again.input_sha256==result.input_sha256 and again.statistics.means==result.statistics.means
    assert captured.region.z==1 and captured.region.channels==2
    copied=captured.region;copied.z=0
    assert captured.region.z==1


@pytest.mark.parametrize('nucleus',[False,True])
def test_masked_roi_statistics_are_independent_of_preview(tmp_path,nucleus):
    doc,values,_=scene(tmp_path)
    body=q.Roi.ellipse(q.Rect(2,3,11,9));inner=q.Roi.rectangle(q.Rect(5,5,4,3))
    cell=doc.add_cell(body,inner,'Cell')
    capture=q.RawAnalysisRequest(doc,request(1,1,c=2,z=2,t=1),cell,nucleus=nucleus,tile_size=16)
    result=q.analyze_raw(capture)
    yy,xx=np.mgrid[:16,:24]
    mask=((xx+.5-7.5)/5.5)**2+((yy+.5-7.5)/4.5)**2<=1 if not nucleus else ((xx+.5>=5)&(xx+.5<9)&(yy+.5>=5)&(yy+.5<8))
    assert result.statistics.valid_pixels==int(mask.sum())
    np.testing.assert_allclose(result.statistics.means,values[1,2][:,mask].mean(axis=1),rtol=1e-12)
    assert result.object_context.name=='Cell' and result.nucleus==nucleus
    assert q.raw_analysis_input_digest(capture).input_sha256==result.input_sha256
    held=result.metadata
    doc.server.close();del result,doc,capture;gc.collect()
    assert held.sample_type==q.SampleType.UINT16 and held.channel_names==['DAPI','Marker']


def test_constructor_and_matches_do_not_open_a_linked_decoder(tmp_path):
    doc,_,source=scene(tmp_path);obj=doc.add_rectangle(q.Rect(0,0,4,4))
    p=q.Project();image=p.add_document(doc,source_path=str(source),reader='tiff');path=tmp_path/'saved.qpxproj';p.save(str(path))
    doc.server.close();del p,doc
    loaded=q.Project.open(str(path));doc=loaded.document(image);source.unlink()
    assert doc.server.reader_source.primary is None
    capture=q.RawAnalysisRequest(doc,request(1,1),obj)
    assert capture.matches(doc) and doc.server.reader_source.primary is None
    with pytest.raises(RuntimeError):q.analyze_raw(capture)


@pytest.mark.parametrize('field,value',[('channels',0),('channels',65537),('z',65536),('t',65536)])
def test_invalid_capture_axes_fail_before_worker(tmp_path,field,value):
    doc,_,_=scene(tmp_path);raw=request(1,1);setattr(raw,field,value)
    with pytest.raises((ValueError,IndexError)):q.RawAnalysisRequest(doc,raw)


def test_native_job_owner_cancel_and_gil_release(tmp_path):
    doc,_,_=scene(tmp_path);capture=q.RawAnalysisRequest(doc,request(24,16,c=2))
    job=q.RawAnalysisJob();errors=[]
    def wrong_owner():
        try:job.request(capture)
        except RuntimeError as error:errors.append(str(error))
    thread=threading.Thread(target=wrong_owner);thread.start();thread.join()
    assert errors and 'owner thread' in errors[0]
    assert job.request(capture)
    job.cancel()
    assert job.wait(doc).state=='cancelled' and job.result is None
    assert job.request(capture)
    assert job.wait(doc).state=='ready'
    assert job.result.input_sha256==q.analyze_raw(capture).input_sha256


def test_wait_releases_gil_while_native_statistics_run(tmp_path):
    values=np.arange(2048*2048,dtype='uint16').reshape(2048,2048)
    source=tmp_path/'gil.tif';tifffile.imwrite(source,values,tile=(256,256),compression='deflate',metadata=None)
    doc=q.Document(q.open_tiff(str(source)));job=q.RawAnalysisJob()
    capture=q.RawAnalysisRequest(doc,request(2048,2048))
    begin=threading.Event();stop=threading.Event();ticks=[0]
    def progress():
        begin.wait()
        while not stop.is_set():ticks[0]+=1
    worker=threading.Thread(target=progress);worker.start()
    try:
        assert job.request(capture)
        begin.set()
        assert job.wait(doc).state=='ready'
        assert ticks[0]>0
    finally:
        stop.set();begin.set();worker.join(5)
    assert not worker.is_alive()


def test_natural_pyramid_hash_and_roi_use_level_zero_pixel_centers(tmp_path):
    base=np.arange(16*24,dtype='uint16').reshape(16,24)
    reduced=base[::2,::2]+1000
    source=tmp_path/'pyramid.tif'
    with tifffile.TiffWriter(source) as writer:
        writer.write(base,subifds=1,metadata=None)
        writer.write(reduced,subfiletype=1,metadata=None)
    doc=q.Document(q.open_tiff(str(source)));raw=request(12,8);raw.region=q.RegionRequest(0,0,1,12,8)
    result=q.analyze_raw(q.RawAnalysisRequest(doc,raw,tile_size=16))
    assert result.input_sha256==canonical_digest(reduced[None,None,None],(0,0,12,8,1,0,1,0,0),shape=(16,24),downsample=2)
    assert result.metadata.downsample==2 and (result.metadata.level_width,result.metadata.level_height)==(12,8)
    assert result.statistics.valid_pixels==96
    obj=doc.add_roi(q.Roi.ellipse(q.Rect(3,3,9,7)),'Natural-grid ellipse')
    masked=q.analyze_raw(q.RawAnalysisRequest(doc,raw,obj,tile_size=16))
    yy,xx=np.mgrid[:8,:12]
    mask=((2*xx+1-7.5)/4.5)**2+((2*yy+1-6.5)/3.5)**2<=1
    assert masked.statistics.valid_pixels==int(mask.sum())
    np.testing.assert_allclose(masked.statistics.means,[reduced[mask].mean()],rtol=1e-12)
    assert masked.statistics.minima==[float(reduced[mask].min())] and masked.statistics.maxima==[float(reduced[mask].max())]
