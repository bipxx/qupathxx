"""Complete thumbnails: independent area oracle, planning and retained buffers."""
import gc
from fractions import Fraction
from pathlib import Path
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q


def options(w=256, h=256):
    result = q.ThumbnailOptions()
    result.max_width, result.max_height = w, h
    return result


def oracle(rgb, width, height):
    sh, sw, _ = rgb.shape
    result = np.empty((height, width, 4), dtype=np.uint8)
    area = Fraction(sw*sh, width*height)
    for y in range(height):
        for x in range(width):
            sums = [Fraction(0)]*3
            for sy in range(sh):
                dy = max(0, min(Fraction((y+1)*sh, height), sy+1)-max(Fraction(y*sh, height), sy))
                for sx in range(sw):
                    dx = max(0, min(Fraction((x+1)*sw, width), sx+1)-max(Fraction(x*sw, width), sx))
                    for c in range(3):
                        sums[c] += int(rgb[sy, sx, c])*dx*dy
            result[y, x] = [int(value/area+Fraction(1, 2)) for value in sums]+[255]
    return result


@pytest.mark.parametrize('shape,bounds', [((7, 11), (4, 4)), ((11, 7), (3, 6)), ((1, 9), (1, 4)), ((3, 2), (20, 20)), ((8, 8), (1, 1))])
def test_exact_complete_area_and_no_upscaling(shape, bounds):
    rgb = np.random.default_rng(42).integers(0, 256, (*shape, 3), dtype=np.uint8)
    server = q.Document(q.Image.from_numpy(rgb)).server
    result = q.read_thumbnail(server, options(*bounds))
    w, h = result.plan.width, result.plan.height
    assert 1 <= w <= min(shape[1], bounds[0]) and 1 <= h <= min(shape[0], bounds[1])
    assert result.plan.source_pixels == shape[0]*shape[1]
    assert result.plan.output_pixels == w*h and result.plan.level == 0
    np.testing.assert_array_equal(result.pixels.numpy(), oracle(rgb, w, h))
    assert result.elapsed_ms >= 0


def test_buffer_retains_result_after_job_server_close_and_gc():
    server = q.Document(q.Image(7, 5, [10, 20, 30]*35)).server
    job = q.ThumbnailJob()
    assert job.request(server, options(3, 3))
    stats = job.wait()
    assert stats.state == 'succeeded' and stats.completed == 1 and not job.busy()
    view = job.result().pixels.numpy()
    server.close()
    del job, server
    gc.collect()
    np.testing.assert_array_equal(view, np.broadcast_to([10, 20, 30, 255], (2, 3, 4)))
    assert not view.flags.writeable


def test_budget_and_invalid_options_do_not_poison_job():
    server = q.Document(q.Image.demo()).server
    job = q.ThumbnailJob()
    for field, value in [('max_width', 0), ('max_height', 4097), ('source_pixel_limit', 0), ('output_pixel_limit', 0)]:
        invalid = options()
        setattr(invalid, field, value)
        with pytest.raises(ValueError):
            job.request(server, invalid)
        assert not job.busy() and job.poll().state == 'idle'
    small = options()
    small.source_pixel_limit = 4
    with pytest.raises((ValueError, IndexError, RuntimeError)):
        q.read_thumbnail(server, small)
    assert job.request(server)
    assert job.wait().state == 'succeeded'
    retained = job.result()
    job.cancel()
    assert job.result() is retained
    server.close()
    assert job.request(server)
    assert job.wait().state == 'failed' and not job.result() and job.poll().completed == 1


def test_cancel_completed_unpublished_job_discards_result():
    server = q.Document(q.Image.demo()).server
    job = q.ThumbnailJob()
    assert job.request(server)
    job.cancel()
    assert job.wait().state == 'cancelled' and not job.result()
    assert job.poll().completed == 0


def test_job_destruction_joins_with_native_reader_ownership():
    # A Python proxy deleter on the worker would deadlock while destruction
    # holds the GIL and joins. Exercise immediate owner loss in a subprocess.
    subprocess.run([sys.executable, '-I', '-c', '''
import gc
import qupathxx as q
for _ in range(10):
    server=q.dense_image_server(q.Image.demo())
    job=q.ThumbnailJob()
    assert job.request(server)
    del server
    gc.collect()
    del job
    gc.collect()
'''], check=True, capture_output=True, timeout=20)


