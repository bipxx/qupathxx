"""Pinned real NDPI adapter/workflow checks; not biological ground truth."""
import gc
import hashlib
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_openslide_extended import rounded_reference

pytestmark = pytest.mark.skipif(not os.environ.get('QUPATHXX_HAMAMATSU_FIXTURE'),
                               reason='opt-in pinned Hamamatsu NDPI fixture')
ROI = (11264, 17664, 256, 256)


@pytest.fixture(scope='module')
def ndpi():
    assert q.has_openslide
    path = Path(os.environ['QUPATHXX_HAMAMATSU_FIXTURE']).resolve()
    manifest = json.loads((Path(__file__).parent/'fixtures/manifest.json').read_text(encoding='utf-8'))
    entry = next(e for e in manifest['downloads'] if e['filename'] == 'CMU-1.ndpi')
    assert path.stat().st_size == entry['bytes']
    with path.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == entry['sha256']
    directory = os.add_dll_directory(str(Path(_native.__file__).parent)) if os.name == 'nt' else None
    try:
        import openslide as api
        assert api.__library_version__ == q.build_environment().openslide_version
        yield path, api, entry
    finally:
        if directory:
            directory.close()


def test_ndpi_metadata_and_anisotropic_calibration(ndpi):
    path, api, entry = ndpi
    source = q.open_slide(str(path))
    try:
        with api.OpenSlide(str(path)) as reference:
            metadata = source.metadata
            assert metadata.vendor == 'hamamatsu'
            assert (metadata.width, metadata.height) == reference.dimensions == (51200, 38144)
            dimensions = [(level.width, level.height) for level in metadata.levels]
            assert dimensions == list(reference.level_dimensions) == [tuple(v) for v in entry['dimensions']]
            assert [level.downsample for level in metadata.levels] == list(reference.level_downsamples) == [2.**i for i in range(9)]
            assert metadata.pixel_width_um == float(reference.properties['openslide.mpp-x'])
            assert metadata.pixel_height_um == float(reference.properties['openslide.mpp-y'])
            np.testing.assert_allclose([metadata.pixel_width_um, metadata.pixel_height_um], entry['pixel_size_um'], rtol=1e-14)
            assert metadata.pixel_width_um != metadata.pixel_height_um
            doc = q.Document(source)
            obj = doc.add_rectangle(q.Rect(*ROI))
            pixels = rounded_reference(reference, api, ROI[0], ROI[1], 0, ROI[2], ROI[3])
            assert (pixels[..., 3] == 255).all() and pixels[..., :3].min() < 128
            assert doc.mean_intensity(obj) == pytest.approx(float(pixels[..., :3].mean()), rel=1e-12)
            assert doc.area_um2(obj) == pytest.approx(ROI[2]*ROI[3]*metadata.pixel_width_um*metadata.pixel_height_um, rel=1e-12)
        with pytest.raises((ValueError, RuntimeError)):
            source.read_region(q.RegionRequest(0, 0, 0, 51200, 38144))
    finally:
        source.close()


@pytest.mark.parametrize('level', range(9))
def test_ndpi_all_levels_padding_and_owned_pixels(ndpi, level):
    path, api, _ = ndpi
    source = q.open_slide(str(path))
    try:
        with api.OpenSlide(str(path)) as reference:
            for x, y in [(0, 0), ROI[:2], (25513, 19081), (-23, -11), (51183, 38125)]:
                expected = rounded_reference(reference, api, x, y, level, 37, 29)
                held = source.read_region(q.RegionRequest(x, y, level, 37, 29)).numpy()
                np.testing.assert_array_equal(held, expected)
                assert not held.flags.writeable
        with pytest.raises(IndexError):
            source.read_region(q.RegionRequest(0, 0, 9, 1, 1))
    finally:
        source.close()
    del source
    gc.collect()
    np.testing.assert_array_equal(held, expected)


@pytest.mark.parametrize('level', range(9))
def test_ndpi_installed_region_cli(ndpi, tmp_path, level):
    path, api, _ = ndpi
    exe = Path(_native.__file__).with_name('qupathxx_region.exe' if os.name == 'nt' else 'qupathxx_region')
    output = tmp_path/'region.pam'
    done = subprocess.run([str(exe), str(path), str(ROI[0]), str(ROI[1]), str(level), '71', '53', str(output)],
                          capture_output=True, timeout=30)
    assert done.returncode == 0, done.stderr.decode('utf-8', errors='replace')
    with api.OpenSlide(str(path)) as reference:
        expected = rounded_reference(reference, api, *ROI[:2], level, 71, 53)
    header, pixels = output.read_bytes().split(b'ENDHDR\n', 1)
    assert b'WIDTH 71\nHEIGHT 53\n' in header and pixels == expected.tobytes()


