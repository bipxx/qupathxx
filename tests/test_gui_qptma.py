"""Real ImGui QPTMA preview/import/export, project persistence and failures."""
import os
from pathlib import Path
import pytest
import qupathxx as q
from test_gui_tma import click,options
from test_qptma import core_value

pytestmark=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in QPTMA desktop')
FIXTURE=Path(__file__).resolve().parent/'fixtures/qupath-tma-0.7.0/fractional-import.qptma'


def settings(input_path=FIXTURE,output_path=''):
    result=options();result.qptma_input_path=str(input_path);result.qptma_output_path=str(output_path)
    result.height=1100
    return result


def document():return q.Document(q.Image(256,192,[255]*256*192*3))


def test_preview_is_explicit_and_does_not_mutate_document():
    doc=document();revision=doc.revision;o=settings();o.inputs=click(4,q.GuiInputTarget.QPTMA_LOAD_PREVIEW)
    report=q.show(doc,frame_limit=30,options=o)
    assert not report.tma_error and report.qptma_state=='preview ready'
    assert report.qptma_preview_cores==6 and report.qptma_preview_missing==2
    assert 0<report.qptma_preview_max_step<=512 and doc.revision==revision and doc.tma_core_count==0
    assert report.ui_overflow_items==0


def test_import_edit_history_export_and_project_reopen(tmp_path):
    assert q.has_project
    doc=document();project=q.Project();image=project.add_document(doc,'TMA target')
    path=tmp_path/'project.qpxproj';project.save(str(path));source=FIXTURE.read_bytes()
    output=tmp_path/'grid.qptma';o=settings(output_path=output)
    o.inputs=[*click(4,q.GuiInputTarget.QPTMA_LOAD_PREVIEW),*click(10,q.GuiInputTarget.QPTMA_APPLY),
              *click(16,q.GuiInputTarget.QPTMA_ACKNOWLEDGE_LOSSES),*click(22,q.GuiInputTarget.QPTMA_EXPORT)]
    capture=tmp_path/'qptma-workflow.ppm'
    report=q.show(project,frame_limit=40,capture_path=str(capture),options=o)
    assert not report.tma_error and report.qptma_state=='saved' and report.qptma_export_bytes==output.stat().st_size
    expected=[core_value(c) for c in q.load_qptma(str(FIXTURE)).cores]
    assert doc.tma_core_count==6 and [core_value(c) for c in q.load_qptma(str(output)).cores]==expected
    assert doc.undo() and doc.tma_core_count==0
    assert doc.redo() and doc.tma_core_count==6
    first=doc.tma_grid().core(0,0).id;doc.set_tma_case_ids({first:'edited case'})
    project.save(str(path));restored=q.Project.open(str(path)).document(image)
    assert restored.tma_grid().core(0,0).tma_core.case_id=='edited case'
    assert FIXTURE.read_bytes()==source and capture.exists() and report.ui_overflow_items==0


@pytest.mark.parametrize('failure',['malformed','outside'])
def test_failed_preview_preserves_document(tmp_path,failure):
    source=tmp_path/'bad.qptma'
    if failure=='malformed':source.write_text('not a grid',encoding='utf-8')
    else:
        data=q.load_qptma(str(FIXTURE));cores=data.cores;cores[-1].bounds=q.Rect(10000,0,20,20);data.cores=cores;q.save_qptma(data,str(source))
    doc=document();revision=doc.revision;o=settings(source);o.inputs=click(4,q.GuiInputTarget.QPTMA_LOAD_PREVIEW)
    report=q.show(doc,frame_limit=30,options=o)
    assert report.qptma_state=='failed' and report.tma_error and doc.revision==revision and not doc.tma_grid()
    assert report.qptma_preview_cores==0 and report.ui_overflow_items==0


def test_preview_discard_is_not_an_import():
    doc=document();revision=doc.revision;o=settings()
    o.inputs=[*click(4,q.GuiInputTarget.QPTMA_LOAD_PREVIEW),*click(12,q.GuiInputTarget.QPTMA_DISCARD_PREVIEW)]
    report=q.show(doc,frame_limit=30,options=o)
    assert report.qptma_state=='idle' and report.qptma_preview_cores==0 and doc.revision==revision


def test_export_existing_file_preserves_bytes(tmp_path):
    doc=document();q.import_qptma(doc,q.load_qptma(str(FIXTURE)));output=tmp_path/'existing.qptma';output.write_bytes(b'preserve this file')
    o=settings(output_path=output)
    o.inputs=[*click(4,q.GuiInputTarget.QPTMA_ACKNOWLEDGE_LOSSES),*click(10,q.GuiInputTarget.QPTMA_EXPORT)]
    report=q.show(doc,frame_limit=30,options=o)
    assert report.qptma_state=='failed' and report.tma_error and output.read_bytes()==b'preserve this file'
    assert doc.tma_core_count==6 and report.ui_overflow_items==0


def test_preview_becomes_stale_after_owner_document_edit():
    from test_gui_tiles import replay
    doc=document();doc.add_rectangle(q.Rect(1,1,2,2),'earlier edit')
    o=settings();o.inputs=[*click(4,q.GuiInputTarget.QPTMA_LOAD_PREVIEW),replay(14,q.GuiInputKind.UNDO_KEY)]
    report=q.show(doc,frame_limit=35,options=o)
    assert report.qptma_state=='stale' and report.qptma_preview_cores==0
    assert report.tma_error and doc.tma_core_count==0 and doc.object_count==0


def test_export_requires_explicit_loss_acknowledgment(tmp_path):
    doc=document();q.import_qptma(doc,q.load_qptma(str(FIXTURE)));output=tmp_path/'unacknowledged.qptma'
    o=settings(output_path=output);report=q.show(doc,frame_limit=20,options=o)
    assert report.qptma_state=='idle' and not output.exists() and doc.tma_core_count==6
