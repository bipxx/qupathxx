"""Exact hash-only source scans for future verified pixel-batch reuse."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
from pathlib import Path
import struct
import time
import numpy as np
import pytest
import qupathxx as q
from test_pixel_features import rgba

FIXTURE=Path(__file__).parent/'data/pixel-streams/rectangle-v1-windows.qps'


def setup(size=64):
    model=q.PixelStreamFile(str(FIXTURE)).read_block(0).model
    rgb=rgba(size,size)[:,:,:3];doc=q.Document(q.Image.from_numpy(rgb))
    r=model.resolution
    if r.pixel_width_um is not None:doc.set_calibration(r.pixel_width_um,r.pixel_height_um)
    root=doc.add_roi(q.Roi.ellipse(q.Rect(3,4,size-7,size-9)))
    return doc,root,model


def options(block=16,tile=16):
    o=q.PixelStreamOptions();o.block_size=block;p=q.PixelPredictionOptions();p.tile_size=tile;p.work_bytes=8*1024*1024;o.prediction=p;return o


@pytest.mark.parametrize('bounds',[(0,0,7,5),(34,28,7,7),(12,10,9,8),(1,1,1,1)])
@pytest.mark.parametrize('channels',['rgb','all'])
def test_feature_hash_only_matches_full_and_tightly_cropped_context(bounds,channels):
    pixels=rgba(35,41);o=q.PixelFeatureOptions();o.sigmas=[0,1,2];o.gradient_magnitude=o.laplacian=True
    if channels=='all':o.optical_density=o.stains=True
    x,y,w,h=bounds;r=o.halo;left,top=max(0,x-r),max(0,y-r);right,bottom=min(41,x+w+r),min(35,y+h+r)
    window=q.PixelFeatureWindow(41,35,x,y,w,h);buffer=q.PixelBuffer.from_numpy(pixels)
    before=buffer.data;expected=q.extract_pixel_features(buffer,o,window=window)
    assert q.pixel_feature_input_sha256(buffer,o,window=window)==expected.input_sha256
    tight=q.PixelBuffer.from_numpy(pixels[top:bottom,left:right]);crop=q.PixelFeatureWindow(41,35,x,y,w,h,left,top)
    assert q.pixel_feature_input_sha256(tight,o,window=crop)==expected.input_sha256 and buffer.data==before


def test_feature_hash_tracks_only_required_halo_bytes_including_invalid_alpha():
    pixels=rgba(64,64);o=q.PixelFeatureOptions();o.sigmas=[1];window=q.PixelFeatureWindow(64,64,24,24,8,8)
    original=q.pixel_feature_input_sha256(q.PixelBuffer.from_numpy(pixels),o,window=window)
    pixels[0,0,0]^=1;assert q.pixel_feature_input_sha256(q.PixelBuffer.from_numpy(pixels),o,window=window)==original
    pixels[24,23,3]^=1;assert q.pixel_feature_input_sha256(q.PixelBuffer.from_numpy(pixels),o,window=window)!=original


@pytest.mark.parametrize('window',[None,(17.25,11.2,28.5,20.7)])
@pytest.mark.parametrize('tile',[16,32])
def test_source_request_input_hash_matches_prediction_without_document_mutation(window,tile):
    doc,root,model=setup();p=options(tile=tile).prediction;rect=q.Rect(*window) if window else None
    request=q.PixelPredictionRequest(doc,root,model,p,rect);revision=doc.revision
    digest=q.input_sha256(request);result=q.predict_pixel_region(request)
    assert digest==result.input_sha256 and len(digest)==64 and doc.revision==revision and request.matches(doc)
    doc.update_roi(root,q.Roi.rectangle(q.Rect(1,1,4,4)))
    assert not request.matches(doc) and q.input_sha256(request)==digest and result.input_sha256==digest


@pytest.mark.parametrize('window',[None,(17.25,11.2,28.5,20.7)])
@pytest.mark.parametrize('block',[16,32,256])
def test_non_consuming_stream_digest_matches_framing_and_verified_file(tmp_path,window,block):
    doc,root,model=setup();o=options(block);stream=q.PixelPredictionStream(doc,root,model,o,q.Rect(*window) if window else None)
    revision=doc.revision;digest=stream.input_sha256()
    assert stream.completed_blocks==stream.valid_pixels==stream.classified_pixels==0 and stream.state=='ready' and not stream.busy
    out=tmp_path/'mask.qps';summary=q.save_pixel_stream(stream,str(out));assert stream.state=='complete'
    file=q.PixelStreamFile(str(out))
    with pytest.raises(RuntimeError):file.input_sha256()
    file.verify();assert file.verified and file.input_sha256()==digest==stream.input_sha256()
    framed=hashlib.sha256();framed.update(b'qpx.pixel-stream-input/1');framed.update(model.sha256.encode());framed.update(struct.pack('<Q',summary.blocks))
    for i in range(summary.blocks):framed.update(struct.pack('<Q',i));framed.update(file.read_block(i).input_sha256.encode())
    assert framed.hexdigest()==digest and doc.revision==revision and stream.completed_blocks==summary.blocks
    file.cancel()
    with pytest.raises(RuntimeError):file.input_sha256()


def test_partial_stream_hash_does_not_advance_or_rewrite_production_counters():
    doc,root,model=setup();s=q.PixelPredictionStream(doc,root,model,options());fresh=s.input_sha256();block=s.next()
    before=(s.completed_blocks,s.valid_pixels,s.classified_pixels,s.class_counts)
    assert s.input_sha256()==fresh and (s.completed_blocks,s.valid_pixels,s.classified_pixels,s.class_counts)==before
    assert s.next() is not None and block.labels_numpy().flags.writeable is False


def test_tile_layout_changes_digest_even_when_numeric_masks_match():
    doc,root,model=setup();a=q.PixelPredictionStream(doc,root,model,options(32,16));b=q.PixelPredictionStream(doc,root,model,options(32,32))
    assert a.input_sha256()!=b.input_sha256()
    first,second=a.next(),b.next()
    for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:np.testing.assert_array_equal(getattr(first,method)(),getattr(second,method)())


def test_cancelled_or_closed_source_hash_does_not_consume_blocks():
    doc,root,model=setup();stream=q.PixelPredictionStream(doc,root,model,options());stream.cancel()
    with pytest.raises(RuntimeError,match='cancelled'):stream.input_sha256()
    assert stream.completed_blocks==0
    stream=q.PixelPredictionStream(doc,root,model,options());request=q.PixelPredictionRequest(doc,root,model);doc.server.close()
    with pytest.raises(RuntimeError):q.input_sha256(request)
    with pytest.raises(RuntimeError,match='closed'):stream.input_sha256()
    assert not stream.busy and stream.completed_blocks==0


def test_hash_only_scan_releases_gil_and_accepts_producer_cancellation():
    doc,root,model=setup(2048);s=q.PixelPredictionStream(doc,root,model,options())
    with ThreadPoolExecutor(max_workers=1) as pool:
        future=pool.submit(s.input_sha256);deadline=time.monotonic()+10
        while not s.busy and not future.done() and time.monotonic()<deadline:time.sleep(.001)
        assert s.busy and s.state=='hashing';s.cancel()
        with pytest.raises(RuntimeError,match='cancelled'):future.result(timeout=15)
    assert not s.busy and s.state=='cancelled' and not s.completed_blocks and not s.valid_pixels


def test_source_changes_are_visible_to_a_new_capture():
    doc,root,model=setup();before=q.PixelPredictionStream(doc,root,model,options()).input_sha256()
    changed=q.Document(q.Image.from_numpy(np.full((64,64,3),128,np.uint8)));r=model.resolution
    if r.pixel_width_um is not None:changed.set_calibration(r.pixel_width_um,r.pixel_height_um)
    target=changed.add_roi(doc.annotation(root).geometry)
    assert q.PixelPredictionStream(changed,target,model,options()).input_sha256()!=before


def test_changed_file_cannot_return_a_previously_verified_input_commitment(tmp_path):
    path=tmp_path/'changed.qps';path.write_bytes(FIXTURE.read_bytes());file=q.PixelStreamFile(str(path));file.verify();assert len(file.input_sha256())==64
    data=bytearray(path.read_bytes());data[-1]^=1;path.write_bytes(data)
    with pytest.raises(RuntimeError):file.read_block(0)
    assert not file.verified
    with pytest.raises(RuntimeError):file.input_sha256()
