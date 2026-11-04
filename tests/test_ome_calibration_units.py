"""Independent OME unit expectations; calibration never rescales stored samples."""
from decimal import Decimal
from xml.etree import ElementTree as ET

import numpy as np
import pytest
import qupathxx as q
import tifffile

pytestmark = pytest.mark.skipif(not q.has_tiff, reason='optional native TIFF reader')
# OME 2016-06 SI symbols, independently converted with decimal powers.
# https://docs.openmicroscopy.org/ome-model/6.3.1/developers/ome-units.html
PREFIXES = [('Y',24),('Z',21),('E',18),('P',15),('T',12),('G',9),('M',6),
            ('k',3),('h',2),('da',1),('',0),('d',-1),('c',-2),('m',-3),
            ('µ',-6),('n',-9),('p',-12),('f',-15),('a',-18),('z',-21),('y',-24)]
PIXELS = np.arange(16, dtype=np.uint16).reshape(4,4)


def image(tmp_path, *, length='1.25', length_unit=None, time='1.25', time_unit=None):
    attributes = dict(PhysicalSizeX=length, PhysicalSizeY=length, PhysicalSizeZ=length,
                      TimeIncrement=time)
    if length_unit is not None:
        attributes.update({name:length_unit for name in ('PhysicalSizeXUnit','PhysicalSizeYUnit','PhysicalSizeZUnit')})
    if time_unit is not None:
        attributes['TimeIncrementUnit'] = time_unit
    ome=ET.Element('OME');node=ET.SubElement(ome,'Image',ID='Image:0')
    node=ET.SubElement(node,'Pixels',ID='Pixels:0',DimensionOrder='XYZCT',Type='uint16',
                       SizeX='4',SizeY='4',SizeZ='1',SizeC='1',SizeT='1',**attributes)
    ET.SubElement(node,'Channel',ID='Channel:0:0',SamplesPerPixel='1')
    ET.SubElement(node,'TiffData',IFD='0',PlaneCount='1')
    path=tmp_path/'units.ome.tif'
    tifffile.imwrite(path,PIXELS,metadata=None,photometric='minisblack',
                     description=ET.tostring(ome,encoding='ascii').decode('ascii'))
    return path


def check_pixels(server):
    request=q.RawRegionRequest();request.region=q.RegionRequest(0,0,0,4,4);request.channels=1
    raw=server.read_raw(request)
    np.testing.assert_array_equal(raw.numpy()[...,0],PIXELS)
    assert raw.valid_numpy().all() and raw.numpy().dtype==np.dtype('uint16')


@pytest.mark.parametrize('prefix,exponent',PREFIXES)
def test_si_length_units_preserve_axes_and_samples(tmp_path,prefix,exponent):
    server=q.open_tiff(str(image(tmp_path,length_unit=prefix+'m')))
    expected=float(Decimal('1.25')*Decimal(10)**(exponent+6))
    metadata=server.raw_metadata
    for actual in (metadata.pixel_width_um,metadata.pixel_height_um,metadata.z_spacing_um):
        assert actual==pytest.approx(expected,rel=2e-15,abs=0)
    check_pixels(server);server.close()


@pytest.mark.parametrize('prefix,exponent',PREFIXES)
def test_si_time_units_preserve_interval_and_samples(tmp_path,prefix,exponent):
    server=q.open_tiff(str(image(tmp_path,time_unit=prefix+'s')))
    expected=float(Decimal('1.25')*Decimal(10)**exponent)
    assert server.raw_metadata.time_interval_seconds==pytest.approx(expected,rel=2e-15,abs=0)
    check_pixels(server);server.close()


@pytest.mark.parametrize('length_unit,time_unit,length_scale,time_scale',[
    (None,None,1.,1.),('um','s',1.,1.),('µm','µs',1.,.000001),
    ('nm','ms',.001,.001),('mm','min',1000.,60.),('cm','h',10000.,3600.),
    ('m','s',1000000.,1.),('Å','ms',.0001,.001),('pm','d',.000001,86400.)])
