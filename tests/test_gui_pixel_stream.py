"""Real F5 export/cancel controls keep whole-target output off the UI thread."""
import os
import numpy as np
import pytest
import qupathxx as q
from test_gui_pixel_classifier import workspace,model_for,click,ready

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_classifier_training or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='requires GUI/training and QUPATHXX_TEST_GUI=1')


def stream_options():
    s=q.PixelStreamOptions();s.block_size=16;p=q.PixelPredictionOptions();p.work_bytes=8*1024*1024;s.prediction=p;return s


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_real_panel_exports_complete_file_without_retaining_mask(tmp_path,dimensions,scale):
    doc,ids,root,o=workspace();o.pixel_model=model_for(doc,ids,o);o.width,o.height=dimensions;o.ui_scale=scale
    path=tmp_path/'panel.qps';o.pixel_stream_path=str(path);o.pixel_stream_options=stream_options();revision=doc.revision
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_EXPORT_STREAM),ready(8,q.GuiInputWait.PIXEL_STREAM_FINISHED)]
    report=q.show(doc,frame_limit=35,options=o)
    assert report.replay_complete and not report.pixel_error and report.pixel_stream.state=='saved'
    assert report.pixel_stream.current and report.pixel_stream.summary and report.pixel_stream.completed_blocks==report.pixel_stream.total_blocks
    f=q.PixelStreamFile(str(path));f.verify();assert f.verified and f.declared_summary.sha256==report.pixel_stream.summary.sha256
    assert report.pixel_result is None and report.pixel_historical_result is None and report.pixel_gpu_bytes==0
    assert doc.revision==revision and report.ui_overflow_items==0,report.ui_overflow_label


def test_real_panel_reports_unrelated_destination_failure(tmp_path):
    doc,ids,_,o=workspace();o.pixel_model=model_for(doc,ids,o);path=tmp_path/'unrelated.svs';path.write_bytes(b'preserve')
    o.pixel_stream_path=str(path);o.pixel_stream_replace=True
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_EXPORT_STREAM),ready(8,q.GuiInputWait.PIXEL_STREAM_FINISHED)]
    report=q.show(doc,frame_limit=30,options=o)
    assert report.replay_complete and report.pixel_stream.state=='failed' and report.pixel_error
    assert report.pixel_stream.summary is None and path.read_bytes()==b'preserve'


def test_real_panel_cancel_leaves_no_partial_file(tmp_path):
    small,ids,_,training=workspace();model=model_for(small,ids,training)
    doc,_,_,o=workspace(2048);o.pixel_model=model;o.pixel_stream_path=str(tmp_path/'cancel.qps');o.pixel_stream_options=stream_options()
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_EXPORT_STREAM),*click(8,q.GuiInputTarget.PIXEL_CANCEL),ready(12,q.GuiInputWait.PIXEL_STREAM_FINISHED)]
    report=q.show(doc,frame_limit=35,options=o)
    assert report.replay_complete and report.pixel_stream.state=='cancelled' and report.pixel_stream.summary is None
    assert not list(tmp_path.iterdir()) and report.pixel_result is None and report.pixel_gpu_bytes==0


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_real_panel_stream_uses_captured_visible_window(tmp_path,dimensions,scale):
    doc,ids,root,o=workspace(256);o.pixel_model=model_for(doc,ids,o);o.width,o.height=dimensions;o.ui_scale=scale
    o.pixel_viewport_only=True;o.pixel_stream_path=str(tmp_path/'window.qps');o.pixel_stream_options=stream_options()
    view=q.Viewport();view.zoom=12;view.origin=q.Point(-60*12,-50*12);o.start_view=view
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_EXPORT_STREAM),ready(8,q.GuiInputWait.PIXEL_STREAM_FINISHED)]
    report=q.show(doc,frame_limit=35,options=o)
    assert report.replay_complete and not report.pixel_error and report.pixel_stream.state=='saved'
    file=q.PixelStreamFile(o.pixel_stream_path);file.verify();assert file.windowed and file.verified
    window=q.Rect(-report.final_origin.x/report.final_zoom,-report.final_origin.y/report.final_zoom,
                  report.canvas_content_bounds.width/report.final_zoom,report.canvas_content_bounds.height/report.final_zoom)
    expected=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,o.pixel_model,window=window))
    assert file.declared_summary.class_counts==expected.class_counts and expected.width<248 and expected.height<248
    for i in range(file.declared_summary.blocks):
        block=file.read_block(i);x=int(block.level_bounds.x-expected.level_bounds.x);y=int(block.level_bounds.y-expected.level_bounds.y)
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:
            np.testing.assert_array_equal(getattr(block,method)(),getattr(expected,method)()[y:y+block.height,x:x+block.width])
    assert report.pixel_result is None and report.pixel_gpu_bytes==0 and report.ui_overflow_items==0


def test_real_panel_replaces_verified_stream_only_when_requested(tmp_path):
    doc,ids,root,o=workspace();o.pixel_model=model_for(doc,ids,o);path=tmp_path/'replace.qps'
    original=q.save_pixel_stream(q.PixelPredictionStream(doc,root,o.pixel_model,stream_options()),str(path))
    o.pixel_stream_path=str(path);o.pixel_stream_replace=True;o.pixel_stream_options=stream_options()
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_EXPORT_STREAM),ready(8,q.GuiInputWait.PIXEL_STREAM_FINISHED)]
    report=q.show(doc,frame_limit=35,options=o)
    assert report.replay_complete and not report.pixel_error and report.pixel_stream.state=='saved'
    assert report.pixel_stream.summary.class_counts==original.class_counts
    q.PixelStreamFile(str(path)).verify()
