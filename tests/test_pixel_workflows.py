"""Native labelled-ROI sampling, source prediction, resolution and controller authority."""
import gc
from concurrent.futures import ThreadPoolExecutor
import numpy as np
import pytest
import qupathxx as q


def scene():
    y,x=np.indices((48,64));rgb=np.empty((48,64,3),dtype=np.uint8)
    rgb[:,:,0]=np.where(x<32,0,200);rgb[:,:,1]=y;rgb[:,:,2]=17
    doc=q.Document(q.Image.from_numpy(rgb))
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,32,48)));b=doc.add_roi(q.Roi.rectangle(q.Rect(32,0,32,48)))
    doc.classify_objects([a],'A');doc.classify_objects([b],'B')
    options=q.PixelTrainingOptions();f=q.PixelFeatureOptions();f.sigmas=[0,1];options.features=f
    options.tile_size=16;options.max_samples=120;options.max_samples_per_class=60
    return doc,a,b,options


def test_sampling_is_exact_balanced_and_independent_of_tile_and_selection_order():
    doc,a,b,options=scene();doc.set_calibration(.5,.25)
    request=q.PixelTrainingRequest(doc,[b,a],options=options);table=q.sample_pixel_training(request)
    assert request.matches(doc) and request.resolution.physical and request.resolution.pixel_width_um==.5
    assert table.classes==['A','B'] and table.sampled_class_counts==[60,60] and table.eligible_class_pixels==[1536,1536]
    assert table.ambiguous_pixels==0 and table.row_count==120 and table.estimated_work_bytes<=options.work_bytes
    coords=table.coordinates_numpy();values=table.numpy();labels=table.labels_numpy();ids=table.annotation_ids_numpy()
    assert coords.dtype==np.int64 and values.dtype==np.float32 and labels.dtype==np.uint32 and ids.dtype==np.uint64
    np.testing.assert_array_equal(values[:,0],np.where(coords[:,0]<32,0,200));np.testing.assert_array_equal(values[:,2],coords[:,1])
    np.testing.assert_array_equal(labels,coords[:,0]>=32);np.testing.assert_array_equal(ids,np.where(coords[:,0]<32,a,b))
    options.tile_size=32;retiled=q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options))
    assert retiled.sha256==table.sha256 and retiled.input_sha256!=table.input_sha256
    original=table.sha256;options.seed=19
    assert q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options)).sha256!=original
    assert request.options.seed==1 and table.options.tile_size==16
    for array in [coords,values,labels,ids]:
        assert not array.flags.writeable
        with pytest.raises(ValueError):array.flat[0]=0
    writable=table.numpy(copy=True);writable[:]=999;assert table.numpy()[0,0]!=999
    saved=values.copy();del table,request,doc;gc.collect();np.testing.assert_array_equal(values,saved)


def test_same_class_overlap_is_deduplicated_and_conflicts_excluded():
    doc,a,b,options=scene();same=doc.add_roi(q.Roi.rectangle(q.Rect(1,1,10,10)));doc.classify_objects([same],'A')
    base=q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options))
    duplicate=q.sample_pixel_training(q.PixelTrainingRequest(doc,[same,b,a],options=options))
    assert duplicate.sha256==base.sha256 and duplicate.eligible_class_pixels==base.eligible_class_pixels
    cross=doc.add_roi(q.Roi.rectangle(q.Rect(25,0,14,48)));doc.classify_objects([cross],'B')
    conflict=q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b,cross],options=options))
    assert conflict.ambiguous_pixels==7*48 and conflict.eligible_class_pixels==[25*48,32*48]
    assert (conflict.coordinates_numpy()[conflict.labels_numpy()==0,0]<25).all()
    entire=doc.add_roi(q.Roi.rectangle(q.Rect(25,0,14,48)));doc.classify_objects([entire],'C')
    with pytest.raises(ValueError,match='two unambiguous'):q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b,entire],options=options))


def test_class_balance_uses_smallest_usable_class_and_optional_caps():
    doc,a,b,options=scene();doc.update_roi(b,q.Roi.rectangle(q.Rect(32,0,2,2)))
    balanced=q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options))
    assert balanced.sampled_class_counts==[4,4]
    options.balanced=False;unbalanced=q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options))
    assert unbalanced.sampled_class_counts==[60,4]


