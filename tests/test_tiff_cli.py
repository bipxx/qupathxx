"""Installed standalone native raw exporter, including Unicode argv on Windows."""
from contextlib import closing
import csv
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
from xml.etree import ElementTree
import zipfile
import zlib
import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
import tifffile
from tools.ci.check_omero_bundle_verifier import change_member

pytestmark=pytest.mark.skipif(not q.has_tiff,reason='TIFF reader')
CLI=Path(os.environ.get('QUPATHXX_TEST_TIFF_REGION') or
         Path(_native.__file__).with_name('qupathxx_region.exe' if os.name=='nt' else 'qupathxx_region'))


def run(*args):
    return subprocess.run([str(CLI),*map(str,args)],capture_output=True,text=True,encoding='utf-8',timeout=40)


def export_ome_xml(source, output, *options):
    return run_format_import('ome-tiff-export-xml', source, output, *options)


def run_format_import(*args):
    importer = os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT')
    if not importer:
        pytest.skip('native format import CLI path is required')
    return subprocess.run([importer, *map(str, args)],
                          capture_output=True, encoding='utf-8', timeout=40)


@pytest.mark.parametrize('bigtiff', [False, True])
def test_export_embedded_ome_xml_preserves_source_text(tmp_path, bigtiff):
    source, output = tmp_path / 'source.tif', tmp_path / 'metadata.xml'
    xml = ('<?xml version="1.0" encoding="UTF-8"?>\n<!-- retained µm text -->\n'
           '<OME xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06">'
           '<Image ID="Image:0"><Pixels ID="Pixels:0" SizeX="3" SizeY="2" SizeC="1" SizeZ="1" SizeT="1" '
           'Type="uint8" DimensionOrder="XYZCT"><Channel ID="Channel:0:0"/><TiffData/></Pixels>'
           '<ROIRef ID="ROI:0"/></Image><ROI ID="ROI:0"><Union><Point ID="Shape:0" X="1" Y="1"/>'
           '</Union></ROI></OME>').encode('utf-8')
    # Extraction returns the original document regardless of raster orientation.
    tifffile.imwrite(source, np.arange(6, dtype='u1').reshape(2, 3), description=xml,
                     metadata=None, bigtiff=bigtiff, byteorder='>' if bigtiff else '<',
                     extratags=[(274, 'H', 1, 6, False)])
    original = source.read_bytes()
    result = export_ome_xml(source, output)
    assert result.returncode == 0, result.stderr
    info = json.loads(result.stdout)
    assert output.read_bytes() == xml
    assert (info['bytes'], info['sha256']) == (len(xml), hashlib.sha256(xml).hexdigest())
    result = export_ome_xml(source, output)
    assert result.returncode == 2 and 'destination already exists' in result.stderr
    result = export_ome_xml(source, source)
    assert result.returncode == 2 and 'must not replace its input' in result.stderr
    assert source.read_bytes() == original and output.read_bytes() == xml
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.parametrize(('description', 'diagnostic'), [
    ('ordinary TIFF', 'malformed OME XML'),
    ('<OME><Image>', 'malformed OME XML'),
    ('<!DOCTYPE OME><OME/>', 'declarations/entities'),
])
def test_export_embedded_ome_xml_refuses_invalid_text(tmp_path, description, diagnostic):
    source, output = tmp_path / 'source.tif', tmp_path / 'refused.xml'
    tifffile.imwrite(source, np.ones((2, 3), dtype='u1'), description=description, metadata=None)
    result = export_ome_xml(source, output)
    assert result.returncode == 2 and diagnostic in result.stderr
    assert not output.exists()


def test_export_embedded_ome_binaryonly_preserves_reference(tmp_path):
    source, output = tmp_path / 'binary-only.tif', tmp_path / 'reference.xml'
    xml = ('<OME UUID="urn:source"><BinaryOnly MetadataFile="offline.ome.xml" UUID="urn:master"/></OME>').encode()
    tifffile.imwrite(source, np.ones((2, 3), dtype='u1'), description=xml, metadata=None)
    result = export_ome_xml(source, output)
    assert result.returncode == 0, result.stderr
    assert output.read_bytes() == xml


@pytest.mark.parametrize('companion_tiff', [False, True])
def test_export_resolved_ome_metadata_preserves_exact_master(tmp_path, companion_tiff):
    source, output = tmp_path / 'binary-only.ome.tif', tmp_path / 'resolved.xml'
    master = tmp_path / ('master.ome.tif' if companion_tiff else 'master.companion.ome')
    pointer = f'<OME UUID="urn:source"><BinaryOnly MetadataFile="{master.name}" UUID="urn:master"/></OME>'
    xml = ('<OME xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06" UUID="urn:master">\n'
           '<Image ID="Image:0"><Pixels ID="Pixels:0" SizeX="3" SizeY="2" SizeC="1" SizeZ="1" SizeT="1" '
           'DimensionOrder="XYZCT" Type="uint8"><Channel ID="Channel:0:0"/>'
           '<TiffData IFD="100"/><!----></Pixels><ROIRef ID="ROI:0"/></Image>'
           '<ROI ID="ROI:0"><Union><Point ID="Shape:0" X="1" Y="1"/></Union></ROI></OME>').encode()
    pixels = np.arange(6, dtype='u1').reshape(2, 3)
    tifffile.imwrite(source, pixels, description=pointer, metadata=None, ome=False)
    if companion_tiff:
        tifffile.imwrite(master, pixels, description=xml, metadata=None, ome=False, bigtiff=True)
    else:
        master.write_bytes(xml)
    before = source.read_bytes(), master.read_bytes()
    result = export_ome_xml(source, output, '--resolve-metadata')
    assert result.returncode == 0, result.stderr
    receipt = json.loads(result.stdout)
    assert Path(receipt['metadata_source']) == master
    assert output.read_bytes() == xml
    assert receipt['sha256'] == hashlib.sha256(xml).hexdigest() and receipt['bytes'] == len(xml)
    result = export_ome_xml(source, master, '--resolve-metadata')
    assert result.returncode == 2 and 'must not replace its input' in result.stderr
    assert (source.read_bytes(), master.read_bytes()) == before


@pytest.mark.parametrize(('master', 'diagnostic'), [
    (None, ''),
    ('<OME UUID="urn:different"><Image/></OME>', 'metadata file UUID differs'),
    ('<OME UUID="urn:master"><BinaryOnly MetadataFile="nested.ome" UUID="urn:nested"/></OME>',
     'metadata master must contain full Image metadata'),
])
def test_export_resolved_ome_metadata_refuses_bad_reference(tmp_path, master, diagnostic):
    source, output = tmp_path / 'binary-only.ome.tif', tmp_path / 'refused.xml'
    pointer = '<OME UUID="urn:source"><BinaryOnly MetadataFile="master.companion.ome" UUID="urn:master"/></OME>'
    tifffile.imwrite(source, np.ones((2, 3), dtype='u1'), description=pointer, metadata=None, ome=False)
    if master is not None:
        (tmp_path / 'master.companion.ome').write_text(master, encoding='utf-8')
    result = export_ome_xml(source, output, '--resolve-metadata')
    assert result.returncode == 2 and diagnostic in result.stderr
    assert not output.exists()


@pytest.mark.parametrize('orientation', range(1, 9))
def test_plain_tiff_orientation_preserves_blocks_pixels_and_calibration(tmp_path, orientation):
    source = tmp_path / 'oriented.tif'
    pixels = (np.arange(19 * 23 * 3, dtype=np.uint16) * 17).astype(np.uint8).reshape(19, 23, 3)
    if orientation % 2:
        storage = dict(tile=(16, 16), planarconfig='contig')
        data = pixels
    else:
        storage = dict(rowsperstrip=5, planarconfig='separate')
        data = np.moveaxis(pixels, -1, 0)
    tifffile.imwrite(source, data, photometric='rgb', metadata=None, resolution=(1000, 2000),
                     resolutionunit='CENTIMETER', extratags=[(274, 'H', 1, orientation, False)], **storage)
    transpose = np.swapaxes(pixels, 0, 1)
    expected = (pixels, pixels[:, ::-1], pixels[::-1, ::-1], pixels[::-1], transpose,
                np.rot90(pixels, -1), transpose[::-1, ::-1], np.rot90(pixels, 1))[orientation - 1]
    height, width, _ = expected.shape
    before = source.read_bytes()
    bundle = tmp_path / 'pixels.zip'
    result = run('--raw', source, bundle, '--channels', 3, '--bundle')
    assert result.returncode == 0, result.stderr
    raw = tmp_path / 'pixels.npy'
    result = run('--extract-raw-bundle', bundle, raw)
    assert result.returncode == 0, result.stderr
    calibration = json.loads(result.stdout)['calibration']
    pitch = calibration['base_pixel_width_um'], calibration['base_pixel_height_um']
    assert pitch == ((5, 10) if orientation >= 5 else (10, 5))
    np.testing.assert_array_equal(np.load(raw), expected)

    # A non-square, offset crop crosses one edge and stored strip/tile boundaries.
    crop = tmp_path / 'crop.npy'
    result = run('--raw', source, crop, '--channel', 1, '--channels', 2,
                 '--x', -2, '--y', 3, '--width', width, '--height', height, '--with-validity')
    assert result.returncode == 0, result.stderr
    window = np.load(crop)
    expected_values = np.zeros((height, width, 2), dtype='u1')
    expected_values[:height - 3, 2:] = expected[3:, :width - 2, 1:]
    valid = np.zeros((height, width), dtype=bool)
    valid[:height - 3, 2:] = True
    np.testing.assert_array_equal(window['values'], expected_values)
    np.testing.assert_array_equal(window['valid'], valid)

    pam = tmp_path / 'pixels.pam'
    result = run('--reader', 'tiff', source, 0, 0, 0, width, height, pam)
    assert result.returncode == 0, result.stderr
    expected_rgba = np.concatenate((expected, np.full((height, width, 1), 255, dtype='u1')), axis=-1)
    assert pam.read_bytes().split(b'ENDHDR\n', 1)[1] == expected_rgba.tobytes()
    if orientation == 6 and os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'oriented.qpxproj'
        result = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                 project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert result.returncode == 0, result.stderr
        report = json.loads(result.stdout)
        assert (report['width'], report['height']) == (width, height)
        original = project.read_bytes()
        replay = tmp_path / 'replay.pam'
        result = run('--project-region', project, 1, 0, 0, 0, width, height, replay, '--receipt')
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == pam.read_bytes() and project.read_bytes() == original
        replay_raw = tmp_path / 'replay.npy'
        result = run('--raw-project', project, 1, replay_raw, '--channels', 3)
        assert result.returncode == 0, result.stderr
        assert replay_raw.read_bytes() == raw.read_bytes() and project.read_bytes() == original
    assert source.read_bytes() == before


