"""Shared spatial controls: installed Python contracts and actual owner/UI delivery."""
import os
import json
import subprocess
import sys
import time
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


def options(sources,targets,run=False,csv=''):
    o=q.GuiOptions();o.width=1280;o.height=1000;o.autosave_interval_seconds=0;o.replay_timeout_seconds=15
    spatial=q.GuiSpatialOptions();spatial.open=True;spatial.run=run;spatial.sources=sources;spatial.targets=targets;spatial.csv_path=str(csv)
    analysis=spatial.analysis;analysis.max_distance_tests=1<<40;spatial.analysis=analysis;o.spatial=spatial
    return o

def busy_scene():
    doc=q.Document(q.Image(64,64,[80]*64*64*3));roi=q.Roi.points([q.Point(32,32)])
    doc.execute([q.ObjectEdit.create_geometry(roi) for _ in range(20000)])
    ids=[a.id for a in doc.annotations];assert len(ids)==20000
    project=q.Project();image=project.add_document(doc,'Coincident cohort')
    return project,image,doc,ids

@GUI
def test_desktop_active_cancel_and_retry_without_publication(tmp_path):
    project,image,doc,ids=busy_scene();revision=doc.revision;o=options(ids,ids)
    o.inputs=[*click(4,q.GuiInputTarget.SPATIAL_RUN),*click(8,q.GuiInputTarget.SPATIAL_CANCEL)]
    report=q.show(project,frame_limit=18,options=o);result=report.spatial
    assert report.replay_complete and result.state=='cancelled' and not result.busy and result.result is None
    assert result.analysis.distance_tests>0 and doc.revision==revision and doc.spatial_record_count==0
    retry=options([ids[0]],[ids[1]],run=True);retry.inputs=[ready(4,q.GuiInputWait.SPATIAL_READY),*click(8,q.GuiInputTarget.SPATIAL_KEEP),ready(12,q.GuiInputWait.SPATIAL_KEPT)]
    report=q.show(project,frame_limit=22,options=retry)
    assert report.replay_complete and report.spatial.result.row(0).distance==0 and doc.spatial_record_count==1

@GUI
@pytest.mark.parametrize('phase',['running','ready','round-trip'])
def test_desktop_image_switch_retires_spatial_authority(phase):
    if phase=='running':
        project,image,doc,ids=busy_scene();o=options(ids,ids,run=True)
        o.inputs=[event(6,q.GuiInputKind.NEXT_IMAGE_KEY)]
    else:
        project,image,doc,a,b=scene();o=options([a],[b],run=True)
        o.inputs=[ready(4,q.GuiInputWait.SPATIAL_READY),event(8,q.GuiInputKind.NEXT_IMAGE_KEY)]
        if phase=='round-trip':o.inputs+=[event(12,q.GuiInputKind.PREVIOUS_IMAGE_KEY)]
    other=q.Document(q.Image(64,64,[40]*64*64*3));other.add_detection(q.Roi.points([q.Point(32,32)]));other.add_detection(q.Roi.points([q.Point(35,36)]))
    other_image=project.add_document(other,'Same IDs, different owner');project.active_image=image;revision=doc.revision
    report=q.show(project,frame_limit=24,options=o)
    assert report.replay_complete and report.image_switches==(2 if phase=='round-trip' else 1)
    assert report.active_image==(image if phase=='round-trip' else other_image)
    assert report.spatial.state=='stale' and report.spatial.result is None and not report.spatial.busy
    assert doc.revision==revision and doc.spatial_record_count==other.spatial_record_count==0

@GUI
@pytest.mark.parametrize('failure',['existing','project','linked-image','byte-limit'])
def test_desktop_spatial_export_refusal_preserves_inputs(tmp_path,failure):
    project,image,doc,a,b=scene();request=q.SpatialDistanceRequest(doc,[a],[b]);result=q.measure_spatial_distances(request);record=doc.keep_spatial(request,result)
    linked=tmp_path/'missing.ppm';project.add_document(q.Document(q.Image.demo()),'Protected link',str(linked),'ppm');project.active_image=image
    source=tmp_path/'source.qpx';project.save(str(source));original=source.read_bytes();output=tmp_path/'distances.csv'
    if failure=='existing':output.write_bytes(b'preserve existing CSV')
    if failure=='project':output=source
    if failure=='linked-image':output=linked
    before=output.read_bytes() if output.exists() else None
    o=options([],[],csv=output);spatial=o.spatial;spatial.record_id=record
    if failure=='byte-limit':spatial.csv_bytes=1
    o.spatial=spatial;o.inputs=[*click(4,q.GuiInputTarget.SPATIAL_EXPORT)]
    report=q.show(project,frame_limit=18,options=o)
    assert report.replay_complete and not report.spatial.export_committed and not report.spatial.busy
    assert report.spatial.error or report.spatial.export_error
    assert source.read_bytes()==original and not linked.exists()
    assert (output.read_bytes() if output.exists() else None)==before
    assert not list(tmp_path.glob('*.qpx-tmp-*')) and doc.spatial_record_count==1

