"""Bounded natural-grid prediction windows, exact halos and durable cropped masks."""
import hashlib
import os
from pathlib import Path
import struct
import numpy as np
import pytest
import qupathxx as q
from test_pixel_workflows import scene

pytestmark=pytest.mark.skipif(not q.has_classifier_training,reason='optional training backend')


def setup():
    doc,a,b,options=scene();features=options.features;features.gradient_magnitude=True;features.laplacian=True;options.features=features
    params=q.pixel_forest_parameters();params.trees=8;params.max_depth=4
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options)),params)
    root=doc.add_roi(q.Roi.ellipse(q.Rect(3,4,57,39)))
    return doc,root,model


@pytest.mark.parametrize('bounds',[(17.25,11.2,28.5,20.7),(-20,-20,43,50),(-20,-20,200,200),(3,4,17,16)])
@pytest.mark.parametrize('tile_size',[16,32])
def test_window_is_exact_crop_of_full_mask_with_identical_feature_context(bounds,tile_size):
    doc,root,model=setup();options=q.PixelPredictionOptions();options.tile_size=tile_size
    full_request=q.PixelPredictionRequest(doc,root,model,options);full=q.predict_pixel_region(full_request)
    request=q.PixelPredictionRequest(doc,root,model,options,window=q.Rect(*bounds));result=q.predict_pixel_region(request)
    assert request.windowed and result.windowed and not full_request.windowed and not full.windowed
    assert result.matches(request) and not result.matches(full_request) and request.matches(doc)
    assert result.roi.wkt==full.roi.wkt and result.object_id==root
    f=full.level_bounds;r=result.level_bounds;x=int(r.x-f.x);y=int(r.y-f.y)
    for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:
        actual=getattr(result,method)();expected=getattr(full,method)()[y:y+result.height,x:x+result.width]
        np.testing.assert_array_equal(actual,expected);assert not actual.flags.writeable
    assert result.class_counts==[int(np.count_nonzero(result.labels_numpy()==i+1)) for i in range(len(model.classes))]
    assert result.estimated_work_bytes<=options.work_bytes and request.total_tiles<=full_request.total_tiles
    if result.width==full.width and result.height==full.height:assert result.sha256==full.sha256


@pytest.mark.parametrize('bounds',[(0,0,0,1),(0,0,1,-1),(100,100,2,2),(float('nan'),0,1,1),(0,0,float('inf'),1),(1e308,0,1e308,1),(1e20,0,1,1),(10.1,10.1,.01,.01)])
def test_invalid_or_empty_center_window_does_not_mutate_document(bounds):
    doc,root,model=setup();revision=doc.revision
    with pytest.raises(ValueError):q.PixelPredictionRequest(doc,root,model,window=q.Rect(*bounds))
    assert doc.revision==revision and doc.pixel_classifier_record_count==0


def test_large_target_fits_small_window_budget_without_allocating_full_mask():
    _,_,model=setup();doc=q.Document(q.Image.from_numpy(np.full((2048,2048,3),80,dtype=np.uint8)))
    root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,2048,2048)));options=q.PixelPredictionOptions();options.work_bytes=8*1024*1024
    with pytest.raises((ValueError,RuntimeError),match='memory budget'):q.PixelPredictionRequest(doc,root,model,options)
    request=q.PixelPredictionRequest(doc,root,model,options,window=q.Rect(1000,900,64,64))
    result=q.predict_pixel_region(request)
    assert (result.width,result.height)==(64,64) and result.estimated_work_bytes<=options.work_bytes
    assert request.total_tiles==1 and result.valid_pixels==4096 and result.roi.area==2048*2048


def test_window_archive_roundtrip_and_old_marker_cannot_reinterpret_cropped_grid():
    doc,root,model=setup();request=q.PixelPredictionRequest(doc,root,model,window=q.Rect(17,11,28,21));result=q.predict_pixel_region(request)
    archive=q.archive_pixel_prediction(result);encoded=archive.to_bytes();assert encoded[:4]==b'QPA\x02'
    loaded=q.PixelClassifierArchive.from_bytes(encoded)
    assert loaded.result.windowed and loaded.to_bytes()==encoded and q.archive_pixel_prediction(loaded.result).to_bytes()==encoded
    assert loaded.result.sha256==result.sha256 and not loaded.result.matches(request)
    assert loaded.annotations[0].geometry.wkt==doc.annotation(root).geometry.wkt
    older=bytearray(encoded);older[3]=1;older[-64:]=hashlib.sha256(older[:-64]).hexdigest().encode()
    with pytest.raises(ValueError,match='grid/ROI'):q.PixelClassifierArchive.from_bytes(bytes(older))
    full=q.archive_pixel_prediction(q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model)))
    assert full.to_bytes()[:4]==b'QPA\x01' and not q.PixelClassifierArchive.from_bytes(full.to_bytes()).result.windowed


