"""Measure real Aperio region reads and verified scheduler cache reuse."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import time

import qupathxx as q
from qupathxx import _native


def timed(function):
    start = time.perf_counter()
    value = function()
    return value, (time.perf_counter() - start) * 1000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--slide', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with args.slide.open('rb') as stream:
        fixture_sha = hashlib.file_digest(stream, 'sha256').hexdigest()
    expected = '00a3d54482cd707abf254fe69dccc8d06b8ff757a1663f1290c23418c480eb30'
    if fixture_sha != expected:
        parser.error('slide must be the pinned CMU-1.svs fixture')
    source = q.open_slide(str(args.slide.resolve()))
    scheduler = None
    try:
        samples = []
        for level in range(3):
            request = q.RegionRequest(22813, 16129, level, 257, 193)
            first, first_ms = timed(lambda: source.read_region(request))
            digest = hashlib.sha256(first.data).hexdigest()
            warm = []
            for _ in range(9):
                pixels, elapsed = timed(lambda: source.read_region(request))
                assert hashlib.sha256(pixels.data).hexdigest() == digest
                warm.append(elapsed)
            samples.append(dict(level=level, first_read_ms=first_ms,
                                warm_median_ms=statistics.median(warm),
                                warm_max_ms=max(warm), rgba_sha256=digest))
        options = q.SchedulerOptions()
        options.cache_bytes = 8 * 1024 * 1024
        options.work_bytes = 4 * 1024 * 1024
        options.workers = 2
        scheduler = q.TileScheduler(source, options)
        request = q.RegionRequest(22813, 16129, 1, 257, 193)

        def completed_read():
            ticket = scheduler.submit(request)
            assert scheduler.wait_idle(15)
            result = scheduler.take(ticket)
            assert result.status == q.TileStatus.READY
            return result

        first, scheduled_ms = timed(completed_read)
        reads = scheduler.stats.reads
        cache_times = []
        for _ in range(9):
            cached, elapsed = timed(completed_read)
            assert cached.pixels.data == first.pixels.data
            cache_times.append(elapsed)
        assert reads == scheduler.stats.reads == 1
        assert scheduler.stats.cache_bytes <= options.cache_bytes
        assert scheduler.stats.reserved_bytes == 0
        thumbnail_options = q.ThumbnailOptions()
        thumbnail_options.max_width = 250
        thumbnail_options.max_height = 150
        thumbnail_options.source_pixel_limit = 8 * 1024 * 1024
        overview, overview_ms = timed(lambda: q.read_thumbnail(source, thumbnail_options))
        assert overview.plan.level == 2 and overview.plan.source_pixels == 2875 * 2057
        with Path(_native.__file__).open('rb') as stream:
            native_sha = hashlib.file_digest(stream, 'sha256').hexdigest()
        receipt = dict(schema='qpx.vendor-reader-benchmark/1', fixture_sha256=fixture_sha,
                       native_sha256=native_sha, platform=platform.platform(), processor=platform.processor(),
                       source_tree_sha256=q.build_environment().source_tree_sha256,
                       openslide_version=q.build_environment().openslide_version,
                       region_shape=[193, 257, 4], warm_iterations=9, regions=samples,
                       scheduler_first_ms=scheduled_ms, scheduler_cache_median_ms=statistics.median(cache_times),
                       scheduler_reads=scheduler.stats.reads, scheduler_cache_hits=scheduler.stats.cache_hits,
                       scheduler_cache_bytes=scheduler.stats.cache_bytes,
                       scheduler_cache_limit=options.cache_bytes,
                       overview_ms=overview_ms, overview_source_pixels=overview.plan.source_pixels,
                       overview_rgba_sha256=hashlib.sha256(overview.pixels.data).hexdigest(),
                       scope='local repeated regions; first read may use OS/backend cache; not RSS or frame acceptance')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(receipt, indent=2))
    finally:
        if scheduler is not None:
            scheduler.shutdown()
        source.close()


if __name__ == '__main__':
    main()