def test_ndpi_complete_overview_and_source_budget(ndpi):
    path, api, entry = ndpi
    source = q.open_slide(str(path))
    try:
        options = q.ThumbnailOptions()
        options.max_width = options.max_height = 1
        thumbnail = q.read_thumbnail(source, options)
        width, height = entry['dimensions'][-1]
        assert thumbnail.plan.level == 8 and thumbnail.plan.source_pixels == width*height
        with api.OpenSlide(str(path)) as reference:
            pixels = rounded_reference(reference, api, 0, 0, 8, width, height)
        alpha = pixels[..., 3].astype('uint64')
        count, total = width*height, int(alpha.sum())
        a = (total+count//2)//count
        expected = [(int((pixels[..., c].astype('uint64')*alpha).sum())+total//2)//total if a else 0 for c in range(3)] + [a]
        np.testing.assert_array_equal(thumbnail.pixels.numpy()[0, 0], expected)
        options.source_pixel_limit = 1
        with pytest.raises((ValueError, RuntimeError), match='source pixel limit'):
            q.read_thumbnail(source, options)
    finally:
        source.close()


def test_ndpi_scheduler_cache_and_pending_close(ndpi):
    path, _, _ = ndpi
    source = q.open_slide(str(path))
    options = q.SchedulerOptions()
    options.cache_bytes = 8*1024*1024
    options.work_bytes = 4*1024*1024
    options.max_requests, options.workers = 32, 2
    scheduler = q.TileScheduler(source, options)
    request = q.RegionRequest(*ROI[:2], 2, 257, 193)
    try:
        first = scheduler.submit(request)
        assert scheduler.wait_idle(15)
        result = scheduler.take(first)
        assert result.status == q.TileStatus.READY
        expected, reads = result.pixels.data, scheduler.stats.reads
        second = scheduler.submit(request)
        assert scheduler.wait_idle(15)
        cached = scheduler.take(second)
        assert cached.status == q.TileStatus.READY and cached.pixels.data == expected
        assert scheduler.stats.reads == reads and scheduler.stats.cache_hits >= 1
        pending = [scheduler.submit(q.RegionRequest(ROI[0]+240*i, ROI[1], 0, 257, 193)) for i in range(16)]
        scheduler.advance_generation()
        source.close()
        assert scheduler.wait_idle(15)
        assert all(scheduler.take(ticket).status == q.TileStatus.STALE for ticket in pending)
        assert scheduler.stats.cache_bytes <= options.cache_bytes and scheduler.stats.reserved_bytes == 0
    finally:
        scheduler.shutdown()
        source.close()
    assert scheduler.stats.stopped
    reopened = q.open_slide(str(path))
    try:
        assert reopened.read_region(request).data == expected
    finally:
        reopened.close()


@pytest.mark.skipif(not q.has_project, reason='native project storage')
def test_ndpi_log_detection_tile_invariance_and_saved_replay(ndpi, tmp_path):
    path, _, _ = ndpi
    source = q.open_slide(str(path))
    try:
        doc = q.Document(source)
        obj = doc.add_rectangle(q.Rect(*ROI))
        transform = q.StainTransform([[.65, .7, .29], [.07, .99, .11]])
        results = []
        for tile in [16, 64, 256]:
            p = q.CellParameters()
            p.algorithm, p.level, p.tile_size = q.CellAlgorithm.LOG_WATERSHED, 0, tile
            p.threshold, p.min_nucleus_pixels, p.max_nucleus_pixels = .4, 16, 512
            p.min_peak_radius, p.min_seed_distance, p.cell_expansion = 1, 4, 5
            request = q.CellRequest(doc, obj, transform, p)
            result = q.detect_cells(request)
            assert result.cell_count > 0 and result.retained_bytes <= request.estimated_work_bytes
            assert q.input_sha256(request) == result.nuclei.input_sha256
            results.append(result)
        assert all(r.nuclei.labels == results[0].nuclei.labels and r.cells.labels == results[0].cells.labels for r in results)
        for result in results[1:]:
            for actual, wanted in zip(result.features, results[0].features, strict=True):
                for key in ['nucleus_pixels', 'cell_pixels', 'nucleus_rgb', 'cell_rgb', 'nucleus_channels', 'cell_channels']:
                    np.testing.assert_array_equal(getattr(actual, key), getattr(wanted, key))
        record = doc.keep_cells(request, results[-1])
        project = q.Project()
        image = project.add_document(doc)
        destination = tmp_path/'ndpi.qpxproj'
        project.save(str(destination))
        loaded = q.Project.open(str(destination)).document(image)
        replay = q.CellRequest.replay(loaded, record)
        assert loaded.verify_cells(record, replay, q.detect_cells(replay))
        assert loaded.pixel_width_um == doc.pixel_width_um
        assert loaded.pixel_height_um == doc.pixel_height_um
    finally:
        source.close()


def test_ndpi_truncated_container_rejects(ndpi, tmp_path):
    path, _, _ = ndpi
    broken = tmp_path/'truncated.ndpi'
    with path.open('rb') as stream:
        broken.write_bytes(stream.read(8192))
    with pytest.raises(RuntimeError):
        q.open_slide(str(broken))


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in NDPI desktop')
def test_ndpi_desktop_pyramid_settles(ndpi, tmp_path):
    path, _, _ = ndpi
    source = q.open_slide(str(path))
    try:
        doc = q.Document(source)
        options = q.GuiOptions()
        options.settle_tiles = True
        report = q.show(doc, frame_limit=80, capture_path=str(tmp_path/'ndpi.ppm'), options=options)
        assert report.uploaded_tiles > 0 and report.pending_tiles == 0 and report.tiles_settled
        assert report.peak_gpu_bytes <= 64*1024*1024 and report.ui_overflow_items == 0
        assert report.cpu_frame_p95_ms > 0
    finally:
        source.close()
