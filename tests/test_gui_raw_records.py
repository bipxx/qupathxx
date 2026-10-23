"""Real native desktop retention, explicit replay and managed raw exports."""
import csv
import io
import os

import numpy as np
import pytest
import qupathxx as q
from test_gui_raw import options
from test_gui_tma import click
from test_gui_tiles import replay
from test_tiff_reader import request
from test_tiff_project import scene
from test_viewer_session import completed,run

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_tiff or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in raw records desktop')


def wait(frame,event):
    value=replay(frame,q.GuiInputKind.MOVE);value.wait_for=event;return value


def test_desktop_measure_keep_and_csv_export(tmp_path):
    project,image,values=scene(tmp_path);o=options();o.raw_tab=1
    o.raw_request=request(24,16,c=2,z=1,t=1);output=tmp_path/'raw.csv';o.raw_csv_path=str(output)
    o.inputs=click(5,q.GuiInputTarget.RAW_MEASURE)+[wait(10,q.GuiInputWait.RAW_TASK_FINISHED)]
    o.inputs+=click(12,q.GuiInputTarget.RAW_RESULTS_TAB)+click(15,q.GuiInputTarget.RAW_KEEP)+[wait(18,q.GuiInputWait.RAW_ANALYSIS_KEPT)]
    o.inputs+=click(21,q.GuiInputTarget.RAW_RECORDS_TAB)+click(24,q.GuiInputTarget.RAW_CSV_EXPORT)+[wait(28,q.GuiInputWait.RAW_MEASUREMENTS_SAVED)]
    report=q.show(project,frame_limit=40,options=o)
    assert not report.raw_error,report.raw_error
    assert report.raw_records==1 and report.raw_record_id and report.raw_csv_state=='saved'
    assert report.raw_csv_rows==2 and report.raw_csv_bytes==output.stat().st_size
    assert project.document(image).raw_analysis_status(report.raw_record_id)=='current'
    table=list(csv.DictReader(io.StringIO(output.read_text(encoding='utf-8'))))
    np.testing.assert_allclose([float(row['mean']) for row in table],values[1,1].mean(axis=(1,2)))
    assert report.ui_overflow_items==0


def test_managed_raw_measure_keep_verify_export_and_save(tmp_path):
    project,image,values=scene(tmp_path);path=tmp_path/'source.qpxproj';project.save(str(path))
    endpoint=q.ViewerSession(str(path));held={};output=tmp_path/'managed.csv';saved=tmp_path/'managed.qpxproj'
    def producer():
        result=completed(endpoint,endpoint.analyze_raw(image,request(24,16,c=2,z=2,t=1),retain_result=True))
        assert result.committed and result.record_id and result.raw_analysis
        held['record']=result.record_id;held['result']=result.raw_analysis
        verified=completed(endpoint,endpoint.verify_raw_analysis(image,result.record_id))
        assert verified.verified and not verified.committed and verified.document_revision==result.document_revision
        exported=completed(endpoint,endpoint.export_raw_measurements(image,str(output),True,[result.record_id]))
        assert exported.committed and exported.rows==2
        completed(endpoint,endpoint.save(str(saved)));completed(endpoint,endpoint.close(True))
    run(endpoint,producer,frames=120)
    np.testing.assert_allclose(held['result'].statistics.means,values[1,2].mean(axis=(1,2)))
    assert q.Project.open(str(saved)).document(image).raw_analysis_status(held['record'])=='unverified'
    assert endpoint.stats.retained_bytes==0


def test_desktop_raw_batch_captures_requested_axes_and_reuses_checkpoint(tmp_path):
    from test_gui_batch import options as batch_options
    project,image,values=scene(tmp_path);doc=project.document(image);doc.add_rectangle(q.Rect(0,0,4,4))
    preset=q.ProjectBatchOptions();preset.kind=q.ProjectBatchKind.RAW;preset.raw_follow_display=False
    preset.raw.z=2;preset.raw.t=1;preset.raw.channels=2
    for attempt in range(2):
        report=q.show(project,frame_limit=35,options=batch_options(tmp_path/'batch',preset))
        assert report.batch_state=='succeeded',report.batch_error
        result=report.batch_result
        assert result.saved==(attempt==0) and result.reused==(attempt==1)
        record=q.Project.open(result.images[0].path).document(image).raw_analysis_records[0]
        np.testing.assert_allclose(record.result.statistics.means,values[1,2,:,:4,:4].mean(axis=(1,2)))
    assert doc.raw_analysis_record_count==0


def archived_scene(tmp_path):
    project,image,values=scene(tmp_path);doc=project.document(image)
    capture=q.RawAnalysisRequest(doc,request(24,16,c=2,z=1,t=1))
    record=doc.keep_raw_analysis(capture,q.analyze_raw(capture))
    path=tmp_path/'archived.qpxproj';project.save(str(path));doc.server.close()
    return q.Project.open(str(path)),image,record,values


def test_desktop_replay_verifies_without_revision_change_then_removes(tmp_path):
    project,image,record,_=archived_scene(tmp_path);doc=project.document(image)
    assert doc.raw_analysis_status(record)=='unverified';revision=doc.revision
    o=options();o.raw_tab=3;o.raw_record_id=record
    o.inputs=click(5,q.GuiInputTarget.RAW_VERIFY)+[wait(10,q.GuiInputWait.RAW_ANALYSIS_VERIFIED)]
    report=q.show(project,frame_limit=22,options=o)
    assert not report.raw_error,report.raw_error
    assert report.raw_record_status=='current' and doc.revision==revision
    o.inputs=click(5,q.GuiInputTarget.RAW_REMOVE)
    report=q.show(project,frame_limit=18,options=o)
    assert report.raw_records==0 and doc.raw_analysis_record_count==0 and doc.revision==revision+1


def test_desktop_historical_csv_works_with_missing_source(tmp_path):
    project,image,record,values=archived_scene(tmp_path)
    (tmp_path/'source.ome.tif').unlink()
    o=options();o.raw_tab=3;o.raw_record_id=record;o.raw_csv_path=str(tmp_path/'historical.csv')
    o.inputs=click(7,q.GuiInputTarget.RAW_CSV_EXPORT)+[wait(12,q.GuiInputWait.RAW_MEASUREMENTS_SAVED)]
    report=q.show(project,frame_limit=24,options=o)
    assert report.raw_csv_state=='saved' and report.raw_csv_rows==2,report.raw_error
    table=list(csv.DictReader(io.StringIO((tmp_path/'historical.csv').read_text(encoding='utf-8'))))
    assert all(row['status_at_capture']=='unverified' for row in table)
    np.testing.assert_allclose([float(row['mean']) for row in table],values[1,1].mean(axis=(1,2)))
