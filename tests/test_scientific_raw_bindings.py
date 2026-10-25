"""Installed typed sampling/retention, ownership and original-source precision."""
import csv
from fractions import Fraction
import gc
import io
import numpy as np
import pytest
import tifffile
import qupathxx as q

requires_tiff=pytest.mark.skipif(not q.has_tiff,reason='quantitative TIFF reader')

def grid(width=4,height=3):
    return q.ScientificGrid(width,height,q.ScientificGridAxis(0,2,1),q.ScientificGridAxis(0,1,1))

@requires_tiff
@pytest.mark.parametrize('dtype',['uint8','uint16','int8','int16','uint32','int32','uint64','int64','float32','float64'])
def test_typed_raw_sampling_exact_mean_and_hash_only(tmp_path,dtype):
    data=np.arange(12,dtype=dtype).reshape(3,4)
    if dtype in ('uint64','int64'):data[0,:2]=[2**53+1,2**53+2]
    source=tmp_path/'raw.tif';tifffile.imwrite(source,data,photometric='minisblack')
    server=q.open_tiff(str(source));sample=q.read_scientific_raw(server,grid())
    expected=float((Fraction.from_float(float(data[0,0]))+Fraction.from_float(float(data[0,1])))/2) if dtype.startswith('float') else float((Fraction(int(data[0,0]))+Fraction(int(data[0,1])))/2)
    assert sample.numpy().shape==(1,1,1) and sample.numpy().dtype==np.float64
    assert sample.numpy()[0,0,0].hex()==expected.hex()
    assert sample.valid_numpy().tolist()==[[[1]]] and sample.reasons_numpy().tolist()==[[[0]]]
    digest=q.digest_scientific_raw(server,grid())
    assert digest.input_sha256==sample.input_sha256 and digest.decoded_source_sha256==sample.decoded_source_sha256
    assert digest.estimated_work_bytes<sample.estimated_work_bytes
    assert q.scientific_raw_work_bytes(server.raw_metadata,grid())==sample.estimated_work_bytes
    assert digest.metadata.sample_type==sample.metadata.sample_type

@requires_tiff
def test_owned_readonly_arrays_and_copied_options_survive_close(tmp_path):
    source=tmp_path/'raw.tif';tifffile.imwrite(source,np.arange(12,dtype='uint16').reshape(3,4))
    server=q.open_tiff(str(source));options=q.ScientificRawOptions();sample=q.read_scientific_raw(server,grid(),options)
    options.channels=0;copied=sample.options;copied.channels=0
    assert sample.options.channels==1
    arrays=[sample.numpy(),sample.valid_numpy(),sample.reasons_numpy()]
    saved=[a.copy() for a in arrays]
    for array in arrays:
        assert not array.flags.writeable
        with pytest.raises(ValueError):array.flat[0]=99
    writable=sample.numpy(copy=True);writable.flat[0]=99;assert sample.numpy().flat[0]!=99
    metadata=sample.metadata;server.close();del sample,server;gc.collect()
    assert metadata.width==4
    for array,expected in zip(arrays,saved):np.testing.assert_array_equal(array,expected)

@requires_tiff
def test_grid_raw_request_job_retention_project_and_typed_csv(tmp_path):
    source=tmp_path/'raw.tif';tifffile.imwrite(source,np.arange(12,dtype='uint16').reshape(3,4))
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    options=q.ScientificRawOptions();request=q.RawAnalysisRequest(doc,grid(),options)
    options.channels=0;assert request.grid_options.channels==1 and request.grid.x.samples==1
    result=q.analyze_raw(request);descriptor=result.grid
    assert descriptor.derived_sample_type==q.SampleType.FLOAT64 and descriptor.source_metadata.sample_type==q.SampleType.UINT16
    assert descriptor.options.channels==1 and result.statistics.means==[.5]
    with pytest.raises(RuntimeError):_=result.region
    assert q.raw_analysis_input_digest(request).input_sha256==result.input_sha256
    kept=doc.keep_raw_analysis(request,result);table=q.RawTableRequest(doc,[kept],True)
    rows=list(csv.DictReader(io.StringIO(q.raw_analysis_csv(table))))
    assert len(rows)==1 and rows[0]['schema']=='qpx.raw-analysis-csv/2'
    assert rows[0]['sampling_kind']=='explicit_grid' and rows[0]['derived_sample_type']=='float64'
    assert rows[0]['downsample']==rows[0]['level']==rows[0]['level_zero_x']==''
    assert rows[0]['grid_extent_x']=='2' and rows[0]['grid_samples_x']=='1'
    destination=tmp_path/'grid.qpxproj';project.save(str(destination));loaded=q.Project.open(str(destination));reopened=loaded.document(image)
    assert reopened.raw_analysis_status(kept)=='unverified'
    replay=q.RawAnalysisRequest.replay(reopened,kept);job=q.RawAnalysisJob();assert job.request(replay)
    assert job.wait(reopened).state=='ready' and job.verify_result(reopened,kept)
    assert reopened.raw_analysis_status(kept)=='current'
    del result,request,doc,project,table;gc.collect();assert descriptor.grid.x.extent==2

@requires_tiff
def test_nonfinite_channel_does_not_invalidate_finite_neighbor(tmp_path):
    source=tmp_path/'channels.ome.tif';data=np.ones((2,3,4),dtype='float64');data[1,0,0]=np.nan
    tifffile.imwrite(source,data,ome=True,photometric='minisblack',metadata={'axes':'CYX'})
    server=q.open_tiff(str(source));options=q.ScientificRawOptions();options.channels=2
    result=q.read_scientific_raw(server,grid(),options)
    assert result.valid_numpy().tolist()==[[[1,0]]] and result.reasons_numpy().tolist()==[[[0,3]]]
    assert result.numpy().tolist()==[[[1.,0.]]]
    doc=q.Document(server);stats=q.analyze_raw(q.RawAnalysisRequest(doc,grid(),options)).statistics
    assert stats.pixels==stats.valid_pixels==1 and stats.finite_samples==[1,0] and stats.nonfinite_samples==[0,1]


def test_raw_grid_exports_available_without_tiff():
    for name in ['ScientificRawOptions','ScientificRawGrid','ScientificRawDigest','ScientificRawReason','RawAnalysisGrid','read_scientific_raw','digest_scientific_raw','scientific_raw_work_bytes']:
        assert name in q.__all__ and hasattr(q,name)
    options=q.ScientificRawOptions();assert options.channels==1
    options.channels=0
    with pytest.raises((ValueError,IndexError)):options.validate()
