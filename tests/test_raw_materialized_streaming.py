"""Exact coarse typed samples under budgets that cannot hold a full raw window."""
from contextlib import closing
from fractions import Fraction
import math
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
import tifffile

pytestmark=pytest.mark.skipif(not q.has_tiff,reason='quantitative TIFF reader')
DTYPES=['uint8','uint16','int8','int16','uint32','int32','uint64','int64','float32','float64']


def fixture(width,height,dtype):
    if dtype=='uint64':
        x=[2**63+4097*i+1 for i in range(width)];y=[8193*i for i in range(height)];c=[0,7,14]
    elif dtype=='int64':
        x=[(-1 if i%2 else 1)*2**60+4097*i for i in range(width)];y=[8193*i for i in range(height)];c=[0,7,14]
    elif dtype.startswith('float'):
        x=[i/8 for i in range(width)];y=[i/16 for i in range(height)];c=[0,1/32,2/32]
    else:
        signed=dtype.startswith('int')
        x=[i%37-(18 if signed else 0) for i in range(width)];y=[i%29-(14 if signed else 0) for i in range(height)];c=[0,1,2]
    wide='uint64' if dtype=='uint64' else 'float64' if dtype.startswith('float') else 'int64'
    data=(np.asarray(x,dtype=wide)[None,:,None]+np.asarray(y,dtype=wide)[:,None,None]+np.asarray(c,dtype=wide)[None,None,:]).astype(dtype)
    nonfinite=[]
    if dtype.startswith('float'):
        data[height//2,1,2]=np.nan;data[height//3,7,1]=-np.inf
        nonfinite=[(1,height//2,2),(7,height//3,1)]
        bits=data.view(np.uint32 if dtype=='float32' else np.uint64)
        bits[height//2,1,2]=0x7fc00001 if dtype=='float32' else 0x7ff8000000000001
    return data,x,y,c,nonfinite


def axis_means(values,axis):
    origin,extent,count,context=axis;result=[]
    for i in range(-context,count+context):
        a,b=Fraction(origin)+Fraction(i*extent,count),Fraction(origin)+Fraction((i+1)*extent,count)
        if a<0 or b>len(values):result.append((a,b,None));continue
        total=Fraction(0)
        for p in range(math.floor(a),math.ceil(b)):
            total+=Fraction(values[p])*(min(b,p+1)-max(a,p))
        result.append((a,b,total/(b-a)))
    return result


def independent_samples(xvalues,yvalues,channels,nonfinite,axes):
    xs,ys=axis_means(xvalues,axes[0]),axis_means(yvalues,axes[1])
    out=np.zeros((len(ys),len(xs),2),dtype=np.float64);reasons=np.zeros(out.shape,dtype=np.uint8)
    for j,(top,bottom,mean_y) in enumerate(ys):
        for i,(left,right,mean_x) in enumerate(xs):
            if mean_y is None or mean_x is None:reasons[j,i]=1;continue
            for c in [1,2]:
                if any(c==channel and left<px+1 and right>px and top<py+1 and bottom>py for px,py,channel in nonfinite):
                    reasons[j,i,c-1]=3
                else:out[j,i,c-1]=float(mean_x+mean_y+Fraction(channels[c]))
    return out,reasons


def run_case(tmp_path,dtype,width=400,height=200,layout='tiles',context=False):
    data,x,y,channels,nonfinite=fixture(width,height,dtype);path=tmp_path/'source.tif'
    kwargs=dict(rowsperstrip=13) if layout=='strips' else dict(tile=(16,16))
    if layout=='big-endian':kwargs['byteorder']='>'
    stored=data
    if layout=='separate':kwargs['planarconfig']='separate';stored=np.moveaxis(data,-1,0)
    tifffile.imwrite(path,stored,metadata=None,photometric='rgb',compression='deflate',**kwargs)
    axes=((-2,width+4,7,1),(-1,height+2,5,1)) if context else ((0,width,7,0),(0,height,5,0))
    grid=q.ScientificGrid(width,height,*(q.ScientificGridAxis(*a) for a in axes))
    options=q.ScientificRawOptions();options.first_channel,options.channels=1,2
    expected,reasons=independent_samples(x,y,channels,nonfinite,axes)
    with closing(q.open_tiff(str(path))) as server:
        result=q.read_scientific_raw(server,grid,options)
        np.testing.assert_array_equal(result.numpy().view(np.uint64),expected.view(np.uint64))
        np.testing.assert_array_equal(result.reasons_numpy(),reasons)
        np.testing.assert_array_equal(result.valid_numpy(),(reasons==0).astype(np.uint8))
        digest=q.digest_scientific_raw(server,grid,options)
        assert result.input_sha256==digest.input_sha256 and result.decoded_source_sha256==digest.decoded_source_sha256
        # This cap cannot even cover the former full-window payload plus its
        # fixed numeric scratch, before output/metadata/capture overhead.
        former_source=width*height*(2*data.dtype.itemsize+1)
        assert result.estimated_work_bytes<=former_source+4*65536
        options.work_bytes=result.estimated_work_bytes-1
        before=server.tiff_read_stats.decoded_blocks
        with pytest.raises((ValueError,RuntimeError),match='budget'):q.read_scientific_raw(server,grid,options)
        assert server.tiff_read_stats.decoded_blocks==before
        options.work_bytes+=1
        again=q.read_scientific_raw(server,grid,options)
        np.testing.assert_array_equal(again.numpy().view(np.uint64),expected.view(np.uint64))
        assert again.input_sha256==result.input_sha256


@pytest.mark.parametrize('dtype',DTYPES)
def test_coarse_samples_match_fraction_oracle_under_reduced_budget(tmp_path,dtype):
    run_case(tmp_path,dtype)


@pytest.mark.parametrize('dtype,width,height,layout',[
    ('uint64',10000,3,'strips'),('float64',400,200,'big-endian'),('int64',400,200,'separate')])
def test_streamed_context_and_layouts_keep_exact_support(tmp_path,dtype,width,height,layout):
    run_case(tmp_path,dtype,width,height,layout,True)


@pytest.mark.skipif(not q.has_project,reason='project storage')
def test_reduced_budget_record_preserves_qra2_and_project_replay(tmp_path):
    data,*_=fixture(400,200,'uint64');source=tmp_path/'source.tif'
    tifffile.imwrite(source,data,metadata=None,photometric='rgb',tile=(16,16),compression='deflate')
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    object_id=doc.add_rectangle(q.Rect(0,0,400,200),'Coarse raw measurement')
    grid=q.ScientificGrid(400,200,q.ScientificGridAxis(0,400,7),q.ScientificGridAxis(0,200,5))
    options=q.ScientificRawOptions();options.first_channel,options.channels=1,2;options.work_bytes=768*1024
    request=q.RawAnalysisRequest(doc,grid,options,object_id);result=q.analyze_raw(request)
    kept=doc.keep_raw_analysis(request,result);destination=tmp_path/'study.qpxproj';project.save(str(destination))
    loaded=q.Project.open(str(destination));reopened=loaded.document(image)
    assert reopened.raw_analysis_status(kept)=='unverified'
    replay=q.RawAnalysisRequest.replay(reopened,kept)
    assert q.raw_analysis_input_digest(replay).input_sha256==result.input_sha256
    job=q.RawAnalysisJob();assert job.request(replay)
    assert job.wait(reopened).state=='ready' and job.verify_result(reopened,kept)
    assert reopened.raw_analysis_status(kept)=='current'



def workflow_scene(tmp_path):
    data,x,y,c,nonfinite=fixture(400,200,'uint64');source=tmp_path/'source.tif'
    tifffile.imwrite(source,data,metadata=None,photometric='rgb',tile=(16,16),compression='deflate')
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    object_id=doc.add_rectangle(q.Rect(0,0,400,200),'Coarse raw measurement')
    counts=q.RawGridSampling();counts.samples_x,counts.samples_y=7,5
    options=q.ScientificRawOptions();options.first_channel,options.channels=1,2;options.work_bytes=768*1024
    values,_=independent_samples(x,y,c,nonfinite,((0,400,7,0),(0,200,5,0)))
    expected=[float(sum((Fraction.from_float(float(value)) for value in values[:,:,channel].flat),Fraction())/35) for channel in range(2)]
    return project,image,doc,object_id,counts,options,expected


@pytest.mark.skipif(not q.has_project,reason='project storage')
def test_reduced_budget_python_batch_and_hash_only_resume(tmp_path):
    project,image,doc,obj,counts,options,expected=workflow_scene(tmp_path)
    preset=q.ProjectBatchOptions();preset.kind=q.ProjectBatchKind.RAW;preset.raw_grid=counts;preset.raw_grid_work_bytes=options.work_bytes
    raw=q.RawRegionRequest();raw.first_channel,raw.channels=1,2;preset.raw=raw;preset.raw_follow_display=False
    request=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=preset)
    for attempt in range(2):
        report=q.run_project_batch(request,str(tmp_path/'batch'))
        assert report.state=='succeeded' and (report.saved,report.reused)==((1,0) if attempt==0 else (0,1))
        assert (report.images[0].raw_tiles,report.images[0].raw_hashed_tiles)==((1,0) if attempt==0 else (0,1))
        record=q.Project.open(report.images[0].path).document(image).raw_analysis_records[0]
        assert [v.hex() for v in record.result.statistics.means]==[v.hex() for v in expected]
    assert doc.raw_analysis_record_count==0


@pytest.mark.skipif(not q.has_project,reason='project storage')
def test_reduced_budget_native_cli_and_resume(tmp_path):
    from qupathxx import _native
    project,image,_,_,_,_,expected=workflow_scene(tmp_path);path=tmp_path/'source.qpxproj';project.save(str(path))
    cli=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    command=[str(cli),str(path),str(tmp_path/'batch'),'--kind','raw','--raw-grid-samples','7,5','--raw-grid-work-bytes',str(768*1024),'--raw-channel','1','--raw-channels','2','--raw-explicit-axes']
    for attempt in range(2):
        done=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert done.returncode==0,done.stderr
        result=next(json.loads(line) for line in done.stdout.splitlines() if json.loads(line)['schema']=='qpx.batch-image/1')
        assert result['state']==('saved' if attempt==0 else 'reused')
        assert (result['raw_tiles'],result['raw_hashed_tiles'])==((1,0) if attempt==0 else (0,1))
        record=q.Project.open(result['path']).document(image).raw_analysis_records[0]
        assert [v.hex() for v in record.result.statistics.means]==[v.hex() for v in expected]



def desktop():
    if not q.has_gui or not q.has_project:pytest.skip('requires quantitative project desktop')
    if os.environ.get('QUPATHXX_TEST_GUI')!='1':pytest.skip('opt-in raw grid desktop')


def test_reduced_budget_imgui_measure_keep_and_render(tmp_path):
    desktop()
    from test_gui_raw import options as gui_options
    from test_gui_raw_records import wait
    from test_gui_replay import event
    from test_gui_tma import click
    project,image,doc,_,counts,sampling_options,expected=workflow_scene(tmp_path)
    options=gui_options();options.raw_tab=1;options.raw_grid=counts;options.raw_grid_work_bytes=sampling_options.work_bytes
    raw=q.RawRegionRequest();raw.first_channel,raw.channels=1,2;options.raw_request=raw
    options.inputs=[event(3,q.GuiInputKind.SELECT_ALL_KEY)]+click(5,q.GuiInputTarget.RAW_MEASURE)+[wait(10,q.GuiInputWait.RAW_TASK_FINISHED)]
    options.inputs+=click(12,q.GuiInputTarget.RAW_RESULTS_TAB)+click(15,q.GuiInputTarget.RAW_KEEP)+[wait(18,q.GuiInputWait.RAW_ANALYSIS_KEPT)]
    capture=tmp_path/'coarse.ppm';report=q.show(project,frame_limit=35,capture_path=str(capture),options=options)
    assert not report.raw_error,report.raw_error
    assert report.raw_records==1 and report.raw_analysis_result.grid is not None
    assert [v.hex() for v in report.raw_analysis_result.statistics.means]==[v.hex() for v in expected]
    assert doc.raw_analysis_status(report.raw_record_id)=='current' and report.ui_overflow_items==0
    assert capture.read_bytes().startswith(b'P6\n')


def test_reduced_budget_managed_viewer_retains_verifies_and_saves(tmp_path):
    desktop()
    from test_viewer_session import completed,run
    project,image,_,obj,counts,options,expected=workflow_scene(tmp_path)
    source=tmp_path/'source.qpxproj';project.save(str(source));session=q.ViewerSession(str(source));held={}
    saved=tmp_path/'saved.qpxproj'
    def producer():
        result=completed(session,session.analyze_raw_grid(image,obj,counts,options,retain_result=True))
        held['record']=result.record_id;assert result.committed
        assert [v.hex() for v in result.raw_analysis.statistics.means]==[v.hex() for v in expected]
        assert completed(session,session.verify_raw_analysis(image,result.record_id)).verified
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    report=run(session,producer);assert report.ui_overflow_items==0
    restored=q.Project.open(str(saved)).document(image)
    assert restored.raw_analysis_status(held['record'])=='unverified'
