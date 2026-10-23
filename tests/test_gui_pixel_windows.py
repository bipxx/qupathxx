"""F5 captures the visible natural-grid window without changing the target ROI."""
import os
import numpy as np
import pytest
import qupathxx as q
from test_gui_pixel_classifier import workspace, model_for, click, ready

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_classifier_training or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='requires GUI/training and QUPATHXX_TEST_GUI=1')


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_visible_window_matches_native_crop_in_responsive_panel(dimensions,scale):
    doc,ids,root,options=workspace(256);model=model_for(doc,ids,options);options.pixel_model=model
    options.width,options.height=dimensions;options.ui_scale=scale;options.pixel_viewport_only=True
    view=q.Viewport();view.zoom=12;view.origin=q.Point(-60*12,-50*12);options.start_view=view
    revision=doc.revision;geometry=doc.annotation(root).geometry.wkt
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.replay_complete and not report.pixel_error and report.pixel_result.windowed
    window=q.Rect(-report.final_origin.x/report.final_zoom,-report.final_origin.y/report.final_zoom,
                  report.canvas_content_bounds.width/report.final_zoom,report.canvas_content_bounds.height/report.final_zoom)
    expected=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,window=window))
    assert report.pixel_result.sha256==expected.sha256 and report.pixel_result.width<248 and report.pixel_result.height<248
    np.testing.assert_array_equal(report.pixel_result.labels_numpy(),expected.labels_numpy())
    assert report.pixel_preview_ready and report.pixel_overlay_visible and report.ui_overflow_items==0
    assert doc.revision==revision and doc.annotation(root).geometry.wkt==geometry


def test_large_target_window_fits_budget_and_survives_retention_as_historical_mask():
    small,ids,_,options=workspace();options.pixel_model=model_for(small,ids,options)
    doc=q.Document(q.Image.from_numpy(np.full((2048,2048,3),80,dtype=np.uint8)));doc.set_calibration(.5,.5)
    root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,2048,2048)));options.pixel_object_id=root;options.pixel_viewport_only=True
    budget=q.PixelPredictionOptions();budget.work_bytes=8*1024*1024;options.pixel_prediction_options=budget
    with pytest.raises((ValueError,RuntimeError),match='memory budget'):q.PixelPredictionRequest(doc,root,options.pixel_model,budget)
    view=q.Viewport();view.zoom=12;view.origin=q.Point(-1000*12,-900*12);options.start_view=view
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_KEEP_PREDICTION),ready(16,q.GuiInputWait.PIXEL_RECORD_KEPT),
        *click(20,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),ready(24,q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY)]
    report=q.show(doc,frame_limit=55,options=options)
    assert report.replay_complete and not report.pixel_error and report.pixel_archive_state=='kept'
    result=report.pixel_historical_result;assert result.windowed and result.estimated_work_bytes<=budget.work_bytes
    assert result.width<128 and result.height<128 and report.pixel_gpu_bytes<128*128*4
    assert doc.pixel_classifier_record(report.pixel_record_id).archive.to_bytes()[:4]==b'QPA\x02'
    assert report.pixel_result is None and not report.pixel_contours_published and doc.object_count==1
