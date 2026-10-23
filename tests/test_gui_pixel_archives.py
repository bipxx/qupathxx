"""Real F5 retention/reuse controls; historical values carry no live authority."""
import os
import numpy as np
import pytest
import qupathxx as q
from test_gui_pixel_classifier import workspace, model_for, click, ready, event

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_classifier_training or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='requires GUI/training and QUPATHXX_TEST_GUI=1')


def retained_prediction():
    doc,ids,root,options=workspace();model=model_for(doc,ids,options)
    request=q.PixelPredictionRequest(doc,root,model)
    result=q.predict_pixel_region(request)
    record=doc.keep_pixel_prediction(request,q.archive_pixel_prediction(result))
    return doc,root,options,record,result


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_keep_training_from_real_panel_and_reuse_model(dimensions,scale):
    doc,ids,root,options=workspace();options.width,options.height=dimensions;options.ui_scale=scale
    revision=doc.revision;doc.clear_history()
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_TRAIN),ready(8,q.GuiInputWait.PIXEL_TRAINED),
        *click(12,q.GuiInputTarget.PIXEL_KEEP_TRAINING),ready(16,q.GuiInputWait.PIXEL_RECORD_KEPT),
        *click(20,q.GuiInputTarget.PIXEL_USE_RECORD_MODEL),ready(24,q.GuiInputWait.PIXEL_RECORD_MODEL_LOADED),
        *click(28,q.GuiInputTarget.PIXEL_PREDICT),ready(32,q.GuiInputWait.PIXEL_PREVIEW_READY)]
    report=q.show(doc,frame_limit=65,options=options)
    assert report.replay_complete and not report.pixel_error
    assert report.pixel_archive_state=='kept' and doc.pixel_classifier_record_count==1
    record=doc.pixel_classifier_record(report.pixel_record_id)
    assert record.archive.purpose=='training' and record.archive.table.row_count==128
    assert [a.id for a in record.archive.annotations]==ids
    assert report.pixel_model.sha256==record.archive.model.sha256
    assert report.pixel_result.object_id==root and report.pixel_preview_ready
    assert report.pixel_historical_result is None and doc.revision==revision+1 and not doc.can_undo
    assert report.pixel_record_catalog_complete and report.pixel_record_catalog_max_step<=1024
    assert report.ui_overflow_items==0,report.ui_overflow_label


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
def test_keep_mask_then_preview_is_historical_and_cannot_publish(dimensions,scale):
    doc,ids,root,options=workspace();options.width,options.height=dimensions;options.ui_scale=scale
    options.pixel_model=model_for(doc,ids,options);doc.clear_history();revision=doc.revision
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREDICT),ready(8,q.GuiInputWait.PIXEL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_KEEP_PREDICTION),ready(16,q.GuiInputWait.PIXEL_RECORD_KEPT),
        *click(20,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),ready(24,q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY),
        event(28,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.PIXEL_TRACE),event(29,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.PIXEL_TRACE),
        event(32,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.PIXEL_PUBLISH),event(33,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.PIXEL_PUBLISH)]
    report=q.show(doc,frame_limit=60,options=options)
    assert report.replay_complete and not report.pixel_error
    record=doc.pixel_classifier_record(report.pixel_record_id)
    assert record.archive.purpose=='prediction' and report.pixel_historical_result is record.archive.result
    assert report.pixel_result is None and report.pixel_preview_ready and report.pixel_overlay_visible
    assert report.pixel_contours is None and not report.pixel_contours_published
    assert report.pixel_selected_record_id==record.id and doc.object_count==3
    assert doc.revision==revision+1 and not doc.can_undo and report.ui_overflow_items==0


@pytest.mark.skipif(not q.has_project,reason='optional project backend')
def test_reopened_mask_uses_original_values_after_target_edit_and_deletion(tmp_path):
    doc,root,options,record,result=retained_prediction()
    doc.update_roi(root,q.Roi.rectangle(q.Rect(1,1,4,4)));doc.remove_objects([root])
    project=q.Project();image=project.add_document(doc);path=tmp_path/'historical.qpxproj';project.save(str(path))
    reopened=q.Project.open(str(path));loaded=reopened.document(image);revision=loaded.revision
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),ready(8,q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY)]
    report=q.show(reopened,frame_limit=35,options=options)
    assert report.replay_complete and not report.pixel_error and report.pixel_preview_ready
    assert report.pixel_selected_record_id==record and report.pixel_historical_result.sha256==result.sha256
    np.testing.assert_array_equal(report.pixel_historical_result.labels_numpy(),result.labels_numpy())
    assert report.pixel_result is None and loaded.revision==revision and loaded.object_count==2


def test_remove_previewed_record_releases_gpu_and_keeps_model():
    doc,_,options,record,result=retained_prediction();revision=doc.revision
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),ready(8,q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_REMOVE_RECORD)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.replay_complete and not report.pixel_error and doc.pixel_classifier_record_count==0
    assert report.pixel_historical_result is None and report.pixel_result is None and report.pixel_gpu_bytes==0
    assert not report.pixel_overlay_visible and report.pixel_model.sha256==result.model.sha256
    assert report.pixel_selected_record_id==0 and doc.revision==revision+1


def test_recompute_from_saved_mask_restores_fresh_authority():
    doc,root,options,_,historical=retained_prediction()
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),ready(8,q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY),
        *click(12,q.GuiInputTarget.PIXEL_PREDICT),ready(16,q.GuiInputWait.PIXEL_PREVIEW_READY)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.replay_complete and not report.pixel_error and report.pixel_historical_result is None
    assert report.pixel_result is not None and report.pixel_result.object_id==root
    assert report.pixel_result.sha256==historical.sha256 and report.pixel_result is not historical


@pytest.mark.skipif(not q.has_project,reason='optional project backend')
def test_image_switch_clears_historical_preview_and_record_selection():
    doc,_,options,record,_=retained_prediction();project=q.Project();first=project.add_document(doc)
    project.add_document(q.Document(q.Image.from_numpy(np.full((64,96,3),100,dtype=np.uint8))));project.active_image=first
    options.inputs=[*click(4,q.GuiInputTarget.PIXEL_PREVIEW_RECORD),ready(8,q.GuiInputWait.PIXEL_HISTORICAL_PREVIEW_READY),
        event(12,q.GuiInputKind.NEXT_IMAGE_KEY)]
    report=q.show(project,frame_limit=40,options=options)
    assert report.replay_complete and report.image_switches==1
    assert report.pixel_selected_record_id==0 and report.pixel_historical_result is None
    assert report.pixel_gpu_bytes==0 and not report.pixel_overlay_visible
    assert doc.pixel_classifier_record(record).archive.result is not None
