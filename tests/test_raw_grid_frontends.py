"""Raw grid producers share exact sampling, retention and checkpoint identity."""
import csv
from fractions import Fraction
import io
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
import tifffile


def sampling():
    value=q.RawGridSampling()
    value.samples_x=value.samples_y=1
    value.context_x=value.context_y=1
    return value


def scene(tmp_path,axes=False):
    if axes:
        values=np.arange(32,dtype='float64').reshape(2,2,2,2,2)
        source=tmp_path/'channels.ome.tif'
        tifffile.imwrite(source,values,ome=True,photometric='minisblack',metadata={'axes':'TZCYX'})
        expected=28.5
    else:
        values=np.arange(4,dtype='uint64').reshape(2,2)+np.uint64(2**53+5)
        source=tmp_path/'wide.tif'
        tifffile.imwrite(source,values,photometric='minisblack',metadata=None)
        expected=float((Fraction(int(values[0,0]))+Fraction(int(values[0,1])))/2)
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    obj=doc.add_rectangle(q.Rect(0,0,2,1),'Raw grid ROI')
    return project,image,doc,obj,expected


def test_raw_grid_gui_options_copy_and_validate():
    value=sampling();options=q.GuiOptions();options.raw_grid=value
    value.samples_x=9
    assert options.raw_grid.samples_x==1
    options.raw_grid.samples_x=7
    assert options.raw_grid.samples_x==1
    options.raw_grid=None
    assert options.raw_grid is None
    value.samples_x=0
    with pytest.raises(ValueError):options.raw_grid=value


def test_raw_grid_endpoint_admission_and_queue_reclamation(tmp_path):
    endpoint=q.ViewerSession(str(tmp_path/'not-opened.qpxproj'))
    value=sampling();options=q.ScientificRawOptions()
    value.samples_x=0
    with pytest.raises(ValueError):endpoint.analyze_raw_grid(1,1,value,options)
    value.samples_x=1
    with pytest.raises(ValueError):endpoint.analyze_raw_grid(1,0,value,options)
    options.channels=0
    with pytest.raises(IndexError):endpoint.analyze_raw_grid(1,1,value,options)
    assert endpoint.stats.tickets==endpoint.stats.queued_bytes==0
    options.channels=1
    ticket=endpoint.analyze_raw_grid(1,1,value,options,retain_result=True)
    value.samples_x=0;options.channels=0
    assert endpoint.poll(ticket).state=='queued' and endpoint.cancel(ticket)
    assert endpoint.take(ticket).state=='cancelled'
    assert endpoint.stats.tickets==endpoint.stats.queued_bytes==endpoint.stats.retained_bytes==0


@pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='requires quantitative TIFF project')
def test_raw_grid_roi_factory_and_python_batch_resume(tmp_path):
    project,image,doc,obj,expected=scene(tmp_path)
    counts=sampling();options=q.ScientificRawOptions()
    request=q.RawAnalysisRequest.for_grid_roi(doc,obj,counts,options)
    counts.samples_x=2;options.channels=0
    assert request.grid.x.samples==1 and request.grid_options.channels==1
    result=q.analyze_raw(request)
    assert result.statistics.means[0].hex()==expected.hex()
    preset=q.ProjectBatchOptions();preset.kind=q.ProjectBatchKind.RAW;preset.raw_grid=sampling()
    batch=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=preset)
    preset.raw_grid=None
    assert json.loads(batch.parameters_json)['raw_grid']['samples_x']==1
    first=q.run_project_batch(batch,str(tmp_path/'batch'))
    assert first.state=='succeeded' and first.saved==1
    again=q.run_project_batch(batch,str(tmp_path/'batch'))
    assert again.state=='succeeded' and again.reused==1 and again.saved==0
    assert again.images[0].raw_tiles==0 and again.images[0].raw_hashed_tiles==1
    loaded=q.Project.open(again.images[0].path).document(image)
    assert loaded.raw_analysis_records[0].result.statistics.means[0].hex()==expected.hex()
    assert doc.raw_analysis_record_count==0


@pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='requires quantitative TIFF project')
@pytest.mark.parametrize('axes',[False,True])
def test_raw_grid_cli_exact_source_axes_unicode_and_resume(tmp_path,axes):
    project,image,_,_,expected=scene(tmp_path,axes)
    path=tmp_path/'项目.qpxproj';project.save(str(path))
    from qupathxx import _native
    cli=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    command=[str(cli),str(path),str(tmp_path/'结果'),'--kind','raw','--raw-grid-samples','1,1','--raw-grid-context','1,1']
    if axes:command+=['--raw-channel','1','--raw-z','1','--raw-time','1']
    for attempt in range(2):
        done=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert done.returncode==0,done.stderr
        result=next(json.loads(line) for line in done.stdout.splitlines() if json.loads(line)['schema']=='qpx.batch-image/1')
        assert result['state']==('saved' if attempt==0 else 'reused')
        assert (result['raw_tiles'],result['raw_hashed_tiles'])==((1,0) if attempt==0 else (0,1))
        record=q.Project.open(result['path']).document(image).raw_analysis_records[0]
        assert record.result.statistics.means[0].hex()==expected.hex()
        rows=list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(result['path']))))
        assert rows[0]['schema']=='qpx.raw-analysis-csv/2'
    for flags in [['--raw-level','0'],['--tile-size','256'],['--level','0']]:
        bad=subprocess.run(command+flags,capture_output=True,timeout=60)
        assert bad.returncode==2 and b'level or tile' in bad.stderr