@pytest.mark.parametrize('matching_orientation', [True, False])
def test_plain_tiff_oriented_pyramid_requires_one_frame(tmp_path, matching_orientation):
    source = tmp_path / 'pyramid.tif'
    pixels = np.arange(19 * 23, dtype=np.uint16).reshape(19, 23)
    with tifffile.TiffWriter(source) as writer:
        writer.write(pixels, subifds=1, metadata=None, extratags=[(274, 'H', 1, 6, False)])
        writer.write(pixels[::2, ::2], subfiletype=1, metadata=None,
                     extratags=[(274, 'H', 1, 6 if matching_orientation else 1, False)])
    target = tmp_path / 'level.npy'
    result = run('--raw', source, target, '--level', 1)
    if matching_orientation:
        assert result.returncode == 0, result.stderr
        np.testing.assert_array_equal(np.load(target)[..., 0], np.rot90(pixels[::2, ::2], -1))
    else:
        assert result.returncode == 2 and 'pyramid orientations differ' in result.stderr
        assert not target.exists()


@pytest.mark.parametrize('orientation', range(1, 9))
def test_ome_tiff_normalizes_coherent_orientation_and_calibration(tmp_path, orientation):
    source = tmp_path / 'rotated.ome.tif'
    pixels = (np.arange(2 * 2 * 5 * 7, dtype=np.uint16) * 17).reshape(2, 2, 5, 7)
    tifffile.imwrite(source, pixels, ome=True, photometric='minisblack',
                     metadata={'axes': 'ZCYX', 'PhysicalSizeX': 2.0, 'PhysicalSizeY': 5.0, 'PhysicalSizeZ': 3.0},
                     extratags=[(274, 'H', 1, orientation, False)], rowsperstrip=2)
    before = source.read_bytes()
    plane = np.moveaxis(pixels[1], 0, -1)
    transposed = plane.swapaxes(0, 1)
    expected = (plane, plane[:, ::-1], plane[::-1, ::-1], plane[::-1], transposed,
                np.rot90(plane, -1), transposed[::-1, ::-1], np.rot90(plane, 1))[orientation - 1]
    bundle, output = tmp_path / 'channel-stack.zip', tmp_path / 'stack.npy'
    result = run('--raw', source, bundle, '--z', 1, '--channels', 2, '--bundle')
    assert result.returncode == 0, result.stderr
    result = run('--extract-raw-bundle', bundle, output)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(output), expected)
    calibration = json.loads(result.stdout)['calibration']
    assert (calibration['base_pixel_width_um'], calibration['base_pixel_height_um']) == (
        (5, 2) if orientation >= 5 else (2, 5))
    assert calibration['z_spacing_um'] == 3
    if orientation == 6 and os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'rotated.qpx'
        created = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                  project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert created.returncode == 0, created.stderr
        info = json.loads(created.stdout)
        assert (info['height'], info['width']) == expected.shape[:2]
        saved = project.read_bytes()
        replay = tmp_path / 'saved.npy'
        result = run('--raw-project', project, 1, replay, '--z', 1, '--channels', 2)
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == output.read_bytes() and project.read_bytes() == saved
    assert source.read_bytes() == before


@pytest.mark.parametrize(('case', 'diagnostic'), [
    ('mixed', 'plane orientations differ'),
    ('swapped-xml', 'plane geometry/type/pyramid differs'),
])
def test_oriented_ome_tiff_refuses_incompatible_geometry(tmp_path, case, diagnostic):
    source = tmp_path / 'invalid.ome.tif'
    width, height = (5, 7) if case == 'swapped-xml' else (7, 5)
    xml = (f'<OME xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06"><Image ID="Image:0">'
           f'<Pixels ID="Pixels:0" SizeX="{width}" SizeY="{height}" SizeZ="2" SizeC="1" SizeT="1" '
           'DimensionOrder="XYZCT" Type="uint16"><Channel ID="Channel:0:0" SamplesPerPixel="1"/>'
           '<TiffData/></Pixels></Image></OME>')
    pixels = np.arange(35, dtype=np.uint16).reshape(5, 7)
    with tifffile.TiffWriter(source) as writer:
        writer.write(pixels, description=xml, metadata=None, extratags=[(274, 'H', 1, 6, False)])
        writer.write(pixels + 100, metadata=None, extratags=[(274, 'H', 1, 8 if case == 'mixed' else 6, False)])
    output = tmp_path / 'never.npy'
    result = run('--raw', source, output)
    assert result.returncode == 2 and diagnostic in result.stderr
    assert not output.exists()


@pytest.mark.parametrize('roi_reference', [False, True])
def test_oriented_ome_compound_pyramid_shares_one_frame(tmp_path, roi_reference):
    paths = [tmp_path / f'channel-{c}.ome.tif' for c in range(2)]
    mapping = ''.join(f'<TiffData IFD="0" FirstC="{c}"><UUID FileName="{path.name}">urn:qpx-{c}</UUID></TiffData>'
                      for c, path in enumerate(paths))
    pixels = np.arange(19 * 23, dtype=np.uint16).reshape(19, 23)
    for c, path in enumerate(paths):
        roi = '<ROIRef ID="ROI:0"/>' if c and roi_reference else ''
        definition = '<ROI ID="ROI:0"><Union><Point ID="Shape:0" X="1" Y="2"/></Union></ROI>' if roi else ''
        xml = (f'<OME UUID="urn:qpx-{c}"><Image><Pixels SizeX="23" SizeY="19" SizeZ="1" SizeC="2" SizeT="1" '
               'DimensionOrder="XYZCT" Type="uint16" PhysicalSizeX="2" PhysicalSizeY="5">'
               f'<Channel/><Channel/>{mapping}</Pixels>{roi}</Image>{definition}</OME>')
        plane = pixels + c * 1000
        with tifffile.TiffWriter(path) as writer:
            writer.write(plane, description=xml, metadata=None, subifds=1, extratags=[(274, 'H', 1, 6, False)])
            writer.write(plane[::2, ::2], subfiletype=1, metadata=None, extratags=[(274, 'H', 1, 6, False)])
    expected = np.stack([np.rot90((pixels + c * 1000)[::2, ::2], -1) for c in range(2)], axis=-1)
    for c, path in enumerate(paths):
        output = tmp_path / f'pyramid-{c}.npy'
        result = run('--raw', path, output, '--channels', 2, '--level', 1)
        assert result.returncode == 0, result.stderr
        np.testing.assert_array_equal(np.load(output), expected)


@pytest.mark.parametrize('orientation', range(1, 9))
def test_ome_tiff_registers_exact_source_rois_to_saved_raster(tmp_path, orientation):
    manifest = os.environ.get('QUPATHXX_TEST_OME_ROI_MANIFEST')
    if not manifest:
        pytest.skip('admitted OME ROI provider manifest is required')
    source, xml_path = tmp_path / 'image.ome.tif', tmp_path / 'image.xml'
    base, saved = tmp_path / 'base.qpx', tmp_path / 'annotations.qpx'
    companion = tmp_path / 'master.ome'
    tiff_data, other_image = '<TiffData/>', ''
    if orientation == 6:
        uuid = '<UUID FileName="image.ome.tif">urn:fixture</UUID>'
        tiff_data = f'<TiffData IFD="1">{uuid}</TiffData>'
        other_image = ('<Image ID="Image:Decoy"><Pixels ID="Pixels:Decoy" SizeX="7" SizeY="5" '
                       'SizeZ="1" SizeC="1" SizeT="1" DimensionOrder="XYZCT" Type="uint8">'
                       f'<Channel ID="Channel:Decoy"/><TiffData IFD="0">{uuid}</TiffData></Pixels></Image>')
    xml = ('<OME UUID="urn:master" xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06">'
           f'{other_image}<Image ID="Image:0"><Pixels ID="Pixels:0" SizeX="7" SizeY="5" SizeZ="1" SizeC="1" SizeT="1" '
           'DimensionOrder="XYZCT" Type="uint8"><Channel ID="Channel:0"/>'
           f'{tiff_data}</Pixels><ROIRef ID="ROI:0"/></Image>'
           '<ROI ID="ROI:0"><Union><Point ID="Shape:Point" X="2.5" Y="1.5"/>'
           '<Rectangle ID="Shape:Rect" X="1" Y="1" Width="2" Height="3"/>'
           '<Mask ID="Shape:Mask" X="2" Y="1" Width="3" Height="2">'
           '<BinData Length="4" BigEndian="true" Compression="none">oA==</BinData></Mask>'
           '</Union></ROI></OME>').encode('ascii')
    description = xml
    if orientation == 6:
        companion.write_bytes(xml)
        description = (b'<OME UUID="urn:fixture" xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06">'
                       b'<BinaryOnly MetadataFile="master.ome" UUID="urn:master"/></OME>')
    pixels = np.arange(1, 36, dtype=np.uint8).reshape(5, 7)
    tags = [(274, 'H', 1, orientation, False)]
    with tifffile.TiffWriter(source) as writer:
        if other_image:
            writer.write(np.zeros_like(pixels), description=description, metadata=None, extratags=tags)
            writer.write(pixels, metadata=None, extratags=tags)
        else:
            writer.write(pixels, description=description, metadata=None, extratags=tags)
    source_before = source.read_bytes()
    result = export_ome_xml(source, xml_path, '--resolve-metadata')
    assert result.returncode == 0, result.stderr
    assert xml_path.read_bytes() == xml
    result = run_format_import('project-create', source, base, '--reader', 'tiff', '--series', int(bool(other_image)))
    assert result.returncode == 0, result.stderr
    base_before = base.read_bytes()
    command = ('--format-plugins', manifest, 'annotations-import', 'org.qupathxx.ome-roi', base, 1)
    if other_image:
        decoy = tmp_path / 'decoy.qpx'
        result = run_format_import('project-create', source, decoy, '--reader', 'tiff')
        assert result.returncode == 0, result.stderr
        result = run_format_import(*command[:4], decoy, 1, xml_path, saved, '--ome-tiff-coordinates', 'stored')
        assert result.returncode == 2 and 'must match the selected native OME-TIFF series' in result.stderr
        assert not saved.exists()
    if orientation != 1:
        result = run_format_import(*command, xml_path, saved)
        assert result.returncode == 2 and 'requires explicit stored-coordinate' in result.stderr
        assert not saved.exists()
    mismatch = tmp_path / 'unverified.xml'
    mismatch.write_bytes(xml + b'\n')
    result = run_format_import(*command, mismatch, saved, '--ome-tiff-coordinates', 'stored')
    assert result.returncode == 2 and 'must match the selected native OME-TIFF series' in result.stderr
    assert not saved.exists()
    result = run_format_import(*command, xml_path, saved, '--ome-tiff-coordinates', 'stored')
    assert result.returncode == 0, result.stderr

    geojson = tmp_path / 'reopened.geojson'
    result = run_format_import('geojson-export', saved, 1, geojson)
    assert result.returncode == 0, result.stderr
    features = {f['properties']['name']: f for f in json.loads(geojson.read_bytes())['features']}
    assert set(features) == {'Shape:Point', 'Shape:Rect', 'Shape:Mask'}
    for feature in features.values():
        metadata = feature['properties']['metadata']
        assert metadata['qpx.annotation-plugin.ome_tiff_orientation'] == str(orientation)
        assert metadata['qpx.annotation-plugin.source_sha256'] == hashlib.sha256(xml).hexdigest()
    expected = (pixels, pixels[:, ::-1], pixels[::-1, ::-1], pixels[::-1], pixels.T,
                np.rot90(pixels, -1), pixels.T[::-1, ::-1], np.rot90(pixels, 1))[orientation - 1]
    raw = tmp_path / 'reopened.npy'
    result = run('--raw-project', saved, 1, raw)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(raw)[..., 0], expected)
    y, x = np.argwhere(expected == pixels[1, 2])[0]
    assert features['Shape:Point']['geometry'] == {'type': 'Point', 'coordinates': [x + .5, y + .5]}

    def geometry_codes(geometry):
        def contains(ring, x, y):
            return sum((y1 > y) != (y2 > y) and x < (x2-x1)*(y-y1)/(y2-y1)+x1
                       for (x1, y1), (x2, y2) in zip(ring, ring[1:])) % 2

        assert geometry['type'] in ('Polygon', 'MultiPolygon')
        polygons = geometry['coordinates']
        if geometry['type'] == 'Polygon':
            polygons = [polygons]
        return {int(expected[y, x]) for y, x in np.ndindex(expected.shape)
                if any(contains(rings[0], x+.5, y+.5) and
                       not any(contains(ring, x+.5, y+.5) for ring in rings[1:]) for rings in polygons)}

    assert geometry_codes(features['Shape:Rect']['geometry']) == set(pixels[1:4, 1:3].flat)
    assert geometry_codes(features['Shape:Mask']['geometry']) == {pixels[1, 2], pixels[1, 4]}
    assert source.read_bytes() == source_before and base.read_bytes() == base_before
    if companion.exists():
        assert companion.read_bytes() == xml


