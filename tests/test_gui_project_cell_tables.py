import os
import pytest
import qupathxx as q
from test_project_cell_tables import workspace,parsed
from test_gui_replay import event

pytestmark=pytest.mark.skipif(not q.has_project or not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="opt-in project desktop")


def export(frame):
    e=event(frame,q.GuiInputKind.EXPORT_CELL_TABLE_KEY);e.shift=True;return e


def test_project_export_uses_all_images_without_selection_and_matches_native_bytes(tmp_path):
    project,_,_,_=workspace();expected=q.cell_measurements_csv(project)
    path=tmp_path/"\u9879\u76ee.csv";options=q.GuiOptions();options.cell_table_path=str(path)
    options.inputs=[export(8),event(12,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(project,frame_limit=30,options=options)
    assert report.cell_table_state=="succeeded" and not report.cell_table_error and not report.selected_ids
    expected=q.cell_measurements_csv(project)  # The viewer publishes its initial camera view before capture.
    assert path.read_bytes()==expected.encode() and report.cell_table_rows==len(parsed(expected))


def test_empty_project_can_export_header_without_an_active_image(tmp_path):
    project=q.Project();path=tmp_path/"empty.csv";options=q.GuiOptions();options.cell_table_path=str(path)
    options.inputs=[export(8),event(12,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(project,frame_limit=25,options=options)
    assert report.cell_table_state=="succeeded" and report.cell_table_rows==0 and path.read_bytes()==q.cell_measurements_csv(project).encode()


def test_current_only_project_failure_preserves_file_and_reports_capture_error(tmp_path):
    project,docs,_,_=workspace();docs[1].server.close()
    path=tmp_path/"old.csv";path.write_bytes(b"sentinel");options=q.GuiOptions();options.cell_table_path=str(path);options.cell_table_require_current=True
    options.inputs=[export(8)]
    report=q.show(project,frame_limit=25,options=options)
    assert report.cell_table_state=="idle" and "current verified" in report.cell_table_error and path.read_bytes()==b"sentinel"


def test_project_byte_limit_failure_is_atomic(tmp_path):
    project,_,_,_=workspace();path=tmp_path/"old.csv";path.write_bytes(b"sentinel")
    options=q.GuiOptions();options.cell_table_path=str(path);options.cell_table_byte_limit=1;options.inputs=[export(8)]
    report=q.show(project,frame_limit=25,options=options)
    assert report.cell_table_state=="failed" and "byte limit" in report.cell_table_error
    assert path.read_bytes()==b"sentinel" and not list(tmp_path.glob("*.qpx-tmp-*"))
