"""Managed viewer commands execute on its owner; Python producers own no model."""
import csv
import gc
import os
from pathlib import Path
import threading
import time

import numpy as np
import pytest
import qupathxx as q

GUI=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in managed viewer')


def setup(tmp_path):
    p=q.Project();doc=q.Document(q.Image(32,24,[16]*32*24*3));obj=doc.add_rectangle(q.Rect(1,2,20,16));p.add_document(doc,'First, "image"')
    other=q.Document(q.Image(16,16,[32]*16*16*3));other.add_rectangle(q.Rect(0,0,16,16));p.add_document(other,'Second')
    path=tmp_path/'source.qpxproj';p.save(str(path));return path,p,doc,obj


def completed(session,ticket,consume=True):
    e=session.wait(ticket,15);assert e.state=='succeeded',(e.action,e.state,e.error)
    return session.take(ticket) if consume else e


def run(session,producer,frames=900,options=None,capture=''):
    errors=[]
    def worker():
        try:producer()
        except BaseException as e:
            errors.append(e)
            try:session.close(True)
            except (ValueError,RuntimeError):pass
    thread=threading.Thread(target=worker,daemon=True);thread.start()
    try:report=q.show(session,frame_limit=frames,capture_path=capture,options=options or q.GuiOptions())
    finally:thread.join(20)
    assert not thread.is_alive(),'producer did not finish'
    if errors:raise errors[0]
    return report


def test_queue_lifecycle_without_renderer(tmp_path):
    s=q.ViewerSession(str(tmp_path/'not-opened.qpxproj'));assert s.stats.state=='created' and s.stats.epoch==1
    ticket=s.catalog();e=s.wait(ticket,0);assert e.state=='queued' and e.action=='catalog' and e.ticket==ticket
    assert s.cancel(ticket) and not s.cancel(ticket);assert s.take(ticket).state=='cancelled'
    assert s.stats.queued==s.stats.queued_bytes==s.stats.tickets==s.stats.retained_bytes==0
    with pytest.raises(IndexError):s.poll(ticket)
    with pytest.raises(ValueError):s.wait(0,-1)
    ticket=s.catalog()
    with pytest.raises(RuntimeError):s.take(ticket)
    s.cancel(ticket);s.take(ticket)


@pytest.mark.parametrize('field,value',[('tickets',0),('tickets',1025),('max_edits',0),('command_bytes',0),('region_bytes',2),('result_bytes',10),('snapshot_bytes',1)])
def test_invalid_limits(field,value,tmp_path):
    limits=q.ViewerLimits();setattr(limits,field,value)
    with pytest.raises(ValueError):q.ViewerSession(str(tmp_path/'p'),limits)


def test_queue_limits_and_cancelled_ticket_reclamation(tmp_path):
    limits=q.ViewerLimits();limits.tickets=1;s=q.ViewerSession(str(tmp_path/'p'),limits)
    for _ in range(100):
        ticket=s.catalog()
        with pytest.raises((ValueError,RuntimeError)):s.catalog()
        s.cancel(ticket);s.take(ticket)
    limits=s.limits;limits.tickets=0;assert s.limits.tickets==1
    with pytest.raises(ValueError):s.execute(1,[])
    with pytest.raises((ValueError,RuntimeError)):s.execute(1,[q.ObjectEdit.remove(1)]*1025)
    with pytest.raises((ValueError,RuntimeError)):s.read_region(1,q.RegionRequest(0,0,0,4096,4096))
    with pytest.raises(ValueError):s.catalog(limit=0)


