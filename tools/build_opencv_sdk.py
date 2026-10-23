"""Build pinned OpenCV core/ml for native classifier training, without UI/codecs."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request

VERSION = '4.11.0'
COMMIT = '31b0eeea0b44b370fd0712312df4214d4ae1b158'
SHA256 = 'bb15d5c85bab38c1377c0c7dd7ff201b078bcdb41cc4ff9f85ac61aa56b5f631'
BYTES = 95080706
OPTIONS = {
    'BUILD_LIST': 'core,ml', 'BUILD_SHARED_LIBS': 'OFF',
    'BUILD_TESTS': 'OFF', 'BUILD_PERF_TESTS': 'OFF', 'BUILD_EXAMPLES': 'OFF',
    'BUILD_opencv_apps': 'OFF', 'BUILD_JAVA': 'OFF',
    'BUILD_opencv_python2': 'OFF', 'BUILD_opencv_python3': 'OFF',
    'BUILD_WITH_STATIC_CRT': 'OFF', 'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
    'WITH_IPP': 'OFF', 'WITH_ITT': 'OFF', 'WITH_OPENCL': 'OFF',
    'WITH_LAPACK': 'OFF', 'WITH_EIGEN': 'OFF', 'WITH_TBB': 'OFF',
    'WITH_OPENMP': 'OFF', 'WITH_PTHREADS_PF': 'OFF',
    'WITH_CUDA': 'OFF', 'WITH_VULKAN': 'OFF', 'WITH_FFMPEG': 'OFF',
    'WITH_GTK': 'OFF', 'WITH_QT': 'OFF', 'OPENCV_ENABLE_NONFREE': 'OFF',
    'WITH_JPEG': 'OFF', 'WITH_PNG': 'OFF', 'WITH_TIFF': 'OFF',
    'WITH_WEBP': 'OFF', 'WITH_OPENJPEG': 'OFF', 'WITH_OPENEXR': 'OFF',
    'WITH_JASPER': 'OFF', 'WITH_AVIF': 'OFF', 'WITH_GDAL': 'OFF',
    'WITH_GDCM': 'OFF', 'PARALLEL_ENABLE_PLUGINS': 'OFF',
    'WITH_PROTOBUF': 'OFF', 'WITH_ADE': 'OFF', 'WITH_FLATBUFFERS': 'OFF',
    'CPU_DISPATCH': '', 'CMAKE_BUILD_TYPE': 'Release',
}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def cmake_config_directory(prefix):
    """Locate the one installed SDK config without assuming a Linux libdir."""
    prefix = Path(prefix)
    candidates = [prefix, prefix/'lib/cmake/opencv4', prefix/'lib64/cmake/opencv4']
    matches = [path for path in candidates if (path/'OpenCVConfig.cmake').is_file()
               and (path/'OpenCVConfig-version.cmake').is_file()]
    if len(matches) != 1:
        raise ValueError('OpenCV SDK must contain exactly one complete CMake config in root, lib or lib64')
    return matches[0]


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix', type=Path, default=root/'build/deps/opencv-sdk')
    parser.add_argument('--build-dir', type=Path, default=root/'build/deps/opencv-build')
    parser.add_argument('--source-dir', type=Path, default=root/'build/deps')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 32:
        parser.error('--jobs must be between 1 and 32')
    deps = root/'build/deps'
    deps.mkdir(parents=True, exist_ok=True)
    archive = deps/f'opencv-{COMMIT}.tar.gz'
    if not archive.exists():
        partial = archive.with_suffix('.partial')
        try:
            urllib.request.urlretrieve(f'https://github.com/opencv/opencv/archive/{COMMIT}.tar.gz', partial)
            if partial.stat().st_size != BYTES or sha(partial) != SHA256:
                raise RuntimeError('OpenCV source checksum mismatch')
            partial.replace(archive)
        finally:
            partial.unlink(missing_ok=True)
    if archive.stat().st_size != BYTES or sha(archive) != SHA256:
        raise RuntimeError('OpenCV source checksum mismatch')
    source_parent = args.source_dir.resolve()
    source_parent.mkdir(parents=True, exist_ok=True)
    source = source_parent/f'opencv-{COMMIT}'
    if not source.exists():
        with tarfile.open(archive) as package:
            for member in package.getmembers():
                if not (source_parent/member.name).resolve().is_relative_to(source_parent):
                    raise ValueError('invalid OpenCV archive path')
            package.extractall(source_parent, filter='data')
    prefix, build = args.prefix.resolve(), args.build_dir.resolve()
    cmake = shutil.which('cmake') or str(root/('.venv/Scripts/cmake.exe' if sys.platform == 'win32' else '.venv/bin/cmake'))
    configure = [cmake, '-S', str(source), '-B', str(build), f'-DCMAKE_INSTALL_PREFIX={prefix}']
    configure += [f'-D{key}={value}' for key, value in OPTIONS.items()]
    if sys.platform == 'win32':
        configure += ['-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCMAKE_CXX_FLAGS=/EHsc /utf-8']
    subprocess.run(configure, check=True)
    subprocess.run([cmake, '--build', str(build), '--config', 'Release', '--parallel', str(args.jobs)], check=True)
    subprocess.run([cmake, '--install', str(build), '--config', 'Release'], check=True)
    notices = prefix/'share/licenses/opencv'
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source/'LICENSE', notices/'LICENSE')
    # core includes the BSD SoftFloat implementation; retain its complete source
    # header. zlib is OpenCV's bundled static FileStorage dependency.
    softfloat = (source/'modules/core/src/softfloat.cpp').read_text(encoding='utf8')
    header = softfloat.split('#include', 1)[0]
    if 'copyright' not in header.lower() or 'redistribution' not in header.lower():
        raise RuntimeError('OpenCV SoftFloat notice not found')
    (notices/'softfloat-notice.txt').write_text(header, encoding='utf8')
    zlib = (source/'3rdparty/zlib/zlib.h').read_text(encoding='utf8')
    header = zlib.split('#ifndef', 1)[0]
    if 'copyright' not in header.lower() or 'permission' not in header.lower():
        raise RuntimeError('OpenCV zlib notice not found')
    (notices/'zlib-notice.txt').write_text(header, encoding='utf8')
    # OpenCV retains BSD/MIT and other notices in individual core/ML files,
    # including notices embedded after the leading header. Keep complete
    # comment blocks rather than assuming the top-level Apache text replaces
    # those conditions. Deduplicate identical blocks, preserving source paths.
    comments = re.compile(r'/\*[\s\S]*?\*/|(?:^[ \t]*//[^\n]*(?:\n|$))+', re.MULTILINE)
    blocks = {}
    for module in ('core', 'ml'):
        for path in sorted((source/'modules'/module).rglob('*')):
            if not path.is_file() or path.suffix not in {'.cpp', '.hpp', '.h', '.inl', '.inc', '.c'}:
                continue
            for match in comments.finditer(path.read_text(encoding='utf8', errors='strict')):
                block = match.group().strip()
                if re.search(r'copyright|redistribution|permission is hereby|public domain|SPDX-License', block, re.IGNORECASE):
                    blocks.setdefault(block, []).append(path.relative_to(source).as_posix())
    if len(blocks) < 10:
        raise RuntimeError('OpenCV source notices unexpectedly absent')
    (notices/'SOURCE-NOTICES.txt').write_text('\n\n'.join('\n'.join(paths)+'\n'+block for block, paths in blocks.items())+'\n', encoding='utf8')
    marker = prefix/'qupathxx-opencv-sdk.json'
    files = {p.relative_to(prefix).as_posix(): sha(p) for p in sorted(prefix.rglob('*')) if p.is_file() and p != marker}
    marker.write_text(json.dumps(dict(schema='qpx.opencv-sdk/1',version=VERSION,commit=COMMIT,source_sha256=SHA256,options=OPTIONS,files=files),indent=2)+'\n',encoding='utf8')
    print(f'OpenCV_DIR={cmake_config_directory(prefix)}')


if __name__ == '__main__':
    main()
