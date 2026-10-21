"""Independent Beer-Lambert / linear-algebra checks; no native reference loop."""
from concurrent.futures import ThreadPoolExecutor
import gc

import numpy as np
import pytest
import qupathxx as q


def image(rgb):
    rgb = np.asarray(rgb, dtype=np.uint8).reshape(1, -1, 3)
    server = q.dense_image_server(q.Image(rgb.shape[1], 1, rgb.ravel().tolist()))
    return server, server.read_region(q.RegionRequest(0, 0, 0, rgb.shape[1], 1))


def values(result):
    return np.frombuffer(result.data, dtype=np.float32).reshape(result.height, result.width, 3)


def reference(rgb, white):
    return np.maximum(0, np.log10(np.asarray(white) / np.maximum(rgb, 1)))


def test_exhaustive_optical_density_and_layout():
    rgb = np.repeat(np.arange(256, dtype=np.uint8)[:, None], 3, axis=1)
    server, pixels = image(rgb)
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]], [255, 240, 210])
    result = transform.optical_densities(pixels)
    assert result.width == 256 and result.height == 1 and result.row_stride == 256 * 12
    expected = reference(rgb.astype(float), transform.white)
    np.testing.assert_allclose(values(result)[0], expected, atol=1e-7, rtol=1e-7)
    np.testing.assert_array_equal(values(result), values(transform.deconvolve(pixels)))
    for i in range(256):
        assert q.optical_density(i, 240) == pytest.approx(expected[i, 1], abs=1e-14)
    assert q.optical_density(255, 200) == 0
    assert q.optical_density(0) == q.optical_density(1)
    server.close()


@pytest.mark.parametrize("basis", [
    [[.65, .70, .29], [.07, .99, .11]],
    [[.65, .70, .29], [.27, .57, .78], [.9, .1, .4]],
    [[1, 2, 3], [-1, 1, 2], [2, 0, 1]],
])
def test_deconvolution_against_independent_matrix_solve(basis):
    rgb = np.random.default_rng(298).integers(0, 256, (4096, 3), dtype=np.uint8)
    _, pixels = image(rgb)
    transform = q.StainTransform(basis, [250, 245, 230])
    normalized = np.asarray(basis, dtype=float)
    normalized /= np.linalg.norm(normalized, axis=1)[:, None]
    if len(basis) == 2:
        residual = np.cross(*normalized)
        normalized = np.vstack([normalized, residual / np.linalg.norm(residual)])
    np.testing.assert_allclose(transform.stains, normalized, atol=1e-15)
    expected = np.linalg.solve(normalized.T, reference(rgb.astype(float), transform.white).T).T
    actual = values(transform.deconvolve(pixels))[0]
    np.testing.assert_allclose(actual, expected, rtol=1e-6, atol=2e-6)
    assert np.any(actual < 0)  # Negative concentrations remain quantitative values.
    np.testing.assert_allclose(actual @ normalized, reference(rgb.astype(float), transform.white), atol=2e-6)


def test_padding_source_lifetime_and_concurrent_reuse():
    server, _ = image([[10, 20, 30], [150, 160, 170]])
    pixels = server.read_region(q.RegionRequest(-1, 0, 0, 4, 1))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    server.close()
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda _: transform.deconvolve(pixels), range(32)))
    before = results[0].data
    assert all(r.data == before for r in results)
    del pixels, transform, server
    gc.collect()
    actual = values(results[0])[0]
    assert np.isnan(actual[[0, 3]]).all()
    np.testing.assert_allclose(actual[1:3], reference(np.array([[10, 20, 30], [150, 160, 170]]), [255]*3), atol=1e-7)
    assert results[0].data == before


@pytest.mark.parametrize("basis", [
    [], [[1, 0, 0]], [[1, 0, 0]]*4,
    [[0, 0, 0], [0, 1, 0]], [[1, 0, 0], [1, 1e-10, 0]],
    [[1, 0, 0], [0, 1, 0], [1, 1, 0]],
    [[float("nan"), 0, 0], [0, 1, 0]], [[float("inf"), 0, 0], [0, 1, 0]],
])
def test_invalid_basis(basis):
    with pytest.raises(ValueError):
        q.StainTransform(basis)


@pytest.mark.parametrize("white", [0, -1, 256, float("nan"), float("inf")])
def test_invalid_white(white):
    with pytest.raises(ValueError):
        q.StainTransform([[1, 0, 0], [0, 1, 0]], [255, white, 255])
    with pytest.raises(ValueError):
        q.optical_density(100, white)


@pytest.mark.parametrize("intensity", [-1, 256, float("nan"), float("inf")])
def test_invalid_intensity(intensity):
    with pytest.raises(ValueError):
        q.optical_density(intensity)


def test_normalization_range_and_parameter_immutability():
    stains = [[1e308, 0, 0], [0, 1e-308, 0]]
    white = [255, 240, 230]
    transform = q.StainTransform(stains, white)
    stains[0][0] = 0
    white[0] = 1
    np.testing.assert_array_equal(transform.stains, np.eye(3))
    np.testing.assert_array_equal(transform.white, [255, 240, 230])


def test_tile_partition_invariance():
    rgb = np.random.default_rng(15).integers(0, 256, (128, 257, 3), dtype=np.uint8)
    server = q.dense_image_server(q.Image(257, 128, rgb.ravel().tolist()))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
    full = values(transform.deconvolve(server.read_region(q.RegionRequest(0, 0, 0, 257, 128))))
    tiled = np.empty_like(full)
    for y in range(0, 128, 31):
        for x in range(0, 257, 37):
            w, h = min(37, 257-x), min(31, 128-y)
            tiled[y:y+h, x:x+w] = values(transform.deconvolve(server.read_region(q.RegionRequest(x, y, 0, w, h))))
    np.testing.assert_array_equal(tiled, full)
