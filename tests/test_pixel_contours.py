"""Exact shared-edge tracing and authority-checked, atomic class publication."""
import gc
import numpy as np
import pytest
import qupathxx as q

pytestmark=pytest.mark.skipif(not (q.has_classifier_training and q.has_geos),reason="optional training and GEOS backends")

@pytest.fixture(scope="module")
def model():
    rgb=np.zeros((8,16,3),dtype=np.uint8);rgb[:,:8]=[240,20,10];rgb[:,8:]=[10,20,240]
    d=q.Document(q.Image.from_numpy(rgb));a=d.add_rectangle(q.Rect(0,0,8,8));b=d.add_rectangle(q.Rect(8,0,8,8))
    d.classify_objects([a],"red");d.classify_objects([b],"blue")
    options=q.PixelTrainingOptions();f=q.PixelFeatureOptions();f.sigmas=[0];options.features=f;options.max_samples=64;options.max_samples_per_class=32
    p=q.pixel_forest_parameters();p.trees=8;p.max_depth=3
    return q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(d,[a,b],options=options)),p)

def prediction(model,shape="nested"):
    y,x=np.indices((23,29))
    if shape=="diagonal": mask=(x+y)%2==0
    else:
        mask=np.zeros((23,29),dtype=bool);mask[2:21,2:27]=True;mask[6:17,6:23]=False;mask[9:14,10:19]=True
    rgb=np.where(mask[:,:,None],np.array([240,20,10],dtype=np.uint8),np.array([10,20,240],dtype=np.uint8))
    doc=q.Document(q.Image.from_numpy(rgb));root=doc.add_rectangle(q.Rect(0,0,29,23))
    request=q.PixelPredictionRequest(doc,root,model);result=q.predict_pixel_region(request)
    return doc,root,request,result

@pytest.mark.parametrize("shape",["nested","diagonal"])
def test_exact_raster_geometry_and_shared_boundaries(model,shape):
    doc,root,request,result=prediction(model,shape);revision=doc.revision
    traced=q.trace_pixel_prediction(result);contours=traced.contours;labels=result.labels_numpy()
    assert traced.prediction_sha256==result.sha256==contours.input_sha256
    assert contours.object_id==root and contours.component_count==2 and result.matches(request)
    assert contours.retained_bytes<=contours.estimated_work_bytes<=contours.options.work_bytes
    for c in contours.components:
        expected=labels==c.label
        assert c.pixels==int(expected.sum()) and c.geometry.area==c.pixels
        for y,x in np.ndindex(labels.shape):
            assert c.geometry.contains(q.Point(x+.5,y+.5))==bool(expected[y,x])
    assert doc.revision==revision and doc.object_count==1
    del doc,request,result,traced;gc.collect()
    assert sum(c.geometry.area for c in contours.components)==23*29


def test_publication_is_one_undoable_classified_child_batch(model):
    doc,root,request,result=prediction(model);traced=q.trace_pixel_prediction(result);doc.clear_history()
    ids=q.publish_pixel_contours(doc,request,traced,name_prefix="Prediction")
    assert len(ids)==2 and doc.children(root)==ids and doc.undo_label=="Add pixel classifier annotations"
    for obj_id,c in zip(ids,traced.contours.components):
        a=doc.annotation(obj_id);name=model.classes[c.label-1]
        assert a.kind==q.ObjectKind.ANNOTATION and a.parent_id==root and a.class_name==name and a.name=="Prediction "+name
        assert a.geometry.area==c.geometry.area
    with pytest.raises(RuntimeError,match="no longer match"):q.publish_pixel_contours(doc,request,traced)
    assert doc.undo() and doc.object_count==1 and not doc.classes and not doc.can_undo
    assert doc.redo() and doc.children(root)==ids

