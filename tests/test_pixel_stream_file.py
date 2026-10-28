"""Atomic compact stream files, indexed historical blocks and full verification."""
from concurrent.futures import ThreadPoolExecutor
import gc
import hashlib
import os
from pathlib import Path
import struct
import time
import numpy as np
import pytest
import qupathxx as q
from test_pixel_prediction_windows import setup
from raw_file_mutation import mutate_header

pytestmark=pytest.mark.skipif(not q.has_classifier_training,reason='optional training backend')


def producer(doc,root,model,block_size=16,window=None):
    options=q.PixelStreamOptions();options.block_size=block_size
    prediction=q.PixelPredictionOptions();prediction.work_bytes=8*1024*1024;prediction.tile_size=16;options.prediction=prediction
    return q.PixelPredictionStream(doc,root,model,options,window)


def saved(tmp_path,block_size=16,window=None):
    doc,root,model=setup();stream=producer(doc,root,model,block_size,window);path=tmp_path/'mask.qps'
    summary=q.save_pixel_stream(stream,str(path));return doc,root,model,path,summary


@pytest.mark.parametrize('operation',['read_first','read_later','verify'])
def test_same_stamp_header_changes_refuse_cached_model_use(tmp_path,operation):
    *_,path,summary=saved(tmp_path);assert summary.blocks>1
    reader=q.PixelStreamFile(str(path));reader.verify();assert reader.verified
    # The QPS header archive starts after the fixed 68-byte grid prefix.
    mutate_header(path,150)
    action={'read_first':lambda:reader.read_block(0),
            'read_later':lambda:reader.read_block(1),'verify':reader.verify}[operation]
    with pytest.raises((RuntimeError,ValueError)):action()
    assert not reader.verified


@pytest.mark.parametrize('block_size',[16,32,256])
@pytest.mark.parametrize('window',[None,(17.25,11.2,28.5,20.7)])
def test_saved_masks_are_indexed_exact_crops_and_verify_complete_totals(tmp_path,block_size,window):
    rect=q.Rect(*window) if window else None;doc,root,model,path,summary=saved(tmp_path,block_size,rect);revision=doc.revision
    file=q.PixelStreamFile(str(path));assert not file.verified and file.windowed==bool(window)
    assert summary.bytes==path.stat().st_size and file.declared_summary.sha256==summary.sha256
    expected=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,window=rect));grid=expected.level_bounds
    coverage=np.zeros((expected.height,expected.width),np.uint8)
    for i in reversed(range(summary.blocks)):
        block=file.read_block(i);x=int(block.level_bounds.x-grid.x);y=int(block.level_bounds.y-grid.y);sl=np.s_[y:y+block.height,x:x+block.width]
        assert block.windowed and block.roi.wkt==expected.roi.wkt
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:
            np.testing.assert_array_equal(getattr(block,method)(),getattr(expected,method)()[sl])
            assert not getattr(block,method)().flags.writeable
        coverage[sl]+=1
    assert np.all(coverage==1)
    file.verify();assert file.verified and file.declared_summary.class_counts==expected.class_counts
    assert file.declared_summary.valid_pixels==expected.valid_pixels and file.declared_summary.classified_pixels==expected.classified_pixels
    assert doc.revision==revision
    with pytest.raises(IndexError):file.read_block(summary.blocks)


def test_saved_blocks_outlive_file_source_and_target_without_live_authority(tmp_path):
    doc,root,model,path,_=saved(tmp_path);file=q.PixelStreamFile(str(path));block=file.read_block(0);view=block.labels_numpy();reference=view.copy()
    request=q.PixelPredictionRequest(doc,root,model,window=block.bounds);assert not block.matches(request)
    if q.has_geos:
        with pytest.raises(RuntimeError):q.publish_pixel_contours(doc,request,q.trace_pixel_prediction(block))
    doc.remove_objects([root]);path.unlink();del file,doc,model,request,block;gc.collect();np.testing.assert_array_equal(view,reference)


@pytest.mark.parametrize('replace',[False,True])
def test_existing_non_stream_file_is_never_overwritten_or_consumed(tmp_path,replace):
    doc,root,model=setup();path=tmp_path/'source.svs';path.write_bytes(b'preserve unrelated source');stream=producer(doc,root,model)
    with pytest.raises((ValueError,RuntimeError)):q.save_pixel_stream(stream,str(path),replace_existing=replace)
    assert path.read_bytes()==b'preserve unrelated source' and stream.completed_blocks==0