def test_legacy_scales_defaults_and_day(tmp_path,length_unit,time_unit,length_scale,time_scale):
    value='1.234567890123456'
    server=q.open_tiff(str(image(tmp_path,length=value,length_unit=length_unit,time=value,time_unit=time_unit)))
    assert server.raw_metadata.pixel_width_um==float(value)*length_scale
    assert server.raw_metadata.time_interval_seconds==float(value)*time_scale
    check_pixels(server);server.close()


@pytest.mark.parametrize('unit',['pixel','reference frame','frog','NM','nm ','s'])
def test_nonphysical_or_unknown_length_units_remain_absent(tmp_path,unit):
    server=q.open_tiff(str(image(tmp_path,length_unit=unit)))
    assert server.raw_metadata.pixel_width_um is None
    assert server.raw_metadata.pixel_height_um is None
    assert server.raw_metadata.z_spacing_um is None
    check_pixels(server);server.close()


@pytest.mark.parametrize('value,unit,expected',[
    ('1e308','Ym',None),('1e-308','ym',None),('1e-300','ym',1e-318),
    ('0','pm',None),('-1','pm',None),('nan','pm',None),('+1','pm',None)])
def test_converted_length_admits_only_positive_finite_representable_values(tmp_path,value,unit,expected):
    server=q.open_tiff(str(image(tmp_path,length=value,length_unit=unit)))
    assert server.raw_metadata.pixel_width_um==expected
    check_pixels(server);server.close()


@pytest.mark.parametrize('value,unit,expected',[
    ('1e308','Ys',None),('1e-308','ys',None),('1e-300','ys',None),
    ('0','ns',None),('-1','ns',None),('nan','ns',None),('+1','ns',None),
    ('2','frog',None),('2','NS',None),('2','ms ',None),('2','m',None)])
def test_converted_time_admits_only_positive_finite_representable_values(tmp_path,value,unit,expected):
    server=q.open_tiff(str(image(tmp_path,time=value,time_unit=unit)))
    assert server.raw_metadata.time_interval_seconds==expected
    check_pixels(server);server.close()


@pytest.mark.parametrize('entry',[0,1])
@pytest.mark.parametrize('mismatch',[None,'PhysicalSizeX','PhysicalSizeZ','TimeIncrement'])
def test_companion_calibration_normalizes_units_and_rejects_disagreement(tmp_path,entry,mismatch):
    from test_multifile_tiff_reader import fixture,request
    directory=tmp_path/'compound';fixture.generate(directory)
    for index,name in enumerate(fixture.FILES):
        path=directory/name;ome=ET.fromstring(tifffile.tiffcomment(path))
        pixels=next(e for e in ome.iter() if e.tag.rsplit('}',1)[-1]=='Pixels')
        # Primary uses newly supported units; companion uses old supported units.
        attrs=(dict(PhysicalSizeX='250000',PhysicalSizeXUnit='pm',PhysicalSizeY='500000',PhysicalSizeYUnit='pm',
                    PhysicalSizeZ='2000000',PhysicalSizeZUnit='pm',TimeIncrement='1000000000',TimeIncrementUnit='ns')
               if index==0 else dict(PhysicalSizeX='.25',PhysicalSizeXUnit='µm',PhysicalSizeY='.5',PhysicalSizeYUnit='µm',
                                     PhysicalSizeZ='2',PhysicalSizeZUnit='µm',TimeIncrement='1',TimeIncrementUnit='s'))
        if index==1 and mismatch:attrs[mismatch]=str(Decimal(attrs[mismatch])*2)
        pixels.attrib.update(attrs)
        tifffile.tiffcomment(path,ET.tostring(ome,encoding='ascii').decode('ascii'))
    if mismatch:
        with pytest.raises(ValueError,match='companion series metadata differs'):
            q.open_tiff(str(directory/fixture.FILES[entry]))
        return
    server=q.open_tiff(str(directory/fixture.FILES[entry]));metadata=server.raw_metadata
    assert (metadata.pixel_width_um,metadata.pixel_height_um,metadata.z_spacing_um,metadata.time_interval_seconds)==(.25,.5,2.,1.)
    raw=server.read_raw(request(19,17,z=1,t=1))
    expected=np.stack([fixture.values(1,1,c) for c in range(2)],axis=-1)
    np.testing.assert_array_equal(raw.numpy(),expected);server.close()