@pytest.mark.parametrize('change', ['empty','duplicate','unlabelled','nonarea','outside','classes','level','scan','budget'])
def test_capture_and_predecode_limits(change):
    doc,a,b,options=scene();ids=[a,b]
    if change=='empty':ids=[]
    elif change=='duplicate':ids=[a,a,b]
    elif change=='unlabelled':doc.classify_objects([a],'')
    elif change=='nonarea':doc.update_roi(a,q.Roi.points([q.Point(1,1)]))
    elif change=='outside':
        with pytest.raises(ValueError,match='inside'):doc.update_roi(a,q.Roi.rectangle(q.Rect(-1,0,32,48)))
        return
    elif change=='classes':doc.classify_objects([b],'A')
    elif change=='level':options.level=1
    elif change=='scan':options.max_scanned_pixels=1
    elif change=='budget':options.work_bytes=1024*1024;options.max_samples=100000;options.max_samples_per_class=50000
    with pytest.raises((ValueError,RuntimeError,IndexError)):
        q.sample_pixel_training(q.PixelTrainingRequest(doc,ids,options=options))


@pytest.mark.skipif(not q.has_classifier_training,reason='optional classifier training backend')
def test_train_save_reopen_source_prediction_and_stale_controllers(tmp_path):
    doc,a,b,options=scene();doc.set_calibration(.5,.25)
    request=q.PixelTrainingRequest(doc,[a,b],options=options);parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    training=q.PixelTrainingJob();assert training.model is None and training.table is None
    assert training.train(request,parameters);status=training.wait(doc);assert status.state=='ready',status.error
    assert status.phase=='finished' and status.completed_tiles==status.total_tiles>0
    model=training.model;table=training.table;assert model is not None and table is not None
    assert model.resolution.physical and model.resolution.pixel_width_um==.5 and model.to_bytes()[3]==2
    path=str(tmp_path/'annotations.qpc');q.save_pixel_classifier(model,path);portable=q.load_pixel_classifier(path)
    assert portable.to_bytes()==model.to_bytes() and q.pixel_classifier_level(doc,portable)==0
    root=doc.add_roi(q.Roi.ellipse(q.Rect(3,4,57,39)));prediction=q.PixelPredictionOptions();prediction.tile_size=16
    region=q.PixelPredictionRequest(doc,root,portable,prediction);job=q.PixelPredictionJob();assert job.result is None and job.request(region)
    status=job.wait(doc);assert status.state=='ready',status.error
    result=job.result;assert result is not None and result.estimated_work_bytes<=prediction.work_bytes
    assert status.completed_tiles==region.total_tiles and result.model.sha256==model.sha256
    bounds=result.level_bounds;yy,xx=np.indices((result.height,result.width));gx=xx+int(bounds.x);gy=yy+int(bounds.y)
    mask=np.array([[doc.annotation(root).geometry.contains(q.Point(x+.5,y+.5)) for x in row] for row,y in zip(gx,gy[:,0])])
    expected=np.where(gx<32,1,2).astype(np.uint32);expected[~mask]=0
    np.testing.assert_array_equal(result.labels_numpy(),expected);np.testing.assert_array_equal(result.valid_numpy(),mask)
    assert np.isnan(result.vote_fractions_numpy()[~mask]).all()
    prediction.tile_size=32;retiled=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,portable,prediction))
    assert retiled.sha256==result.sha256 and retiled.input_sha256!=result.input_sha256
    labels=result.labels_numpy();saved=labels.copy();doc.rename_annotation(root,'Changed')
    assert not region.matches(doc) and job.poll(doc).state=='stale' and job.result is None
    assert training.poll(doc).state=='stale' and training.model is None and training.table is None
    del result,retiled,job,doc;gc.collect();np.testing.assert_array_equal(labels,saved)


@pytest.mark.skipif(not q.has_classifier_training,reason='optional classifier training backend')
def test_sampling_resolution_is_an_explicit_model_contract():
    doc,a,b,options=scene();doc.set_calibration(.5,.25)
    table=q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options));parameters=q.ForestParameters();parameters.trees=4;parameters.max_depth=3
    model=q.train_pixel_classifier(table,parameters);assert q.pixel_classifier_level(doc,model)==0
    detached=q.PixelClassifier(model.forest,model.options,model.transform);assert detached.resolution is None and detached.to_bytes()[3]==1
    with pytest.raises(ValueError,match='no source sampling'):q.pixel_classifier_level(doc,detached)
    doc.set_calibration(.6,.25)
    with pytest.raises(ValueError,match='no natural'):q.pixel_classifier_level(doc,model)
    declared=q.PixelResolution();declared.downsample=2;declared.pixel_width_um=1;declared.pixel_height_um=.5
    physical=q.PixelClassifier(model.forest,model.options,model.transform,declared)
    doc.set_calibration(1,.5);assert q.pixel_classifier_level(doc,physical)==0
    doc.set_calibration(.6,.25)
    with pytest.raises(ValueError,match='no natural'):q.pixel_classifier_level(doc,physical)
    options.calibrated=False;raw=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options)),parameters)
    assert not raw.resolution.physical and q.pixel_classifier_level(doc,raw)==0
    doc.clear_calibration();assert q.pixel_classifier_level(doc,raw)==0
    bad=q.PixelResolution();bad.pixel_width_um=.5
    with pytest.raises(ValueError):bad.validate()


