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


@pytest.mark.skipif(not q.has_project,reason='project support')
@pytest.mark.parametrize('grid',[False,True])
def test_project_restart_retains_compound_raw_archive_and_explicit_replay(corpus,tmp_path,grid):
    import sqlite3
    directory,_=corpus;project=q.Project();image=project.add_tiff_image(str(directory/fixture.FILES[0]))
    doc=project.document(image);obj=doc.add_rectangle(q.Rect(0,0,19,17),'Independent compound ROI')
    view=doc.server.tiff_view;view.z=view.t=1;view.channels=[1,0,-1];doc.set_tiff_view(view)
    if grid:
        sampling=q.RawGridSampling();sampling.samples_x=19;sampling.samples_y=17
        options=q.ScientificRawOptions();options.channels=2;options.z=1;options.t=1
        capture=q.RawAnalysisRequest.for_grid_roi(doc,obj,sampling,options)
    else: capture=q.RawAnalysisRequest(doc,request(19,17,z=1,t=1),obj,tile_size=16)
    result=q.analyze_raw(capture);record=doc.keep_raw_analysis(capture,result)
    assert result.source.scope=='compound_files' and len(result.source.companions)==1
    np.testing.assert_allclose(result.statistics.means,[fixture.values(1,1,c).mean() for c in range(2)],rtol=0,atol=1e-10)
    path=tmp_path/'compound.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        original=db.execute('SELECT payload FROM raw_analysis_records').fetchone()[0]
        assert original[:4]==(b'QRA\x04' if grid else b'QRA\x03')
    restored=q.Project.open(str(path));current=restored.document(image)
    assert current.server.reader_source.scope=='unopened'
    assert current.server.raw_metadata.channels==2
    assert current.server.reader_source.scope=='compound_files'
    assert current.server.tiff_view.channels==[1,0,-1] and current.server.tiff_view.z==1
    assert current.raw_analysis_status(record)=='unverified'
    replay=q.RawAnalysisRequest.replay(current,record);proof=q.analyze_raw(replay)
    assert proof.input_sha256==result.input_sha256 and current.verify_raw_analysis(record,replay,proof)
    assert current.raw_analysis_status(record)=='current'
    restored.save(str(path))
    with sqlite3.connect(path) as db: assert db.execute('SELECT payload FROM raw_analysis_records').fetchone()[0]==original


@pytest.mark.skipif(not q.has_project,reason='project support')
def test_project_relocation_and_explicit_relink_resolve_companions_again(corpus,tmp_path):
    import shutil
    directory,_=corpus;project=q.Project();image=project.add_tiff_image(str(directory/fixture.FILES[0]))
    view=project.document(image).server.tiff_view;view.z=view.t=1;project.set_tiff_view(image,view)
    path=tmp_path/'linked.qpxproj';project.save(str(path));project.document(image).server.close()
    moved=tmp_path/'moved';shutil.move(str(directory),moved)
    restored=q.Project.open(str(path));restored.relink(image,str(moved/fixture.FILES[0]))
    server=restored.document(image).server
    assert server.raw_metadata.channels==2
    assert len(server.reader_source.companions)==1 and Path(server.reader_source.companions[0].path).is_relative_to(moved)
    np.testing.assert_array_equal(server.read_raw(request(19,17,z=1,t=1)).numpy()[...,1],fixture.values(1,1,1))


@pytest.mark.skipif(not q.has_project,reason='project support')
def test_compound_batch_checkpoint_and_resume(corpus,tmp_path):
    directory,_=corpus;project=q.Project();image=project.add_tiff_image(str(directory/fixture.FILES[0]))
    doc=project.document(image);doc.add_rectangle(q.Rect(0,0,19,17),'Compound batch ROI')
    view=doc.server.tiff_view;view.z=view.t=1;doc.set_tiff_view(view)
    options=q.ProjectBatchOptions();options.kind=q.ProjectBatchKind.RAW;options.raw.channels=2;options.roi_tile_size=16
    capture=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=options)
    output=str(tmp_path/'batch')
    first=q.run_project_batch(capture,output);assert first.state=='succeeded',first.error
    kept=q.Project.open(first.images[0].path).document(image).raw_analysis_records[0].result
    assert kept.source.scope=='compound_files' and len(kept.source.companions)==1
    np.testing.assert_allclose(kept.statistics.means,[fixture.values(1,1,c).mean() for c in range(2)],rtol=0,atol=1e-10)
    again=q.run_project_batch(capture,output);assert again.state=='succeeded' and again.reused==1


