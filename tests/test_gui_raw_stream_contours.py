"""Actual ImGui and managed viewer consumers of captured streamed contours."""
import os
from pathlib import Path
import pytest
import qupathxx as q
from test_gui_raw_pixel import scene, options, model_for, click
from test_gui_pixel_classifier import event, ready
from test_viewer_session import completed, run

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_tiff or not q.has_project or not q.has_classifier_training or not q.has_geos or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in actual GUI and typed TIFF/training/GEOS')


@pytest.mark.parametrize('undo,redo',[(False,False),(True,False),(True,True)])
def test_desktop_streamed_preview_apply_undo_redo(tmp_path,undo,redo):
    project,_,doc,ids,root,settings=scene(tmp_path)
    try:
        o=options(settings,root,tmp_path);raw=o.raw_pixel;raw.model=model_for(doc,ids,settings);raw.stream_contours=True
        request=q.RawPixelTiledPredictionRequest(doc,raw.model,settings.lattice,raw.stream,object_id=root)
        expected=q.trace_raw_pixel_stream(request,raw.stream_contour_options);o.raw_pixel=raw;doc.clear_history()
        o.inputs=[*click(4,q.GuiInputTarget.RAW_PIXEL_CONTOURS_TAB),*click(8,q.GuiInputTarget.RAW_PIXEL_TRACE),ready(12,q.GuiInputWait.RAW_PIXEL_CONTOURS_READY),*click(16,q.GuiInputTarget.RAW_PIXEL_APPLY),ready(20,q.GuiInputWait.RAW_PIXEL_CONTOURS_PUBLISHED),*click(24,q.GuiInputTarget.RAW_PIXEL_HIDE)]
        if undo:o.inputs+=[event(28,q.GuiInputKind.UNDO_KEY)]
        if redo:o.inputs+=[event(32,q.GuiInputKind.REDO_KEY)]
        output=os.environ.get('QUPATHXX_STREAM_FRONTEND_EVIDENCE_DIR');capture=tmp_path/'streamed.ppm'
        if output:
            folder=Path(output);folder.mkdir(parents=True,exist_ok=True);capture=folder/('streamed-'+str(int(undo))+str(int(redo))+'.ppm')
        report=q.show(project,frame_limit=55,capture_path=str(capture),options=o);result=report.raw_pixel
        assert report.replay_complete and not result.error,result.error
        assert result.stream_contours is not None and result.contours is None
        assert result.contour_preview_ready and 0<result.contour_preview_bytes<=raw.preview_budget
        assert result.stream_contours.prediction_sha256==expected.prediction_sha256
        assert result.contours_published and len(result.published_ids)==expected.contours.component_count>0
        if undo and not redo:assert doc.object_count==3 and not doc.children(root) and doc.can_redo
        else:
            assert doc.children(root)==result.published_ids
            for obj,component in zip(result.published_ids,expected.contours.components):
                assert doc.annotation(obj).geometry.equals(component.geometry) and doc.annotation(obj).parent_id==root
        assert report.ui_overflow_items==0,report.ui_overflow_label
    finally:doc.server.close()


def test_desktop_edit_retires_streamed_preview(tmp_path):
    project,_,doc,ids,root,settings=scene(tmp_path)
    try:
        o=options(settings,root,tmp_path);raw=o.raw_pixel;raw.model=model_for(doc,ids,settings);raw.stream_contours=True;o.raw_pixel=raw
        o.inputs=[*click(4,q.GuiInputTarget.RAW_PIXEL_CONTOURS_TAB),*click(8,q.GuiInputTarget.RAW_PIXEL_TRACE),ready(12,q.GuiInputWait.RAW_PIXEL_CONTOURS_READY),*click(16,q.GuiInputTarget.RAW_PIXEL_HIDE),event(20,q.GuiInputKind.SELECT_ALL_KEY),event(24,q.GuiInputKind.DELETE_KEY)]
        report=q.show(project,frame_limit=45,options=o)
        assert report.replay_complete and doc.object_count==0
        assert report.raw_pixel.state=='stale' and report.raw_pixel.stream_contours is None
        assert not report.raw_pixel.contour_preview_ready and not report.raw_pixel.contour_preview_bytes
    finally:doc.server.close()


def test_managed_cropped_streamed_preview_apply_save_reopen(tmp_path):
    project,image,doc,ids,root,settings=scene(tmp_path);model=model_for(doc,ids,settings)
    source=tmp_path/'source.qpx';saved=tmp_path/'streamed.qpx';project.save(str(source));doc.server.close()
    session=q.ViewerSession(str(source));held={};stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=2;stream.work_bytes=16*1024**2
    def producer():
        preview=completed(session,session.contour_raw_pixel_stream(image,model,settings.lattice,stream,region=q.RawPixelStreamRegion(1,1,3,2),object_id=root))
        assert preview.raw_pixel_stream_contours and preview.raw_pixel_contours is None and not preview.committed and not preview.object_ids
        assert preview.raw_pixel_stream_contours.completed_samples==6 and preview.bytes>=preview.raw_pixel_stream_contours.payload_bytes
        held['preview']=preview.raw_pixel_stream_contours
        applied=completed(session,session.contour_raw_pixel_stream(image,model,settings.lattice,stream,region=q.RawPixelStreamRegion(1,1,3,2),object_id=root,apply=True,expected_revision=preview.document_revision))
        assert applied.committed and applied.raw_pixel_stream_contours and applied.components==len(applied.object_ids)>0
        held['ids']=applied.object_ids;held['geometry']=[c.geometry for c in applied.raw_pixel_stream_contours.contours.components]
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    run(session,producer)
    assert held['preview'].completed_samples==6
    reopened=q.Project.open(str(saved))
    try:
        current=reopened.document(image);assert current.children(root)==held['ids']
        for obj,geometry in zip(held['ids'],held['geometry']):assert current.annotation(obj).geometry.equals(geometry)
    finally:reopened.document(image).server.close()


def test_managed_streamed_result_budget_refuses_before_commit(tmp_path):
    project,image,doc,ids,root,settings=scene(tmp_path);model=model_for(doc,ids,settings);source=tmp_path/'quota.qpx';project.save(str(source));doc.server.close()
    limits=q.ViewerLimits();limits.result_bytes=65536;session=q.ViewerSession(str(source),limits)
    def producer():
        result=completed(session,session.contour_raw_pixel_stream(image,model,settings.lattice,object_id=root,apply=True))
        assert result.state=='failed' and 'result budget' in result.error and not result.committed and not result.object_ids
        completed(session,session.close(True))
    run(session,producer)