def test_window_encoding_rejects_training_marker_and_grid_outside_original_target():
    training=bytearray((Path(__file__).parent/'data/pixel-archives/training-v1-windows.qpa').read_bytes());training[3]=2
    training[-64:]=hashlib.sha256(training[:-64]).hexdigest().encode()
    with pytest.raises(ValueError,match='cannot contain a training cohort'):q.PixelClassifierArchive.from_bytes(bytes(training))
    doc,root,model=setup();result=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,window=q.Rect(17,11,28,21)))
    encoded=bytearray(q.archive_pixel_prediction(result).to_bytes())
    def blob_end(at):return at+8+struct.unpack_from('<Q',encoded,at)[0]
    at=blob_end(12);at=blob_end(at);at+=8+24;at=blob_end(at);at=blob_end(at) # annotations count, identity, class, geometry
    struct.pack_into('<Q',encoded,at+8,0) # x=0 is inside source but outside original ROI's sampled bounds.
    encoded[-64:]=hashlib.sha256(encoded[:-64]).hexdigest().encode()
    with pytest.raises(ValueError,match='grid/ROI'):q.PixelClassifierArchive.from_bytes(bytes(encoded))


@pytest.mark.skipif(not q.has_project,reason='optional project backend')
def test_project_roundtrip_retains_window_and_original_target(tmp_path):
    doc,root,model=setup();request=q.PixelPredictionRequest(doc,root,model,window=q.Rect(17,11,28,21));result=q.predict_pixel_region(request)
    record=doc.keep_pixel_prediction(request,q.archive_pixel_prediction(result));original=doc.annotation(root).geometry.wkt
    doc.remove_objects([root]);project=q.Project();image=project.add_document(doc);path=tmp_path/'window.qpxproj';project.save(str(path))
    loaded=q.Project.open(str(path)).document(image).pixel_classifier_record(record)
    assert loaded.archive.result.windowed and loaded.archive.result.sha256==result.sha256
    assert loaded.archive.annotations[0].geometry.wkt==original
    np.testing.assert_array_equal(loaded.archive.result.labels_numpy(),result.labels_numpy())


@pytest.mark.skipif(not q.has_geos,reason='optional GEOS backend')
def test_window_contours_publish_only_current_cropped_mask_and_undo_atomically():
    doc,root,model=setup();window=q.Rect(17,11,28,21);request=q.PixelPredictionRequest(doc,root,model,window=window)
    result=q.predict_pixel_region(request);traced=q.trace_pixel_prediction(result);doc.clear_history();before=doc.object_count
    ids=q.publish_pixel_contours(doc,request,traced)
    assert len(ids)==2 and doc.object_count==before+2 and doc.children(root)==ids
    for obj in ids:
        bounds=doc.annotation(obj).geometry.bounds;assert bounds.x>=window.x and bounds.y>=window.y
        assert bounds.x+bounds.width<=window.x+window.width and bounds.y+bounds.height<=window.y+window.height
    assert doc.undo() and doc.object_count==before and doc.redo() and doc.children(root)==ids


def test_aperio_window_uses_source_context_and_matches_full_target_crop():
    path=os.environ.get('QUPATHXX_APERIO_FIXTURE')
    if not q.has_openslide or not path or not Path(path).is_file():pytest.skip('requires identified Aperio fixture')
    server=q.open_slide(path);doc=q.Document(server);width=server.metadata.width;height=server.metadata.height
    x=width//2-128;y=height//2-128
    a=doc.add_roi(q.Roi.rectangle(q.Rect(x,y,64,64)));b=doc.add_roi(q.Roi.rectangle(q.Rect(x+128,y+128,64,64)))
    doc.classify_objects([a],'A');doc.classify_objects([b],'B')
    options=q.PixelTrainingOptions();options.calibrated=False;options.max_samples=64;options.max_samples_per_class=32
    features=options.features;features.sigmas=[0,1];features.gradient_magnitude=True;options.features=features
    parameters=q.pixel_forest_parameters();parameters.trees=8;parameters.max_depth=4
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options)),parameters)
    root=doc.add_roi(q.Roi.rectangle(q.Rect(x,y,256,256)));full=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model))
    result=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,window=q.Rect(x+71,y+61,64,64)))
    assert result.windowed and result.width==result.height==64
    for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:
        np.testing.assert_array_equal(getattr(result,method)(),getattr(full,method)()[61:125,71:135])
