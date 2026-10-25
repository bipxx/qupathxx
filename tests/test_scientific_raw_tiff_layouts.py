"""Storage-independent quantitative footprints against an exact Python oracle.

These are synthetic reader/sampler integration checks, not vendor or biological
acceptance. Expected values never use native grid overlap or mean helpers.
"""
from contextlib import closing
from fractions import Fraction
import math

import numpy as np
import pytest
import qupathxx as q
import tifffile

pytestmark = pytest.mark.skipif(not q.has_tiff, reason='quantitative TIFF reader')


def values(dtype):
    out = np.empty((35, 39, 3), dtype=dtype)
    for y in range(35):
        for x in range(39):
            for c in range(3):
                n = (y * 39 + x) * 3 + c
                if dtype == 'uint64': value = 2**63 + n * 7 + 1
                elif dtype == 'int64': value = (-1 if (x+y+c) % 2 else 1) * (2**60 + n * 3 + 1)
                else: value = (-1 if (x+y+c) % 2 else 1) * (n + 1) / 8
                out[y, x, c] = value
    if dtype == 'float64':
        out[16, 16, 0] = np.nan  # Unselected channel must not affect support.
        out[16, 17, 2] = np.inf
    return out


def oracle(data, axes, first=1, channels=2):
    (ox, ex, nx, cx), (oy, ey, ny, cy) = axes
    means = np.zeros((ny+2*cy, nx+2*cx, channels), dtype=np.float64)
    reasons = np.zeros(means.shape, dtype=np.uint8)
    for j in range(-cy, ny+cy):
        top, bottom = Fraction(oy)+Fraction(j*ey, ny), Fraction(oy)+Fraction((j+1)*ey, ny)
        for i in range(-cx, nx+cx):
            left, right = Fraction(ox)+Fraction(i*ex, nx), Fraction(ox)+Fraction((i+1)*ex, nx)
            index = (j+cy, i+cx)
            if left < 0 or top < 0 or right > data.shape[1] or bottom > data.shape[0]:
                reasons[index] = 1
                continue
            for c in range(channels):
                total = Fraction(0)
                nonfinite = False
                for y in range(math.floor(top), math.ceil(bottom)):
                    wy = min(bottom, y+1)-max(top, y)
                    for x in range(math.floor(left), math.ceil(right)):
                        wx = min(right, x+1)-max(left, x)
                        value = data[y, x, first+c]
                        if data.dtype.kind == 'f':
                            if not math.isfinite(float(value)):
                                nonfinite = True
                                continue
                            exact = Fraction.from_float(float(value))
                        else: exact = Fraction(int(value))
                        total += exact * wx * wy
                if nonfinite: reasons[index+(c,)] = 3
                else: means[index+(c,)] = float(total / ((right-left)*(bottom-top)))
    return means, reasons


def make_grid(axes):
    return q.ScientificGrid(39, 35, *(q.ScientificGridAxis(*a) for a in axes))


def assert_samples(server, data, axes, options=None):
    if options is None:
        options = q.ScientificRawOptions()
        options.first_channel, options.channels = 1, 2
    grid = make_grid(axes)
    sample = q.read_scientific_raw(server, grid, options)
    expected, reasons = oracle(data, axes)
    # Compare bits, including zero sign; floating tolerances could hide lost
    # low integer bits or intermediate rounding before the weighted sum.
    np.testing.assert_array_equal(sample.numpy().view(np.uint64), expected.view(np.uint64))
    np.testing.assert_array_equal(sample.reasons_numpy(), reasons)
    np.testing.assert_array_equal(sample.valid_numpy(), (reasons == 0).astype(np.uint8))
    digest = q.digest_scientific_raw(server, grid, options)
    assert digest.input_sha256 == sample.input_sha256
    assert digest.decoded_source_sha256 == sample.decoded_source_sha256
    return sample


@pytest.mark.parametrize('dtype', ['uint64', 'int64', 'float64'])
@pytest.mark.parametrize('layout', ['strips', 'tiles', 'tiles-big-endian', 'tiles-separate'])
@pytest.mark.parametrize('phase', ['interior', 'image-edge'])
def test_exact_cross_block_footprints_are_storage_independent(tmp_path, dtype, layout, phase):
    data = values(dtype)
    axes = ((9, 23, 9, 1), (7, 21, 7, 1)) if phase == 'interior' else ((-2, 43, 7, 1), (30, 9, 4, 1))
    path = tmp_path/'layout.tif'
    kwargs = dict(metadata=None, photometric='rgb', compression='deflate')
    if layout == 'strips': kwargs['rowsperstrip'] = 7
    else: kwargs['tile'] = (16, 16)
    if layout == 'tiles-big-endian': kwargs['byteorder'] = '>'
    if layout == 'tiles-separate':
        kwargs['planarconfig'] = 'separate'
        stored = np.moveaxis(data, -1, 0)
    else: stored = data
    tifffile.imwrite(path, stored, **kwargs)
    options = q.TiffOpenOptions()
    options.cache_bytes = 0  # Exercise eviction-independent decoded results.
    with closing(q.open_tiff(str(path), options)) as server:
        sample = assert_samples(server, data, axes)
        assert server.tiff_read_stats.decoded_blocks > 0
        assert server.tiff_read_stats.cache_bytes == server.tiff_read_stats.peak_cache_bytes == 0
    # Same geometry/channels and bytes in another storage layout must retain
    # both decoded and scientific input identity (file identity is separate).
    baseline = tmp_path/'baseline.tif'
    tifffile.imwrite(baseline, data, metadata=None, photometric='rgb', rowsperstrip=5)
    with closing(q.open_tiff(str(baseline))) as server:
        other = assert_samples(server, data, axes)
        assert other.decoded_source_sha256 == sample.decoded_source_sha256
        assert other.input_sha256 == sample.input_sha256