@pytest.mark.skipif(not q.has_project,reason='project support')
@pytest.mark.parametrize('grid',[False,True])
def test_compound_cli_batch_and_resume(corpus,tmp_path,grid):
    import os
    import subprocess
    from qupathxx import _native
    directory,_=corpus;project=q.Project();image=project.add_tiff_image(str(directory/fixture.FILES[0]))
    project.document(image).add_rectangle(q.Rect(0,0,19,17),'Compound CLI ROI')
    path=tmp_path/'cli.qpxproj';project.save(str(path))
    cli=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    command=[str(cli),str(path),str(tmp_path/'cli-batch'),'--kind','raw','--raw-channels','2','--raw-z','1','--raw-time','1']
    if grid: command+=['--raw-grid-samples','19,17','--raw-grid-context','0,0']
    for attempt in range(2):
        done=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert done.returncode==0,done.stderr
        saved=next(json.loads(line) for line in done.stdout.splitlines() if json.loads(line)['schema']=='qpx.batch-image/1')
        assert saved['state']==('saved' if attempt==0 else 'reused')
        result=q.Project.open(saved['path']).document(image).raw_analysis_records[0].result
        assert len(result.source.companions)==1 and result.source.scope=='compound_files'
        np.testing.assert_allclose(result.statistics.means,[fixture.values(1,1,c).mean() for c in range(2)],rtol=0,atol=1e-10)


def desktop():
    import os
    if not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1':
        pytest.skip('opt-in compound TIFF desktop')


def test_compound_imgui_measure_keep_and_csv(corpus,tmp_path):
    desktop()
    import csv
    from test_gui_raw import options
    from test_gui_raw_records import wait
    from test_gui_tma import click
    directory,_=corpus;project=q.Project();image=project.add_tiff_image(str(directory/fixture.FILES[0]))
    o=options();o.raw_tab=1;o.raw_request=request(19,17,z=1,t=1);o.raw_csv_path=str(tmp_path/'compound.csv')
    o.inputs=click(5,q.GuiInputTarget.RAW_MEASURE)+[wait(10,q.GuiInputWait.RAW_TASK_FINISHED)]
    o.inputs+=click(12,q.GuiInputTarget.RAW_RESULTS_TAB)+click(15,q.GuiInputTarget.RAW_KEEP)+[wait(18,q.GuiInputWait.RAW_ANALYSIS_KEPT)]
    o.inputs+=click(21,q.GuiInputTarget.RAW_RECORDS_TAB)+click(24,q.GuiInputTarget.RAW_CSV_EXPORT)+[wait(28,q.GuiInputWait.RAW_MEASUREMENTS_SAVED)]
    report=q.show(project,frame_limit=40,options=o)
    assert not report.raw_error,report.raw_error
    assert report.raw_records==1 and report.raw_csv_state=='saved' and report.ui_overflow_items==0
    assert len(report.raw_analysis_result.source.companions)==1
    with Path(o.raw_csv_path).open(encoding='utf-8',newline='') as stream: rows=list(csv.DictReader(stream))
    np.testing.assert_allclose([float(row['mean']) for row in rows],[fixture.values(1,1,c).mean() for c in range(2)],rtol=0,atol=1e-10)
    assert project.document(image).raw_analysis_status(report.raw_record_id)=='current'


