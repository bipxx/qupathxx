"""Canonical digest and bounded hash-only workspace through installed TIFF IO."""
from contextlib import closing
import hashlib
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q
import tifffile

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('raw_stream_reference',ROOT/'tools/reference_raw_streaming.py')
reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
requires_tiff=pytest.mark.skipif(not q.has_tiff,reason='quantitative TIFF reader')


def test_independent_native_stream_hashes_regenerate():
    assert (ROOT/'cpp/tests/raw_streaming_reference.hpp').read_bytes()==reference.native_header().encode()
    assert len({reference.expected(*shape) for shape in reference.SHAPES})==2


def test_stream_reference_refuses_existing_output(tmp_path):
    target=tmp_path/'reference.hpp'
    command=[sys.executable,'-I',str(ROOT/'tools/reference_raw_streaming.py'),'--native-header',str(target)]
    first=subprocess.run(command,capture_output=True);assert first.returncode==0,first.stderr
    before=target.read_bytes();second=subprocess.run(command,capture_output=True)
    assert second.returncode==2 and b'output must be a new path' in second.stderr and target.read_bytes()==before


def image(width,height,dtype):
    data=np.arange(width*height*3,dtype=np.uint64).reshape(height,width,3)
    if dtype=='uint64':return data*7+np.uint64(2**63+1)
    data=data.astype(np.float64)/8
    bits=data.view(np.uint64);bits[height//2,0,1]=0x7ff8000000000001
    bits[height//2,1,2]=0xfff0000000000000;bits[0,0,1]=0x8000000000000000
    return data


def decoded_sha(data):
    height,width,_=data.shape
    digest=hashlib.sha256(b'qpx.raw-level-zero-window/1')
    for value in [width,height,3,1,1,0,1,6 if data.dtype.kind=='u' else 9,0,0,width,height,1,2,0,0]:
        digest.update(struct.pack('<Q',value))
    digest.update(data[:,:,1:3].astype(data.dtype.newbyteorder('<'),copy=False).tobytes(order='C'))
    digest.update(bytes([1])*(width*height))
    return digest.hexdigest()


@requires_tiff
@pytest.mark.parametrize('width,height',[(400,200),(10000,3)])
@pytest.mark.parametrize('dtype',['uint64','float64'])
@pytest.mark.parametrize('layout',['strips','big-endian-tiles'])
def test_streamed_tiff_hash_preserves_every_bit_and_uses_bounded_workspace(tmp_path,width,height,dtype,layout):
    data=image(width,height,dtype);path=tmp_path/'source.tif'
    kwargs=dict(rowsperstrip=13) if layout=='strips' else dict(tile=(16,16),byteorder='>')
    tifffile.imwrite(path,data,metadata=None,photometric='rgb',compression='deflate',**kwargs)
    grid=q.ScientificGrid(width,height,q.ScientificGridAxis(0,width,7),q.ScientificGridAxis(0,height,5))
    options=q.ScientificRawOptions();options.first_channel,options.channels=1,2
    with closing(q.open_tiff(str(path))) as server:
        sample=q.read_scientific_raw(server,grid,options)
        digest=q.digest_scientific_raw(server,grid,options)
        assert sample.decoded_source_sha256==digest.decoded_source_sha256==decoded_sha(data)
        assert sample.input_sha256==digest.input_sha256
        # Subtract the documented full-window materialization/output payload;
        # a streamed digest retains only validity and at most one 64KiB chunk.
        pixels=width*height
        fixed=sample.estimated_work_bytes-pixels*17-10*7*5*2
        assert digest.estimated_work_bytes<=fixed+pixels+65536
        options.work_bytes=digest.estimated_work_bytes
        assert q.digest_scientific_raw(server,grid,options).input_sha256==sample.input_sha256
        before=server.tiff_read_stats.decoded_blocks
        options.work_bytes-=1
        with pytest.raises((ValueError,RuntimeError),match='budget'):q.digest_scientific_raw(server,grid,options)
        assert server.tiff_read_stats.decoded_blocks==before
        options.work_bytes+=1
        with pytest.raises((ValueError,RuntimeError),match='budget'):q.read_scientific_raw(server,grid,options)
        assert server.tiff_read_stats.decoded_blocks==before


@requires_tiff
@pytest.mark.skipif(not q.has_project,reason='project storage')
def test_streamed_raw_record_digest_survives_project_replay(tmp_path):
    data=image(400,200,'uint64');path=tmp_path/'source.tif'
    tifffile.imwrite(path,data,metadata=None,photometric='rgb',tile=(16,16),compression='deflate')
    project=q.Project();image_id=project.add_tiff_image(str(path));doc=project.document(image_id)
    object_id=doc.add_rectangle(q.Rect(0,0,400,200),'Raw grid')
    grid=q.ScientificGrid(400,200,q.ScientificGridAxis(0,400,7),q.ScientificGridAxis(0,200,5))
    options=q.ScientificRawOptions();options.first_channel,options.channels=1,2
    request=q.RawAnalysisRequest(doc,grid,options,object_id)
    result=q.analyze_raw(request);kept=doc.keep_raw_analysis(request,result)
    assert q.raw_analysis_input_digest(request).input_sha256==result.input_sha256
    destination=tmp_path/'study.qpxproj';project.save(str(destination))
    loaded=q.Project.open(str(destination));reopened=loaded.document(image_id)
    assert reopened.raw_analysis_status(kept)=='unverified'
    replay=q.RawAnalysisRequest.replay(reopened,kept)
    assert q.raw_analysis_input_digest(replay).input_sha256==result.input_sha256
    job=q.RawAnalysisJob();assert job.request(replay)
    assert job.wait(reopened).state=='ready' and job.verify_result(reopened,kept)
    assert reopened.raw_analysis_status(kept)=='current'
