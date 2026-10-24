"""Arbitrary output grids: independent rational area oracle and owned buffers."""
from fractions import Fraction as F
import gc
import numpy as np
import pytest
import qupathxx as q


def oracle(rgba, width, height):
    sh, sw, _ = rgba.shape
    result = np.zeros((height, width, 4), dtype=np.uint8)
    area = F(sw*sh, width*height)
    for y in range(height):
        for x in range(width):
            alpha, sums = F(0), [F(0)]*3
            for sy in range(sh):
                dy = max(0, min(F((y+1)*sh, height), sy+1)-max(F(y*sh, height), sy))
                for sx in range(sw):
                    dx = max(0, min(F((x+1)*sw, width), sx+1)-max(F(x*sw, width), sx))
                    weight = dx*dy*int(rgba[sy, sx, 3])
                    alpha += weight
                    for c in range(3):
                        sums[c] += weight*int(rgba[sy, sx, c])
            a = int(alpha/area+F(1, 2))
            result[y, x] = [int(value/alpha+F(1, 2)) if a else 0 for value in sums]+[a]
    return result


@pytest.mark.parametrize('source_shape,output', [((3, 7), (2, 5)), ((2, 3), (7, 5)), ((5, 4), (4, 5)), ((1, 9), (3, 1)), ((7, 1), (2, 3))])
def test_fractional_downsample_enlargement_anisotropic_and_identity(source_shape, output):
    rgba = np.random.default_rng(154).integers(0, 256, (*source_shape, 4), dtype=np.uint8)
    rgba[0, 0, 3] = 0
    pixels = q.PixelBuffer.from_numpy(rgba)
    result = q.resample_area(pixels, *output)
    np.testing.assert_array_equal(result.numpy(), oracle(rgba, *output))
    np.testing.assert_array_equal(pixels.numpy(), rgba)
    assert not result.numpy().flags.writeable


def test_padded_region_and_buffer_outlives_closed_server():
    server = q.Document(q.Image(2, 1, [10, 20, 30, 110, 120, 130])).server
    result = q.read_resampled_region(server, q.RegionRequest(-1, 0, 0, 4, 1), 2, 1).numpy()
    server.close()
    del server
    gc.collect()
    np.testing.assert_array_equal(result, [[[10, 20, 30, 128], [110, 120, 130, 128]]])


@pytest.mark.parametrize('width,height', [(0, 1), (1, -1), (67108865, 1), (2**62, 2**62)])
def test_invalid_output_rejects(width, height):
    server = q.Document(q.Image.demo()).server
    with pytest.raises((ValueError, RuntimeError)):
        q.read_resampled_region(server, q.RegionRequest(0, 0, 0, 1, 1), width, height)


def test_excessive_cross_axis_work_rejects_before_huge_decode():
    server = q.Document(q.Image.demo()).server
    with pytest.raises(RuntimeError, match='overlap work limit'):
        q.read_resampled_region(server, q.RegionRequest(0, 0, 0, 67108864, 1), 1, 67108864)


def test_closed_server_and_invalid_level():
    server = q.Document(q.Image.demo()).server
    with pytest.raises(IndexError):
        q.read_resampled_region(server, q.RegionRequest(0, 0, 99, 1, 1), 2, 2)
    server.close()
    with pytest.raises(RuntimeError):
        q.read_resampled_region(server, q.RegionRequest(0, 0, 0, 1, 1), 2, 2)