@GUI
@pytest.mark.parametrize('history',['removed','changed'])
def test_desktop_history_survives_removal_or_failed_fresh_replay(tmp_path,history):
    project,image,doc,a,b=scene();request=q.SpatialDistanceRequest(doc,[a],[b]);result=q.measure_spatial_distances(request);record=doc.keep_spatial(request,result)
    if history=='changed':doc.rename_annotation(a,'Changed after capture')
    csv=tmp_path/'history.csv';o=options([],[],csv=csv);spatial=o.spatial;spatial.record_id=record;o.spatial=spatial
    action=q.GuiInputTarget.SPATIAL_REMOVE if history=='removed' else q.GuiInputTarget.SPATIAL_VERIFY
    o.inputs=[*click(4,action),*click(8,q.GuiInputTarget.SPATIAL_EXPORT),ready(12,q.GuiInputWait.SPATIAL_EXPORTED)]
    report=q.show(project,frame_limit=22,options=o);view=report.spatial
    assert report.replay_complete and view.result.sha256==result.sha256 and view.export_committed
    assert view.record_status==('removed' if history=='removed' else 'objects_or_calibration_changed')
    assert csv.read_text(encoding='utf-8')==q.spatial_distance_csv(result)
    assert doc.spatial_record_count==(0 if history=='removed' else 1)

@GUI
def test_desktop_failed_analysis_never_exposes_partial_rows():
    project,image,doc,a,b=scene();o=options([a,b],[a,b],run=True);spatial=o.spatial;analysis=spatial.analysis;analysis.max_distance_tests=1;spatial.analysis=analysis;o.spatial=spatial
    revision=doc.revision;report=q.show(project,frame_limit=14,options=o)
    assert report.spatial.state=='failed' and report.spatial.result is None and not report.spatial.busy
    assert 'budget' in report.spatial.error and doc.revision==revision and not doc.spatial_record_count

@GUI
@pytest.mark.parametrize('action',['cancel','edit'])
def test_managed_running_spatial_cancel_or_edit_then_retry(tmp_path,action):
    project,image,doc,ids=busy_scene();source=tmp_path/'source.qpx';project.save(str(source));session=q.ViewerSession(str(source))
    def producer():
        before=completed(session,session.inspect(image,ids[0]));analysis=q.SpatialDistanceOptions();analysis.max_distance_tests=1<<40
        ticket=session.spatial_distances(image,ids,ids,analysis,keep=True,expected_revision=before.document_revision)
        deadline=time.monotonic()+5
        while session.poll(ticket).state=='queued' and time.monotonic()<deadline:time.sleep(.001)
        assert session.poll(ticket).state=='running'
        if action=='cancel':assert session.cancel(ticket)
        else:completed(session,session.execute(image,[q.ObjectEdit.remove(ids[-1])]))
        rejected=session.wait(ticket,15);assert rejected.state==('cancelled' if action=='cancel' else 'stale') and not rejected.committed and rejected.spatial is None
        session.take(ticket);after=completed(session,session.inspect(image,ids[0]))
        assert after.document_revision==before.document_revision+(action=='edit')
        retry=completed(session,session.spatial_distances(image,[ids[0]],[ids[1]],keep=True,expected_revision=after.document_revision))
        assert retry.committed and retry.record_id and retry.spatial.row(0).distance==0
        completed(session,session.close(True))
    run(session,producer);assert session.stats.retained_bytes==0

@GUI
def test_actual_window_close_cancels_and_joins_spatial_worker(tmp_path):
    code="""
import json,sys
import qupathxx as q
doc=q.Document(q.Image(64,64,[80]*64*64*3));roi=q.Roi.points([q.Point(32,32)])
doc.execute([q.ObjectEdit.create_geometry(roi) for _ in range(20000)])
ids=[a.id for a in doc.annotations];project=q.Project();project.add_document(doc,'Close during analysis');project.save(sys.argv[1])
o=q.GuiOptions();o.width=1280;o.height=1000;o.autosave_interval_seconds=0
spatial=q.GuiSpatialOptions();spatial.open=spatial.run=True;spatial.sources=spatial.targets=ids
analysis=spatial.analysis;analysis.max_distance_tests=1<<40;spatial.analysis=analysis;o.spatial=spatial
e=q.GuiInput();e.frame=8;e.kind=q.GuiInputKind.CLOSE_WINDOW
# Viewing persists the camera, so acknowledge the ordinary dirty-close dialog.
down=q.GuiInput();down.frame=10;down.kind=q.GuiInputKind.LEFT_DOWN;down.target=q.GuiInputTarget.DISCARD_CLOSE;down.wait_for=q.GuiInputWait.TARGET_READY
up=q.GuiInput();up.frame=11;up.kind=q.GuiInputKind.LEFT_UP;up.target=q.GuiInputTarget.DISCARD_CLOSE
o.inputs=[e,down,up]
report=q.show(project,frame_limit=60,options=o)
print(json.dumps(dict(replay_complete=report.replay_complete,state=report.spatial.state,busy=report.spatial.busy,frames=report.frames)))
assert report.frames<60
assert report.replay_complete and report.spatial.state=='cancelled' and not report.spatial.busy
assert report.spatial.result is None and doc.spatial_record_count==0
print(json.dumps(dict(state=report.spatial.state,distance_tests=report.spatial.analysis.distance_tests)))
"""
    child=subprocess.run([sys.executable,'-I','-X','utf8','-c',code,str(tmp_path/'source.qpx')],capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert child.returncode==0,child.stdout+child.stderr
    observed=json.loads(child.stdout.splitlines()[-1]);assert observed['state']=='cancelled' and observed['distance_tests']>0
