"""Real multi-file failure boundaries, independent samples and job publication."""
import gc
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_multifile_tiff_reader import corpus,fixture,request

pytestmark=pytest.mark.skipif(not q.has_tiff,reason='optional native TIFF reader')


def write_pair(directory,side=256,tiles=(16,256)):
    directory.mkdir();paths=[directory/'primary.ome.tif',directory/'companion.ome.tif']
    yy,xx=np.mgrid[:side,:side];values=[(xx+31*yy+1000*c).astype('uint16') for c in range(2)]
    mappings=''.join(f'<TiffData IFD="0" FirstC="{c}" PlaneCount="1"><UUID FileName="{path.name}">urn:file-{c}</UUID></TiffData>' for c,path in enumerate(paths))
    for c,path in enumerate(paths):
        xml=f'<OME UUID="urn:file-{c}"><Image><Pixels DimensionOrder="XYZCT" Type="uint16" SizeX="{side}" SizeY="{side}" SizeC="2" SizeZ="1" SizeT="1"><Channel Name="Raw 0"/><Channel Name="Raw 1"/>{mappings}</Pixels></Image></OME>'
        tifffile.imwrite(path,values[c],tile=(tiles[c],tiles[c]),byteorder='<' if c==0 else '>',compression='deflate' if c==0 else None,metadata=None,description=xml)
    return paths,values


@pytest.mark.parametrize('entry',fixture.FILES)
@pytest.mark.parametrize('level',[0,1])
def test_corrupt_companion_decode_is_atomic_and_other_channel_remains_readable(corpus,entry,level):
    directory,manifest=corpus;bad_index=1-fixture.FILES.index(entry);bad=directory/fixture.FILES[bad_index]
    original=bad.read_bytes();plane=manifest['files'][bad_index]['planes'][0]
    with tifffile.TiffFile(bad,mode='r+b') as source:
        page=source.pages[0] if level==0 else source.pages[0].pages[0]
        if bad_index==1:
            offsets=list(page.dataoffsets);offsets[0]=bad.stat().st_size-1;page.tags['TileOffsets'].overwrite(tuple(offsets))
        else:
            offset,length=page.dataoffsets[0],page.databytecounts[0]
    if bad_index==0:
        with bad.open('r+b') as stream:stream.seek(offset);stream.write(bytes(length))
    server=q.open_tiff(str(directory/entry));source=server.reader_source
    assert source.scope=='compound_files' and len(source.companions)==1
    scale=2**level;h,w=(17+scale-1)//scale,(19+scale-1)//scale
    region=request(w,h,z=plane['z'],t=plane['t'],level=level)
    good=request(w,h,z=plane['z'],t=plane['t'],level=level);good.channels=1
    retained=server.read_raw(good).numpy()
    np.testing.assert_array_equal(retained[...,0],fixture.values(plane['t'],plane['z'],0,scale))
    for _ in range(2):
        with pytest.raises(RuntimeError,match='TIFF block decoding'):server.read_raw(region)
        np.testing.assert_array_equal(server.read_raw(good).numpy(),retained)
    view=server.tiff_view;view.z=plane['z'];view.t=plane['t'];view.channels=[1,0,-1]
    projected=server.with_tiff_view(view)
    with pytest.raises(RuntimeError,match='TIFF block decoding'):projected.read_region(q.RegionRequest(0,0,level,w,h))
    projected.close();server.close()
    bad.write_bytes(original)
    reopened=q.open_tiff(str(directory/entry));actual=reopened.read_raw(region).numpy()
    np.testing.assert_array_equal(actual,np.stack([fixture.values(plane['t'],plane['z'],c,scale) for c in range(2)],axis=-1))
    assert reopened.reader_source.generation!=source.generation
    reopened.close();gc.collect()
    np.testing.assert_array_equal(retained[...,0],fixture.values(plane['t'],plane['z'],0,scale))


def test_companion_large_tile_decode_budget_refuses_without_poisoning_shared_cache(tmp_path):
    paths,values=write_pair(tmp_path/'tiles')
    options=q.TiffOpenOptions();options.decode_bytes=65536;options.cache_bytes=1024
    server=q.open_tiff(str(paths[0]),options);r=request(256,256);good=request(256,256);good.channels=1
    with pytest.raises((RuntimeError,ValueError),match='decode budget'):server.read_raw(r)
    before=server.tiff_read_stats
    assert before.decoded_blocks==256 and before.peak_cache_bytes<=1024
    np.testing.assert_array_equal(server.read_raw(good).numpy()[...,0],values[0])
    server.close();options.decode_bytes=262144
    admitted=q.open_tiff(str(paths[0]),options)
    np.testing.assert_array_equal(admitted.read_raw(r).numpy(),np.stack(values,axis=-1))
    assert admitted.tiff_read_stats.peak_cache_bytes<=1024
    admitted.close()


