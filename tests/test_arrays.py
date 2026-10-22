"""Native array exchange: exact values/layout, isolation and retained ownership."""
import gc
import os
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import pytest
import qupathxx as q


def rgb():return np.arange(12*17*3,dtype=np.uint8).reshape(12,17,3)


def pixels():
    source=rgb();image=q.Image.from_numpy(source);doc=q.Document(image)
    return source,image,doc,doc.server.read_region(q.RegionRequest(0,0,0,17,12))


@pytest.mark.parametrize('factory,channels',[(q.Image.from_numpy,3),(q.PixelBuffer.from_numpy,4)])
@pytest.mark.parametrize('layout',['c','f','reverse','transpose','broadcast','subsample','readonly'])
def test_import_exact_strides_and_native_ownership(factory,channels,layout):
    source=np.arange(12*17*channels,dtype=np.uint8).reshape(12,17,channels)
    if layout=='f':source=np.asfortranarray(source)
    elif layout=='reverse':source=source[::-1,::-1,::-1]
    elif layout=='transpose':source=source.transpose(1,0,2)
    elif layout=='broadcast':source=np.broadcast_to(source[:1,:1],(12,17,channels))
    elif layout=='subsample':source=source[::2,::3]
    elif layout=='readonly':source.setflags(write=False)
    expected=source.copy();native=factory(source);array=native.numpy()
    assert array.dtype==np.uint8 and array.flags.c_contiguous and not array.flags.writeable
    np.testing.assert_array_equal(array,expected)
    assert not np.shares_memory(array,source)
    if source.flags.writeable:source[...] = 0
    del source,native;gc.collect();np.testing.assert_array_equal(array,expected)


@pytest.mark.parametrize('factory,channels',[(q.Image.from_numpy,3),(q.PixelBuffer.from_numpy,4)])
@pytest.mark.parametrize('bad',['dtype','channels','ndim','empty','list','huge'])
def test_import_rejects_invalid_arrays_without_coercion(factory,channels,bad):
    array=np.zeros((2,3,channels),dtype=np.uint8)
    if bad=='dtype':array=array.astype(np.int16)
    elif bad=='channels':array=array[:,:,:channels-1]
    elif bad=='ndim':array=array[0]
    elif bad=='empty':array=array[:0]
    elif bad=='list':array=array.tolist()
    elif bad=='huge':array=np.broadcast_to(np.zeros((1,1,channels),np.uint8),(2**30,2**30,channels))
    with pytest.raises((TypeError,ValueError)):factory(array)


@pytest.mark.parametrize('which',['image','pixels','float','labels'])
def test_zero_copy_readonly_views_and_writable_independent_copies(which):
    source,image,doc,p=pixels()
    if which=='image':native=image;view=lambda copy=False:native.numpy(copy=copy)
    elif which=='pixels':native=p;view=lambda copy=False:native.numpy(copy=copy)
    elif which=='float':native=q.StainTransform([[1,0,0],[0,1,0]]).deconvolve(p);view=lambda copy=False:native.numpy(copy=copy)
    else:
        obj=doc.add_rectangle(q.Rect(0,0,17,12));native=q.detect_tissue(q.TissueRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]])));view=lambda copy=False:native.labels_numpy(copy=copy)
    first,second=view(),view();copy=view(True);expected=first.copy()
    assert np.shares_memory(first,second) and not np.shares_memory(first,copy)
    assert first.flags.c_contiguous and not first.flags.writeable and copy.flags.writeable
    with pytest.raises(ValueError):first.flat[0]=0
    with pytest.raises(ValueError):first.setflags(write=True)
    copy.flat[0]=123;np.testing.assert_array_equal(first,expected)
    del native,doc,image,p,view,first,copy;gc.collect();np.testing.assert_array_equal(second,expected)


def test_pixels_float_labels_and_mask_match_bytes():
    source,image,doc,p=pixels();a=p.numpy()
    assert a.shape==(12,17,4) and a.strides==(68,4,1) and a.tobytes()==p.data
    np.testing.assert_array_equal(a[:,:,:3],source);assert np.all(a[:,:,3]==255)
    t=q.StainTransform([[1,0,0],[0,1,0]])
    for transformed in [t.optical_densities(p),t.deconvolve(p)]:
        array=transformed.numpy();assert array.shape==(12,17,3) and array.dtype==np.float32 and array.tobytes()==transformed.data
    obj=doc.add_rectangle(q.Rect(0,0,17,12));result=q.detect_tissue(q.TissueRequest(doc,obj,t));labels=result.labels_numpy();mask=result.mask_numpy()
    assert labels.shape==mask.shape==(12,17) and labels.dtype==np.uint32 and mask.dtype==np.uint8
    assert labels.tobytes()==result.labels and mask.tobytes()==result.mask and mask.flags.writeable
    np.testing.assert_array_equal(mask,np.where(labels,255,0));mask[:]=0;assert result.mask_numpy().tobytes()==result.mask


def test_array_after_document_and_server_closure():
    source,image,doc,p=pixels();rgbview=doc.image.numpy();region=p.numpy();expected=source.copy();doc.server.close()
    del image,doc,p,source;gc.collect();np.testing.assert_array_equal(rgbview,expected);np.testing.assert_array_equal(region[:,:,:3],expected)


