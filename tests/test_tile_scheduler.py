import gc

import pytest

import qupathxx as q


def source():
    return q.dense_image_server(q.Image(3, 1, [10, 20, 30, 100, 110, 120, 200, 210, 220]))


def options(**values):
    result = q.SchedulerOptions()
    for name, value in values.items():
        setattr(result, name, value)
    return result


def test_deduplication_and_cache_lifetime():
    server = source()
    scheduler = q.TileScheduler(server, options(workers=1, cache_bytes=8))
    request = q.RegionRequest(1, 0, 0, 1, 1)
    first = scheduler.submit(request)
    assert scheduler.wait_idle(5)
    assert scheduler.submit(request, priority=10) == first
    result = scheduler.take(first)
    assert result.status == q.TileStatus.READY
    assert result.pixels.data == bytes([100, 110, 120, 255])
    second = scheduler.submit(request)
    assert second != first
    assert scheduler.wait_idle(5)
    assert scheduler.take(second).pixels.data == result.pixels.data
    assert scheduler.stats.reads == 1
    assert scheduler.stats.cache_hits == 1
    assert scheduler.stats.reserved_bytes == 0
    scheduler.shutdown()
    assert not server.closed
    server.close()
    del scheduler, server
    gc.collect()
    assert result.pixels.data == bytes([100, 110, 120, 255])


def test_backpressure_and_request_limit():
    scheduler = q.TileScheduler(source(), options(workers=1, work_bytes=8, max_requests=2))
    first = scheduler.submit(q.RegionRequest(0, 0, 0, 1, 1))
    assert scheduler.wait_idle(5)
    second = scheduler.submit(q.RegionRequest(1, 0, 0, 1, 1))
    assert not scheduler.wait_idle(0)
    assert scheduler.stats.reserved_bytes == 8
    with pytest.raises(RuntimeError, match="request limit"):
        scheduler.submit(q.RegionRequest(2, 0, 0, 1, 1))
    assert scheduler.take(first).status == q.TileStatus.READY
    assert scheduler.wait_idle(5)
    assert scheduler.take(second).status == q.TileStatus.READY
    assert scheduler.stats.requests == 0
    scheduler.shutdown()


def test_generation_rejects_unconsumed_result():
    scheduler = q.TileScheduler(source())
    first = scheduler.submit(q.RegionRequest(0, 0, 0, 1, 1))
    assert scheduler.wait_idle(5)
    assert scheduler.advance_generation() == 2
    stale = scheduler.take(first)
    assert stale.generation == 1
    assert stale.status == q.TileStatus.STALE
    assert stale.pixels is None
    second = scheduler.submit(q.RegionRequest(0, 0, 0, 1, 1))
    assert scheduler.wait_idle(5)
    fresh = scheduler.take(second)
    assert fresh.generation == 2 and fresh.status == q.TileStatus.READY
    assert scheduler.stats.cache_hits == 1  # Immutable pixels can be reused across view generations.
    scheduler.shutdown()


def test_cancel_release_and_shutdown():
    scheduler = q.TileScheduler(source(), options(work_bytes=8))
    first = scheduler.submit(q.RegionRequest(0, 0, 0, 1, 1))
    assert scheduler.wait_idle(5)
    assert scheduler.cancel(first)
    assert not scheduler.cancel(first)
    assert scheduler.take(first).status == q.TileStatus.CANCELLED
    assert scheduler.stats.reserved_bytes == 0
    assert not scheduler.cancel(first)
    with pytest.raises(IndexError, match="consumed"):
        scheduler.take(first)
    scheduler.shutdown()
    scheduler.shutdown()
    assert scheduler.stats.stopped
    with pytest.raises(RuntimeError, match="shut down"):
        scheduler.submit(q.RegionRequest(0, 0, 0, 1, 1))
    with pytest.raises(RuntimeError, match="shut down"):
        scheduler.advance_generation()


def test_lru_bytes_and_entry_limits():
    scheduler = q.TileScheduler(source(), options(workers=1, cache_bytes=8, max_cache_entries=2))
    def read(x):
        ticket = scheduler.submit(q.RegionRequest(x, 0, 0, 1, 1))
        assert scheduler.wait_idle(5)
        return scheduler.take(ticket)
    for x in [0, 1, 0, 2, 1]:
        assert read(x).status == q.TileStatus.READY
    assert scheduler.stats.reads == 4  # Touching 0 evicted 1, so its last read decodes again.
    assert scheduler.stats.cache_hits == 1
    assert scheduler.stats.cache_entries == 2
    assert scheduler.stats.cache_bytes == 8
    scheduler.clear_cache()
    assert scheduler.stats.cache_bytes == 0
    scheduler.shutdown()


@pytest.mark.parametrize("values", [{"workers": 0}, {"workers": 65}, {"work_bytes": 0}, {"max_requests": 0}])
def test_invalid_configuration(values):
    with pytest.raises(ValueError, match="limits"):
        q.TileScheduler(source(), options(**values))


def test_oversized_tile_and_bad_timeout():
    scheduler = q.TileScheduler(source(), options(work_bytes=8, cache_bytes=0))
    with pytest.raises(ValueError, match="work budget"):
        scheduler.submit(q.RegionRequest(0, 0, 0, 2, 1))
    with pytest.raises(ValueError, match="timeout"):
        scheduler.wait_idle(float("nan"))
    with pytest.raises(ValueError, match="timeout"):
        scheduler.wait_idle(-1)
    scheduler.shutdown()
