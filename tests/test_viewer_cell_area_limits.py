"""Live producers submit data; the native viewer owner resolves and archives it."""
import csv
import math

import pytest
import qupathxx as q
from test_cell_calibration import scene
from test_viewer_session import GUI, completed, run


@pytest.mark.parametrize('minimum,maximum', [(-1.,None),(None,math.inf),(math.nan,None),(12.,11.)])
def test_invalid_physical_limits_do_not_consume_a_viewer_ticket(tmp_path,minimum,maximum):
    session=q.ViewerSession(str(tmp_path/'unopened.qpxproj'))
    with pytest.raises(ValueError):
        session.cells_with_nucleus_area_limits(1,1,q.StainTransform([[1,0,0],[0,1,0]]),minimum,maximum)
    assert session.stats.tickets==session.stats.queued==session.stats.queued_bytes==0


def test_physical_cell_command_can_be_cancelled_without_opening_a_renderer(tmp_path):
    session=q.ViewerSession(str(tmp_path/'unopened.qpxproj'))
    ticket=session.cells_with_nucleus_area_limits(1,1,q.StainTransform([[1,0,0],[0,1,0]]),maximum_um2=0.)
    assert session.poll(ticket).action=='cells' and session.poll(ticket).state=='queued'
    assert session.cancel(ticket) and session.take(ticket).state=='cancelled'
    assert session.stats.tickets==session.stats.queued_bytes==0


@GUI
@pytest.mark.skipif(not q.has_project,reason='project persistence')
def test_python_producer_archives_and_exports_the_same_physical_recipe(tmp_path):
    doc,obj,base,_=scene();project=q.Project();image=project.add_document(doc)
    path=tmp_path/'input.qpxproj';project.save(str(path));session=q.ViewerSession(str(path))
    output=tmp_path/'output.qpxproj';table=tmp_path/'cells.csv';held={}
    def producer():
        parameters=base.parameters
        ticket=session.cells_with_nucleus_area_limits(image,obj,q.StainTransform([[1,0,0],[0,1,0]]),
            minimum_um2=10.1,maximum_um2=11.25,parameters=parameters,retain_result=True)
        parameters.min_nucleus_pixels=10000
        event=completed(session,ticket)
        assert event.committed and event.record_id and event.cells.cell_count==1
        assert (event.cells.parameters.min_nucleus_pixels,event.cells.parameters.max_nucleus_pixels)==(81,90)
        assert event.cells.calibrated_feature(1).nucleus_sample_area_um2==11.25
        held['result']=event.cells
        exported=completed(session,session.export_cells(image,str(table),record_ids=[event.record_id]))
        assert exported.committed and exported.rows==1
        completed(session,session.save(str(output)));completed(session,session.close(True))
    run(session,producer)
    restored=q.Project.open(str(output)).document(image).latest_cells(obj).result
    assert restored.cell_count==1 and restored.parameters.min_nucleus_pixels==81
    assert held['result'].calibrated_feature(1).nucleus_sample_area_um2==11.25
    assert len(list(csv.DictReader(table.open(encoding='utf-8',newline=''))))==1


@GUI
@pytest.mark.skipif(not q.has_project,reason='project persistence')
def test_unknown_units_fail_the_ticket_without_archiving_a_new_run(tmp_path):
    doc,obj,base,_=scene(None);project=q.Project();image=project.add_document(doc)
    path=tmp_path/'input.qpxproj';project.save(str(path));session=q.ViewerSession(str(path))
    output=tmp_path/'output.qpxproj'
    def producer():
        ticket=session.cells_with_nucleus_area_limits(image,obj,q.StainTransform([[1,0,0],[0,1,0]]),
            minimum_um2=0.,parameters=base.parameters,retain_result=True)
        result=session.wait(ticket,15)
        assert result.state=='failed' and 'both pixel calibration axes' in result.error
        assert not result.committed and not result.record_id and result.cells is None
        session.take(ticket)
        completed(session,session.save(str(output)));completed(session,session.close(True))
    run(session,producer)
    assert q.Project.open(str(output)).document(image).cell_record_count==1
