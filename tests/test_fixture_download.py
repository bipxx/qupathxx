"""Transport retries must never publish or accept corrupt fixture bytes."""
import hashlib
import importlib.util
import io
from pathlib import Path
import urllib.error

import pytest

spec = importlib.util.spec_from_file_location('qpx_fixture_fetch', Path(__file__).resolve().parents[1]/'tools/fetch_reader_fixtures.py')
fetcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetcher)
PAYLOAD = b'complete fixture'
ENTRY = dict(filename='slide.svs', url='https://example.invalid/slide.svs', bytes=len(PAYLOAD),
             sha256=hashlib.sha256(PAYLOAD).hexdigest())


class Interrupted(io.BytesIO):
    def read(self, size=-1):
        if self.tell():
            raise TimeoutError('connection interrupted after partial bytes')
        return super().read(3)


@pytest.mark.parametrize('failure', ['dns', 'partial', 'http503'])
def test_transport_retry_restarts_download_and_publishes_only_verified_bytes(tmp_path, monkeypatch, failure):
    calls, delays = [], []
    def open_url(url, timeout):
        calls.append((url, timeout))
        if len(calls) == 1:
            if failure == 'dns':
                raise urllib.error.URLError('temporary DNS failure')
            if failure == 'http503':
                raise urllib.error.HTTPError(url, 503, 'unavailable', None, None)
            return Interrupted(PAYLOAD)
        return io.BytesIO(PAYLOAD)
    monkeypatch.setattr(fetcher.urllib.request, 'urlopen', open_url)
    monkeypatch.setattr(fetcher.time, 'sleep', delays.append)
    path = fetcher.fetch(ENTRY, tmp_path)
    assert path.read_bytes() == PAYLOAD and calls == [(ENTRY['url'], 30)]*2 and delays == [1]
    assert not list(tmp_path.glob('*.partial'))


@pytest.mark.parametrize('payload', [b'corrupt fixture!', b'longer than the allowed fixture', b''])
def test_bad_identity_is_fatal_without_retry_or_publication(tmp_path, monkeypatch, payload):
    calls = []
    def open_url(*args, **kwargs):
        calls.append(1)
        return io.BytesIO(payload)
    monkeypatch.setattr(fetcher.urllib.request, 'urlopen', open_url)
    monkeypatch.setattr(fetcher.time, 'sleep', lambda _: pytest.fail('integrity failures must not retry'))
    with pytest.raises(RuntimeError, match='Fixture'):
        fetcher.fetch(ENTRY, tmp_path)
    assert calls == [1] and list(tmp_path.iterdir()) == []


@pytest.mark.parametrize('status, attempts, delays', [(404, 1, []), (503, 4, [1, 2, 4])])
def test_transport_attempts_are_bounded_and_permanent_http_errors_do_not_retry(tmp_path, monkeypatch, status, attempts, delays):
    calls, actual_delays = [], []
    def open_url(*args, **kwargs):
        calls.append(1)
        raise urllib.error.HTTPError(ENTRY['url'], status, 'failed', None, None)
    monkeypatch.setattr(fetcher.urllib.request, 'urlopen', open_url)
    monkeypatch.setattr(fetcher.time, 'sleep', actual_delays.append)
    with pytest.raises(urllib.error.HTTPError):
        fetcher.fetch(ENTRY, tmp_path)
    assert len(calls) == attempts and actual_delays == delays and list(tmp_path.iterdir()) == []


@pytest.mark.parametrize('payload', [PAYLOAD, b'corrupt fixture!'])
def test_cache_is_verified_without_network_access(tmp_path, monkeypatch, payload):
    path = tmp_path/ENTRY['filename']
    path.write_bytes(payload)
    monkeypatch.setattr(fetcher.urllib.request, 'urlopen', lambda *a, **kw: pytest.fail('cache must not download'))
    if payload == PAYLOAD:
        assert fetcher.fetch(ENTRY, tmp_path) == path
    else:
        with pytest.raises(RuntimeError, match='checksum/size mismatch'):
            fetcher.fetch(ENTRY, tmp_path)
        assert path.read_bytes() == payload