def test_explicit_replacement_verifies_old_file_and_invalidates_old_reader(tmp_path):
    doc,root,model,path,summary=saved(tmp_path);old=q.PixelStreamFile(str(path));old.verify()
    replacement=q.save_pixel_stream(producer(doc,root,model),str(path),replace_existing=True)
    assert replacement.class_counts==summary.class_counts
    with pytest.raises(RuntimeError,match='changed'):old.read_block(0)
    assert not old.verified
    new=q.PixelStreamFile(str(path));new.verify();assert new.verified
    broken=bytearray(path.read_bytes());at=132+struct.unpack_from('<Q',broken,60)[0]+136;broken[at]^=1;path.write_bytes(broken)
    unchanged=path.read_bytes();stream=producer(doc,root,model)
    with pytest.raises((ValueError,RuntimeError)):q.save_pixel_stream(stream,str(path),replace_existing=True)
    assert path.read_bytes()==unchanged and stream.completed_blocks==0


@pytest.mark.parametrize('limit',[0,127,128,1024])
def test_byte_limits_reject_before_large_allocation_or_final_output(tmp_path,limit):
    doc,root,model=setup();path=tmp_path/'limited.qps';stream=producer(doc,root,model)
    with pytest.raises((ValueError,RuntimeError)):q.save_pixel_stream(stream,str(path),max_bytes=limit)
    assert not path.exists() and stream.completed_blocks==0 and not list(tmp_path.glob('*.qpx-tmp-*'))


def test_header_charge_is_included_in_final_byte_limit(tmp_path):
    doc,root,model,path,summary=saved(tmp_path);small=tmp_path/'too-small.qps';stream=producer(doc,root,model)
    with pytest.raises((ValueError,RuntimeError),match='byte limit'):q.save_pixel_stream(stream,str(small),max_bytes=summary.bytes-1)
    assert not small.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))
    with pytest.raises((ValueError,RuntimeError)):q.PixelStreamFile(str(path),max_bytes=summary.bytes-1)


def test_only_fresh_producers_can_be_saved(tmp_path):
    doc,root,model=setup();stream=producer(doc,root,model);next(stream)
    with pytest.raises(ValueError,match='fresh'):q.save_pixel_stream(stream,str(tmp_path/'partial.qps'))
    stream=producer(doc,root,model);stream.cancel()
    with pytest.raises(ValueError,match='fresh'):q.save_pixel_stream(stream,str(tmp_path/'cancelled.qps'))


@pytest.mark.parametrize('replace',[False,True])
def test_cancelled_writer_cleans_temporary_and_preserves_existing_destination(tmp_path,replace):
    _,_,model=setup();doc=q.Document(q.Image.from_numpy(np.full((1024,1024,3),80,np.uint8)));root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1024,1024)));path=tmp_path/'cancel.qps'
    before=None
    if replace:q.save_pixel_stream(producer(doc,root,model,128),str(path));before=path.read_bytes()
    stream=producer(doc,root,model,16)
    with ThreadPoolExecutor(1) as pool:
        future=pool.submit(q.save_pixel_stream,stream,str(path),replace)
        deadline=time.monotonic()+15
        while stream.completed_blocks<1 and not future.done() and time.monotonic()<deadline:time.sleep(.001)
        assert stream.completed_blocks>=1 and not future.done();stream.cancel()
        with pytest.raises(RuntimeError,match='cancelled'):future.result(timeout=15)
    assert (path.read_bytes()==before) if replace else not path.exists()
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def test_replace_flag_does_not_clobber_unvalidated_raced_in_destination(tmp_path):
    _,_,model=setup();doc=q.Document(q.Image.from_numpy(np.full((1024,1024,3),80,np.uint8)));root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1024,1024)));path=tmp_path/'raced.qps';stream=producer(doc,root,model,128)
    with ThreadPoolExecutor(1) as pool:
        future=pool.submit(q.save_pixel_stream,stream,str(path),True)
        deadline=time.monotonic()+15
        while stream.completed_blocks<1 and not future.done() and time.monotonic()<deadline:time.sleep(.001)
        assert stream.completed_blocks>=1 and not future.done();path.write_bytes(b'preserve raced-in destination')
        with pytest.raises(RuntimeError,match='commit'):future.result(timeout=15)
    assert path.read_bytes()==b'preserve raced-in destination' and not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.parametrize('damage',['magic','header_length','header_checksum','scope','grid','truncated','trailing','footer_checksum','digest'])
def test_malformed_file_headers_and_completion_are_rejected(tmp_path,damage):
    _,_,_,path,_=saved(tmp_path);b=bytearray(path.read_bytes());length=struct.unpack_from('<Q',b,60)[0];header=68+length
    if damage=='magic':b[0]^=1
    elif damage=='header_length':struct.pack_into('<Q',b,60,2**63)
    elif damage=='header_checksum':b[header]^=1
    elif damage=='scope':struct.pack_into('<Q',b,52,2)
    elif damage=='grid':struct.pack_into('<Q',b,4,0);b[header:header+64]=hashlib.sha256(b[:header]).hexdigest().encode()
    elif damage=='truncated':b=b[:-1]
    elif damage=='trailing':b+=b'extra'
    elif damage=='footer_checksum':b[-1]^=1
    else:
        b[-128:-64]=b'X'*64;footer=len(b)-164;b[-64:]=hashlib.sha256(b[header:header+64]+b[footer:-64]).hexdigest().encode()
    path.write_bytes(b)
    with pytest.raises((ValueError,RuntimeError)):q.PixelStreamFile(str(path))