@pytest.mark.parametrize(('photo', 'orientation', 'storage'), [
    ('minisblack', 1, {'rowsperstrip': 3}),
    ('miniswhite', 6, {'tile': (16, 16), 'compression': 'deflate'}),
])
def test_ome_bit_stacks_preserve_samples_polarity_and_saved_view(tmp_path, photo, orientation, storage):
    source = tmp_path / 'binary.ome.tif'
    z, y, x = np.indices((3, 19, 21))
    bits = (7 * x + 3 * y + 2 * z) % 11 < 4
    tifffile.imwrite(source, bits, ome=True, photometric=photo,
                     metadata={'axes': 'ZYX', 'PhysicalSizeX': 2.0, 'PhysicalSizeY': 4.0},
                     extratags=[(274, 'H', 1, orientation, False)], **storage)
    with tifffile.TiffFile(source) as tiff:
        assert all(page.bitspersample == 1 for page in tiff.pages)
        assert 'Type="bit"' in tiff.ome_metadata
    before = source.read_bytes()
    expected = np.rot90(bits, -1, axes=(1, 2)) if orientation == 6 else bits
    raw, bundle = tmp_path / 'raw.npy', tmp_path / 'bit.zip'
    result = run('--raw', source, bundle, '--z', 2, '--bundle')
    assert result.returncode == 0, result.stderr
    result = run('--extract-raw-bundle', bundle, raw)
    assert result.returncode == 0, result.stderr
    assert np.load(raw).dtype == 'u1'
    np.testing.assert_array_equal(np.load(raw)[..., 0], expected[2])
    calibration = json.loads(result.stdout)['calibration']
    assert (calibration['base_pixel_width_um'], calibration['base_pixel_height_um']) == (
        (4, 2) if orientation == 6 else (2, 4))
    height, width = expected.shape[1:]
    pam = tmp_path / 'display.pam'
    result = run('--reader', 'tiff', source, 0, 0, 0, width, height, pam)
    assert result.returncode == 0, result.stderr
    visible = ~expected[0] if photo == 'miniswhite' else expected[0]
    rgba = np.full((height, width, 4), 255, dtype='u1')
    rgba[..., :3] = (visible.astype('u1') * 255)[..., None]
    assert pam.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba.tobytes()
    if os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'binary.qpx'
        created = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                  project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert created.returncode == 0, created.stderr
        saved = project.read_bytes()
        replay = tmp_path / 'replay.npy'
        result = run('--raw-project', project, 1, replay, '--z', 2)
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == raw.read_bytes() and project.read_bytes() == saved
    assert source.read_bytes() == before


@pytest.mark.parametrize(('case', 'diagnostic'), [
    ('byte-storage', 'one-bit grayscale TIFF storage'),
    ('mixed-storage', 'one-bit grayscale TIFF storage'),
    ('significant-bits', 'SignificantBits of one'),
    ('wrong-type', 'outside the native pixel type profile'),
])
def test_ome_bit_rejects_conflicting_storage(tmp_path, case, diagnostic):
    source = tmp_path / 'invalid-bit.ome.tif'
    declared = 'uint8' if case == 'wrong-type' else 'bit'
    significant = 2 if case == 'significant-bits' else 1
    xml = (f'<OME><Image><Pixels SizeX="7" SizeY="5" SizeZ="2" SizeC="1" SizeT="1" '
           f'DimensionOrder="XYZCT" Type="{declared}" SignificantBits="{significant}">'
           '<Channel/><TiffData/></Pixels></Image></OME>')
    values = np.arange(35).reshape(5, 7) % 2 == 0
    with tifffile.TiffWriter(source) as writer:
        writer.write(values.astype('u1') if case == 'byte-storage' else values, description=xml, metadata=None)
        writer.write(values.astype('u1') if case in ('byte-storage', 'mixed-storage') else values, metadata=None)
    target = tmp_path / 'refused.npy'
    result = run('--raw', source, target)
    assert result.returncode == 2 and diagnostic in result.stderr
    assert not target.exists()


