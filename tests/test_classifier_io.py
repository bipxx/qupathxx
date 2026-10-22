import pytest
import qupathxx as q
from test_classifier import fixture


def test_model_files_round_trip_new_file_and_explicit_valid_model_replacement(tmp_path):
    model = q.ForestModel.from_bytes(fixture())
    path = tmp_path/'模型.qpxforest'
    q.save_forest_model(model, str(path))
    assert path.read_bytes() == fixture() and q.load_forest_model(str(path)).sha256 == model.sha256
    with pytest.raises(RuntimeError):
        q.save_forest_model(model, str(path))
    assert path.read_bytes() == fixture()
    q.save_forest_model(model, str(path), replace_existing=True)
    assert q.load_forest_model(str(path)).sha256 == model.sha256
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def test_invalid_existing_files_and_bad_inputs_are_preserved(tmp_path):
    model = q.ForestModel.from_bytes(fixture())
    path = tmp_path/'image.ppm'
    original = b'P6\n1 1\n255\n\xff\x80\x00'
    path.write_bytes(original)
    for replace in [False, True]:
        with pytest.raises((RuntimeError, ValueError)):
            q.save_forest_model(model, str(path), replace_existing=replace)
        assert path.read_bytes() == original
    with pytest.raises((RuntimeError, ValueError)):
        q.load_forest_model(str(path))
    with pytest.raises((RuntimeError, ValueError)):
        q.load_forest_model(str(tmp_path))
    with pytest.raises((RuntimeError, ValueError)):
        q.save_forest_model(model, str(tmp_path/'missing'/'file.qpxforest'))
    assert not list(tmp_path.glob('*.qpx-tmp-*'))
