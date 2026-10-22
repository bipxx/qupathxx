"""Fetch and verify the pinned optional QuPath comparison runtime (Windows)."""
import argparse
import hashlib
import json
import ntpath
from pathlib import Path, PurePosixPath
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT/'tests/fixtures/qupath-0.7.0/contract.json'


def file_sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def tree_identity(root):
    """SHA256 of sorted (BE uint32 path byte length, UTF-8 path, file digest)."""
    root = Path(root)
    digest, count = hashlib.sha256(), 0
    for path in sorted(root.rglob('*'), key=lambda p: p.relative_to(root).as_posix()):
        if path.is_symlink():
            raise ValueError('runtime may not contain symbolic links')
        if path.is_file():
            name = path.relative_to(root).as_posix().encode('utf8')
            digest.update(len(name).to_bytes(4, 'big'))
            digest.update(name)
            digest.update(bytes.fromhex(file_sha256(path)))
            count += 1
    return digest.hexdigest(), count


def fetch(destination):
    spec = json.loads(CONTRACT.read_text(encoding='utf8'))['upstream']
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination/spec['bundle_filename']
    if not archive.exists():
        # The download is capped independently of the ZIP's own size claims.
        with tempfile.NamedTemporaryFile(dir=destination, suffix='.partial', delete=False) as stream:
            partial = Path(stream.name)
            try:
                request = urllib.request.Request(spec['bundle_url'], headers={'User-Agent': 'qupathxx-reference'})
                with urllib.request.urlopen(request, timeout=60) as response:
                    size = 0
                    while chunk := response.read(1024*1024):
                        size += len(chunk)
                        if size > spec['bundle_bytes']:
                            raise ValueError('QuPath bundle exceeds pinned size')
                        stream.write(chunk)
                stream.close()
                if size != spec['bundle_bytes'] or file_sha256(partial) != spec['bundle_sha256']:
                    raise ValueError('QuPath bundle checksum/size mismatch')
                partial.replace(archive)
            finally:
                stream.close()
                partial.unlink(missing_ok=True)
    if archive.stat().st_size != spec['bundle_bytes'] or file_sha256(archive) != spec['bundle_sha256']:
        raise ValueError('cached QuPath bundle checksum/size mismatch')
    runtime = destination/'runtime'
    if not runtime.exists():
        with tempfile.TemporaryDirectory(prefix='qupath-extract-', dir=destination) as directory:
            stage = Path(directory)/'runtime'
            stage.mkdir()
            with zipfile.ZipFile(archive) as bundle:
                size = 0
                for entry in bundle.infolist():
                    name = entry.filename
                    path = PurePosixPath(name)
                    size += entry.file_size
                    if (path.is_absolute() or ntpath.splitdrive(name)[0] or '..' in path.parts
                            or '\\' in name or '\0' in name or (entry.external_attr >> 16) & 0o170000 == 0o120000
                            or size > 2*1024**3):
                        raise ValueError('unsupported/unsafe QuPath ZIP member')
                    target = stage.joinpath(*path.parts).resolve()
                    if not target.is_relative_to(stage.resolve()):
                        raise ValueError('QuPath ZIP member leaves extraction directory')
                bundle.extractall(stage)
            if tree_identity(stage) != (spec['runtime_tree_sha256'], spec['runtime_files']):
                raise ValueError('extracted QuPath runtime differs from pinned distribution')
            stage.rename(runtime)
    if tree_identity(runtime) != (spec['runtime_tree_sha256'], spec['runtime_files']):
        raise ValueError('cached QuPath runtime differs; use a fresh destination')
    return runtime/spec['executable']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--destination', type=Path, default=ROOT/'build/upstream/qupath')
    args = parser.parse_args()
    print(fetch(args.destination))


if __name__ == '__main__':
    main()
