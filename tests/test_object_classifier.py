import gc
import numpy as np
import pytest
import qupathxx as q


def workspace():
    doc = q.Document(q.Image(512, 512, [80]*512*512*3))
    ids = []
    for i, side in enumerate([2, 3, 4, 20, 21, 22, 3.5, 21.5]):
        label = 'small' if i < 3 else 'large' if i < 6 else ''
        ids.append(doc.add_cell(q.Roi.rectangle(q.Rect(i*50, 10, side, side)),
                                q.Roi.rectangle(q.Rect(i*50+side/4, 10+side/4, side/2, side/2)),
                                f'Cell {i}', class_name=label))
    return doc, ids


def parameters():
    p = q.ForestParameters()
    p.trees, p.max_depth = 24, 4
    return p


def trained(doc, ids):
    if not q.has_classifier_training:
        pytest.skip('optional classifier training backend')
    request = q.ObjectFeatureRequest(doc, ids)
    return q.train_object_forest(q.extract_object_features(request), parameters())


def test_shape_features_match_anisotropic_formula_and_numpy_ownership():
    doc, ids = workspace()
    doc.set_calibration(2, .5)
    options = q.ObjectFeatureOptions()
    options.nucleus_shape = True
    request = q.ObjectFeatureRequest(doc, ids[::-1], options)
    table = q.extract_object_features(request)
    assert table.object_kinds == [q.ObjectKind.CELL]*8
    assert table.object_ids == ids
    assert [f.unit for f in table.features] == ['um2', 'um', 'um', 'um', '1']*2
    side = np.array([2, 3, 4, 20, 21, 22, 3.5, 21.5])
    expected = np.column_stack([side**2, 5*side, 2*side, .5*side, np.full(8, 4*np.pi/25),
                                side**2/4, 2.5*side, side, side/4, np.full(8, 4*np.pi/25)])
    view = table.numpy()
    np.testing.assert_allclose(view, expected, rtol=1e-6)
    assert view.dtype == np.float32 and not view.flags.writeable
    assert table.sha256 == q.extract_object_features(q.ObjectFeatureRequest(doc, ids, options)).sha256
    features = table.features
    features[0].unit = 'changed'
    assert table.features[0].unit == 'um2'
    metadata = table.options
    metadata.calibrated = False
    assert table.options.calibrated
    independent = table.numpy(copy=True)
    independent[0, 0] = 0
    assert view[0, 0] == 4
    doc.rename_annotation(ids[0], 'Changed after snapshot')
    assert not request.matches(doc)
    del request, table, doc
    gc.collect()
    np.testing.assert_allclose(view, expected, rtol=1e-6)


def test_snapshot_validation_and_units():
    doc, ids = workspace()
    with pytest.raises(ValueError, match='duplicate'):
        q.ObjectFeatureRequest(doc, ids+[ids[0]])
    with pytest.raises(ValueError):
        q.ObjectFeatureRequest(doc, [])
    with pytest.raises(IndexError):
        q.ObjectFeatureRequest(doc, [99999])
    options = q.ObjectFeatureOptions()
    options.cytoplasm_rgb = True
    with pytest.raises(ValueError):
        q.ObjectFeatureRequest(doc, ids, options)
    options.cytoplasm_rgb, options.cell_rgb = False, True
    with pytest.raises(ValueError, match='verified'):
        q.ObjectFeatureRequest(doc, ids, options)
    raw = q.ObjectFeatureRequest(doc, ids)
    doc.set_calibration(.5, .25)
    assert not raw.matches(doc)
    options = q.ObjectFeatureOptions()
    options.calibrated = False
    assert q.extract_object_features(q.ObjectFeatureRequest(doc, ids, options)).features[0].unit == 'px2'