@pytest.mark.parametrize('dtype', ['uint8', 'uint16'])
def test_plain_tiff_palette_keeps_indices_and_uses_display_colors(tmp_path, dtype):
    path = tmp_path / 'indexed.tif'
    count = np.iinfo(dtype).max + 1
    indices = np.resize(np.array([0, 1, count // 2, count - 1], dtype=dtype), (17, 19))
    colormap = np.zeros((3, count), dtype=np.uint16)
    colormap[:, 0] = [32768, 65535, 128]
    colormap[:, 1] = [65407, 16448, 32896]
    colormap[:, count // 2] = [32767, 129, 65535]
    colormap[:, -1] = [8192, 0, 65535]
    rotated = dtype == 'uint16'
    tifffile.imwrite(path, indices, photometric='palette', colormap=colormap, byteorder='>' if rotated else '<',
                     tile=(16, 16), compression='deflate', metadata=None,
                     extratags=[(274, 'H', 1, 6 if rotated else 1, False)])
    with tifffile.TiffFile(path) as source:
        np.testing.assert_array_equal(source.asarray(), indices)
        np.testing.assert_array_equal(source.pages[0].colormap, colormap)
    before = path.read_bytes()
    expected = np.rot90(indices, -1) if rotated else indices
    raw = tmp_path / 'indices.npy'
    result = run('--raw', path, raw)
    assert result.returncode == 0, result.stderr
    actual = np.load(raw)
    assert actual.dtype == indices.dtype
    np.testing.assert_array_equal(actual[..., 0], expected)

    h, w = expected.shape
    display = tmp_path / 'display.pam'
    result = run('--reader', 'tiff', path, -1, -2, 0, w + 1, h + 2, display)
    assert result.returncode == 0, result.stderr
    rgba = np.zeros((h + 2, w + 1, 4), dtype='u1')
    rgba[2:, 1:, :3] = np.rint(colormap.T[expected] / 257).astype('u1')
    rgba[2:, 1:, 3] = 255
    assert display.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba.tobytes()

    if os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        tool = os.environ['QUPATHXX_TEST_FORMAT_IMPORT']
        project = tmp_path / 'indexed.qpxproj'
        create = subprocess.run([tool, 'project-create', path, project, '--reader', 'tiff'],
                                capture_output=True, encoding='utf-8', timeout=40)
        assert create.returncode == 0, create.stderr
        saved_bytes = project.read_bytes()
        replay = tmp_path / 'replay.pam'
        result = run('--project-region', project, 1, -1, -2, 0, w + 1, h + 2, replay, '--receipt')
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == display.read_bytes()
        replay_raw = tmp_path / 'replay.npy'
        result = run('--raw-project', project, 1, replay_raw)
        assert result.returncode == 0, result.stderr
        assert replay_raw.read_bytes() == raw.read_bytes()

        custom = tmp_path / 'custom.qpxproj'
        updated = subprocess.run([tool, 'project-set-view', project, '1', custom, '--display-upper', '1,1,1'],
                                 capture_output=True, encoding='utf-8', timeout=40)
        assert updated.returncode == 0, updated.stderr
        index_view = tmp_path / 'custom.pam'
        result = run('--project-region', custom, 1, 0, 0, 0, w, h, index_view)
        assert result.returncode == 0, result.stderr
        rgba_index = np.full((h, w, 4), 255, dtype='u1')
        rgba_index[..., :3] = ((expected != 0) * 255)[..., None]
        assert index_view.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba_index.tobytes()
        assert project.read_bytes() == saved_bytes
    assert path.read_bytes() == before


def test_plain_tiff_palette_reads_selected_subifd_map(tmp_path):
    source = tmp_path / 'palette-pyramid.tif'
    base = np.ones((17, 19), dtype=np.uint8)
    red = np.zeros((3, 256), dtype=np.uint16)
    blue = red.copy()
    red[0, 1] = blue[2, 1] = 65535
    with tifffile.TiffWriter(source) as writer:
        writer.write(base, photometric='palette', colormap=red, subifds=1, metadata=None)
        writer.write(base[::2, ::2], photometric='palette', colormap=blue, subfiletype=1, metadata=None)
    for level, color in enumerate((bytes([255, 0, 0, 255]), bytes([0, 0, 255, 255]))):
        h, w = base[::2**level, ::2**level].shape
        target = tmp_path / f'level-{level}.pam'
        result = run('--reader', 'tiff', source, 0, 0, level, w, h, target)
        assert result.returncode == 0, result.stderr
        assert target.read_bytes().split(b'ENDHDR\n', 1)[1] == color * (h * w)


def test_ome_tiff_palette_requires_a_separate_channel_contract(tmp_path):
    source = tmp_path / 'indexed.ome.tif'
    colormap = np.zeros((3, 256), dtype=np.uint16)
    tifffile.imwrite(source, np.ones((8, 9), dtype=np.uint8), photometric='palette', colormap=colormap,
                     ome=True, metadata={'axes': 'YX'})
    output = tmp_path / 'never.npy'
    result = run('--raw', source, output)
    assert result.returncode == 2 and 'OME-TIFF palette display is unsupported' in result.stderr
    assert not output.exists()


@pytest.mark.parametrize(('bigtiff', 'byteorder', 'photometric'), [
    (False, '<', 'minisblack'), (True, '>', 'minisblack'), (False, '>', 'rgb'),
])
def test_tiff_rejects_palette_declaration_without_color_map(tmp_path, bigtiff, byteorder, photometric):
    source = tmp_path / 'no-colormap.tif'
    shape = (8, 9, 3) if photometric == 'rgb' else (8, 9)
    pixels = np.arange(np.prod(shape), dtype='u1').reshape(shape)
    with tifffile.TiffWriter(source, bigtiff=bigtiff, byteorder=byteorder) as writer:
        writer.write(pixels, metadata=None, photometric=photometric)
        writer.write(pixels, metadata=None, photometric=photometric)
    with tifffile.TiffFile(source, mode='r+b') as tiff:
        tiff.pages[0].tags['PhotometricInterpretation'].overwrite(3)
    with tifffile.TiffFile(source) as tiff:
        assert tiff.pages[0].photometric == 3 and tiff.pages[0].colormap is None
    original = source.read_bytes()
    raw = tmp_path / 'never.npy'
    output = run('--raw', source, raw)
    assert output.returncode == 2 and 'requires its three ColorMap tables' in output.stderr
    assert not raw.exists()
    pam = tmp_path / 'never.pam'
    output = run('--reader', 'tiff', source, 0, 0, 0, 9, 8, pam)
    assert output.returncode == 1 and 'requires its three ColorMap tables' in output.stderr
    assert not pam.exists()
    allowed = tmp_path / 'second.npy'
    channels = 3 if photometric == 'rgb' else 1
    output = run('--raw', source, allowed, '--series', 1, '--channels', channels)
    assert output.returncode == 0, output.stderr
    np.testing.assert_array_equal(np.load(allowed), pixels if channels == 3 else pixels[..., None])
    if os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'never.qpxproj'
        result = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                 project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert result.returncode == 2 and 'requires its three ColorMap tables' in result.stderr
        assert not project.exists()
    assert source.read_bytes() == original


def test_tiff_rejects_pyramid_subifd_palette_without_color_map(tmp_path):
    source = tmp_path / 'missing-subifd-colormap.tif'
    pixels = np.arange(19 * 23, dtype=np.uint8).reshape(19, 23)
    with tifffile.TiffWriter(source) as writer:
        writer.write(pixels, photometric='minisblack', subifds=1, metadata=None)
        writer.write(pixels[::2, ::2], photometric='minisblack', subfiletype=1, metadata=None)
    with tifffile.TiffFile(source, mode='r+b') as tiff:
        tiff.pages[0].pages[0].tags['PhotometricInterpretation'].overwrite(3)
    target = tmp_path / 'never.npy'
    result = run('--raw', source, target, '--level', 1)
    assert result.returncode == 2 and 'requires its three ColorMap tables' in result.stderr
    assert not target.exists()


def write_packed_tiff(source, pixels, tiled, colormap=None, fill_order=1, compression=1,
                      orientation=1, gray_photo=None, bits=None):
    height, width = pixels.shape
    bilevel = colormap is None and gray_photo is None
    bits = bits if bits is not None else (1 if bilevel else 4)
    if tiled:
        grid = np.pad(pixels, ((0, -height % 16), (0, -width % 16)))
        blocks = [grid[y:y+16, x:x+16] for y in range(0, grid.shape[0], 16)
                  for x in range(0, grid.shape[1], 16)]
    else:
        blocks = [pixels[y:y+3] for y in range(0, height, 3)]

    def pack(block):
        if bits == 1:
            packed = np.packbits(block, axis=1, bitorder='big' if fill_order == 1 else 'little')
        else:
            # Nonzero pad samples make an accidental continuous row stride visible.
            per_byte = 8 // bits
            padded = np.pad(block, ((0, 0), (0, -block.shape[1] % per_byte)), constant_values=(1 << bits) - 1)
            shifts = np.arange(8 - bits, -1, -bits, dtype='u1')
            packed = np.bitwise_or.reduce(padded.reshape(block.shape[0], -1, per_byte) << shifts, axis=2)
        if compression == 32773:
            return b''.join(bytes([len(row) - 1]) + row.tobytes() for row in packed)
        if compression == 8:
            return zlib.compress(packed.tobytes())
        return packed.tobytes()

    encoded = [pack(block) for block in blocks]
    offset_tag, count_tag = (324, 325) if tiled else (273, 279)
    tags = {
        256: (4, [width]), 257: (4, [height]), 258: (3, [bits]),
        259: (3, [compression]), 262: (3, [gray_photo if gray_photo is not None else (1 if bilevel else 3)]),
        266: (3, [fill_order]), 274: (3, [orientation]), 277: (3, [1]),
        offset_tag: (4, [0] * len(encoded)), count_tag: (4, [len(block) for block in encoded]),
    }
    if colormap is not None:
        tags[320] = (3, colormap.ravel().tolist())
    tags.update({322: (4, [16]), 323: (4, [16])} if tiled else {278: (4, [3])})
    base = 8 + 2 + 12 * len(tags) + 4
    position = base + sum(len(values) * (2 if kind == 3 else 4)
                          for kind, values in tags.values() if len(values) * (2 if kind == 3 else 4) > 4)
    offsets = []
    for block in encoded:
        offsets.append(position)
        position += len(block)
    tags[offset_tag] = (4, offsets)
    directory, external = bytearray(), bytearray()
    for tag, (kind, values) in sorted(tags.items()):
        payload = struct.pack('<' + ('H' if kind == 3 else 'I') * len(values), *values)
        directory += struct.pack('<HHI', tag, kind, len(values))
        if len(payload) > 4:
            directory += struct.pack('<I', base + len(external))
            external += payload
        else:
            directory += payload.ljust(4, b'\0')
    source.write_bytes(struct.pack('<2sHIH', b'II', 42, 8, len(tags)) + directory +
                       b'\0' * 4 + external + b''.join(encoded))


@pytest.mark.parametrize(('tiled', 'compression', 'orientation'), [
    (False, 1, 1), (True, 1, 1), (False, 32773, 1), (True, 8, 6),
])
def test_four_bit_tiff_palette_reads_exact_row_and_tile_indices(tmp_path, tiled, compression, orientation):
    source = tmp_path / 'packed.tif'
    rows, columns = np.indices((19, 21))
    pixels = ((rows * 9 + columns * 7) % 16).astype('u1')
    colormap = (np.arange(48).astype('u2') * 1273).reshape(3, 16)
    write_packed_tiff(source, pixels, tiled, colormap, compression=compression, orientation=orientation)
    with tifffile.TiffFile(source) as tiff:
        page = tiff.pages[0]
        assert (page.bitspersample, page.is_tiled, page.compression) == (4, tiled, compression)
        np.testing.assert_array_equal(page.colormap, colormap)
    expected = np.rot90(pixels, -1) if orientation == 6 else pixels
    height, width = expected.shape
    raw = tmp_path / 'image.npy'
    result = run('--raw', source, raw)
    assert result.returncode == 0, result.stderr
    image = np.load(raw)
    assert image.dtype == np.uint8
    np.testing.assert_array_equal(image[..., 0], expected)
    crop = tmp_path / 'crop.npy'
    result = run('--raw', source, crop, '--x', 15, '--y', 1, '--width', 4, '--height', 17)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(crop)[..., 0], expected[1:18, 15:19])
    pam = tmp_path / 'image.pam'
    result = run('--reader', 'tiff', source, 0, 0, 0, width, height, pam)
    assert result.returncode == 0, result.stderr
    rgba = np.full((height, width, 4), 255, dtype='u1')
    rgba[..., :3] = np.rint(colormap.T[expected] / 257).astype('u1')
    assert pam.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba.tobytes()
    if not tiled and os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'packed.qpxproj'
        create = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                 project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert create.returncode == 0, create.stderr
        replay = tmp_path / 'replay.npy'
        result = run('--raw-project', project, 1, replay)
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == raw.read_bytes()


def test_four_bit_tiff_palette_rejects_nonstandard_fill_order(tmp_path):
    source = tmp_path / 'fill-order.tif'
    write_packed_tiff(source, np.ones((5, 7), dtype='u1'), False, np.zeros((3, 16), dtype='u2'), fill_order=2)
    target = tmp_path / 'never.npy'
    result = run('--raw', source, target)
    assert result.returncode == 2 and 'requires most-significant-bit FillOrder' in result.stderr
    assert not target.exists()


@pytest.mark.parametrize('profile', ['miniswhite', 'reverse-fill', 'group3', 'group4'])
def test_bilevel_tiff_preserves_bits_and_display_polarity(tmp_path, profile):
    source = tmp_path / 'bilevel.tif'
    y, x = np.indices((19, 21))
    pixels = (x * 7 + y * 3) % 11 < 4
    if profile == 'miniswhite':
        tifffile.imwrite(source, pixels, photometric='miniswhite', rowsperstrip=5, metadata=None)
    elif profile == 'reverse-fill':
        write_packed_tiff(source, pixels.astype('u1'), True, fill_order=2, orientation=6)
    else:
        from PIL import Image

        Image.fromarray(pixels).save(source, compression=profile)
        with Image.open(source) as independent:
            np.testing.assert_array_equal(np.asarray(independent), pixels)
    with tifffile.TiffFile(source) as tiff:
        page = tiff.pages[0]
        assert page.bitspersample == 1
        assert page.compression == {'miniswhite': 1, 'reverse-fill': 1, 'group3': 3, 'group4': 4}[profile]
        white_is_zero = page.photometric == 0
        if profile == 'miniswhite':
            np.testing.assert_array_equal(page.asarray(), pixels)
    expected = np.rot90(pixels, -1) if profile == 'reverse-fill' else pixels
    if white_is_zero and profile in ('group3', 'group4'):
        expected = ~expected
    initial = source.read_bytes()
    raw = tmp_path / 'raw.npy'
    result = run('--raw', source, raw)
    assert result.returncode == 0, result.stderr
    data = np.load(raw)
    assert data.dtype == np.dtype('u1')
    np.testing.assert_array_equal(data[..., 0], expected)

    h, w = expected.shape
    crop = tmp_path / 'crop.npy'
    result = run('--raw', source, crop, '--x', 7, '--y', 1, '--width', 9, '--height', h - 1)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(crop)[..., 0], expected[1:, 7:16])

    display = tmp_path / 'display.pam'
    result = run('--reader', 'tiff', source, -1, 0, 0, w + 1, h, display)
    assert result.returncode == 0, result.stderr
    rgba = np.zeros((h, w + 1, 4), dtype='u1')
    rgba[:, 1:, :3] = ((~expected if white_is_zero else expected) * 255)[..., None]
    rgba[:, 1:, 3] = 255
    assert display.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba.tobytes()
    if profile == 'miniswhite' and os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'bilevel.qpxproj'
        result = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                 project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert result.returncode == 0, result.stderr
        saved = project.read_bytes()
        replay_raw, replay_display = tmp_path / 'replay.npy', tmp_path / 'replay.pam'
        result = run('--raw-project', project, 1, replay_raw)
        assert result.returncode == 0, result.stderr
        result = run('--project-region', project, 1, -1, 0, 0, w + 1, h, replay_display)
        assert result.returncode == 0, result.stderr
        assert replay_raw.read_bytes() == raw.read_bytes() and replay_display.read_bytes() == display.read_bytes()
        assert project.read_bytes() == saved
    assert source.read_bytes() == initial


@pytest.mark.parametrize(('photo', 'tiled', 'compression', 'orientation'), [
    (0, False, 32773, 1), (1, True, 8, 6),
])
def test_four_bit_grayscale_preserves_exact_codes_and_display(tmp_path, photo, tiled, compression, orientation):
    source = tmp_path / 'gray.tif'
    y, x = np.indices((19, 21))
    stored = ((7 * y + 5 * x) % 16).astype('u1')
    write_packed_tiff(source, stored, tiled, compression=compression, orientation=orientation, gray_photo=photo)
    with tifffile.TiffFile(source) as tiff:
        page = tiff.pages[0]
        assert (page.bitspersample, page.photometric) == (4, photo)
    expected = np.rot90(stored, -1) if orientation == 6 else stored
    h, w = expected.shape
    raw = tmp_path / 'gray.npy'
    result = run('--raw', source, raw)
    assert result.returncode == 0, result.stderr
    values = np.load(raw)
    assert values.dtype == np.dtype('u1')
    np.testing.assert_array_equal(values[..., 0], expected)
    crop = tmp_path / 'crop.npy'
    result = run('--raw', source, crop, '--x', 1, '--y', 2, '--width', w - 1, '--height', h - 2)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(crop)[..., 0], expected[2:, 1:])

    display = tmp_path / 'gray.pam'
    result = run('--reader', 'tiff', source, -1, 0, 0, w + 1, h, display)
    assert result.returncode == 0, result.stderr
    rgba = np.zeros((h, w + 1, 4), dtype='u1')
    rgba[:, 1:, :3] = (17 * (expected if photo == 1 else 15 - expected))[..., None]
    rgba[:, 1:, 3] = 255
    assert display.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba.tobytes()
    importer = os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT')
    if importer and photo == 0:
        project = tmp_path / 'gray.qpx'
        result = subprocess.run([importer, 'project-create', source, project, '--reader', 'tiff'],
                                capture_output=True, encoding='utf-8', timeout=40)
        assert result.returncode == 0, result.stderr
        replay = tmp_path / 'replay.npy'
        result = run('--raw-project', project, 1, replay)
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == raw.read_bytes()


@pytest.mark.parametrize(('photo', 'tiled', 'compression', 'orientation'), [
    (0, False, 32773, 1), (1, True, 8, 6), (3, False, 1, 1), (3, True, 8, 8),
])
def test_two_bit_tiff_preserves_codes_and_display(tmp_path, photo, tiled, compression, orientation):
    source = tmp_path / 'four-codes.tif'
    y, x = np.indices((19, 21))
    stored = ((3*y + x) % 4).astype('u1')
    colormap = (np.arange(12, dtype='u2') * 5000).reshape(3, 4) if photo == 3 else None
    write_packed_tiff(source, stored, tiled, colormap, compression=compression,
                      orientation=orientation, gray_photo=photo, bits=2)
    with tifffile.TiffFile(source) as tiff:
        page = tiff.pages[0]
        assert (page.bitspersample, page.photometric, page.is_tiled) == (2, photo, tiled)
    expected = np.rot90(stored, {1: 0, 6: -1, 8: 1}[orientation])
    raw, crop = tmp_path / 'raw.npy', tmp_path / 'crop.npy'
    result = run('--raw', source, raw)
    assert result.returncode == 0, result.stderr
    values = np.load(raw)
    assert values.dtype == np.uint8
    np.testing.assert_array_equal(values[..., 0], expected)
    result = run('--raw', source, crop, '--x', 3, '--y', 2, '--width', 14, '--height', 17)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(crop)[..., 0], expected[2:19, 3:17])
    h, w = expected.shape
    pam = tmp_path / 'display.pam'
    result = run('--reader', 'tiff', source, 0, 0, 0, w, h, pam)
    assert result.returncode == 0, result.stderr
    rgba = np.full((h, w, 4), 255, dtype='u1')
    rgba[..., :3] = (np.rint(colormap.T[expected] / 257).astype('u1') if photo == 3 else
                     (85 * (3 - expected if photo == 0 else expected))[..., None])
    assert pam.read_bytes().split(b'ENDHDR\n', 1)[1] == rgba.tobytes()
    if os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project, replay = tmp_path / 'packed.qpx', tmp_path / 'replayed.npy'
        result = run_format_import('project-create', source, project, '--reader', 'tiff')
        assert result.returncode == 0, result.stderr
        result = run('--raw-project', project, 1, replay)
        assert result.returncode == 0, result.stderr
        assert replay.read_bytes() == raw.read_bytes()


