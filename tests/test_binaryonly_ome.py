"""Partial OME headers with tracked XML or TIFF metadata masters."""
import gc
import hashlib
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
import pytest
import qupathxx as q
import tifffile

spec=importlib.util.spec_from_file_location('binaryonly_fixture',Path(__file__).resolve().parents[1]/'tools/reference_binaryonly_ome.py')
fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
pytestmark=pytest.mark.skipif(not q.has_tiff,reason='optional native TIFF reader')


def request(*,level=0,z=1,t=1,outside=False):
    scale=1<<level;r=q.RawRegionRequest();r.channels=2;r.z=z;r.t=t
    r.region=q.RegionRequest(-100 if outside else 0,-100 if outside else 0,level,(19+scale-1)//scale,(17+scale-1)//scale)
    return r


def root(path):
    return ET.fromstring(path.read_bytes() if path.suffix=='.ome' else tifffile.tiffcomment(path))


def write(path,element):
    data=ET.tostring(element,encoding='ascii',xml_declaration=True)
    if path.suffix=='.ome':path.write_bytes(data)
    else:tifffile.tiffcomment(path,data.decode('ascii'))


@pytest.mark.parametrize('master',['xml','tiff','tiff-default'])
def test_independent_fixture_has_exact_samples_and_explicit_metadata(tmp_path,master):
    manifest=fixture.generate(tmp_path/'corpus',master);directory=tmp_path/'corpus'
    metadata=root(directory/manifest['metadata_file'])
    assert metadata.get('UUID')==manifest['metadata_uuid'] and metadata.find(fixture.tag('Image')) is not None
    for index,name in enumerate(fixture.planes.FILES):
        with tifffile.TiffFile(directory/name) as source:
            for page,(t,z,c) in zip(source.pages,fixture.planes.PLANES[index],strict=True):
                np.testing.assert_array_equal(page.asarray(),fixture.planes.values(t,z,c))
                np.testing.assert_array_equal(page.pages[0].asarray(),fixture.planes.values(t,z,c,2))
        partial=root(directory/name);assert partial.get('UUID')==fixture.planes.UUIDS[index]
        binary=partial.find(fixture.tag('BinaryOnly'))
        if binary is not None:
            assert binary.get('UUID')==metadata.get('UUID')
            assert ((directory/name).parent/binary.get('MetadataFile')).resolve()==(directory/manifest['metadata_file']).resolve()
    for entry in manifest['files']:
        assert hashlib.sha256((directory/entry['path']).read_bytes()).hexdigest()==entry['sha256']


@pytest.mark.parametrize('master',['xml','tiff','tiff-default'])
@pytest.mark.parametrize('entry',[0,1])
def test_open_either_member_reads_all_axes_levels_and_tracks_metadata(tmp_path,master,entry):
    directory=tmp_path/'corpus';manifest=fixture.generate(directory,master)
    server=q.open_tiff(str(directory/fixture.planes.FILES[entry]));meta=server.raw_metadata
    assert (meta.width,meta.height,meta.channels,meta.z_planes,meta.timepoints)==(19,17,2,2,2)
    assert (meta.pixel_width_um,meta.pixel_height_um)==(.25,.5)
    source=server.reader_source
    observed={source.primary.path,*[f.path for f in source.companions]}
    assert observed=={str((directory/e['path']).resolve()) for e in manifest['files']}
    assert source.scope=='compound_files'
    for level in (0,1):
        for t in (0,1):
            for z in (0,1):
                raw=server.read_raw(request(level=level,z=z,t=t))
                expected=np.stack([fixture.planes.values(t,z,c,1<<level) for c in (0,1)],axis=-1)
                np.testing.assert_array_equal(raw.numpy(),expected);assert raw.valid_numpy().all()
    projected=server.with_tiff_view(server.tiff_view);array=raw.numpy();server.close();del server,raw;gc.collect()
    np.testing.assert_array_equal(array,expected)
    np.testing.assert_array_equal(projected.read_raw(request()).numpy(),np.stack([fixture.planes.values(1,1,c) for c in (0,1)],axis=-1))
    projected.close()


@pytest.mark.parametrize('master',['xml','tiff'])
@pytest.mark.parametrize('operation',['raw','cached','display','outside','view'])
def test_metadata_change_rejects_cached_and_uncached_operations(tmp_path,master,operation):
    directory=tmp_path/'corpus';manifest=fixture.generate(directory,master)
    server=q.open_tiff(str(directory/fixture.planes.FILES[1]));server.read_raw(request())
    path=directory/manifest['metadata_file'];metadata=root(path)
    pixels=metadata.find('.//'+fixture.tag('Pixels'));pixels.set('PhysicalSizeX','.375');write(path,metadata)
    with pytest.raises(RuntimeError,match='changed'):
        if operation in ('raw','cached'):server.read_raw(request(z=0 if operation=='raw' else 1))
        elif operation=='display':server.read_region(q.RegionRequest(0,0,0,19,17))
        elif operation=='outside':server.read_raw(request(outside=True))
        else:server.with_tiff_view(server.tiff_view)
    server.close()


@pytest.mark.parametrize('failure',['missing','empty','malformed','declaration','oversize','uuid','no_filename',
                                  'no_uuid','data_uuid','mixed','cycle','xml_plane','unresolved_plane',
                                  'alias','no_root_uuid','duplicate_binary'])
def test_invalid_metadata_dependency_is_refused(tmp_path,failure):
    directory=tmp_path/'corpus';manifest=fixture.generate(directory)
    path=directory/manifest['metadata_file'];primary=directory/fixture.planes.FILES[0]
    metadata=root(path);partial=root(primary);binary=partial.find(fixture.tag('BinaryOnly'))
    if failure=='missing':path.unlink()
    elif failure=='empty':path.write_bytes(b'')
    elif failure=='malformed':path.write_bytes(b'<OME')
    elif failure=='declaration':path.write_bytes(b'<!DOCTYPE OME><OME/>')
    elif failure=='oversize':path.write_bytes(b' '*(1024*1024+1))
    elif failure=='uuid':metadata.set('UUID','urn:wrong');write(path,metadata)
    elif failure=='no_filename':binary.attrib.pop('MetadataFile');write(primary,partial)
    elif failure=='no_uuid':binary.attrib.pop('UUID');write(primary,partial)
    elif failure=='data_uuid':metadata.set('UUID',fixture.planes.UUIDS[0]);write(path,metadata)
    elif failure=='mixed':partial.append(ET.Element(fixture.tag('Image')));write(primary,partial)
    elif failure=='cycle':binary.set('MetadataFile',fixture.planes.FILES[0]);binary.set('UUID',fixture.planes.UUIDS[0]);write(primary,partial)
    elif failure=='alias':
        alias=directory/'metadata/alias.tif';alias.hardlink_to(primary)
        binary.set('MetadataFile','metadata/alias.tif');write(primary,partial)
    elif failure=='no_root_uuid':partial.attrib.pop('UUID');write(primary,partial)
    elif failure=='duplicate_binary':partial.append(ET.fromstring(ET.tostring(binary)));write(primary,partial)
    elif failure=='xml_plane':
        reference=metadata.find('.//'+fixture.tag('TiffData')+'/'+fixture.tag('UUID'))
        reference.set('FileName',Path(manifest['metadata_file']).name);reference.text=manifest['metadata_uuid'];write(path,metadata)
    else:
        for reference in metadata.iter(fixture.tag('UUID')):
            if reference.text==fixture.planes.UUIDS[1]:reference.attrib.pop('FileName')
        write(path,metadata)
    with pytest.raises((RuntimeError,ValueError,IndexError)):q.open_tiff(str(primary))


@pytest.mark.skipif(not q.has_project,reason='project storage')
@pytest.mark.parametrize('master',['xml','tiff'])
def test_project_reopen_retains_metadata_observation_and_verifies_raw_record(tmp_path,master):
    directory=tmp_path/'corpus';manifest=fixture.generate(directory,master)
    project=q.Project();image=project.add_tiff_image(str(directory/fixture.planes.FILES[1]));doc=project.document(image)
    obj=doc.add_rectangle(q.Rect(0,0,19,17),'BinaryOnly source')
    capture=q.RawAnalysisRequest(doc,request(),obj,tile_size=16);result=q.analyze_raw(capture)
    record=doc.keep_raw_analysis(capture,result);path=tmp_path/'dataset.qpxproj';project.save(str(path))
    restored=q.Project.open(str(path));current=restored.document(image)
    replay=q.RawAnalysisRequest.replay(current,record);actual=q.analyze_raw(replay)
    assert actual.input_sha256==result.input_sha256 and current.verify_raw_analysis(record,replay,actual)
    assert current.raw_analysis_status(record)=='current'
    metadata_path=str((directory/manifest['metadata_file']).resolve())
    assert metadata_path in {actual.source.primary.path,*[f.path for f in actual.source.companions]}
    restored.save(str(path))


@pytest.mark.parametrize('operation',['raw','outside','view'])
def test_missing_xml_dependency_rejects_after_open(tmp_path,operation):
    directory=tmp_path/'corpus';manifest=fixture.generate(directory)
    server=q.open_tiff(str(directory/fixture.planes.FILES[0]));server.read_raw(request())
    (directory/manifest['metadata_file']).unlink()
    with pytest.raises(RuntimeError):
        if operation=='view':server.with_tiff_view(server.tiff_view)
        else:server.read_raw(request(outside=operation=='outside'))
    server.close()


@pytest.mark.skipif(not q.has_project,reason='project storage')
@pytest.mark.parametrize('master',['xml','tiff'])
@pytest.mark.parametrize('grid',[False,True])
def test_cli_batch_resume_tracks_master_metadata_change(tmp_path,master,grid):
    import json
    import os
    import subprocess
    from qupathxx import _native
    directory=tmp_path/'corpus';manifest=fixture.generate(directory,master)
    project=q.Project();image=project.add_tiff_image(str(directory/fixture.planes.FILES[1]))
    project.document(image).add_rectangle(q.Rect(0,0,19,17),'BinaryOnly batch ROI')
    path=tmp_path/'input.qpxproj';project.save(str(path))
    cli=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    command=[str(cli),str(path),str(tmp_path/'output'),'--kind','raw','--raw-channels','2','--raw-z','1','--raw-time','1']
    if grid:command+=['--raw-grid-samples','19,17','--raw-grid-context','0,0']
    for state in ('saved','reused','saved'):
        if state=='saved' and (tmp_path/'output').exists():
            metadata_path=directory/manifest['metadata_file'];metadata=root(metadata_path)
            metadata.find('.//'+fixture.tag('Pixels')).set('PhysicalSizeX','.375');write(metadata_path,metadata)
        done=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert done.returncode==0,done.stderr
        report=next(json.loads(line) for line in done.stdout.splitlines() if json.loads(line)['schema']=='qpx.batch-image/1')
        assert report['state']==state
        result=q.Project.open(report['path']).document(image).raw_analysis_records[0].result
        assert result.source.scope=='compound_files' and len(result.source.companions)==(2 if master=='xml' else 1)
        np.testing.assert_allclose(result.statistics.means,[fixture.planes.values(1,1,c).mean() for c in (0,1)],rtol=0,atol=1e-10)
