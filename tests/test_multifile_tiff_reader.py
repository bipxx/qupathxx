"""Actual compound TIFF reads against independently generated coordinate samples."""
import gc
import hashlib
import importlib.util
import json
from pathlib import Path

import numpy as np
import pytest
import qupathxx as q
import tifffile

spec=importlib.util.spec_from_file_location('multifile_reference',Path(__file__).resolve().parents[1]/'tools/reference_multifile_ome.py')
fixture=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),fixture.__dict__)
pytestmark=pytest.mark.skipif(not q.has_tiff,reason='optional native TIFF reader')


@pytest.fixture
def corpus(tmp_path):
    directory=tmp_path/'corpus'
    return directory,fixture.generate(directory)


def request(w,h,*,z=0,t=0,level=0,x=0,y=0):
    r=q.RawRegionRequest();r.region=q.RegionRequest(x,y,level,w,h)
    r.channels=2;r.z=z;r.t=t
    return r


@pytest.mark.parametrize('entry',fixture.FILES)
@pytest.mark.parametrize('cache',[0,1024,1024*1024])
def test_exact_all_axes_levels_padding_and_shared_cache(corpus,entry,cache):
    directory,manifest=corpus
    options=q.TiffOpenOptions();options.cache_bytes=cache
    server=q.open_tiff(str(directory/entry),options)
    meta=server.raw_metadata
    assert (meta.channels,meta.z_planes,meta.timepoints)==(2,2,2)
    assert meta.pixel_width_um==.25 and meta.pixel_height_um==.5
    assert len(meta.levels)==2
    for repeat in range(2):
        for level,scale in enumerate((1,2)):
            h,w=(17+scale-1)//scale,(19+scale-1)//scale
            for t in range(2):
                for z in range(2):
                    raw=server.read_raw(request(w,h,z=z,t=t,level=level))
                    expected=np.stack([fixture.values(t,z,c,scale) for c in range(2)],axis=-1)
                    np.testing.assert_array_equal(raw.numpy(),expected)
                    assert raw.valid_numpy().all() and raw.numpy().dtype==np.uint16
                    for c in range(2):
                        plane=next(p for f in manifest['files'] for p in f['planes'] if (p['t'],p['z'],p['c'])==(t,z,c))
                        assert hashlib.sha256(raw.numpy()[...,c].astype('<u2').tobytes()).hexdigest()==plane['levels'][level]['canonical_le_sha256']
                    padded=server.read_raw(request(w+2,h+2,z=z,t=t,level=level,x=-scale,y=-scale))
                    np.testing.assert_array_equal(padded.numpy()[1:-1,1:-1],expected)
                    valid=np.zeros((h+2,w+2),dtype=np.uint8);valid[1:-1,1:-1]=1
                    np.testing.assert_array_equal(padded.valid_numpy(),valid)
                    assert not padded.numpy()[valid==0].any()
    assert server.tiff_read_stats.peak_cache_bytes<=cache
    if cache>1024: assert server.tiff_read_stats.cache_hits>0
    server.close()


@pytest.mark.parametrize('entry',fixture.FILES)
def test_nonempty_companion_ownership_and_logical_view_preview(corpus,entry):
    directory,_=corpus;server=q.open_tiff(str(directory/entry))
    source=server.reader_source;companions=source.companions;observed=companions[0]
    assert source.scope=='compound_files' and len(companions)==1
    assert {source.primary.path,observed.path}=={str((directory/name).resolve()) for name in fixture.FILES}
    wire=json.loads(q.reader_source_json(source));assert wire['schema']=='qpx.reader-source/2'
    companions.clear();assert len(source.companions)==1
    with pytest.raises(AttributeError): observed.path='changed'
    view=q.TiffView();view.z=1;view.t=1;view.channels=[1,0,-1];view.lower=[0,0,0];view.upper=[12000,12000,12000]
    projected=server.with_tiff_view(view)
    assert projected.reader_source.scope=='compound_files' and len(projected.reader_source.companions)==1
    server.close();del server;gc.collect()
    raw=projected.read_raw(request(19,17,z=1,t=1));array=raw.numpy()
    display=projected.read_region(q.RegionRequest(0,0,0,19,17)).numpy()
    expected=np.zeros((17,19,4),dtype=np.uint8);expected[...,3]=255
    for color,c in enumerate((1,0)):
        expected[...,color]=np.floor(fixture.values(1,1,c).astype(float)/12000*255+.5).astype('uint8')
    np.testing.assert_array_equal(display,expected)
    projected.close();del projected,source;gc.collect()
    assert Path(observed.path).is_file() and observed.bytes>0
    np.testing.assert_array_equal(array[...,1],fixture.values(1,1,1))