@pytest.mark.skipif(not q.has_project,reason='requires projects')
def test_image_view_survives_project_relink_and_removal(tmp_path):
    source=rgb();path=tmp_path/'first.ppm';path.write_bytes(b'P6\n17 12\n255\n'+source.tobytes());other=tmp_path/'other.ppm';other.write_bytes(b'P6\n17 12\n255\n'+bytes([200])*source.size)
    p=q.Project();id=p.add_image(str(path));doc=p.document(id)
    old_image=doc.image;view=old_image.numpy();p.relink(id,str(other))
    assert old_image.width==17 and old_image.height==12
    np.testing.assert_array_equal(old_image.numpy(),source)
    np.testing.assert_array_equal(doc.server.read_region(q.RegionRequest(0,0,0,17,12)).numpy()[:,:,:3],200)
    p.remove_image(id);del doc,p;gc.collect();np.testing.assert_array_equal(view,source)


def test_scheduled_region_array_survives_eviction_shutdown_and_gc():
    source,image,doc,_=pixels();scheduler=q.TileScheduler(doc.server);ticket=scheduler.submit(q.RegionRequest(0,0,0,17,12));assert scheduler.wait_idle(10)
    result=scheduler.take(ticket);array=result.pixels.numpy();scheduler.clear_cache();scheduler.shutdown();doc.server.close()
    del scheduler,result,doc,image;gc.collect();np.testing.assert_array_equal(array[:,:,:3],source)


def test_job_label_arrays_survive_archive_removal_and_owner_gc():
    _,_,doc,_=pixels();obj=doc.add_rectangle(q.Rect(0,0,17,12));t=q.StainTransform([[1,0,0],[0,1,0]])
    job=q.CellJob();assert job.request(q.CellRequest(doc,obj,t));assert job.wait(doc,obj).state=='ready'
    result=job.result;labels=result.nuclei.labels_numpy();expected=labels.copy()
    request=q.CellRequest(doc,obj,t);record=doc.keep_cells(request,q.detect_cells(request))
    doc.remove_cells(record);del result,job,doc;gc.collect();np.testing.assert_array_equal(labels,expected)


def test_interpreter_shutdown_with_outstanding_arrays():
    code='''
import numpy as np, qupathxx as q
image=q.Image.from_numpy(np.ones((64,64,3),np.uint8))
doc=q.Document(image);a=doc.image.numpy()
p=doc.server.read_region(q.RegionRequest(0,0,0,64,64));b=p.numpy()
c=q.StainTransform([[1,0,0],[0,1,0]]).deconvolve(p).numpy()
del image,doc,p
assert a.sum()==64*64*3 and b.shape==(64,64,4) and c.shape==(64,64,3)
'''
    r=subprocess.run([sys.executable,'-I','-X','faulthandler','-c',code],capture_output=True,text=True,timeout=30)
    assert r.returncode==0,r.stderr
    assert 'nanobind: leaked' not in r.stderr


@pytest.mark.skipif(not q.has_project,reason='requires projects')
def test_documented_region_workflow_example(tmp_path):
    source=rgb();path=tmp_path/'input.ppm';path.write_bytes(b'P6\n17 12\n255\n'+source.tobytes())
    example=Path(__file__).resolve().parents[1]/'examples'/'region_workflow.py';output=tmp_path/'result'
    r=subprocess.run([sys.executable,'-I',str(example),str(path),str(output)],capture_output=True,text=True,timeout=30);assert r.returncode==0,r.stderr
    reopened=q.Project.open(str(output/'study.qpxproj'));assert len(reopened.images)==1 and len(reopened.document(reopened.images[0].id).analysis_records)==1
    assert (output/'measurements.csv').read_bytes().count(b'\r\n')==2
    if os.environ.get('QUPATHXX_TEST_GUI')=='1':assert q.show(reopened,frame_limit=8).frames==8


def test_empty_sample_roi_rejects_before_export():
    doc=q.Document(q.Image.from_numpy(np.zeros((4,4,3),np.uint8)));obj=doc.add_rectangle(q.Rect(.1,.1,.1,.1));t=q.StainTransform([[1,0,0],[0,1,0]])
    with pytest.raises(ValueError,match='no pixel centers'):q.TissueRequest(doc,obj,t)


def test_array_reads_and_imports_during_native_job_completion():
    source=np.full((512,512,3),32,np.uint8);doc=q.Document(q.Image.from_numpy(source));obj=doc.add_rectangle(q.Rect(0,0,512,512))
    pixels=doc.server.read_region(q.RegionRequest(0,0,0,512,512));array=pixels.numpy();job=q.CellJob();job.request(q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]])))
    deadline=time.monotonic()+20;iterations=0
    while job.busy:
        assert int(array.sum())==(32*3+255)*512*512
        imported=q.Image.from_numpy(source[::-1,::2]);assert imported.numpy()[0,0].tolist()==[32]*3
        state=job.poll(doc,obj);iterations+=1
        assert time.monotonic()<deadline
    assert iterations and state.state=='ready',state.error
    labels=job.result.nuclei.labels_numpy();expected=labels.copy();del job,doc,pixels;gc.collect();np.testing.assert_array_equal(labels,expected)