@pytest.mark.parametrize(('photo', 'diagnostic'), [(0, 'most-significant-bit FillOrder'),
                                                   (3, 'missing required "Colormap" field')])
def test_two_bit_tiff_rejects_unsupported_packing_and_missing_palette(tmp_path, photo, diagnostic):
    source, target = tmp_path / 'invalid.tif', tmp_path / 'never.npy'
    write_packed_tiff(source, np.ones((2, 5), dtype='u1'), False, gray_photo=photo,
                      fill_order=2 if photo == 0 else 1, bits=2)
    result = run('--raw', source, target)
    assert result.returncode == 2 and diagnostic in result.stderr
    assert not target.exists()


@pytest.mark.parametrize('photo', [0, 3])
def test_native_label_import_preserves_three_two_bit_tiff_ids(tmp_path, photo):
    labels = os.environ.get('QUPATHXX_TEST_LABELS')
    if not labels:
        pytest.skip('native labels CLI path is required')

    def label_command(*args):
        result = subprocess.run([labels, *map(str, args)], capture_output=True, encoding='utf-8', timeout=40)
        assert result.returncode == 0, result.stderr
        return json.loads(result.stdout)

    image, mask = tmp_path / 'source.tif', tmp_path / 'packed-labels.tif'
    stored = np.array([[0, 1, 2, 3, 1], [3, 0, 3, 1, 2]], dtype='u1')
    samples = np.arange(30, dtype='u1').reshape(2, 5, 3)
    tifffile.imwrite(image, samples, photometric='rgb', metadata=None)
    # A flat palette deliberately displays every label with the same color.
    write_packed_tiff(mask, stored, False, np.zeros((3, 4), dtype='u2') if photo == 3 else None,
                      gray_photo=photo, bits=2)
    base, selected, annotated = (tmp_path / name for name in ['base.qpx', 'selected.qpx', 'annotated.qpx'])
    region = tmp_path / 'crop.geojson'
    region.write_text(json.dumps({'type': 'FeatureCollection', 'features': [
        {'type': 'Feature', 'id': '1', 'properties': {'objectType': 'annotation'},
         'geometry': {'type': 'Polygon', 'coordinates': [[[0, 0], [5, 0], [5, 2], [0, 2], [0, 0]]]}}
    ]}), encoding='utf-8')
    result = run_format_import('project-create', image, base, '--reader', 'tiff')
    assert result.returncode == 0, result.stderr
    result = run_format_import('geojson-import', base, 1, region, selected, '--assume-pixels')
    assert result.returncode == 0, result.stderr
    roi, = json.loads(result.stdout)['objects']
    protected = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (image, mask, selected)}
    capture = ['--image', 1, '--roi', roi['object_id'], '--channels', 3]
    identity = label_command('--label-input', selected, *capture)['input_sha256']
    saved = label_command('--import-label-mask', selected, annotated, *capture, '--mask', mask,
                          '--producer', 'authored-fixture', '--producer-version', 1,
                          '--model-sha256', 'a' * 64, '--input-sha256', identity)
    assert saved['regions'] == 3
    options = ['--image', 1, '--record', saved['record']]
    replay = label_command('--verify-labels', annotated, *options)
    assert replay['raster_sha256'] == saved['raster_sha256']
    measurements = tmp_path / 'measurements.csv'
    label_command('--export-labels', annotated, measurements, *options)
    with measurements.open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 9 and {int(row['source_label']) for row in rows} == {1, 2, 3}
    for code in (1, 2, 3):
        codes = stored == code
        reported = [row for row in rows if int(row['source_label']) == code]
        assert all(int(row['pixels']) == np.count_nonzero(codes) for row in reported)
        sums = {int(row['source_channel_or_rgb_component']): float(row['sum']) for row in reported}
        assert sums == dict(enumerate(samples[codes].sum(axis=0)))
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in protected.items())


