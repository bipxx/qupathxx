"""Real inspector controls keep/reopen/relink historical project descriptors."""
import os
from pathlib import Path
import shutil
import numpy as np
import pytest
import qupathxx as q
from test_gui_pixel_classifier import click,ready,event
from test_pixel_stream_read_job import FIXTURE

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='requires GUI/project and opt-in replay')


def workspace(path):
    doc=q.Document(q.Image.from_numpy(np.full((48,64,3),80,np.uint8)));project=q.Project();image=project.add_document(doc)
    o=q.GuiOptions();o.pixel_stream_browser_open=True;o.pixel_stream_read_path=str(path);o.autosave_interval_seconds=0;o.replay_timeout_seconds=25
    return project,image,doc,o


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_keep_verified_file_link_from_real_inspector_survives_project_reopen(tmp_path,dimensions,scale):
    path=tmp_path/'linked.qps';shutil.copyfile(FIXTURE,path);project,image,doc,o=workspace(path);revision=doc.revision;o.width,o.height=dimensions;o.ui_scale=scale
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(12,q.GuiInputTarget.PIXEL_STREAM_VERIFY),ready(16,q.GuiInputWait.PIXEL_STREAM_VERIFIED),
        *click(20,q.GuiInputTarget.PIXEL_STREAM_KEEP_LINK),ready(24,q.GuiInputWait.PIXEL_STREAM_LINKED)]
    report=q.show(project,frame_limit=55,options=o)
    assert report.replay_complete and not report.pixel_error and doc.pixel_stream_record_count==1
    assert report.pixel_stream_record_id==report.pixel_stream_selected_record_id and report.pixel_stream_record_id>0
    record=doc.pixel_stream_record(report.pixel_stream_record_id);assert record.link.summary.sha256==report.pixel_stream_read.declared_summary.sha256
    assert doc.revision==revision+1 and project.dirty and report.pixel_result is None and doc.object_count==0
    assert report.pixel_stream_record_catalog_complete and report.pixel_stream_record_catalog_max_step<=1024 and report.ui_overflow_items==0,report.ui_overflow_label
    source=tmp_path/'project.qpxproj';project.save(str(source));loaded=q.Project.open(str(source));restored=loaded.document(image)
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN_RECORD),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY)]
    replay=q.show(loaded,frame_limit=35,options=o)
    assert replay.replay_complete and replay.pixel_stream_read.verified and replay.pixel_stream_read_preview_ready and not replay.pixel_error
    assert restored.pixel_stream_record_count==1 and not loaded.dirty


def test_real_inspector_never_keeps_an_unverified_link():
    project,_,doc,o=workspace(FIXTURE)
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),event(12,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.PIXEL_STREAM_KEEP_LINK),event(13,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.PIXEL_STREAM_KEEP_LINK)]
    report=q.show(project,frame_limit=35,options=o)
    assert report.replay_complete and doc.pixel_stream_record_count==0 and report.pixel_stream_record_id==0
    assert not report.pixel_stream_read.verified and report.pixel_stream_read.link is None


def test_saved_record_missing_file_is_preserved_and_reports_error(tmp_path):
    path=tmp_path/'gone.qps';shutil.copyfile(FIXTURE,path);project,_,doc,o=workspace(path)
    record=doc.keep_pixel_stream(q.prepare_pixel_stream_link(str(path)));path.unlink();revision=doc.revision
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN_RECORD),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_FINISHED)]
    report=q.show(project,frame_limit=35,options=o)
    assert report.replay_complete and report.pixel_stream_read.state=='failed' and report.pixel_stream_read.error
    assert doc.pixel_stream_record_count==1 and doc.pixel_stream_record(record).link.path==str(path.resolve()) and doc.revision==revision
    assert report.pixel_stream_read.block is None and report.pixel_stream_read_gpu_bytes==0


def test_real_relink_updates_path_only_after_matching_file_verification(tmp_path):
    old=tmp_path/'old.qps';new=tmp_path/'moved.qps';shutil.copyfile(FIXTURE,old);shutil.copyfile(old,new)
    project,_,doc,o=workspace(new);record=doc.keep_pixel_stream(q.prepare_pixel_stream_link(str(old)));old.unlink();revision=doc.revision
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(12,q.GuiInputTarget.PIXEL_STREAM_VERIFY),ready(16,q.GuiInputWait.PIXEL_STREAM_VERIFIED),
        *click(20,q.GuiInputTarget.PIXEL_STREAM_RELINK),*click(24,q.GuiInputTarget.PIXEL_STREAM_OPEN_RECORD),ready(28,q.GuiInputWait.PIXEL_STREAM_READ_READY)]
    report=q.show(project,frame_limit=55,options=o)
    assert report.replay_complete and not report.pixel_error and report.pixel_stream_read.verified
    assert doc.pixel_stream_record(record).link.path==str(new.resolve()) and doc.revision==revision+1 and new.exists()


def test_real_remove_link_keeps_external_file_and_historical_preview(tmp_path):
    path=tmp_path/'kept.qps';shutil.copyfile(FIXTURE,path);project,_,doc,o=workspace(path)
    doc.keep_pixel_stream(q.prepare_pixel_stream_link(str(path)));revision=doc.revision
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN_RECORD),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),*click(12,q.GuiInputTarget.PIXEL_STREAM_REMOVE_RECORD)]
    report=q.show(project,frame_limit=40,options=o)
    assert report.replay_complete and not report.pixel_error and doc.pixel_stream_record_count==0 and doc.revision==revision+1
    assert path.exists() and report.pixel_stream_read_preview_ready and report.pixel_stream_read.block is not None