@pytest.mark.parametrize('resign',[False,True])
def test_corrupt_block_is_detected_lazily_by_checksum_or_numeric_hash(tmp_path,resign):
    _,_,_,path,_=saved(tmp_path);b=bytearray(path.read_bytes());start=132+struct.unpack_from('<Q',b,60)[0];n=16*16;data=start+136;end=data+n*9
    slot=next(i for i in range(n) if b[data+i*9+4] and struct.unpack_from('<I',b,data+i*9)[0] in [1,2])
    at=data+slot*9;label=struct.unpack_from('<I',b,at)[0];struct.pack_into('<I',b,at,3-label)
    if resign:b[end:end+64]=hashlib.sha256(b'qpx.pixel-stream-block/1'+struct.pack('<Q',1)+b[start:end]).hexdigest().encode()
    path.write_bytes(b);file=q.PixelStreamFile(str(path));assert not file.verified and file.read_block(0).windowed
    with pytest.raises(ValueError,match='hash|checksum'):file.read_block(1)
    with pytest.raises(ValueError):file.verify()
    assert not file.verified


def test_re_signed_declared_totals_do_not_bypass_complete_verification(tmp_path):
    _,_,_,path,_=saved(tmp_path);b=bytearray(path.read_bytes());header=68+struct.unpack_from('<Q',b,60)[0];footer=len(b)-164
    for at in [footer+4,footer+12,footer+20,footer+28]:struct.pack_into('<Q',b,at,0)
    b[-64:]=hashlib.sha256(b[header:header+64]+b[footer:-64]).hexdigest().encode();path.write_bytes(b)
    file=q.PixelStreamFile(str(path));assert file.declared_summary.valid_pixels==0 and not file.verified
    with pytest.raises(ValueError,match='completion'):file.verify()


def test_reader_rejects_later_file_mutation_and_keeps_returned_values(tmp_path):
    _,_,_,path,_=saved(tmp_path);file=q.PixelStreamFile(str(path));file.verify();block=file.read_block(0);values=block.labels_numpy().copy()
    b=bytearray(path.read_bytes());b[-1]^=1;path.write_bytes(b)
    with pytest.raises(RuntimeError,match='changed'):file.read_block(0)
    assert not file.verified;np.testing.assert_array_equal(block.labels_numpy(),values)


@pytest.mark.parametrize('offset',[80,-1],ids=['header','footer'])
@pytest.mark.parametrize('operation',['read_first','verify'])
def test_cached_reader_rechecks_content_after_timestamp_restored_mutation(tmp_path,offset,operation):
    _,_,_,path,_=saved(tmp_path);file=q.PixelStreamFile(str(path));file.verify();original=path.stat()
    block=file.read_block(0);expected=block.labels_numpy().copy();data=bytearray(path.read_bytes());data[offset]^=1;path.write_bytes(data)
    os.utime(path,ns=(original.st_atime_ns,original.st_mtime_ns))
    with pytest.raises(RuntimeError,match='changed'):
        if operation=='read_first':file.read_block(0)
        else:file.verify()
    assert not file.verified;np.testing.assert_array_equal(block.labels_numpy(),expected)


def test_reader_cancellation_is_terminal_and_keeps_prior_arrays(tmp_path):
    _,_,_,path,_=saved(tmp_path);file=q.PixelStreamFile(str(path));block=file.read_block(0);values=block.labels_numpy().copy();file.cancel()
    assert file.cancelled and not file.verified
    with pytest.raises(RuntimeError,match='cancelled'):file.verify()
    with pytest.raises(RuntimeError,match='cancelled'):file.read_block(0)
    np.testing.assert_array_equal(block.labels_numpy(),values)


def test_long_complete_verification_can_be_cancelled_from_another_python_thread(tmp_path):
    _,_,model=setup();doc=q.Document(q.Image.from_numpy(np.full((1024,1024,3),80,np.uint8)));root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1024,1024)));path=tmp_path/'verify-cancel.qps'
    q.save_pixel_stream(producer(doc,root,model,16),str(path));file=q.PixelStreamFile(str(path))
    with ThreadPoolExecutor(1) as pool:
        future=pool.submit(file.verify);time.sleep(.001);assert not future.done();file.cancel()
        with pytest.raises(RuntimeError,match='cancelled'):future.result(timeout=15)
    assert file.cancelled and not file.verified