@pytest.mark.skipif(not q.has_classifier_training,reason='optional classifier training backend')
def test_workers_cancel_and_reuse_after_completion():
    doc,a,b,options=scene();parameters=q.ForestParameters();parameters.trees=4;parameters.max_depth=3
    job=q.PixelTrainingJob();job.cancel();assert job.train(q.PixelTrainingRequest(doc,[a,b],options=options),parameters)
    job.cancel();assert job.wait(doc).state=='cancelled' and job.model is None
    assert job.train(q.PixelTrainingRequest(doc,[a,b],options=options),parameters);assert job.wait(doc).state=='ready'
    region=q.PixelPredictionRequest(doc,a,job.model);prediction=q.PixelPredictionJob();assert prediction.request(region)
    prediction.cancel();assert prediction.wait(doc).state=='cancelled' and prediction.result is None
    assert prediction.request(region);assert prediction.wait(doc).state=='ready'
    doc.server.close();assert prediction.poll(doc).state=='stale' and prediction.result is None


def test_snapshot_worker_sampling_is_safe_with_concurrent_native_calls():
    doc,a,b,options=scene();request=q.PixelTrainingRequest(doc,[a,b],options=options)
    with ThreadPoolExecutor(2) as pool:tables=list(pool.map(lambda _:q.sample_pixel_training(request),range(4)))
    assert len({t.sha256 for t in tables})==1


@pytest.mark.skipif(not q.has_openslide or not q.has_classifier_training,reason='optional reader/training backend')
def test_real_aperio_sampling_and_tiled_source_prediction_match_direct_context():
    import os
    path=os.environ.get('QUPATHXX_APERIO_FIXTURE')
    if not path:pytest.skip('set QUPATHXX_APERIO_FIXTURE for identified reader fixture')
    server=q.open_slide(path);doc=q.Document(server)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,32,48)));b=doc.add_roi(q.Roi.rectangle(q.Rect(32,0,32,48)))
    doc.classify_objects([a],'left');doc.classify_objects([b],'right')
    options=q.PixelTrainingOptions();options.tile_size=16;options.max_samples=80;options.max_samples_per_class=40
    f=q.PixelFeatureOptions();f.sigmas=[0,1];options.features=f
    parameters=q.ForestParameters();parameters.trees=4;parameters.max_depth=3
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=options)),parameters)
    target=doc.add_roi(q.Roi.rectangle(q.Rect(7,9,41,29)));prediction=q.PixelPredictionOptions();prediction.tile_size=16
    result=q.predict_pixel_region(q.PixelPredictionRequest(doc,target,model,prediction))
    halo=f.halo;left=7-halo;top=9-halo;width=41+2*halo;height=29+2*halo
    pixels=server.read_region(q.RegionRequest(left,top,0,width,height))
    direct=q.classify_pixels(pixels,model,window=q.PixelFeatureWindow(server.metadata.width,server.metadata.height,7,9,41,29,left,top))
    np.testing.assert_array_equal(result.labels_numpy(),direct.labels_numpy())
    np.testing.assert_array_equal(result.vote_fractions_numpy(),direct.vote_fractions_numpy())
    assert result.provenance.reader=='openslide' and result.options.tile_size==16
    server.close()


@pytest.mark.skipif(not q.has_classifier_training,reason='optional classifier training backend')
def test_default_three_class_pixel_training_fits_default_work_budget():
    rgb=np.empty((96,192,3),dtype=np.uint8)
    rgb[:,:64]=[220,20,10];rgb[:,64:128]=[10,220,20];rgb[:,128:]=[20,10,220]
    doc=q.Document(q.Image.from_numpy(rgb));ids=[]
    for index,name in enumerate(['A','B','C']):
        object_id=doc.add_roi(q.Roi.rectangle(q.Rect(index*64,0,64,96)))
        doc.classify_objects([object_id],name);ids.append(object_id)
    preset=q.pixel_forest_parameters();assert preset.trees==32 and preset.max_depth==8
    job=q.PixelTrainingJob();assert job.train(q.PixelTrainingRequest(doc,ids));status=job.wait(doc)
    assert status.state=='ready',status.error
    assert job.table.row_count==6000 and job.table.sampled_class_counts==[2000,2000,2000]
    assert job.model.forest.parameters.trees==32 and job.model.forest.parameters.max_depth==8
    assert q.train_pixel_classifier(job.table).forest.parameters.max_depth==8