@pytest.mark.parametrize('dtype',['uint16','int64','uint64','float64'])
def test_native_raw_export_exact_and_existing_output_preserved(tmp_path,dtype):
    values=np.arange(2*3*2*19*23,dtype='uint64').reshape(2,3,2,19,23).astype(dtype)
    if dtype=='uint64':values[1,2,1,0,0]=2**64-1
    path=tmp_path/'量化.ome.tif';output=tmp_path/'原始.npy'
    if dtype in ['int64','uint64']:
        tifffile.imwrite(path,values[1,2,1],ome=False,photometric='minisblack',tile=(16,16),compression='deflate',metadata=None)
        axes=[]
    else:
        tifffile.imwrite(path,values,ome=True,photometric='minisblack',tile=(16,16),compression='deflate',metadata={'axes':'TZCYX'})
        axes=['--z',2,'--time',1,'--channel',1]
    args=['--raw',path,output,*axes,'--width',23,'--height',19]
    done=run(*args);assert done.returncode==0,done.stderr
    receipt=json.loads(done.stdout);assert receipt['dtype']==dtype and receipt['valid_pixels']==19*23
    np.testing.assert_array_equal(np.load(output)[...,0],values[1,2,1])
    before=output.read_bytes();assert run(*args).returncode!=0 and output.read_bytes()==before
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def test_input_and_hardlink_cannot_be_overwritten(tmp_path):
    source=tmp_path/'input.tif';tifffile.imwrite(source,np.ones((8,8),dtype='uint16'),metadata=None)
    alias=tmp_path/'alias.npy';os.link(source,alias);before=source.read_bytes()
    for output in [source,alias]:
        done=run('--raw',source,output)
        assert done.returncode!=0 and source.read_bytes()==before


def test_ome_default_mapping_ignores_unmapped_thumbnail(tmp_path):
    values = (np.arange(120, dtype='uint16') * 123).reshape(2, 2, 2, 3, 5)  # Z, C, T, Y, X
    xml = ('<OME xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06"><Image ID="Image:0">'
           '<Pixels ID="Pixels:0" SizeX="5" SizeY="3" SizeZ="2" SizeC="2" SizeT="2" Type="uint16" '
           'DimensionOrder="XYTCZ" PhysicalSizeX="0.5" PhysicalSizeY="1.25" PhysicalSizeZ="2">'
           '<Channel ID="Channel:0:0" Name="DNA" SamplesPerPixel="1"/>'
           '<Channel ID="Channel:0:1" Name="RNA" SamplesPerPixel="1"/>'
           '<TiffData/></Pixels></Image></OME>')

    def write(name, description, count, extra):
        path = tmp_path / name
        with tifffile.TiffWriter(path, ome=False) as writer:
            for index, plane in enumerate(values.reshape(-1, 3, 5)[:count]):
                writer.write(plane, description=description if index == 0 else None,
                             photometric='minisblack', metadata=None)
            if extra:
                writer.write(np.full((2, 3, 3), 231, dtype='uint8'), photometric='rgb', metadata=None)
        return path

    source = write('preview.ome.tif', xml, 8, True)
    original = source.read_bytes()
    for z in range(2):
        for time in range(2):
            output = tmp_path / f'z{z}-t{time}.npy'
            done = run('--reader', 'tiff', '--raw', source, output, '--z', z, '--time', time, '--channels', 2)
            assert done.returncode == 0, done.stderr
            raw = np.load(output, allow_pickle=False)
            assert raw.dtype == values.dtype
            np.testing.assert_array_equal(raw, np.moveaxis(values[z, :, time], 0, -1))
    bundle = tmp_path / 'calibration.zip'
    done = run('--reader', 'tiff', '--raw', source, bundle, '--bundle')
    assert done.returncode == 0, done.stderr
    with zipfile.ZipFile(bundle) as packet:
        calibration = json.loads(packet.read('receipt.json'))['calibration']
    assert calibration['base_pixel_width_um'] == 0.5 and calibration['base_pixel_height_um'] == 1.25
    assert calibration['z_spacing_um'] == 2
    assert source.read_bytes() == original

    explicit = write('explicit.ome.tif', xml.replace('<TiffData/>', '<TiffData PlaneCount="9"/>'), 8, True)
    incomplete = write('incomplete.ome.tif', xml, 7, False)
    bad = tmp_path / 'rejected.npy'
    for invalid, error in ((explicit, 'exceeds plane or IFD bounds'), (incomplete, 'mapping is incomplete')):
        done = run('--reader', 'tiff', '--raw', invalid, bad)
        assert done.returncode != 0 and error in done.stderr and not bad.exists()


@pytest.mark.parametrize('case,interval', [
    ('regular', 0.1), ('mixed-units', 0.1), ('explicit', 0.25), ('offset', None),
    ('irregular', None), ('disagree', None), ('missing', None), ('duplicate', None), ('repeated-tiny', None),
])
def test_ome_plane_timing_calibrates_only_regular_single_file_grids(tmp_path, case, interval):
    source = tmp_path / 'timestamps.ome.tif'
    pixels = np.arange(4 * 2 * 2 * 8 * 9, dtype=np.uint16).reshape(4, 2, 2, 8, 9)
    values = [t * 100 for t in range(4) for _ in range(4)]
    units = ['ms'] * len(values)
    if case == 'mixed-units':
        for plane in range(0, len(values), 2):
            values[plane] /= 1000
            units[plane] = 's'
    elif case == 'offset':
        values = [value + 50 for value in values]
    elif case == 'irregular':
        values[-4:] = [360] * 4
    elif case == 'disagree':
        values[-1] += 1
    elif case == 'repeated-tiny':
        values = [value for value in (0, 5e-324, 5e-324, 1.5e-323) for _ in range(4)]
        units = ['s'] * len(values)
    metadata = {'axes': 'TZCYX', 'Plane': {'DeltaT': values, 'DeltaTUnit': units}}
    if case == 'explicit':
        metadata.update(TimeIncrement=250, TimeIncrementUnit='ms')
    tifffile.imwrite(source, pixels, ome=True, photometric='minisblack', metadata=metadata)
    with tifffile.TiffFile(source) as reader:
        ome = ElementTree.fromstring(reader.ome_metadata)
    planes = ome.findall('.//{*}Plane')
    assert len(planes) == 16
    assert [float(row.attrib['DeltaT']) for row in planes] == values
    if case == 'missing':
        del planes[0].attrib['DeltaT']
    elif case == 'duplicate':
        for axis in ('TheC', 'TheZ', 'TheT'):
            planes[1].set(axis, planes[0].attrib[axis])
    if case in ('missing', 'duplicate'):
        tifffile.tiffcomment(source, ElementTree.tostring(ome, encoding='unicode'))
    archive = tmp_path / 'timed.zip'
    finished = run('--raw', source, archive, '--z', 1, '--time', 3, '--channels', 2, '--bundle')
    assert finished.returncode == 0, finished.stderr
    extracted = tmp_path / 'timed.npy'
    finished = run('--extract-raw-bundle', archive, extracted)
    assert finished.returncode == 0, finished.stderr
    receipt = json.loads(finished.stdout)
    assert receipt['calibration'].get('time_interval_seconds') == interval
    np.testing.assert_array_equal(np.load(extracted), np.moveaxis(pixels[3, 1], 0, -1))


@pytest.mark.parametrize(('case', 'spacing'), [
    ('regular', 0.375), ('mixed-units', 0.375), ('explicit', 2.0),
    ('invalid-explicit', None), ('offset', None), ('irregular', None), ('disagree', None),
    ('missing', None), ('reference-frame', None), ('duplicate', None),
])
def test_ome_plane_z_spacing_requires_regular_physical_positions(tmp_path, case, spacing):
    source = tmp_path / 'planes.ome.tif'
    pixels = np.arange(2 * 4 * 2 * 2 * 3, dtype=np.uint16).reshape(2, 4, 2, 2, 3)
    positions = [375 * z for _ in range(2) for z in range(4) for _ in range(2)]
    units = ['nm'] * len(positions)
    if case == 'mixed-units':
        for plane in range(0, len(positions), 2):
            positions[plane] /= 1_000_000
            units[plane] = 'mm'
    elif case == 'offset':
        positions = [value + 10 for value in positions]
    elif case == 'irregular':
        positions = [1200 if z == 1125 else z for z in positions]
    elif case == 'disagree':
        positions[-1] += 1
    metadata = {'axes': 'TZCYX', 'Plane': {'PositionZ': positions}}
    if case != 'reference-frame':
        metadata['Plane']['PositionZUnit'] = units
    if case in ('explicit', 'invalid-explicit'):
        metadata.update(PhysicalSizeZ=2000 if case == 'explicit' else -2000, PhysicalSizeZUnit='nm')
    tifffile.imwrite(source, pixels, ome=True, photometric='minisblack', metadata=metadata)
    with tifffile.TiffFile(source) as reader:
        ome = ElementTree.fromstring(reader.ome_metadata)
        np.testing.assert_array_equal(reader.asarray(), pixels)
    planes = ome.findall('.//{*}Plane')
    assert [float(plane.attrib['PositionZ']) for plane in planes] == positions
    if case == 'missing':
        del planes[-1].attrib['PositionZ']
    elif case == 'duplicate':
        for axis in ('TheC', 'TheZ', 'TheT'):
            planes[1].set(axis, planes[0].attrib[axis])
    if case in ('missing', 'duplicate'):
        tifffile.tiffcomment(source, ElementTree.tostring(ome, encoding='unicode'))

    def export(argv, label):
        archive = tmp_path / (label + '.zip')
        finished = run(*argv, archive, '--z', 3, '--time', 1, '--channels', 2, '--bundle')
        assert finished.returncode == 0, finished.stderr
        extract = tmp_path / (label + '.npy')
        finished = run('--extract-raw-bundle', archive, extract)
        assert finished.returncode == 0, finished.stderr
        receipt = json.loads(finished.stdout)
        assert receipt['calibration'].get('z_spacing_um') == spacing
        np.testing.assert_array_equal(np.load(extract), np.moveaxis(pixels[1, 3], 0, -1))
        return extract.read_bytes(), receipt

    initial = source.read_bytes()
    direct, _ = export(['--raw', source], 'direct')
    if case == 'regular' and os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT'):
        project = tmp_path / 'planes.qpxproj'
        finished = subprocess.run([os.environ['QUPATHXX_TEST_FORMAT_IMPORT'], 'project-create', source,
                                   project, '--reader', 'tiff'], capture_output=True, encoding='utf-8', timeout=40)
        assert finished.returncode == 0, finished.stderr
        previous = project.read_bytes()
        saved, receipt = export(['--raw-project', project, '1'], 'saved')
        assert saved == direct
        assert receipt['region']['project']['project_sha256'] == hashlib.sha256(previous).hexdigest()
        assert project.read_bytes() == previous
    assert source.read_bytes() == initial