def test_training_prediction_atomic_apply_undo_redo_and_reload(tmp_path):
    doc, ids = workspace()
    model = trained(doc, ids)
    assert model.training_rows == 6 and model.classes == ['large', 'small']
    inferred = q.ObjectFeatureOptions.from_model(model)
    assert not inferred.calibrated and not inferred.nucleus_shape and not inferred.cell_rgb
    path = tmp_path/'model.qpxforest'
    q.save_forest_model(model, str(path))
    loaded = q.load_forest_model(str(path))
    assert loaded.sha256 == model.sha256
    request = q.ObjectFeatureRequest(doc, ids[-2:])
    result = q.classify_objects(request, loaded)
    assert result.model_sha256 == model.sha256
    original = [doc.annotation(i) for i in ids[-2:]]
    doc.clear_history()
    revision = doc.revision
    assert q.apply_object_classification(doc, request, result) == ids[-2:]
    assert doc.revision == revision+1 and doc.undo_label == 'Apply object classifier'
    assert [doc.annotation(i).class_name for i in ids[-2:]] == ['small', 'large']
    for before, obj in zip(original, ids[-2:]):
        after = doc.annotation(obj)
        assert after.name == before.name and after.parent_id == before.parent_id
        assert after.geometry.equals(before.geometry) and after.nucleus.equals(before.nucleus)
    assert doc.undo() and all(not doc.annotation(i).class_name for i in ids[-2:])
    assert doc.redo()
    with pytest.raises(ValueError, match='stale'):
        q.apply_object_classification(doc, request, result)
    if q.has_project:
        project = q.Project()
        image = project.add_document(doc)
        project.save(str(tmp_path/'classified.qpxproj'))
        restored = q.Project.open(str(tmp_path/'classified.qpxproj')).document(image)
        assert [restored.annotation(i).class_name for i in ids[-2:]] == ['small', 'large']
        assert not request.matches(restored)


def test_job_no_backlog_cancellation_staleness_and_owner_publication():
    doc, ids = workspace()
    model = trained(doc, ids)
    request = q.ObjectFeatureRequest(doc, ids[-2:])
    job = q.ObjectClassifierJob()
    assert job.model is None and job.table is None and job.result is None
    assert job.train(q.ObjectFeatureRequest(doc, ids), parameters())
    assert not job.predict(request, model)
    assert job.wait(doc).state == 'ready' and job.model.sha256 == model.sha256
    assert job.predict(request, job.model)
    job.cancel()
    assert job.wait(doc).state == 'cancelled' and job.model is None
    assert job.predict(request, model)
    doc.rename_annotation(ids[0], 'Changed')
    assert job.wait(doc).state == 'stale' and job.result is None
    fresh = q.ObjectFeatureRequest(doc, ids[-2:])
    assert job.predict(fresh, model) and job.wait(doc).state == 'ready'
    assert job.apply(doc) == ids[-2:] and job.poll(doc).state == 'applied'
    with pytest.raises(RuntimeError):
        job.apply(doc)


def test_other_request_and_unit_mismatch_never_publish():
    doc, ids = workspace()
    model = trained(doc, ids)
    request = q.ObjectFeatureRequest(doc, ids[-2:])
    result = q.classify_objects(request, model)
    equivalent = q.ObjectFeatureRequest(doc, ids[-2:])
    with pytest.raises(ValueError, match='another request'):
        q.apply_object_classification(doc, equivalent, result)
    doc.set_calibration(.5, .5)
    with pytest.raises(ValueError, match='schema'):
        q.classify_objects(q.ObjectFeatureRequest(doc, ids), model)


def test_rgb_uses_verified_archived_features_and_links_survive_classification():
    if not q.has_geos:
        pytest.skip('cell publication requires GEOS')
    from test_cell_feature_links import setup, signature
    doc, _, _, _, record = setup()
    ids = doc.publish_cell_record(record)
    options = q.ObjectFeatureOptions()
    options.cell_rgb, options.cytoplasm_rgb = True, True
    request = q.ObjectFeatureRequest(doc, ids, options)
    table = q.extract_object_features(request)
    assert len(table.features) == 14
    expected = [list(doc.cell_object_feature(i).cell_rgb)+list(doc.cell_object_feature(i).nucleus_rgb)+
                list(doc.cell_object_feature(i).cytoplasm_rgb) for i in ids]
    np.testing.assert_allclose(table.numpy()[:, 5:], expected, rtol=1e-6)
    original = [signature(doc.cell_object_feature(i)) for i in ids]
    doc.classify_objects(ids, 'Classified')
    assert [signature(doc.cell_object_feature(i)) for i in ids] == original
    assert all(doc.cell_object_status(i) == 'current' for i in ids)
    assert doc.undo()
    doc.update_roi(ids[0], doc.annotation(ids[0]).geometry.translated(.1, 0))
    with pytest.raises(ValueError, match='verified'):
        q.ObjectFeatureRequest(doc, ids, options)
