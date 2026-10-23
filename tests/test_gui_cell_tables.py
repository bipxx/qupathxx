import csv
import io
import os

import pytest
import qupathxx as q

from test_cells import phantom
from test_tissue import document
from test_cell_records import capture
from test_gui_replay import event

pytestmark=pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="opt-in desktop replay")


def setup():
    doc=document(phantom(64))
    obj=doc.add_rectangle(q.Rect(0,0,64,64))
    _,result,record=capture(doc,obj)
    return doc,obj,result,record


def test_gui_export_matches_native_bytes_and_preserves_document(tmp_path):
    doc,obj,result,record=setup()
    expected=q.cell_measurements_csv(doc,[record])
    revision=doc.revision
    options=q.GuiOptions()
    path=tmp_path/"\u6d4b\u91cf.csv"
    options.cell_table_path=str(path)
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.EXPORT_CELL_TABLE_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED),
                    event(10,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(doc,frame_limit=30,options=options)
    assert report.cell_table_state=="succeeded" and not report.cell_table_error
    assert report.cell_table_rows==result.cell_count and report.cell_table_bytes==path.stat().st_size
    assert report.cell_table_path==str(path.resolve()) and path.read_bytes()==expected.encode()
    assert report.selected_ids==[obj] and doc.revision==revision and doc.object_count==1


@pytest.mark.parametrize('calibration',[None,(.5,.25)])
def test_gui_capture_calibration_export_keeps_historical_units(tmp_path,calibration):
    doc,obj,_,_=setup()
    # Capture a second run after assigning physical units; the first run remains
    # unknown. Later edits must not replace either run's captured calibration.
    if calibration is not None:
        doc.set_calibration(*calibration)
        capture(doc,obj)
    doc.set_calibration(2,3)
    table_options=q.CellTableOptions()
    table_options.include_capture_calibration=True
    # The selected-annotation command exports its latest archived run.
    expected=q.cell_measurements_csv(doc,[doc.latest_cells(obj).id],options=table_options)
    options=q.GuiOptions()
    path=tmp_path/'captured.csv'
    options.cell_table_path=str(path)
    options.cell_table_capture_calibration=True
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),
                    event(8,q.GuiInputKind.EXPORT_CELL_TABLE_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED),
                    event(10,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(doc,frame_limit=30,options=options)
    assert report.cell_table_state=='succeeded' and not report.cell_table_error
    assert path.read_bytes()==expected.encode()
    values=list(csv.DictReader(io.StringIO(expected)))
    assert {row['capture_pixel_width_um'] for row in values}==({''} if calibration is None else {'0.5'})
    assert all(row['table_schema']=='qpx.cell-table/2' for row in values)


def test_detect_archive_and_export_use_one_native_run(tmp_path):
    doc=document(phantom(64))
    obj=doc.add_rectangle(q.Rect(0,0,64,64))
    path=tmp_path/"run.csv"
    options=q.GuiOptions()
    options.cell_table_path=str(path)
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.DETECT_CELLS_KEY),
                    event(10,q.GuiInputKind.KEEP_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_READY),
                    event(12,q.GuiInputKind.EXPORT_CELL_TABLE_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED),
                    event(14,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(doc,frame_limit=35,options=options)
    assert report.cell_table_state=="succeeded" and report.saved_cell_status=="current" and doc.cell_record_count==1
    assert path.read_bytes()==q.cell_measurements_csv(doc,[doc.latest_cells(obj).id]).encode()


def test_failed_gui_export_keeps_existing_file(tmp_path):
    doc,_,_,_=setup()
    path=tmp_path/"old.csv"
    path.write_bytes(b"sentinel")
    options=q.GuiOptions()
    options.cell_table_path=str(path)
    options.cell_table_byte_limit=1
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.EXPORT_CELL_TABLE_KEY)]
    report=q.show(doc,frame_limit=30,options=options)
    assert report.cell_table_state=="failed" and "byte limit" in report.cell_table_error
    assert path.read_bytes()==b"sentinel" and not list(tmp_path.glob("*.qpx-tmp-*"))


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_gui_requires_recheck_when_current_only_is_selected(tmp_path):
    doc,obj,_,record=setup()
    project=q.Project()
    image=project.add_document(doc)
    storage=tmp_path/"cells.qpxproj"
    project.save(str(storage))
    restored=q.Project.open(str(storage)).document(image)
    path=tmp_path/"verified.csv"
    options=q.GuiOptions()
    options.cell_table_path=str(path)
    options.cell_table_require_current=True
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.EXPORT_CELL_TABLE_KEY)]
    report=q.show(restored,frame_limit=20,options=options)
    assert report.cell_table_state=="idle" and not path.exists()
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.RECHECK_CELL_RUN_KEY),
                    event(10,q.GuiInputKind.EXPORT_CELL_TABLE_KEY,wait=q.GuiInputWait.CELLS_VERIFIED),
                    event(12,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(restored,frame_limit=30,options=options)
    assert report.cell_table_state=="succeeded" and restored.cell_status(record)=="current"
    assert all(r["status_at_capture"]=="current" for r in csv.DictReader(io.StringIO(path.read_text())))


def test_cell_table_wait_timeout_does_not_create_a_file(tmp_path):
    doc,_,_,_=setup()
    options=q.GuiOptions()
    options.cell_table_path=str(tmp_path/"absent.csv")
    options.replay_timeout_seconds=.1
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    with pytest.raises(RuntimeError,match="GUI replay timed out"):
        q.show(doc,frame_limit=10,options=options)
    assert not (tmp_path/"absent.csv").exists()


def test_csv_retains_capture_identity_after_source_deletion(tmp_path):
    doc,obj,result,record=setup()
    expected=q.cell_measurements_csv(doc,[record])
    path=tmp_path/"history.csv"
    options=q.GuiOptions()
    options.cell_table_path=str(path)
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.EXPORT_CELL_TABLE_KEY),
                    event(10,q.GuiInputKind.DELETE_KEY,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report=q.show(doc,frame_limit=25,options=options)
    assert report.cell_table_state=="succeeded" and report.cell_table_rows==result.cell_count
    assert doc.object_count==0 and doc.cell_status(record)=="object_changed" and path.read_bytes()==expected.encode()