def test_format_cli_saves_tiff_series_and_rejects_other_readers(tmp_path):
    executable = Path(os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT') or Path(_native.__file__).with_name(
        'qupathxx_format_import.exe' if os.name == 'nt' else 'qupathxx_format_import'))
    if not executable.is_file() or not q.has_project:
        pytest.skip('native project format CLI is unavailable')

    def format_command(*args, error=None):
        done = subprocess.run([executable, *map(str, args)], capture_output=True, encoding='utf-8', timeout=40)
        if error:
            assert done.returncode == 2 and error in json.loads(done.stderr)['error'], done
            return None
        assert done.returncode == 0, done
        return json.loads(done.stdout)

    path = tmp_path / 'series.tif'
    pixels = np.arange(30, dtype='uint16').reshape(5, 6)
    with tifffile.TiffWriter(path, ome=False) as writer:
        for offset in (10, 110):
            writer.write(pixels + offset, photometric='minisblack', metadata=None)
    before = path.read_bytes()
    first, both = tmp_path / 'first.qpx', tmp_path / 'both.qpx'
    report = format_command('project-create', path, first, '--reader', 'tiff', '--series', 1)
    assert (report['reader'], report['series'], report['image_id']) == ('tiff', 1, 1)
    report = format_command('project-add-image', first, path, both, '--reader', 'tiff', '--series', 0)
    assert (report['reader'], report['series'], report['image_id']) == ('tiff', 0, 2)

    with closing(sqlite3.connect(both.as_uri() + '?mode=ro&immutable=1', uri=True)) as db:
        rows = db.execute('SELECT image_id,payload FROM tiff_views ORDER BY image_id').fetchall()
    assert [(image, payload.splitlines()[1]) for image, payload in rows] == [('1', b'1 0 0'), ('2', b'0 0 0')]
    ppm = tmp_path / 'source.ppm'
    ppm.write_bytes(b'P6\n1 1\n255\nabc')
    denied = tmp_path / 'denied.qpx'
    format_command('project-create', ppm, denied, '--series', 0, error='requires the native TIFF reader')
    format_command('project-create', path, denied, '--reader', 'tiff', '--series', 2, error='outside the file')
    format_command('project-create', path, denied, '--series', -1, error='series index')
    format_command('project-create', path, denied, '--series', 0, '--series', 1, error='Duplicate')
    assert not denied.exists() and path.read_bytes() == before

    project_bytes = both.read_bytes()
    catalog = format_command('project-images', both)
    assert catalog['state'] == 'listed' and catalog['active_image_id'] == 1
    assert catalog['reader_diagnostics'] == []
    assert [(image['image_id'], image['view']['series']) for image in catalog['images']] == [(1, 1), (2, 0)]
    for image in catalog['images']:
        assert Path(image['source']) == path and image['reader'] == 'tiff' and image['available']
        assert (image['width'], image['height'], image['objects'], image['embedded']) == (6, 5, 0, False)

    relocated = path.rename(tmp_path / 'relocated.tif')
    offline = format_command('project-images', both)
    assert offline == {**catalog, 'images': [{**image, 'available': False} for image in catalog['images']]}
    sidecar = both.with_name(both.name + '-wal')
    sidecar.write_bytes(b'preserve this sidecar')
    format_command('project-images', both, error='SQLite sidecar present')
    assert sidecar.read_bytes() == b'preserve this sidecar'
    assert both.read_bytes() == project_bytes and relocated.read_bytes() == before


@pytest.mark.parametrize('flag,value',[('--z','-1'),('--channels','0'),('--width','999999999'),('--level','9'),('--unknown','1')])
def test_invalid_arguments_never_create_output(tmp_path,flag,value):
    source=tmp_path/'input.tif';tifffile.imwrite(source,np.ones((8,8),dtype='uint16'),metadata=None)
    output=tmp_path/'bad.npy';done=run('--raw',source,output,flag,value)
    assert done.returncode==2 and not output.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))


def test_format_cli_updates_project_plane_and_region_defaults(tmp_path):
    executable = Path(os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT') or Path(_native.__file__).with_name(
        'qupathxx_format_import.exe' if os.name == 'nt' else 'qupathxx_format_import'))
    if not executable.is_file() or not q.has_project:
        pytest.skip('native project format CLI is unavailable')

    def format_command(*args, error=None):
        result = subprocess.run([executable, *map(str, args)], capture_output=True, encoding='utf-8', timeout=30)
        if error:
            assert result.returncode == 2 and error in json.loads(result.stderr)['error'], result
            return
        assert result.returncode == 0, result.stderr
        return json.loads(result.stdout)

    image = tmp_path / 'views.ome.tif'
    pixels = np.arange(2 * 3 * 2 * 8 * 9, dtype=np.uint16).reshape(2, 3, 2, 8, 9)
    tifffile.imwrite(image, pixels, ome=True, photometric='minisblack', metadata={'axes': 'TZCYX'})
    base = tmp_path / 'base.qpx'
    image_id = format_command('project-create', image, base, '--reader', 'tiff')['image_id']
    saved = tmp_path / 'annotated.qpx'
    annotations = tmp_path / 'annotation.geojson'
    annotations.write_text(json.dumps({
        'type': 'Feature', 'geometry': {'type': 'Point', 'coordinates': [2, 3],
                                       'plane': {'c': 1, 'z': 1, 't': 1}},
        'properties': {'objectType': 'annotation', 'name': 'Point on a nondefault plane'},
    }), encoding='utf-8')
    format_command('geojson-import', base, image_id, annotations, saved, '--assume-pixels')
    original = saved.read_bytes()
    catalog = format_command('project-images', saved)
    [initial] = catalog['images']
    selected = tmp_path / 'selected.qpx'
    view = format_command('project-set-view', saved, image_id, selected, '--z', 2, '--time', 1)['view']
    assert view == {**initial['view'], 'z': 2, 't': 1}
    [reopened] = format_command('project-images', selected)['images']
    assert reopened == {**initial, 'view': view}
    default = tmp_path / 'saved-plane.npy'
    result = run('--raw-project', selected, image_id, default, '--channel', 1)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(default)[..., 0], pixels[1, 2, 1])

    earlier_time = tmp_path / 'earlier-time.qpx'
    partial = format_command('project-set-view', selected, image_id, earlier_time, '--time', 0)
    assert partial['view'] == {**view, 't': 0}
    raw = tmp_path / 'earlier-time.npy'
    result = run('--raw-project', earlier_time, image_id, raw, '--channel', 1)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(raw)[..., 0], pixels[0, 2, 1])
    original_objects, exported_objects = tmp_path / 'original.geojson', tmp_path / 'exported.geojson'
    format_command('geojson-export', saved, image_id, original_objects)
    format_command('geojson-export', selected, image_id, exported_objects)
    assert original_objects.read_bytes() == exported_objects.read_bytes()

    denied = tmp_path / 'denied.qpx'
    format_command('project-set-view', saved, image_id, denied, '--z', 65536, error='must be 0..65535')
    format_command('project-set-view', saved, image_id, denied, '--z', 3, error='outside this series')
    format_command('project-set-view', saved, image_id, denied, '--time', 0, '--time', 1, error='Duplicate')
    format_command('project-set-view', saved, image_id, saved, '--z', 1, error='must not replace its input')
    assert not denied.exists() and saved.read_bytes() == original

    selected_bytes = selected.read_bytes()
    color = tmp_path / 'display.qpx'
    display_view = {**view, 'channels': [1, 0, -1], 'lower': [780, 710, 0], 'upper': [820, 750, 255]}
    assert format_command(
        'project-set-view', selected, image_id, color, '--display-channels', '1,0,-1',
        '--display-lower', '780,710,0', '--display-upper', '820,750,255',
    )['view'] == display_view
    [display_image] = format_command('project-images', color)['images']
    assert display_image == {**reopened, 'view': display_view}

    rendered = tmp_path / 'display.pam'
    result = run('--project-region', color, image_id, 0, 0, 0, 4, 2, rendered)
    assert result.returncode == 0, result.stderr
    header, rgba = rendered.read_bytes().split(b'ENDHDR\n', 1)
    assert b'WIDTH 4\nHEIGHT 2\nDEPTH 4\n' in header
    expected = np.zeros((2, 4, 4), dtype=np.uint8)
    expected[..., 3] = 255
    for component, source_channel in enumerate((1, 0)):
        lower = display_view['lower'][component]
        upper = display_view['upper'][component]
        samples = pixels[1, 2, source_channel, :2, :4].astype(float)
        expected[..., component] = np.floor(np.clip((samples - lower) / (upper - lower), 0, 1) * 255 + 0.5)
    assert rgba == expected.tobytes()

    raw_after_display = tmp_path / 'after-display.npy'
    result = run('--raw-project', color, image_id, raw_after_display, '--channel', 1)
    assert result.returncode == 0, result.stderr
    assert raw_after_display.read_bytes() == default.read_bytes()

    partial_display = tmp_path / 'contrast-only.qpx'
    assert format_command(
        'project-set-view', color, image_id, partial_display, '--display-upper', '824,754,255',
    )['view'] == {**display_view, 'upper': [824, 754, 255]}
    for options, message in [
        (('--display-channels', '1,0'), 'three comma-separated'),
        (('--display-lower', 'nan,0,0'), 'three comma-separated finite'),
        (('--display-channels', '2,0,-1'), 'channel is outside the image'),
        (('--display-channels', '-1,-1,-1'), 'requires an enabled channel'),
        (('--display-channels', '1,0,-1', '--display-channels', '0,1,-1'), 'Duplicate'),
        (('--display-upper', '700,600,255'), 'range/span must be finite and increasing'),
    ]:
        format_command('project-set-view', color, image_id, denied, *options, error=message)
        assert not denied.exists()
    assert selected.read_bytes() == selected_bytes

    image.rename(tmp_path / 'offline.ome.tif')
    [offline] = format_command('project-images', color)['images']
    assert offline == {**display_image, 'available': False}