@GUI
def test_live_end_to_end_python_producer_and_native_owner(tmp_path):
    path,original,original_doc,_=setup(tmp_path);s=q.ViewerSession(str(path));output=tmp_path/'saved.qpxproj';roi_csv=tmp_path/'roi.csv';cell_csv=tmp_path/'cells.csv';held={}
    def producer():
        catalog=completed(s,s.catalog());assert [i.name for i in catalog.images]==['First, "image"','Second']
        before=completed(s,s.inspect(1,1));assert before.object.geometry.bounds.area==320
        edit=q.ObjectEdit.create(q.Rect(4,4,8,8),'from Python')
        e=completed(s,s.execute(1,[edit],expected_revision=before.document_revision));assert e.committed and len(e.object_ids)==1;obj=e.object_ids[0]
        e=completed(s,s.select(1,[obj]));assert e.image_id==1
        e=completed(s,s.read_region(1,q.RegionRequest(0,0,0,8,8)));held['array']=e.pixels.numpy();assert np.all(held['array'][:,:,:3]==16)
        transform=q.StainTransform([[1,0,0],[0,1,0]])
        e=completed(s,s.analyze(1,obj,transform));assert e.committed and e.samples==64 and e.analysis.samples==64
        e=completed(s,s.tissue(1,obj,transform,retain_result=True));assert e.committed and e.tissue.samples==64;held['labels']=e.tissue.labels_numpy()
        e=completed(s,s.cells(1,obj,transform,retain_result=True));assert e.committed and e.cells is not None
        e=completed(s,s.export_measurements(0,q.MeasurementTableKind.ROI,str(roi_csv),True));assert e.committed and e.rows==1
        e=completed(s,s.export_cells(0,str(cell_csv),True));assert e.committed
        completed(s,s.activate(2));assert s.stats.active_image==2
        completed(s,s.activate(1));completed(s,s.select(1,[obj]))
        e=completed(s,s.save(str(output)));assert e.committed and Path(e.path)==output
        completed(s,s.close(True))
    report=run(s,producer);assert s.stats.state=='closed' and not s.stats.queued and not s.stats.tickets
    assert report.frames>5 and report.selected_ids==[2]
    assert len(original_doc.annotations)==1 and not original_doc.analysis_records
    restored=q.Project.open(str(output));doc=restored.document(1);assert len(doc.annotations)==2 and len(doc.analysis_records)==len(doc.tissue_records)==len(doc.cell_records)==1
    rows=list(csv.DictReader(roi_csv.open(newline='',encoding='utf8')));assert len(rows)==1 and rows[0]['samples']=='64'
    del restored,doc,report;gc.collect();assert held['array'].shape==(8,8,4) and held['labels'].shape==(8,8)
    with pytest.raises(RuntimeError):s.catalog()
    with pytest.raises(RuntimeError):q.show(s,frame_limit=2)


@GUI
def test_command_error_atomicity_revision_staleness_and_cancel(tmp_path):
    path,_,_,_=setup(tmp_path);s=q.ViewerSession(str(path));output=tmp_path/'state.qpxproj'
    def producer():
        before=completed(s,s.inspect(1,1))
        zero=s.execute(1,[q.ObjectEdit.remove(1)],expected_revision=0);assert s.wait(zero,10).state=='stale';s.take(zero)
        ticket=s.execute(1,[q.ObjectEdit.create(q.Rect(0,0,2,2)),q.ObjectEdit.remove(999)])
        e=s.wait(ticket,10);assert e.state=='failed' and not e.committed;s.take(ticket)
        after=completed(s,s.inspect(1,1));assert before.document_revision==after.document_revision
        completed(s,s.execute(1,[q.ObjectEdit.create(q.Rect(0,0,2,2))]))
        ticket=s.execute(1,[q.ObjectEdit.remove(1)],expected_revision=before.document_revision);assert s.wait(ticket,10).state=='stale';s.take(ticket)
        ticket=s.catalog();s.cancel(ticket);assert s.wait(ticket,10).state in ('cancelled','succeeded');s.take(ticket)
        ticket=s.inspect(999);assert s.wait(ticket,10).state=='failed';s.take(ticket)
        completed(s,s.save(str(output)));completed(s,s.close(True))
    run(s,producer);assert len(q.Project.open(str(output)).document(1).annotations)==2


@GUI
def test_retained_result_budget_and_source_change_discard(tmp_path):
    path,_,_,_=setup(tmp_path);limits=q.ViewerLimits();limits.result_bytes=65536;s=q.ViewerSession(str(path),limits)
    def producer():
        ticket=s.read_region(1,q.RegionRequest(0,0,0,256,256));e=s.wait(ticket,10);assert e.state=='failed' and 'budget' in e.error;s.take(ticket);assert s.stats.retained_bytes==0
        completed(s,s.inspect(1));completed(s,s.close(True))
    run(s,producer)