@pytest.mark.parametrize('entry',fixture.FILES)
def test_compound_scientific_workspace_exact_and_minus_one_admission(corpus,entry):
    directory,_=corpus;options=q.TiffOpenOptions();options.cache_bytes=0
    server=q.open_tiff(str(directory/entry),options)
    grid=q.ScientificGrid(19,17,q.ScientificGridAxis(0,19,7),q.ScientificGridAxis(0,17,5))
    opts=q.ScientificRawOptions();opts.channels=2;opts.z=1;opts.t=1
    full=q.digest_scientific_raw(server,grid,opts);opts.work_bytes=full.estimated_work_bytes-1
    before=server.tiff_read_stats.decoded_blocks
    with pytest.raises((ValueError,RuntimeError),match='budget'):q.digest_scientific_raw(server,grid,opts)
    assert server.tiff_read_stats.decoded_blocks==before
    opts.work_bytes+=1
    exact=q.digest_scientific_raw(server,grid,opts)
    assert exact.input_sha256==full.input_sha256 and exact.decoded_source_sha256==full.decoded_source_sha256
    before=server.tiff_read_stats.decoded_blocks
    with pytest.raises((ValueError,RuntimeError),match='budget'):q.read_scientific_raw(server,grid,opts)
    assert server.tiff_read_stats.decoded_blocks==before
    server.close()


@pytest.mark.skipif(not q.has_project,reason='project support')
def test_failed_companion_job_never_replaces_existing_record(tmp_path):
    paths,values=write_pair(tmp_path/'decode');bad=paths[1]
    with tifffile.TiffFile(bad,mode='r+b') as source:
        page=source.pages[0];offsets=list(page.dataoffsets);offsets[0]=bad.stat().st_size-1
        page.tags['TileOffsets'].overwrite(tuple(offsets))
    doc=q.Document(q.open_tiff(str(paths[0])))
    good=request(16,16);good.channels=1
    capture=q.RawAnalysisRequest(doc,good,tile_size=16);earlier=q.analyze_raw(capture);record=doc.keep_raw_analysis(capture,earlier)
    before=doc.revision;bad_request=q.RawAnalysisRequest(doc,request(16,16),tile_size=16)
    job=q.RawAnalysisJob();assert job.request(bad_request)
    state=job.wait(doc)
    assert state.state=='failed' and 'TIFF block decoding' in state.error and job.result is None
    with pytest.raises(RuntimeError):job.keep_result(doc)
    assert doc.revision==before and doc.raw_analysis_record_count==1
    assert doc.raw_analysis_record(record).result.input_sha256==earlier.input_sha256
    assert job.request(q.RawAnalysisRequest(doc,good,tile_size=16))
    assert job.wait(doc).state=='ready'
    assert job.result.input_sha256==earlier.input_sha256
    np.testing.assert_array_equal(doc.server.read_raw(good).numpy()[...,0],values[0][:16,:16])
    doc.server.close()


def cancellation_child(directory,phase):
    paths,_=write_pair(directory/'cancel',side=1024,tiles=(16,16))
    options=q.TiffOpenOptions();options.cache_bytes=0
    doc=q.Document(q.open_tiff(str(paths[0]),options));small=request(1,1)
    capture=q.RawAnalysisRequest(doc,small,tile_size=16);prior=q.analyze_raw(capture)
    record=doc.keep_raw_analysis(capture,prior);revision=doc.revision
    job=q.RawAnalysisJob();region=request(1024,1024) if phase=='partial' else small
    assert job.request(q.RawAnalysisRequest(doc,region,tile_size=16))
    deadline=time.monotonic()+15
    if phase=='partial':
        while True:
            state=job.poll(doc)
            assert state.state=='running','worker must be observed before publication'
            if 2<=state.completed_tiles<state.total_tiles:break
            assert time.monotonic()<deadline,'no partial companion-read progress'
    else:
        while job.busy:
            assert time.monotonic()<deadline,'worker did not finish'
        assert job.result is None,'completion must not implicitly publish'
    job.cancel();state=job.wait(doc)
    assert state.state=='cancelled' and job.result is None
    if phase=='partial':assert 2<=state.completed_tiles<state.total_tiles
    else:assert state.completed_tiles==state.total_tiles==1
    with pytest.raises(RuntimeError):job.keep_result(doc)
    assert doc.revision==revision and doc.raw_analysis_record_count==1
    assert doc.raw_analysis_record(record).result.input_sha256==prior.input_sha256
    assert job.request(q.RawAnalysisRequest(doc,small,tile_size=16))
    assert job.wait(doc).state=='ready' and job.result.input_sha256==prior.input_sha256
    assert list(job.result.statistics.means)==[0.,1000.]
    doc.server.close()
    print(json.dumps(dict(phase=phase,cancelled_completed_tiles=state.completed_tiles,total_tiles=state.total_tiles,retry='ready',prior_record_preserved=True)))


@pytest.mark.skipif(not q.has_project,reason='project support')
@pytest.mark.parametrize('phase',['partial','completed_unpublished'])
def test_real_compound_job_cancellation_is_bounded_and_reusable(tmp_path,phase):
    code="import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from test_multifile_tiff_failures import cancellation_child;cancellation_child(Path(sys.argv[2]),sys.argv[3])"
    done=subprocess.run([sys.executable,'-I','-c',code,str(Path(__file__).parent),str(tmp_path),phase],capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert done.returncode==0,done.stdout+done.stderr
    data=json.loads(done.stdout.splitlines()[-1]);assert data['retry']=='ready' and data['prior_record_preserved']
    print(done.stdout.strip())