def test_region_cli_thumbnail_matches_native_and_preserves_source(tmp_path):
    executable = Path(q.__file__).parent/('qupathxx_region.exe' if __import__('os').name == 'nt' else 'qupathxx_region')
    if not executable.exists():
        pytest.skip('standalone executable absent')
    path = tmp_path/'image.ppm'
    image = q.Image.demo()
    path.write_bytes(f'P6\n{image.width} {image.height}\n255\n'.encode()+image.numpy().tobytes())
    original = path.read_bytes()
    output = tmp_path/'thumb.pam'
    command = [str(executable), '--thumbnail', str(path), '31', '23', str(output)]
    subprocess.run(command, check=True, capture_output=True, timeout=30)
    raw = output.read_bytes()
    header, pixels = raw.split(b'ENDHDR\n')
    result = q.read_thumbnail(q.Document(image).server, options(31, 23))
    assert f'WIDTH {result.plan.width}\nHEIGHT {result.plan.height}'.encode() in header
    assert pixels == result.pixels.numpy().tobytes()
    command[-1] = str(path)
    assert subprocess.run(command, capture_output=True, timeout=30).returncode == 1
    assert path.read_bytes() == original
    command[3], command[-1] = '4294967297', str(tmp_path/'invalid.pam')
    assert subprocess.run(command, capture_output=True, timeout=30).returncode == 1
    assert not Path(command[-1]).exists()


def test_real_pyramid_uses_bounded_complete_natural_level():
    path = Path(__file__).resolve().parents[1]/'build/fixtures/large-pyramid.tif'
    if not q.has_openslide or not path.exists():
        pytest.skip('requires generated OpenSlide pyramid')
    server = q.open_slide(str(path))
    result = q.read_thumbnail(server, options(250, 150))
    assert result.plan.level > 0 and result.plan.source_pixels <= 4*1024*1024
    assert result.pixels.numpy().shape == (result.plan.height, result.plan.width, 4)
    assert result.plan.source_width >= result.plan.width and result.plan.source_height >= result.plan.height
    np.testing.assert_array_equal(result.pixels.numpy(), np.broadcast_to([40, 80, 160, 255], result.pixels.numpy().shape))
    server.close()


@pytest.mark.skipif(not q.has_openslide, reason='requires OpenSlide')
@pytest.mark.parametrize('dimensions', [
    [(128, 64), (64, 32), (32, 16)],
    [(128, 64), (32, 16), (8, 4)],
    [(129, 67), (33, 17), (9, 5)],
    [(65, 129), (17, 33), (5, 9)],
])
def test_small_pyramid_selection_and_complete_pixels_against_fraction_oracle(tmp_path, dimensions):
    tifffile = pytest.importorskip('tifffile')
    path = tmp_path/'pyramid.tif'
    levels = [np.random.default_rng(i).integers(0, 256, (h, w, 3), dtype=np.uint8)
              for i, (w, h) in enumerate(dimensions)]
    with tifffile.TiffWriter(path) as writer:
        for i, rgb in enumerate(levels):
            writer.write(rgb, photometric='rgb', tile=(16, 16), compression='deflate', metadata=None, subfiletype=0 if i == 0 else 1)
    server = q.open_slide(str(path))
    # Verify decoding independently, including levels narrower than one tile
    # and partial right/bottom tiles. Legacy system stacks silently corrupted
    # the small levels in this corpus; the pinned SDK must preserve them.
    for level, rgb in enumerate(levels):
        h, w, _ = rgb.shape
        decoded = server.read_region(q.RegionRequest(0, 0, level, w, h)).numpy()
        np.testing.assert_array_equal(decoded[:, :, :3], rgb)
        assert np.all(decoded[:, :, 3] == 255)
    result = q.read_thumbnail(server, options(6, 6))
    assert result.plan.level == 2 and result.plan.source_pixels == len(levels[2].reshape(-1, 3))
    np.testing.assert_array_equal(result.pixels.numpy(), oracle(levels[2], result.plan.width, result.plan.height))
    server.close()