def test_project_region_renders_saved_channel_view_and_guards_sources(tmp_path):
    from test_project_source import fingerprint

    if not q.has_project:
        pytest.skip('project storage is unavailable')
    image = tmp_path / 'source.ome.tif'
    pixels = np.zeros((2, 2, 2, 8, 9), dtype=np.uint8)
    pixels[1, 1, 0] = 17
    pixels[1, 1, 1] = 83
    def write(path):
        tifffile.imwrite(path, pixels, ome=True, photometric='minisblack', metadata={'axes': 'TZCYX'})
    write(image)
    project = q.Project()
    image_id = project.add_tiff_image(str(image))
    view = project.document(image_id).server.tiff_view
    view.z = view.t = 1
    view.channels = [1, 0, -1]
    project.set_tiff_view(image_id, view)
    project.apply_source_review(project.review_source_verification(image_id, fingerprint(image)))
    saved = tmp_path / 'saved.qpx'
    project.save(str(saved))
    del project
    original = saved.read_bytes()

    def render(output, *prefix, receipt=False):
        suffix = ('--receipt',) if receipt else ()
        return run(*prefix, '--project-region', saved, image_id, -1, 0, 0, 4, 2, output, *suffix)

    destination = tmp_path / 'view.pam'
    result = render(destination)
    assert result.returncode == 0 and not result.stdout, result.stderr
    header, rgba = destination.read_bytes().split(b'ENDHDR\n', 1)
    assert b'WIDTH 4\nHEIGHT 2\nDEPTH 4\n' in header
    assert rgba == (bytes((0, 0, 0, 0)) + bytes((83, 17, 0, 255)) * 3) * 2
    receipted = tmp_path / 'receipted.pam'
    result = render(receipted, receipt=True)
    assert result.returncode == 0, result.stderr
    data = receipted.read_bytes()
    assert data == destination.read_bytes()
    report = json.loads(result.stdout)
    assert report['schema'] == 'qpx.project-display-region/1' and report['format'] == 'pam-rgba8'
    assert Path(report['output']) == receipted
    assert report['bytes'] == len(data) and report['sha256'] == hashlib.sha256(data).hexdigest()
    assert report['project']['project_sha256'] == hashlib.sha256(original).hexdigest()
    assert report['project']['image_id'] == image_id
    assert report['reader'] == 'tiff' and Path(report['reader_source']['locator']) == image
    assert report['region'] == {'x': -1, 'y': 0, 'level': 0, 'width': 4, 'height': 2}
    assert report['view'] == {'series': 0, 'z': 1, 't': 1, 'channels': [1, 0, -1],
                              'lower': [0, 0, 0], 'upper': [255, 255, 255]}
    occupied = render(receipted, receipt=True)
    assert occupied.returncode == 1 and not occupied.stdout and receipted.read_bytes() == data
    assert render(destination).returncode == 1
    assert render(tmp_path / 'override.pam', '--reader', 'tiff').returncode == 2
    sidecar = saved.with_name(saved.name + '-wal')
    assert render(sidecar).returncode == 1 and not sidecar.exists()
    assert render(saved).returncode == 1 and saved.read_bytes() == original

    pixels[1, 1, 1] = 100
    replacement = tmp_path / 'replacement.ome.tif'
    write(replacement)
    replacement.replace(image)
    refused = tmp_path / 'refused.pam'
    result = render(refused, receipt=True)
    assert result.returncode == 1 and 'differs from saved source verification' in result.stderr
    assert not result.stdout
    assert not refused.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))


def test_raw_project_uses_saved_view_and_protects_links(tmp_path):
    if not q.has_project:
        pytest.skip('project storage is unavailable')
    source = tmp_path / 'pixels.ome.tif'
    values = np.arange(2 * 3 * 2 * 8 * 9, dtype=np.uint16).reshape(2, 3, 2, 8, 9)
    tifffile.imwrite(source, values, ome=True, photometric='minisblack', metadata={'axes': 'TZCYX'})
    project = q.Project()
    image = project.add_tiff_image(str(source))
    view = project.document(image).server.tiff_view
    view.z, view.t = 2, 1
    project.set_tiff_view(image, view)
    project.document(image).set_image_axis_calibration(q.ImageAxisCalibration(4.5, [-7]))
    other = tmp_path / 'original.ppm'
    other.write_bytes(b'P6\n1 1\n255\nabc')
    project.add_image(str(other))
    saved = tmp_path / 'saved.qpx'
    project.save(str(saved))
    del project
    before = {path: path.read_bytes() for path in (saved, source)}
    with closing(sqlite3.connect(saved.as_uri() + '?mode=ro&immutable=1', uri=True)) as db:
        project_id, generation = db.execute('SELECT identity,generation FROM project').fetchone()
    expected_project = {
        'project_id': project_id,
        'project_sha256': hashlib.sha256(before[saved]).hexdigest(),
        'generation': generation,
        'image_id': image,
    }
    other.rename(tmp_path / 'relocated.ppm')

    def export(output, *options, error=None):
        result = run('--raw-project', saved, image, output, *options)
        if error:
            assert result.returncode == 2 and error in result.stderr, result
            return None
        assert result.returncode == 0, result.stderr
        return json.loads(result.stdout)

    default = tmp_path / 'default.npy'
    receipt = export(default, '--channel', 1)
    assert (receipt['z'], receipt['time'], receipt['series']) == (2, 1, 0)
    assert receipt['project'] == expected_project
    assert receipt['project_axes'] == {'z_spacing_um': 4.5, 'z_position_um': 9, 'timepoint_seconds': None}
    np.testing.assert_array_equal(np.load(default)[..., 0], values[1, 2, 1])
    explicit = tmp_path / 'explicit.zip'
    receipt = export(explicit, '--z', 0, '--time', 0, '--channel', 1, '--with-validity', '--bundle')
    assert (receipt['z'], receipt['time']) == (0, 0)
    assert receipt['project'] == expected_project
    assert receipt['project_axes'] == {'z_spacing_um': 4.5, 'z_position_um': 0, 'timepoint_seconds': -7}
    unpacked = tmp_path / 'explicit.npy'
    result = run('--extract-raw-bundle', explicit, unpacked)
    assert result.returncode == 0, result.stderr
    bundle = json.loads(result.stdout)
    assert bundle['schema'] == 'qpx.raw-region-bundle/3' and bundle['region'] == receipt
    structured = np.load(unpacked)
    assert structured['valid'].all()
    np.testing.assert_array_equal(structured['values'][..., 0], values[0, 0, 1])
    same = tmp_path / 'standalone.npy'
    result = run('--raw', source, same, '--time', 1, '--z', 2, '--channel', 1)
    assert result.returncode == 0 and same.read_bytes() == default.read_bytes(), result.stderr

    invalid = tmp_path / 'invalid.npy'
    export(invalid, '--series', 0, error='saved reader')
    export(saved, error='must not replace its input')
    export(other, error='must not replace its input')
    alias = tmp_path / 'alias.npy'
    os.link(source, alias)
    export(alias, error='aliases its input')
    assert not invalid.exists() and not other.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))
    assert {path: path.read_bytes() for path in before} == before

    saved.rename(tmp_path / 'relocated.qpx')
    source.rename(tmp_path / 'relocated.ome.tif')
    offline = tmp_path / 'offline.npy'
    result = run('--extract-raw-bundle', explicit, offline)
    assert result.returncode == 0 and json.loads(result.stdout) == bundle, result.stderr
    assert offline.read_bytes() == unpacked.read_bytes()

    def canonical(data):
        return json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode('utf-8')

    inconsistent = {
        **bundle,
        'region': {**receipt, 'project_axes': {**receipt['project_axes'], 'z_position_um': 1.0}},
    }
    del inconsistent['receipt_sha256']
    inconsistent['receipt_sha256'] = hashlib.sha256(canonical(inconsistent)).hexdigest()
    bad_axes = tmp_path / 'bad-project-axes.zip'
    bad_axes.write_bytes(change_member(explicit.read_bytes(), 'receipt.json',
                                       lambda _: canonical(inconsistent) + b'\n'))
    rejected = run('--extract-raw-bundle', bad_axes, invalid)
    assert rejected.returncode == 2 and 'inconsistent project Z calibration' in rejected.stderr
    assert not invalid.exists()

    for label, changed_project, refresh_digest, error in (
        ('bad-hash', {**expected_project, 'project_sha256': 'x' * 64}, True, 'invalid project source'),
        ('zero-id', {**expected_project, 'image_id': 0}, True, 'invalid project source'),
        ('missing-project', None, True, 'inconsistent project source'),
        ('changed-id', {**expected_project, 'image_id': 2}, False, 'receipt digest differs'),
    ):
        updated = {**bundle, 'region': receipt.copy()}
        if changed_project is None:
            updated['region']['context'] = updated['region'].pop('project')
        else:
            updated['region']['project'] = changed_project
        if refresh_digest:
            del updated['receipt_sha256']
            updated['receipt_sha256'] = hashlib.sha256(canonical(updated)).hexdigest()
        altered = tmp_path / f'{label}.zip'
        altered.write_bytes(change_member(explicit.read_bytes(), 'receipt.json',
                                         lambda _: canonical(updated) + b'\n'))
        refused = run('--extract-raw-bundle', altered, invalid)
        assert refused.returncode == 2 and error in refused.stderr, refused
        assert not invalid.exists()


def test_raw_project_requires_review_for_changed_verified_tiff(tmp_path):
    from test_project_source import fingerprint

    if not q.has_project:
        pytest.skip('project storage is unavailable')
    source = tmp_path / 'image.tif'
    original = np.full((6, 9), 23, dtype=np.uint16)
    tifffile.imwrite(source, original, metadata=None)
    project = q.Project()
    image = project.add_tiff_image(str(source))
    project.apply_source_review(project.review_source_verification(image, fingerprint(source)))
    saved = tmp_path / 'verified.qpx'
    project.save(str(saved))
    del project
    saved_before = saved.read_bytes()

    identical = tmp_path / 'identical.tif'
    identical.write_bytes(source.read_bytes())
    identical.replace(source)
    output = tmp_path / 'matching.npy'
    result = run('--raw-project', saved, image, output)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(output)[..., 0], original)

    changed = np.full_like(original, 99)
    replacement = tmp_path / 'replacement.tif'
    tifffile.imwrite(replacement, changed, metadata=None)
    assert replacement.stat().st_size == source.stat().st_size
    replacement.replace(source)
    refused = tmp_path / 'refused.zip'
    result = run('--raw-project', saved, image, refused, '--bundle')
    assert result.returncode == 2 and 'differs from saved source verification' in result.stderr, result
    assert not refused.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))

    project = q.Project.open(str(saved))
    review = project.review_source_verification(image, fingerprint(source))
    assert review.comparison == 'changed_bytes' and review.requires_acceptance
    project.apply_source_review(review, accept_changed=True)
    adopted = tmp_path / 'adopted.qpx'
    project.save(str(adopted))
    verified = tmp_path / 'adopted.npy'
    result = run('--raw-project', adopted, image, verified)
    assert result.returncode == 0, result.stderr
    np.testing.assert_array_equal(np.load(verified)[..., 0], changed)
    assert saved.read_bytes() == saved_before