def desktop():
    if not q.has_gui or not q.has_tiff or not q.has_project:
        pytest.skip('requires quantitative project desktop')
    if os.environ.get('QUPATHXX_TEST_GUI')!='1':pytest.skip('opt-in raw grid desktop')


def test_raw_grid_desktop_measure_keep_csv_and_render(tmp_path):
    desktop()
    from test_gui_raw import options
    from test_gui_raw_records import wait
    from test_gui_replay import event
    from test_gui_tma import click
    project,image,doc,_,expected=scene(tmp_path);o=options();o.raw_tab=1;o.raw_grid=sampling()
    output=tmp_path/'grid.csv';o.raw_csv_path=str(output)
    o.inputs=[event(3,q.GuiInputKind.SELECT_ALL_KEY)]+click(5,q.GuiInputTarget.RAW_MEASURE)+[wait(10,q.GuiInputWait.RAW_TASK_FINISHED)]
    o.inputs+=click(12,q.GuiInputTarget.RAW_RESULTS_TAB)+click(15,q.GuiInputTarget.RAW_KEEP)+[wait(18,q.GuiInputWait.RAW_ANALYSIS_KEPT)]
    o.inputs+=click(21,q.GuiInputTarget.RAW_RECORDS_TAB)+click(24,q.GuiInputTarget.RAW_CSV_EXPORT)+[wait(28,q.GuiInputWait.RAW_MEASUREMENTS_SAVED)]
    capture=tmp_path/'grid.ppm';report=q.show(project,frame_limit=40,capture_path=str(capture),options=o)
    assert not report.raw_error,report.raw_error
    assert report.raw_records==1 and report.raw_analysis_result.grid is not None
    assert report.raw_analysis_result.statistics.means[0].hex()==expected.hex()
    assert report.raw_csv_state=='saved' and report.raw_csv_rows==1
    rows=list(csv.DictReader(io.StringIO(output.read_text(encoding='utf-8'))))
    assert rows[0]['schema']=='qpx.raw-analysis-csv/2'
    assert doc.raw_analysis_status(report.raw_record_id)=='current' and report.ui_overflow_items==0
    assert capture.read_bytes().startswith(b'P6\n')


def test_raw_grid_managed_producer_retains_verifies_and_saves(tmp_path):
    desktop()
    from test_viewer_session import completed,run
    project,image,_,obj,expected=scene(tmp_path)
    source=tmp_path/'source.qpxproj';project.save(str(source));session=q.ViewerSession(str(source))
    saved=tmp_path/'saved.qpxproj';table=tmp_path/'managed.csv';held={}
    def producer():
        counts=sampling();options=q.ScientificRawOptions()
        ticket=session.analyze_raw_grid(image,obj,counts,options,retain_result=True)
        counts.samples_x=0;options.channels=0
        result=completed(session,ticket);held['result']=result.raw_analysis;held['record']=result.record_id
        assert result.committed and result.raw_analysis.grid.grid.x.samples==1
        assert completed(session,session.verify_raw_analysis(image,result.record_id)).verified
        assert completed(session,session.export_raw_measurements(image,str(table),True)).rows==1
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    report=run(session,producer)
    assert report.ui_overflow_items==0
    assert held['result'].statistics.means[0].hex()==expected.hex()
    restored=q.Project.open(str(saved)).document(image)
    assert restored.raw_analysis_status(held['record'])=='unverified'
    assert restored.raw_analysis_records[0].result.grid is not None


def test_raw_grid_desktop_batch_matches_native_recipe_and_resume(tmp_path):
    desktop()
    from test_gui_batch import options
    project,image,_,_,expected=scene(tmp_path)
    preset=q.ProjectBatchOptions();preset.kind=q.ProjectBatchKind.RAW;preset.raw_grid=sampling()
    request=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=preset)
    o=options(tmp_path/'batch',preset)
    for attempt in range(2):
        report=q.show(project,frame_limit=40,options=o)
        assert not report.batch_error and report.batch_state=='succeeded'
        result=report.batch_result
        assert result.parameters_sha256==request.parameters_sha256
        assert (result.saved,result.reused)==((1,0) if attempt==0 else (0,1))
        record=q.Project.open(result.images[0].path).document(image).raw_analysis_records[0]
        assert record.result.grid is not None and record.result.statistics.means[0].hex()==expected.hex()
        assert report.ui_overflow_items==0