@pytest.mark.parametrize('layout', ['tiles', 'tiles-separate'])
def test_scientific_footprints_use_level_zero_not_preview_pyramid(tmp_path, layout):
    data = values('uint64')
    source = tmp_path/'pyramid.tif'
    planar = 'separate' if layout == 'tiles-separate' else 'contig'
    def stored(a): return np.moveaxis(a, -1, 0) if planar == 'separate' else a
    with tifffile.TiffWriter(source, bigtiff=True) as writer:
        writer.write(stored(data), tile=(16,16), compression='deflate', subifds=1,
                     metadata=None, photometric='rgb', planarconfig=planar)
        # Deliberately different coarse pixels: an automatic preview-level
        # substitution cannot accidentally satisfy the exact source oracle.
        writer.write(stored(np.zeros_like(data[::2, ::2])), tile=(16,16),
                     compression='deflate', subfiletype=1, metadata=None,
                     photometric='rgb', planarconfig=planar)
    with closing(q.open_tiff(str(source))) as server:
        assert len(server.raw_metadata.levels) == 2
        assert server.raw_metadata.levels[1].downsample == 2
        sample = assert_samples(server, data, ((1, 36, 3, 0), (1, 32, 2, 0)))
        assert np.all(sample.numpy() > 2**62)


def test_tiled_sampling_budget_rejects_before_decode_and_source_change_rejects(tmp_path):
    source = tmp_path/'bounded.tif'
    data = values('uint64')
    tifffile.imwrite(source, data, metadata=None, photometric='rgb', tile=(16,16), compression='deflate')
    with closing(q.open_tiff(str(source))) as server:
        options = q.ScientificRawOptions()
        options.first_channel, options.channels = 1, 2
        grid = make_grid(((9, 23, 9, 1), (7, 21, 7, 1)))
        before = server.tiff_read_stats.decoded_blocks
        options.work_bytes = q.scientific_raw_work_bytes(server.raw_metadata, grid, options)-1
        with pytest.raises((ValueError, RuntimeError), match='budget'):
            q.read_scientific_raw(server, grid, options)
        assert server.tiff_read_stats.decoded_blocks == before
        options.work_bytes += 1
        sample = q.read_scientific_raw(server, grid, options)
        assert sample.estimated_work_bytes == options.work_bytes
        with source.open('ab') as stream: stream.write(b'changed')
        with pytest.raises(RuntimeError, match='changed'):
            q.digest_scientific_raw(server, grid, options)


@pytest.mark.parametrize('byteorder', ['<', '>'])
def test_tiled_ome_captured_channels_z_time_and_calibration(tmp_path, byteorder):
    plane = values('float64')
    data = np.empty((2,2,3,35,39), dtype=np.float64)
    for t in range(2):
        for z in range(2):
            data[t,z] = np.moveaxis(plane + 10000*t + 1000*z, -1, 0)
    source = tmp_path/'axes.ome.tif'
    tifffile.imwrite(source, data, ome=True, metadata={
        'axes':'TZCYX', 'PhysicalSizeX':.25, 'PhysicalSizeXUnit':'um',
        'PhysicalSizeY':.5, 'PhysicalSizeYUnit':'um',
        'Channel':{'Name':['unused','A','B']}}, photometric='minisblack',
        tile=(16,16), compression='deflate', byteorder=byteorder)
    with closing(q.open_tiff(str(source))) as server:
        options = q.ScientificRawOptions()
        options.first_channel, options.channels, options.z, options.t = 1, 2, 1, 1
        sample = assert_samples(server, plane+11000, ((9,23,9,1),(7,21,7,1)), options)
        assert sample.metadata.channel_names == ['unused','A','B']
        assert sample.metadata.pixel_width_um == .25 and sample.metadata.pixel_height_um == .5
        assert sample.options.z == sample.options.t == 1
        options.z = options.t = 0
        other = assert_samples(server, plane, ((9,23,9,1),(7,21,7,1)), options)
        assert sample.input_sha256 != other.input_sha256
        assert sample.options.z == sample.options.t == 1


def test_tiled_nondefault_series_uses_selected_pixels_and_binds_identity(tmp_path):
    data = values('int64')
    source = tmp_path/'series.tif'
    with tifffile.TiffWriter(source) as writer:
        writer.write(np.zeros_like(data), tile=(16,16), metadata=None, photometric='rgb')
        writer.write(data, tile=(16,16), metadata=None, photometric='rgb')
    options = q.TiffOpenOptions()
    options.view.series = 1
    with closing(q.open_tiff(str(source), options)) as server:
        sample = assert_samples(server, data, ((9,23,9,1),(7,21,7,1)))
        assert sample.metadata.series == 1 and sample.metadata.series_count == 2
    with closing(q.open_tiff(str(source))) as server:
        other = assert_samples(server, np.zeros_like(data), ((9,23,9,1),(7,21,7,1)))
        assert other.metadata.series == 0
        assert other.input_sha256 != sample.input_sha256