@pytest.mark.parametrize("change",["edit","undo","close","document","request"])
def test_stale_or_equivalent_but_unrelated_requests_cannot_publish(model,change):
    doc,root,request,result=prediction(model);traced=q.trace_pixel_prediction(result)
    if change in ("edit","undo"):
        doc.rename_annotation(root,"Changed")
        if change=="undo":doc.undo()
    elif change=="close":doc.server.close()
    elif change=="document":
        doc=q.Document(doc.server);assert doc.add_rectangle(q.Rect(0,0,29,23))==root
    else:
        request=q.PixelPredictionRequest(doc,root,model)
        assert request.matches(doc) and not result.matches(request)
    count,revision=doc.object_count,doc.revision
    with pytest.raises(RuntimeError,match="no longer match"):q.publish_pixel_contours(doc,request,traced)
    assert doc.object_count==count and doc.revision==revision

@pytest.mark.parametrize("limit",["max_components","max_edges","max_vertices","work_bytes"])
def test_limits_fail_without_document_mutation(model,limit):
    doc,_,_,result=prediction(model,"diagonal");options=q.TissueContourOptions();setattr(options,limit,1)
    revision=doc.revision
    with pytest.raises((RuntimeError,ValueError)):q.trace_pixel_prediction(result,options)
    assert doc.revision==revision and doc.object_count==1


def test_fractional_clip_root_publication_and_absent_class(model):
    doc,root,_,_=prediction(model)
    doc.update_roi(root,q.Roi.rectangle(q.Rect(3.2,3.1,4.5,3.6)))
    request=q.PixelPredictionRequest(doc,root,model);result=q.predict_pixel_region(request)
    options=q.TissueContourOptions();options.clip_to_roi=False
    raw=q.trace_pixel_prediction(result,options)
    assert sum(c.pixels for c in raw.contours.components)==result.classified_pixels
    ids=q.publish_pixel_contours(doc,request,raw)
    assert all(doc.annotation(i).parent_id==0 for i in ids)
    request=q.PixelPredictionRequest(doc,root,model);clipped=q.trace_pixel_prediction(q.predict_pixel_region(request))
    assert all(doc.annotation(root).geometry.covers(c.geometry) for c in clipped.contours.components)
    assert 1<=clipped.contours.component_count<=2


def test_empty_mask_publication_is_a_noop(model):
    doc,root,_,_=prediction(model)
    shell=[q.Point(3,3),q.Point(6,3),q.Point(6,6),q.Point(3,6)]
    hole=[q.Point(3.1,3.1),q.Point(3.1,5.9),q.Point(5.9,5.9),q.Point(5.9,3.1)]
    doc.update_roi(root,q.Roi.multipolygon([q.PolygonPart(shell,[hole])]))
    request=q.PixelPredictionRequest(doc,root,model);result=q.predict_pixel_region(request)
    assert result.classified_pixels==0
    traced=q.trace_pixel_prediction(result);assert traced.contours.component_count==0
    doc.clear_history();revision=doc.revision
    assert q.publish_pixel_contours(doc,request,traced)==[] and doc.revision==revision and not doc.can_undo


@pytest.mark.skipif(not q.has_project,reason="optional SQLite project backend")
def test_published_geometry_classes_and_parent_survive_project_roundtrip(model,tmp_path):
    doc,root,request,result=prediction(model)
    ids=q.publish_pixel_contours(doc,request,q.trace_pixel_prediction(result))
    project=q.Project();image=project.add_document(doc);path=str(tmp_path/"pixel-annotations.qpxproj")
    project.save(path);restored=q.Project.open(path).document(image)
    assert restored.children(root)==ids and restored.object_count==doc.object_count
    for obj_id in ids:
        before,after=doc.annotation(obj_id),restored.annotation(obj_id)
        assert after.class_name==before.class_name and after.geometry.wkt==before.geometry.wkt and after.parent_id==root
    with pytest.raises(RuntimeError,match="no longer match"):q.publish_pixel_contours(restored,request,q.trace_pixel_prediction(result))


def test_parallel_tracing_uses_independent_geometry_contexts(model):
    from concurrent.futures import ThreadPoolExecutor
    _,_,_,result=prediction(model,"diagonal")
    with ThreadPoolExecutor(2) as pool:
        traces=list(pool.map(lambda _:q.trace_pixel_prediction(result),range(4)))
    assert all(t.prediction_sha256==result.sha256 for t in traces)
    assert all([c.pixels for c in t.contours.components]==result.class_counts for t in traces)