@pytest.mark.parametrize('operation',['cached','outside','display','view'])
def test_companion_change_rejects_even_when_primary_unchanged(corpus,operation):
    directory,_=corpus;primary=directory/fixture.FILES[0];companion=directory/fixture.FILES[1]
    server=q.open_tiff(str(primary));server.read_raw(request(19,17))
    primary_bytes=primary.read_bytes()
    with companion.open('ab') as stream: stream.write(b'companion version changed')
    assert primary.read_bytes()==primary_bytes
    with pytest.raises(RuntimeError,match='changed'):
        if operation=='cached': server.read_raw(request(19,17))
        elif operation=='outside': server.read_raw(request(2,2,x=-100,y=-100))
        elif operation=='display': server.read_region(q.RegionRequest(0,0,0,19,17))
        else: server.with_tiff_view(q.TiffView())
    server.close()


@pytest.mark.parametrize('failure',['missing','uuid','duplicate_uuid','size','type','channels','calibration','incomplete','duplicate_slot','ifd','no_filename','alias'])
def test_invalid_fileset_is_refused(corpus,failure):
    directory,_=corpus;primary=directory/fixture.FILES[0];companion=directory/fixture.FILES[1]
    if failure=='missing': companion.unlink()
    else:
        path=companion if failure in ('uuid','duplicate_uuid','size','type','channels','calibration') else primary
        with tifffile.TiffFile(path) as source: xml=source.ome_metadata
        if failure=='uuid': xml=xml.replace('UUID="'+fixture.UUIDS[1]+'"','UUID="urn:wrong"',1)
        elif failure=='duplicate_uuid': xml=xml.replace('UUID="'+fixture.UUIDS[1]+'"','UUID="'+fixture.UUIDS[0]+'"',1)
        elif failure=='size': xml=xml.replace('SizeX="19"','SizeX="20"')
        elif failure=='type': xml=xml.replace('Type="uint16"','Type="int16"')
        elif failure=='channels': xml=xml.replace('Raw 0','different name')
        elif failure=='calibration': xml=xml.replace('PhysicalSizeX="0.25"','PhysicalSizeX="0.5"')
        elif failure=='incomplete': xml=xml.replace('<TiffData IFD="0" FirstT="1" FirstZ="0" FirstC="1" PlaneCount="1">','<TiffData IFD="0" FirstT="1" FirstZ="0" FirstC="1" PlaneCount="0">',1)
        elif failure=='duplicate_slot': xml=xml.replace('FirstT="0" FirstZ="1" FirstC="0"','FirstT="1" FirstZ="0" FirstC="1"',1)
        elif failure=='ifd': xml=xml.replace('IFD="0"','IFD="99"',1)
        elif failure=='no_filename': xml=xml.replace(' FileName="companions/planes-&#946;.ome.tif"','')
        elif failure=='alias':
            alias=directory/'alias.tif';alias.hardlink_to(primary)
            xml=xml.replace('FileName="primary.ome.tif"','FileName="alias.tif"')
        tifffile.tiffcomment(path,xml)
    with pytest.raises((RuntimeError,ValueError,IndexError)):
        q.open_tiff(str(primary))
