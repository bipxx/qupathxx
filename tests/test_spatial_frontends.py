"""Shared spatial controls: installed Python contracts and actual owner/UI delivery."""
import os
from pathlib import Path
import pytest
import qupathxx as q
from test_gui_pixel_classifier import event, click, ready
from test_viewer_session import completed, run

GUI=pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in actual spatial desktop/managed viewer')

def scene():
    doc=q.Document(q.Image.demo())
    a=doc.add_detection(q.Roi.points([q.Point(32,32)]))
    b=doc.add_detection(q.Roi.points([q.Point(35,36)]))
    project=q.Project();image=project.add_document(doc,'Spatial distances')
    return project,image,doc,a,b

@pytest.mark.parametrize('field,value',[('csv_bytes',0),('csv_bytes',1024**3+1),('csv_path','bad\x00path'),('run',True)])
def test_spatial_config_rejects_invalid_values(field,value):
    options=q.GuiSpatialOptions();setattr(options,field,value)
    with pytest.raises((ValueError,RuntimeError)):options.validate()

def test_spatial_options_and_queue_without_renderer(tmp_path):
    options=q.GuiSpatialOptions();options.sources=[1];options.targets=[2];options.run=True;options.validate()
    options.record_id=1
    with pytest.raises(ValueError):options.validate()
    gui=q.GuiOptions();assert gui.spatial is None;gui.spatial=options;assert gui.spatial.sources==[1];gui.spatial=None;assert gui.spatial is None
    session=q.ViewerSession(str(tmp_path/'source.qpx'))
    ticket=session.spatial_distances(1,[1],[2]);assert session.poll(ticket).action=='spatial_measure'
    assert session.cancel(ticket);assert session.take(ticket).state=='cancelled';assert session.stats.retained_bytes==0
    for submit in (lambda:session.spatial_distances(1,[],[]),lambda:session.inspect_spatial(1,0),lambda:session.verify_spatial(1,0),lambda:session.export_spatial(1,1,str(tmp_path/'out.csv'),0)):
        with pytest.raises((ValueError,RuntimeError)):submit()

@GUI
def test_desktop_spatial_run_keep_export_verify_reopen(tmp_path):
    project,image,doc,a,b=scene();csv=tmp_path/'distances.csv';o=q.GuiOptions();o.width=1280;o.height=1000;o.autosave_interval_seconds=0;o.replay_timeout_seconds=20
    spatial=q.GuiSpatialOptions();spatial.open=True;spatial.sources=[a];spatial.targets=[b];spatial.csv_path=str(csv);o.spatial=spatial
    o.inputs=[*click(4,q.GuiInputTarget.SPATIAL_RUN),ready(8,q.GuiInputWait.SPATIAL_READY),*click(12,q.GuiInputTarget.SPATIAL_KEEP),ready(16,q.GuiInputWait.SPATIAL_KEPT),*click(20,q.GuiInputTarget.SPATIAL_VERIFY),ready(24,q.GuiInputWait.SPATIAL_VERIFIED),*click(28,q.GuiInputTarget.SPATIAL_EXPORT),ready(32,q.GuiInputWait.SPATIAL_EXPORTED)]
    capture=tmp_path/'spatial.ppm';folder=os.environ.get('QUPATHXX_SPATIAL_FRONTEND_EVIDENCE_DIR')
    if folder:Path(folder).mkdir(parents=True,exist_ok=True);capture=Path(folder)/'spatial-verified.ppm'
    report=q.show(project,frame_limit=55,options=o,capture_path=str(capture));result=report.spatial
    assert report.replay_complete and result and not result.error,(report.replay_complete,result.error if result else None)
    assert result.state=='verified' and result.record_status=='current' and result.result.row(0).distance==5
    assert result.export_committed and result.export_rows==1 and result.export_bytes==csv.stat().st_size
    assert csv.read_text(encoding='utf-8')==q.spatial_distance_csv(result.result)
    assert report.ui_overflow_items==0,report.ui_overflow_label
    saved=tmp_path/'saved.qpx';project.save(str(saved));reopened=q.Project.open(str(saved));historical=reopened.document(image)
    assert historical.spatial_record(result.record_id).result.sha256==result.result.sha256
    assert historical.spatial_status(result.record_id)=='unverified'

@GUI
def test_managed_spatial_keep_history_verify_export_save(tmp_path):
    project,image,doc,a,b=scene();source=tmp_path/'source.qpx';project.save(str(source));session=q.ViewerSession(str(source));held={};saved=tmp_path/'saved.qpx';csv=tmp_path/'distances.csv'
    def producer():
        preview=completed(session,session.spatial_distances(image,[a],[b]));assert not preview.committed and preview.spatial.row(0).distance==5
        kept=completed(session,session.spatial_distances(image,[a],[b],keep=True,expected_revision=preview.document_revision));assert kept.committed and kept.record_id
        held['result']=kept.spatial;held['record']=kept.record_id
        inspected=completed(session,session.inspect_spatial(image,kept.record_id));assert inspected.spatial.sha256==kept.spatial.sha256
        verified=completed(session,session.verify_spatial(image,kept.record_id));assert verified.verified and not verified.committed
        exported=completed(session,session.export_spatial(image,kept.record_id,str(csv)));assert exported.committed and exported.rows==1
        duplicate=session.export_spatial(image,kept.record_id,str(csv));failed=session.wait(duplicate,15);assert failed.state=='failed' and not failed.committed;session.take(duplicate)
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    run(session,producer)
    assert held['result'].row(0).distance==5 and session.stats.retained_bytes==0
    reopened=q.Project.open(str(saved));assert reopened.document(image).spatial_record(held['record']).result.sha256==held['result'].sha256
