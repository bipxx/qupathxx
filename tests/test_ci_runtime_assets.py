"""Pinned Linux SDK extraction must reject corruption before compilation."""
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import tarfile

import pytest

spec = importlib.util.spec_from_file_location('qpx_runtime_assets', Path(__file__).resolve().parents[1]/'tools/ci/assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


def archive(tmp_path, monkeypatch, name='sdk/lib/reader.so', link=None):
    path = tmp_path/'sdk.tar.xz'
    with tarfile.open(path, 'w:xz') as bundle:
        member = tarfile.TarInfo(name)
        payload = b'verified native runtime'
        member.size = len(payload)
        bundle.addfile(member, io.BytesIO(payload))
        if link:
            member = tarfile.TarInfo('sdk/lib/reader-link.so')
            member.type, member.linkname = tarfile.SYMTYPE, link
            bundle.addfile(member)
    monkeypatch.setitem(assets.SPEC, 'openslide_linux', dict(filename=path.name, bytes=path.stat().st_size,
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), url='https://example.invalid/sdk', prefix='sdk'))


def test_linux_sdk_rechecks_extracted_runtime_even_with_valid_archive(tmp_path, monkeypatch):
    archive(tmp_path, monkeypatch)
    sdk = assets.openslide_linux(tmp_path)
    runtime = sdk/'lib/reader.so'
    assert runtime.read_bytes() == b'verified native runtime'
    runtime.write_bytes(b'corrupted native image')
    with pytest.raises(ValueError, match='extraction differs|cached SDK file changed'):
        assets.openslide_linux(tmp_path)


def test_linux_sdk_rejects_archive_traversal_before_extracting(tmp_path, monkeypatch):
    archive(tmp_path, monkeypatch, '../escape')
    with pytest.raises(ValueError, match='invalid SDK archive path'):
        assets.openslide_linux(tmp_path)
    assert not (tmp_path/'openslide-linux').exists() and not (tmp_path.parent/'escape').exists()


@pytest.mark.skipif(os.name == 'nt', reason='Linux SDK symlink semantics')
def test_linux_sdk_rechecks_link_targets(tmp_path, monkeypatch):
    archive(tmp_path, monkeypatch, link='reader.so')
    sdk = assets.openslide_linux(tmp_path)
    link = sdk/'lib/reader-link.so'
    assert link.resolve() == (sdk/'lib/reader.so').resolve()
    link.unlink()
    link.symlink_to('missing.so')
    with pytest.raises(ValueError, match='SDK link changed'):
        assets.openslide_linux(tmp_path)
