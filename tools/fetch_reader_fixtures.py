"""Fetch public reader fixtures, verifying their committed identities."""
import argparse
import hashlib
import json
import http.client
from pathlib import Path
import sys
import time
import urllib.error
import urllib.request


def sha256(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def fetch(entry, destination):
    path = destination / entry['filename']
    partial = path.with_suffix(path.suffix + '.partial')
    if not path.exists():
        for attempt in range(4):
            try:
                with urllib.request.urlopen(entry['url'], timeout=30) as source, partial.open('wb') as target:
                    size = 0
                    digest = hashlib.sha256()
                    while chunk := source.read(1024*1024):
                        size += len(chunk)
                        if size > entry['bytes']:
                            raise RuntimeError('Fixture exceeds pinned size: '+entry['filename'])
                        target.write(chunk)
                        digest.update(chunk)
                if size != entry['bytes'] or digest.hexdigest() != entry['sha256']:
                    raise RuntimeError('Fixture checksum/size mismatch: '+entry['filename'])
                partial.replace(path)
                break
            except (urllib.error.URLError, TimeoutError, ConnectionError, http.client.HTTPException) as error:
                if isinstance(error, urllib.error.HTTPError) and error.code not in [429, 500, 502, 503, 504]:
                    raise
                if attempt == 3:
                    raise
                print(f'Fixture transport error: {error}; retry {attempt+1}/3', file=sys.stderr, flush=True)
                time.sleep(2**attempt)
            finally:
                partial.unlink(missing_ok=True)
    if path.stat().st_size != entry['bytes'] or sha256(path) != entry['sha256']:
        raise RuntimeError('Fixture checksum/size mismatch: '+entry['filename'])
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extended', action='store_true', help='Also fetch the pinned full multilevel vendor slide (about 169 MiB)')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / "tests/fixtures/manifest.json").read_text(encoding="utf-8"))
    destination = root / "build/fixtures"
    destination.mkdir(parents=True, exist_ok=True)
    for entry in manifest["downloads"]:
        if entry.get('tier', 'smoke') != 'extended' or args.extended:
            print(fetch(entry, destination))


if __name__ == "__main__":
    main()
