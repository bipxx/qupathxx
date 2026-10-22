"""Portable forest contracts, independent of the optional training backend."""
import concurrent.futures
import struct
import numpy as np
import pytest
import qupathxx as q


def fixture():
    b = bytearray()
    def u32(*values):
        b.extend(struct.pack('<'+'I'*len(values), *values))
    def text(s):
        raw = s.encode('utf8')
        u32(len(raw))
        b.extend(raw)
    u32(0x46585051, 1, 2, 2, 2, 0)
    b.extend(struct.pack('<QQQ', 1, 4*1024*1024, 4))
    text('a'*64)
    text('independent-test/1')
    u32(1)
    text('area')
    text('um2')
    u32(2)
    text('A')
    text('B')
    u32(2, 0, 3, 4)
    u32(0, 1, 2, 0, 0x3f800000)
    u32(0, 0, 0, 0, 0)
    u32(0, 0, 0, 1, 0)
    u32(0, 0, 0, 1, 0)
    return bytes(b)


def schema():
    return [q.ClassifierFeature('area', 'um2')]


def test_portable_boundary_tie_schema_and_snapshots():
    assert type(q.has_classifier_training) is bool
    f = q.ForestModel.from_bytes(fixture())
    x = np.array([[np.nextafter(np.float32(1), np.float32(0))], [1],
                  [np.nextafter(np.float32(1), np.float32(2))]], dtype=np.float32)
    result = f.predict(x, schema())
    assert [r.votes for r in result] == [[1, 1], [1, 1], [0, 2]]
    assert [r.class_index for r in result] == [0, 0, 1]
    assert [r.vote_fraction for r in result] == [.5, .5, 1.]
    assert [r.class_index for r in f.predict(x[::-1], schema())] == [1, 0, 0]
    assert f.to_bytes() == fixture()
    assert len(f.sha256) == 64
    returned = f.features
    returned[0].unit = 'px2'
    assert f.features[0].unit == 'um2'
    parameters = f.parameters
    parameters.trees = 5
    assert f.tree_count == 2 and f.parameters.trees == 2
    with pytest.raises(ValueError, match='schema'):
        f.predict(x, returned)


@pytest.mark.parametrize('value', [float('nan'), float('inf'), -float('inf'), np.finfo(np.float32).max])
def test_reject_feature_values(value):
    with pytest.raises(ValueError):
        q.ForestModel.from_bytes(fixture()).predict(np.array([[value]], np.float32), schema())


def test_reject_shape_dtype_and_format():
    f = q.ForestModel.from_bytes(fixture())
    for x in [np.ones((1, 1), np.float64), np.ones((1,), np.float32), [[1.]]]:
        with pytest.raises(TypeError):
            f.predict(x, schema())
    for x in [np.ones((0, 1), np.float32), np.ones((1, 2), np.float32)]:
        with pytest.raises(ValueError):
            f.predict(x, schema())
    for n in range(len(fixture())):
        with pytest.raises(ValueError):
            q.ForestModel.from_bytes(fixture()[:n])
    with pytest.raises(ValueError):
        q.ForestModel.from_bytes(fixture()+b'\0')
    for offset, value in [(16, 5), (20, 2)]:
        corrupt = bytearray(fixture())
        struct.pack_into('<I', corrupt, offset, value)
        with pytest.raises(ValueError, match='metadata'):
            q.ForestModel.from_bytes(bytes(corrupt))


@pytest.mark.parametrize('seed', [1, 723, 2**63+7])
def test_training_repeatable_reload_concurrent_and_strides(seed):
    if not q.has_classifier_training:
        pytest.skip('optional OpenCV training backend')
    rng = np.random.default_rng(893)
    x = rng.normal(size=(500, 4)).astype(np.float32)
    y = np.where(x[:, 0]+x[:, 1]*.3 < 0, 0, 1).astype(np.uint32)
    features = [q.ClassifierFeature(f'f{i}', '1') for i in range(4)]
    p = q.ForestParameters()
    p.seed, p.trees, p.max_depth = seed, 24, 6
    original_x, original_y = x.copy(), y.copy()
    def train():
        return q.train_forest(x, y, features, ['negative', 'positive'], p)
    f = train()
    np.testing.assert_array_equal(x, original_x)
    np.testing.assert_array_equal(y, original_y)
    assert 'OpenCV/4.11.0/' in f.backend
    assert f.training_rows == 500
    assert f.sha256 == train().sha256
    with concurrent.futures.ThreadPoolExecutor(2) as pool:
        assert list(pool.map(lambda _: train().sha256, range(2))) == [f.sha256]*2
    loaded = q.ForestModel.from_bytes(f.to_bytes())
    a, b = f.predict(x, features), loaded.predict(x, features)
    assert [r.votes for r in a] == [r.votes for r in b]
    assert np.mean([r.class_index for r in a] == y) > .94
    # Two-dimensional and label negative strides are copied by logical order.
    reversed_model = q.train_forest(x[::-1, ::-1], y[::-1], features[::-1], ['negative', 'positive'], p)
    assert reversed_model.training_rows == 500
    assert len(reversed_model.predict(x[::-1, ::-1], features[::-1])) == 500


def test_training_validation():
    x = np.arange(20, dtype=np.float32).reshape(-1, 1)
    y = (x[:, 0] > 10).astype(np.uint32)
    for labels in [y[:-1], np.full(20, 7, np.uint32), np.zeros(20, np.uint32)]:
        with pytest.raises(ValueError):
            q.train_forest(x, labels, schema(), ['A', 'B'])
    with pytest.raises(TypeError):
        q.train_forest(x, y.astype(np.int32), schema(), ['A', 'B'])
    for features, classes in [([q.ClassifierFeature('area', '')], ['A', 'B']),
                              (schema(), ['A', 'A']), (schema(), ['', 'B'])]:
        with pytest.raises(ValueError):
            q.train_forest(x, y, features, classes)
    p = q.ForestParameters()
    p.work_bytes = 1024*1024
    with pytest.raises((ValueError, IndexError)):
        q.train_forest(x, y, schema(), ['A', 'B'], p)
