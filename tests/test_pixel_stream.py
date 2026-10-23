"""Native single-consumer blocks, source halos, quotas and explicit snapshot lifetime."""
import gc
import numpy as np
import pytest
import qupathxx as q
from test_pixel_prediction_windows import setup

pytestmark=pytest.mark.skipif(not q.has_classifier_training,reason='optional training backend')


def bounds(rect):return rect.x,rect.y,rect.width,rect.height


@pytest.mark.parametrize('block_size',[16,32,64])
@pytest.mark.parametrize('tile_size',[16,32])
@pytest.mark.parametrize('window',[None,(17.25,11.2,28.5,20.7)])
def test_stream_covers_grid_once_and_matches_full_labels_votes_and_halos(block_size,tile_size,window):
    doc,root,model=setup();options=q.PixelStreamOptions();options.block_size=block_size
    prediction=q.PixelPredictionOptions();prediction.tile_size=tile_size;prediction.work_bytes=8*1024*1024;prediction.min_vote_fraction=.75;options.prediction=prediction
    rect=q.Rect(*window) if window else None
    expected=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,prediction,rect))
    stream=q.PixelPredictionStream(doc,root,model,options,rect)
    assert stream.matches(doc) and stream.state=='ready' and not stream.busy and stream.windowed==bool(window)
    assert stream.object_id==root and stream.document_revision==doc.revision and stream.level==expected.level and stream.downsample==expected.downsample
    assert bounds(stream.level_bounds)==bounds(expected.level_bounds) and stream.estimated_work_bytes<=prediction.work_bytes
    labels=np.zeros((expected.height,expected.width),np.uint32);valid=np.zeros_like(labels,dtype=np.uint8);votes=np.full(labels.shape,np.nan,np.float32);coverage=np.zeros_like(valid)
    for index,block in enumerate(stream,1):
        assert block.windowed and block.width<=block_size and block.height<=block_size
        assert not block.labels_numpy().flags.writeable and block.model.sha256==model.sha256 and block.roi.wkt==expected.roi.wkt
        x=int(block.level_bounds.x-expected.level_bounds.x);y=int(block.level_bounds.y-expected.level_bounds.y);sl=np.s_[y:y+block.height,x:x+block.width]
        labels[sl]=block.labels_numpy();valid[sl]=block.valid_numpy();votes[sl]=block.vote_fractions_numpy();coverage[sl]+=1
        assert stream.completed_blocks==index and not stream.busy
        saved=q.archive_pixel_prediction(block);loaded=q.PixelClassifierArchive.from_bytes(saved.to_bytes())
        assert loaded.result.sha256==block.sha256 and loaded.result.windowed
    np.testing.assert_array_equal(coverage,1)
    for actual,reference in [(labels,expected.labels_numpy()),(valid,expected.valid_numpy()),(votes,expected.vote_fractions_numpy())]:np.testing.assert_array_equal(actual,reference)
    assert stream.state=='complete' and stream.completed_blocks==stream.total_blocks
    assert stream.class_counts==expected.class_counts and stream.valid_pixels==expected.valid_pixels and stream.classified_pixels==expected.classified_pixels
    assert stream.next() is None and list(stream)==[]
    stream.cancel();assert stream.state=='complete'


@pytest.mark.parametrize('field,value',[('block_size',0),('block_size',15),('block_size',513),('max_pixels',0),('max_pixels',10),('max_blocks',0),('max_blocks',1)])
def test_invalid_or_exceeded_stream_limits_fail_before_publication(field,value):
    doc,root,model=setup();options=q.PixelStreamOptions();options.block_size=16;setattr(options,field,value);revision=doc.revision
    with pytest.raises((ValueError,RuntimeError)):q.PixelPredictionStream(doc,root,model,options)
    assert doc.revision==revision and doc.pixel_classifier_record_count==0


def test_cancel_before_start_and_between_blocks_never_advances_partial_counts():
    doc,root,model=setup();options=q.PixelStreamOptions();options.block_size=16
    stream=q.PixelPredictionStream(doc,root,model,options);stream.cancel()
    with pytest.raises(RuntimeError,match='cancelled'):stream.next()
    assert stream.state=='cancelled' and stream.completed_blocks==stream.valid_pixels==0
    stream=q.PixelPredictionStream(doc,root,model,options);block=next(stream);counts=stream.class_counts;completed=stream.completed_blocks
    stream.cancel()
    with pytest.raises(RuntimeError,match='cancelled'):next(stream)
    assert completed==stream.completed_blocks==1 and stream.class_counts==counts and block.width==16


def test_stream_blocks_remain_values_after_edits_and_never_gain_publication_authority():
    doc,root,model=setup();options=q.PixelStreamOptions();options.block_size=16
    stream=q.PixelPredictionStream(doc,root,model,options);first=next(stream);original=first.roi.wkt
    doc.update_roi(root,q.Roi.rectangle(q.Rect(1,1,2,2)));revision=doc.revision
    assert not stream.matches(doc)
    second=next(stream);assert second.roi.wkt==original and second.provenance.document_revision<revision
    # Numerical production continues from the captured ROI; caller decides whether
    # to cancel obsolete work. No live request is returned by this producer.
    assert stream.state=='ready' and doc.revision==revision and doc.object_count==3
    if q.has_geos:
        fresh=q.PixelPredictionRequest(doc,root,model)
        with pytest.raises(RuntimeError):q.publish_pixel_contours(doc,fresh,q.trace_pixel_prediction(first))
    assert doc.revision==revision


def test_source_close_is_terminal_failure_and_arrays_outlive_stream_and_document():
    doc,root,model=setup();options=q.PixelStreamOptions();options.block_size=16
    stream=q.PixelPredictionStream(doc,root,model,options);block=next(stream);view=block.labels_numpy();saved=view.copy()
    doc.server.close()
    with pytest.raises(RuntimeError,match='closed'):stream.next()
    assert stream.state=='failed' and stream.completed_blocks==1 and not stream.matches(doc)
    with pytest.raises(RuntimeError,match='failed'):stream.next()
    del stream,block,doc,model;gc.collect();np.testing.assert_array_equal(view,saved)


def test_large_full_target_streams_within_budget_that_rejects_full_mask():
    _,_,model=setup();doc=q.Document(q.Image.from_numpy(np.full((1024,1024,3),80,dtype=np.uint8)));root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1024,1024)))
    options=q.PixelStreamOptions();options.block_size=64;p=q.PixelPredictionOptions();p.work_bytes=8*1024*1024;options.prediction=p
    with pytest.raises((ValueError,RuntimeError),match='memory budget'):q.PixelPredictionRequest(doc,root,model,p)
    stream=q.PixelPredictionStream(doc,root,model,options);assert stream.total_blocks==256 and stream.estimated_work_bytes<=p.work_bytes
    for block in stream:assert block.width==block.height==64
    assert stream.completed_blocks==256 and stream.valid_pixels==1024*1024 and sum(stream.class_counts)==stream.classified_pixels


def test_input_options_and_window_are_captured_by_value():
    doc,root,model=setup();options=q.PixelStreamOptions();options.block_size=16;window=q.Rect(17,11,28,21)
    stream=q.PixelPredictionStream(doc,root,model,options,window);captured=bounds(stream.level_bounds)
    options.block_size=512;window.x=50;window.width=1
    assert bounds(stream.level_bounds)==captured and next(stream).width==16
