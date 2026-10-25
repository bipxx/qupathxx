"""Cross-check generated files using a separate TIFF/OME reader and scalar pixels."""
import hashlib
import importlib.util
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
import pytest
import tifffile

spec=importlib.util.spec_from_file_location('multifile_fixture',Path(__file__).resolve().parents[1]/'tools/reference_multifile_ome.py')
fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)


@pytest.fixture
def corpus(tmp_path):
    directory=tmp_path/'corpus'
    return directory,fixture.generate(directory)


def test_every_stored_plane_and_subifd_matches_scalar_coordinate_equation(corpus):
    directory,manifest=corpus
    positions=[]
    for info in manifest['files']:
        path=directory/info['name']
        assert path.stat().st_size==info['bytes']
        assert hashlib.sha256(path.read_bytes()).hexdigest()==info['sha256']
        with tifffile.TiffFile(path) as source:
            assert len(source.pages)==4
            assert source.byteorder==('<' if info['byteorder']=='little' else '>')
            root=ET.fromstring(source.ome_metadata)
            assert root.attrib['UUID']==info['uuid']
            for plane in info['planes']:
                positions.append((plane['t'],plane['z'],plane['c']))
                pages=[source.pages[plane['ifd']],source.pages[plane['ifd']].pages[0]]
                for scale,page,level in zip((1,2),pages,plane['levels'],strict=True):
                    actual=page.asarray()
                    assert actual.shape==(level['height'],level['width'])
                    for y in range(actual.shape[0]):
                        for x in range(actual.shape[1]):
                            assert actual[y,x]==10000*plane['t']+1000*plane['z']+100*plane['c']+19*y*scale+x*scale
                    assert hashlib.sha256(actual.astype('<u2').tobytes()).hexdigest()==level['canonical_le_sha256']
    assert sorted(positions)==[(t,z,c) for t in range(2) for z in range(2) for c in range(2)]


@pytest.mark.parametrize('entry',fixture.FILES)
def test_independent_ome_reader_resolves_base_fileset_from_either_member(corpus,entry):
    directory,_=corpus
    with tifffile.TiffFile(directory/entry) as source:
        series=source.series[0]
        assert series.axes=='TCZYX'
        # tifffile2024.9.20 explicitly cannot assemble multi-file pyramid
        # levels. Base C/Z/T assembly is independent here; every SubIFD pixel
        # is separately checked through direct IFD access above.
        assert series.shape==(2,2,2,17,19)
        actual=series.asarray()
        expected=np.empty(actual.shape,dtype=np.uint16)
        for t in range(2):
            for c in range(2):
                for z in range(2):
                    for y in range(expected.shape[-2]):
                        for x in range(expected.shape[-1]):
                            expected[t,c,z,y,x]=10000*t+1000*z+100*c+19*y+x
        np.testing.assert_array_equal(actual,expected)


def test_metadata_refs_have_matching_uuid_and_relative_paths(corpus):
    directory,manifest=corpus
    for info in manifest['files']:
        path=directory/info['name']
        with tifffile.TiffFile(path) as source:
            root=ET.fromstring(source.ome_metadata)
        entries=root.findall('.//{*}TiffData')
        assert len(entries)==8
        for entry in entries:
            uuid=entry.find('{*}UUID')
            target=(path.parent/uuid.attrib['FileName']).resolve()
            with tifffile.TiffFile(target) as companion:
                assert ET.fromstring(companion.ome_metadata).attrib['UUID']==uuid.text
                assert 0<=int(entry.attrib['IFD'])<len(companion.pages)
    before=(directory/'manifest.json').read_bytes()
    with pytest.raises(FileExistsError): fixture.generate(directory)
    assert (directory/'manifest.json').read_bytes()==before
    assert json.loads(before)['synthetic']