def test_compound_managed_measure_verify_and_project_save(corpus,tmp_path):
    desktop()
    from test_viewer_session import completed,run
    directory,_=corpus;project=q.Project();image=project.add_tiff_image(str(directory/fixture.FILES[1]))
    path=tmp_path/'managed-source.qpxproj';project.save(str(path));session=q.ViewerSession(str(path))
    saved=tmp_path/'managed-saved.qpxproj';held={}
    def producer():
        result=completed(session,session.analyze_raw(image,request(19,17,z=1,t=1),retain_result=True))
        assert result.committed and result.record_id
        held['result']=result.raw_analysis;held['record']=result.record_id
        assert completed(session,session.verify_raw_analysis(image,result.record_id)).verified
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    report=run(session,producer,frames=120)
    assert report.ui_overflow_items==0 and session.stats.retained_bytes==0
    result=held['result'];assert result.source.scope=='compound_files' and len(result.source.companions)==1
    np.testing.assert_allclose(result.statistics.means,[fixture.values(1,1,c).mean() for c in range(2)],rtol=0,atol=1e-10)
    restored=q.Project.open(str(saved)).document(image)
    assert restored.raw_analysis_status(held['record'])=='unverified'
    assert len(restored.raw_analysis_records[0].result.source.companions)==1


@pytest.mark.parametrize('count',[64,65])
def test_fileset_file_bound_and_global_cache_budget(tmp_path,count):
    paths=[tmp_path/f'file-{i:02}.tif' for i in range(count)]
    mappings=''.join(f'<TiffData IFD="0" FirstC="{i}" PlaneCount="1"><UUID FileName="{p.name}">urn:file-{i:02}</UUID></TiffData>' for i,p in enumerate(paths))
    for i,path in enumerate(paths):
        xml=f'<OME UUID="urn:file-{i:02}"><Image><Pixels SizeX="2" SizeY="2" SizeZ="1" SizeC="{count}" SizeT="1" DimensionOrder="XYZCT" Type="uint16">{mappings}</Pixels></Image></OME>'
        tifffile.imwrite(path,np.full((2,2),i,dtype='uint16'),metadata=None,description=xml)
    options=q.TiffOpenOptions();options.cache_bytes=1024;options.decode_bytes=65536
    if count==65:
        with pytest.raises((RuntimeError,ValueError),match='64 files'): q.open_tiff(str(paths[0]),options)
    else:
        server=q.open_tiff(str(paths[0]),options)
        assert len(server.reader_source.companions)==63
        r=request(2,2);r.channels=count
        raw=server.read_raw(r)
        np.testing.assert_array_equal(raw.numpy(),np.broadcast_to(np.arange(count,dtype='uint16'),(2,2,count)))
        assert server.tiff_read_stats.peak_cache_bytes<=1024
        server.close()


@pytest.mark.parametrize('aggregate',[False,True])
def test_xml_admission_limits_are_fileset_wide(tmp_path,aggregate):
    count=5 if aggregate else 1
    paths=[tmp_path/f'metadata-{i}.tif' for i in range(count)]
    mappings=''.join(f'<TiffData IFD="0" FirstC="{i}" PlaneCount="1"><UUID FileName="{p.name}">urn:metadata-{i}</UUID></TiffData>' for i,p in enumerate(paths))
    comment='x'*(900*1024 if aggregate else 1024*1024)
    for i,path in enumerate(paths):
        xml=f'<OME UUID="urn:metadata-{i}"><!--{comment}--><Image><Pixels SizeX="1" SizeY="1" SizeZ="1" SizeC="{count}" SizeT="1" DimensionOrder="XYZCT" Type="uint16">{mappings}</Pixels></Image></OME>'
        tifffile.imwrite(path,np.zeros((1,1),dtype='uint16'),metadata=None,description=xml)
    with pytest.raises((RuntimeError,ValueError),match='4 MiB' if aggregate else 'text exceeds limit'):
        q.open_tiff(str(paths[0]))


@pytest.mark.parametrize('failure',['base','pyramid','entity'])
def test_companion_geometry_and_xml_declarations_reject(corpus,failure):
    directory,_=corpus;companion=directory/fixture.FILES[1]
    if failure=='entity':
        with tifffile.TiffFile(companion) as source: xml=source.ome_metadata
        tifffile.tiffcomment(companion,'<!DOCTYPE OME [<!ENTITY x "value">]>'+xml)
    else:
        with tifffile.TiffFile(companion,mode='r+b') as source:
            page=source.pages[0] if failure=='base' else source.pages[0].pages[0]
            page.tags['ImageWidth'].overwrite(18 if failure=='base' else 9)
    with pytest.raises((RuntimeError,ValueError),match='geometry|dimensions|declarations'):
        q.open_tiff(str(directory/fixture.FILES[0]))
