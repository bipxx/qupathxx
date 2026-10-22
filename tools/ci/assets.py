"""Verified CI SDKs; Mesa is test-only and never added to distribution artifacts."""
import hashlib
import json
import ntpath
import os
import platform
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import tarfile
import urllib.request
import zipfile

SPEC = json.loads(Path(__file__).with_name('runtime-assets.json').read_text(encoding='utf8'))


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def download(spec, cache):
    cache = Path(cache).resolve()
    cache.mkdir(parents=True, exist_ok=True)
    target = cache/spec['filename']
    if not target.exists():
        with tempfile.NamedTemporaryFile(dir=cache, suffix='.partial', delete=False) as stream:
            temporary = Path(stream.name)
            try:
                request = urllib.request.Request(spec['url'], headers={'User-Agent': 'qupathxx-ci'})
                with urllib.request.urlopen(request, timeout=60) as source:
                    total = 0
                    while chunk := source.read(1024*1024):
                        total += len(chunk)
                        if total > spec['bytes']:
                            raise ValueError('SDK exceeds pinned size')
                        stream.write(chunk)
                stream.close()
                if total != spec['bytes'] or sha(temporary) != spec['sha256']:
                    raise ValueError('SDK checksum mismatch')
                temporary.replace(target)
            finally:
                stream.close()
                temporary.unlink(missing_ok=True)
    if target.stat().st_size != spec['bytes'] or sha(target) != spec['sha256']:
        raise ValueError('cached SDK checksum mismatch')
    return target


def openslide(cache):
    if os.name != 'nt':
        if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
            raise ValueError('verified OpenSlide SDK supports Windows x64 and Linux x86_64')
        return openslide_linux(cache)
    spec = SPEC['openslide']
    archive = download(spec, cache)
    target = Path(cache).resolve()/'openslide'
    if not target.exists():
        with tempfile.TemporaryDirectory(dir=Path(cache).resolve()) as temporary:
            stage = Path(temporary)/'sdk'
            stage.mkdir()
            with zipfile.ZipFile(archive) as bundle:
                for info in bundle.infolist():
                    path = PurePosixPath(info.filename)
                    if path.is_absolute() or '..' in path.parts or ntpath.splitdrive(info.filename)[0] or '\\' in info.filename:
                        raise ValueError('invalid SDK archive path')
                bundle.extractall(stage)
            stage.rename(target)
    # Verify every extracted file against the pinned archive, including licenses.
    with zipfile.ZipFile(archive) as bundle:
        for info in bundle.infolist():
            if info.is_dir():
                continue
            path = (target/info.filename).resolve()
            if not path.is_relative_to(target) or path.stat().st_size != info.file_size:
                raise ValueError('SDK extraction differs')
            with bundle.open(info) as source:
                expected = hashlib.file_digest(source, 'sha256').hexdigest()
            if sha(path) != expected:
                raise ValueError('cached SDK file changed: '+info.filename)
    return target/spec['prefix']


def openslide_linux(cache):
    spec = SPEC['openslide_linux']
    archive = download(spec, cache)
    target = Path(cache).resolve()/'openslide-linux'
    with tarfile.open(archive, 'r:xz') as bundle:
        members = bundle.getmembers()
        for member in members:
            path = PurePosixPath(member.name)
            if path.is_absolute() or '..' in path.parts or ntpath.splitdrive(member.name)[0] or '\\' in member.name:
                raise ValueError('invalid SDK archive path')
            if not (member.isfile() or member.isdir() or member.issym()):
                raise ValueError('unsupported SDK archive entry')
            if member.issym() and (PurePosixPath(member.linkname).is_absolute() or ntpath.splitdrive(member.linkname)[0] or '\\' in member.linkname):
                raise ValueError('invalid SDK archive link')
        if not target.exists():
            with tempfile.TemporaryDirectory(dir=Path(cache).resolve()) as temporary:
                stage = Path(temporary)/'sdk'
                stage.mkdir()
                bundle.extractall(stage, filter='data')
                stage.rename(target)
        for member in members:
            path = target/member.name
            if not path.resolve().is_relative_to(target):
                raise ValueError('SDK extraction escapes its cache')
            if member.isdir():
                if not path.is_dir():
                    raise ValueError('SDK directory changed')
            elif member.issym():
                if not path.is_symlink() or os.readlink(path) != member.linkname:
                    raise ValueError('SDK link changed: '+member.name)
            else:
                if path.is_symlink() or not path.is_file() or path.stat().st_size != member.size:
                    raise ValueError('SDK extraction differs: '+member.name)
                with bundle.extractfile(member) as source:
                    expected = hashlib.file_digest(source, 'sha256').hexdigest()
                if sha(path) != expected:
                    raise ValueError('cached SDK file changed: '+member.name)
    return target/spec['prefix']


def mesa(cache):
    spec = SPEC['mesa']
    archive = download(spec, cache)
    target = Path(cache).resolve()/'mesa'
    if not target.exists():
        with tempfile.TemporaryDirectory(dir=Path(cache).resolve()) as temporary:
            stage = Path(temporary)/'sdk'
            stage.mkdir()
            # Windows Server 2022 tar lacks LZMA; use the runner's 7-Zip.
            extractor = shutil.which('7z') or str(Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'7-Zip/7z.exe')
            if not Path(extractor).is_file():
                raise RuntimeError('Mesa extraction requires 7-Zip (7z.exe)')
            subprocess.run([extractor, 'x', '-y', '-o'+str(stage), str(archive)], check=True, stdout=subprocess.DEVNULL)
            stage.rename(target)
    for name, expected in spec['files'].items():
        if sha(target/name) != expected:
            raise ValueError('cached Mesa runtime changed: '+name)
    return target/'x64'


def software_python(destination, mesa_root):
    """Put the actual Python executable and Mesa in a new, owned directory.

    No base interpreter, system OpenGL file or global environment is changed.
    An explicit _pth keeps the current environment's installed packages.
    """
    import sys
    import sysconfig
    if sys.platform != 'win32':
        raise ValueError('software Python launcher is Windows-only')
    for name in ['opengl32.dll', 'libgallium_wgl.dll']:
        if sha(Path(mesa_root)/name) != SPEC['mesa']['files']['x64/'+name]:
            raise ValueError('software renderer identity differs')
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=False)
    base = Path(sys.base_prefix)
    for name in ['python.exe', f'python{sys.version_info.major}{sys.version_info.minor}.dll', 'python3.dll',
                 'vcruntime140.dll', 'vcruntime140_1.dll']:
        if (base/name).is_file():
            shutil.copy2(base/name, destination/name)
    for name in ['opengl32.dll', 'libgallium_wgl.dll']:
        shutil.copy2(Path(mesa_root)/name, destination/name)
    paths = [str(base/'Lib'), str(base/'DLLs'), sysconfig.get_paths()['purelib'], 'import site']
    (destination/f'python{sys.version_info.major}{sys.version_info.minor}._pth').write_text('\n'.join(paths)+'\n', encoding='utf8')
    return destination/'python.exe'


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--software-python', type=Path, required=True)
    parser.add_argument('--mesa-root', type=Path, required=True)
    args = parser.parse_args()
    print(software_python(args.software_python, args.mesa_root))