@GUI
def test_finite_viewer_shutdown_completes_pending_tickets(tmp_path):
    path,_,_,_=setup(tmp_path);s=q.ViewerSession(str(path));tickets=[s.catalog() for _ in range(30)]
    q.show(s,frame_limit=3);assert s.stats.state=='closed'
    states=[s.take(t).state for t in tickets];assert 'succeeded' in states and 'cancelled' in states and not s.stats.tickets


@GUI
def test_owned_parameter_options_and_no_external_project_alias(tmp_path):
    path,project,_,_=setup(tmp_path);s=q.ViewerSession(str(path));options=q.GuiOptions();options.project=project
    with pytest.raises(ValueError):q.show(s,frame_limit=3,options=options)
    assert s.stats.state=='created'
    options=q.GuiOptions();p=q.StainEstimateParameters();options.stain_estimate_parameters=p
    def producer():
        completed(s,s.inspect(1));p.sample_limit=10 # source option stays Python-owned; viewer uses a native copy
        completed(s,s.close(True))
    run(s,producer,options=options)


@GUI
def test_main_thread_and_single_window_guards(tmp_path):
    path,_,_,_=setup(tmp_path);s=q.ViewerSession(str(path));errors=[]
    def wrong_thread():
        try:q.show(s,frame_limit=3)
        except RuntimeError as e:errors.append(str(e))
    t=threading.Thread(target=wrong_thread);t.start();t.join();assert errors and 'main thread' in errors[0] and s.stats.state=='created'
    def producer():
        completed(s,s.catalog())
        with pytest.raises(RuntimeError,match='already running'):q.show(q.Document(q.Image.demo()),frame_limit=2)
        completed(s,s.close(True))
    run(s,producer)


@GUI
def test_initial_open_failure_wakes_waiting_producer(tmp_path):
    s=q.ViewerSession(str(tmp_path/'missing.qpxproj'));ticket=s.catalog();events=[]
    thread=threading.Thread(target=lambda:events.append(s.wait(ticket,5)));thread.start()
    with pytest.raises((ValueError,RuntimeError)):q.show(s,frame_limit=3)
    thread.join(5);assert not thread.is_alive() and events[0].state=='cancelled' and s.stats.state=='failed'


@GUI
def test_close_can_cancel_initial_opening_before_commands(tmp_path):
    s=q.ViewerSession(str(tmp_path/'not-opened.qpxproj'));pending=s.catalog();closing=s.close(True)
    report=q.show(s,frame_limit=3)
    assert report.startup_cancelled and s.stats.state=='closed'
    assert s.take(closing).state=='succeeded' and s.take(pending).state=='cancelled'


@GUI
def test_force_close_bypasses_commands_paused_by_dirty_dialog(tmp_path):
    path,_,_,_=setup(tmp_path);s=q.ViewerSession(str(path));held={}
    def producer():
        completed(s,s.execute(1,[q.ObjectEdit.create(q.Rect(0,0,2,2))]))
        completed(s,s.close(False));held['paused']=s.inspect(1)
        completed(s,s.close(True))
    run(s,producer);assert s.take(held['paused']).state=='cancelled' and s.stats.state=='closed'


def test_transform_payload_is_included_in_queue_budget(tmp_path):
    limits=q.ViewerLimits();limits.command_bytes=4096;limits.queued_bytes=4096;s=q.ViewerSession(str(tmp_path/'p'),limits)
    with pytest.raises((ValueError,RuntimeError)):s.analyze(1,1,q.StainTransform([[1,0,0],[0,1,0]]))


@GUI
def test_export_can_select_new_record_from_existing_archives(tmp_path):
    path,p,doc,obj=setup(tmp_path);t=q.StainTransform([[1,0,0],[0,1,0]]);request=q.AnalysisRequest(doc,obj,t);doc.keep_analysis(request,q.analyze(request));p.save(str(path))
    s=q.ViewerSession(str(path));csvpath=tmp_path/'selected.csv'
    def producer():
        result=completed(s,s.analyze(1,obj,t))
        exported=completed(s,s.export_measurements(1,q.MeasurementTableKind.ROI,str(csvpath),record_ids=[result.record_id]))
        assert exported.rows==1
        completed(s,s.close(True))
    run(s,producer);assert len(list(csv.DictReader(csvpath.open(encoding='utf8',newline=''))))==1
