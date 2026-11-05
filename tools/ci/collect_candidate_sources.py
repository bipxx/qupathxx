"""Collect pinned dependency source archives for the frozen Windows candidate."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def fetch_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent':'qupathxx-source-handoff'}), timeout=30) as stream:
        return json.load(stream)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cache', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)
    sources = [
        ('geos','3.15.0','https://download.osgeo.org/geos/geos-3.15.0.tar.bz2','d5e5192a686d065eaed082de14dd26244c5c8e02bff16b2c6cce3265f648e00e','geos-3.15.0.tar.bz2'),
        ('opencv','31b0eeea0b44b370fd0712312df4214d4ae1b158','https://codeload.github.com/opencv/opencv/tar.gz/31b0eeea0b44b370fd0712312df4214d4ae1b158','bb15d5c85bab38c1377c0c7dd7ff201b078bcdb41cc4ff9f85ac61aa56b5f631','opencv-31b0eeea0b44b370fd0712312df4214d4ae1b158.tar.gz'),
        ('sqlite','3.54.1','https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip','0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9','sqlite-amalgamation-3540100.zip'),
        ('nlohmann-json','3.12.0','https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz','42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa','json-3.12.0.tar.xz'),
        ('openslide-sdk-and-dependencies','4.0.1.2','https://github.com/openslide/openslide-bin/releases/download/v4.0.1.2/openslide-bin-4.0.1.2.tar.gz','83c9a4a598f3a5fe1dea83e123745e48b94ce7eab99833f04902bdf4fa0af1db','openslide-bin-4.0.1.2.tar.gz'),
        ('zlib','1.3.2','https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz','bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16','zlib-1.3.2.tar.gz'),
        ('libtiff','4.7.1','https://download.osgeo.org/libtiff/tiff-4.7.1.tar.xz','b92017489bdc1db3a4c97191aa4b75366673cb746de0dce5d7a749d5954681ba','tiff-4.7.1.tar.xz'),
        ('tinyxml2','11.0.0','https://codeload.github.com/leethomason/tinyxml2/tar.gz/refs/tags/11.0.0','5556deb5081fb246ee92afae73efd943c889cef0cafea92b0b82422d6a18f289','tinyxml2-11.0.0.tar.gz'),
        ('libjpeg-turbo','3.1.3','https://codeload.github.com/libjpeg-turbo/libjpeg-turbo/tar.gz/refs/tags/3.1.3','3a13a5ba767dc8264bc40b185e41368a80d5d5f945944d1dbaa4b2fb0099f4e5','libjpeg-turbo-3.1.3.tar.gz'),
        ('imgui','b916186e66aa3c02b72c6ca17455516512ed8049','https://codeload.github.com/ocornut/imgui/tar.gz/b916186e66aa3c02b72c6ca17455516512ed8049',None,'imgui-b916186.tar.gz'),
    ]
    glfw = fetch_json('https://api.github.com/repos/glfw/glfw/git/ref/tags/3.4')['object']
    if glfw['type']=='tag':
        glfw = fetch_json(glfw['url'])['object']
    sources.append(('glfw',glfw['sha'],'https://codeload.github.com/glfw/glfw/tar.gz/'+glfw['sha'],None,'glfw-3.4.tar.gz'))
    nano = fetch_json('https://pypi.org/pypi/nanobind/3.1.0/json')
    sdist = next(x for x in nano['urls'] if x['packagetype']=='sdist')
    sources.append(('nanobind-and-robin-map','3.1.0',sdist['url'],sdist['digests']['sha256'],sdist['filename']))

    def obtain(item):
        name,version,url,expected,filename = item
        path = args.cache/filename
        if not path.exists():
            with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'qupathxx-source-handoff'}),timeout=60) as stream, path.open('xb') as target:
                while data:=stream.read(1024*1024): target.write(data)
        with path.open('rb') as stream: digest=hashlib.file_digest(stream,'sha256').hexdigest()
        if expected and digest!=expected:
            raise ValueError('source archive hash mismatch: '+name)
        return dict(component=name,version=version,url=url,filename=filename,
                    sha256=digest,bytes=path.stat().st_size,
                    pin='recorded archive SHA256' if expected else 'resolved immutable upstream commit')

    with ThreadPoolExecutor(max_workers=4) as pool:
        inventory=list(pool.map(obtain,sources))
    nano_path=args.cache/sdist['filename']
    with tarfile.open(nano_path) as tar:
        assert any(x.name.endswith('/include/tsl/robin_map.h') for x in tar.getmembers()), 'nanobind source omits robin-map'
    with tarfile.open(args.cache/'openslide-bin-4.0.1.2.tar.gz') as tar:
        names=[m.name for m in tar.getmembers()]
        assert any('subprojects' in n for n in names) and any('bintool' in n for n in names)
    archive=args.output/'qupathxx-dependency-sources.zip'
    with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_STORED) as target:
        for entry in inventory: target.write(args.cache/entry['filename'],entry['filename'])
        target.writestr('sources.json',json.dumps(inventory,indent=2))
        target.writestr('README.txt','Dependency sources for native source6219a08f.\nEach original archive retains upstream licenses and build recipes.\nUse qupathxx-source-6219a08f.zip for the application, pinned CMake and SDK build scripts.\nOpenSlide source distribution includes its dependency sources and builder.\nNoto fonts are unmodified supplied font assets; notices/source URLs remain in application source and binaries.\nNumPy is separately installed from PyPI, not bundled in the native candidate.\nMSVC/Windows/OpenGL driver are external prerequisites. Test-only Mesa is not shipped.\n')
    with archive.open('rb') as stream: digest=hashlib.file_digest(stream,'sha256').hexdigest()
    result=dict(archive=archive.name,sha256=digest,sources=inventory)
    (args.output/'dependency-sources.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(dict(archives=len(inventory),sha256=digest,bytes=archive.stat().st_size),indent=2))


if __name__=='__main__':
    main()
