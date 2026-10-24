"""Inventory a Windows qupathxx wheel without extracting or loading its binaries.

Hashes every file and records a bounded static PE dependency graph. Dynamic
loading, OS/runtime availability, signing and supported release are separate.
"""
import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tempfile
import zipfile
import zlib

spec = importlib.util.spec_from_file_location('qpx_pe_dependencies', Path(__file__).with_name('pe_dependencies.py'))
pe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pe)

SCHEMA = 'qpx.windows-package-manifest/1'
OS_DLLS = frozenset(('advapi32 bcrypt cabinet cfgmgr32 comctl32 comdlg32 crypt32 d3d11 dbghelp dnsapi dwmapi dxgi '
                    'gdi32 imm32 iphlpapi kernel32 mpr msimg32 ncrypt netapi32 normaliz ntdll ole32 oleaut32 '
                    'opengl32 powrprof propsys psapi rpcrt4 secur32 setupapi shell32 shlwapi user32 userenv '
                    'usp10 uxtheme version winhttp wininet winmm winspool wintrust ws2_32 wtsapi32').split())
MSVC_DLLS = frozenset(('concrt140.dll msvcp140.dll msvcp140_1.dll msvcp140_2.dll '
                      'vcomp140.dll vcruntime140.dll vcruntime140_1.dll').split())
RESERVED = frozenset(['con', 'prn', 'aux', 'nul', 'conin$', 'conout$']+
                     ['com'+str(i) for i in range(1, 10)]+['lpt'+str(i) for i in range(1, 10)]+
                     [prefix+digit for prefix in ['com', 'lpt'] for digit in '¹²³'])


def require(condition, message):
    if not condition:
        raise ValueError(message)


def canonical_name(name, directory=False):
    require(isinstance(name, str) and bool(name), 'empty archive name')
    if directory:
        require(name.endswith('/'), 'invalid archive directory')
        name = name[:-1]
    parts = name.split('/')
    require(all(part and part not in ('.', '..') for part in parts), 'noncanonical archive path')
    for part in parts:
        require(not any(ord(c) < 32 or ord(c) == 127 or c in '\\:<>"|?*' for c in part), 'invalid Windows archive path')
        require(not part.endswith(('.', ' ')), 'Windows archive name alias')
        require(part.split('.')[0].rstrip(' ').lower() not in RESERVED, 'reserved Windows archive name')
    require(len(name.encode('utf-8')) <= 32768, 'archive path exceeds budget')
    return name


def version(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def file_hash(stream):
    stream.seek(0)
    digest = hashlib.sha256()
    while True:
        chunk = stream.read(1024*1024)
        if not chunk:
            return digest.hexdigest()
        digest.update(chunk)


def external_kind(name, python_dll):
    if re.fullmatch(r'python[0-9]+(?:_d)?\.dll', name):
        return 'cpython-host-runtime'
    if name in MSVC_DLLS:
        return 'msvc-runtime'
    if name[:-4] in OS_DLLS:
        return 'windows-system'
    if re.fullmatch(r'(?:api|ext)-ms-win-[a-z0-9-]+\.dll', name):
        return 'windows-api-set'
    return None


def _dependency_closures(roots, edges, python_dll):
    # Build once, then visit only the outgoing edges of each reached image.
    # Preserve original edge order so the manifest bytes remain unchanged.
    by_source = {}
    for edge in edges:
        by_source.setdefault(edge['source'], []).append(edge)
    closures = []
    for root in roots:
        visited, pending, requirements = set(), [root], set()
        while pending:
            current = pending.pop()
            if current in visited:
                continue
            visited.add(current)
            for edge in by_source.get(current, ()):
                if edge['target']:
                    pending.append(edge['target'])
                else:
                    requirements.add((edge['dependency'], edge['resolution']))
        if root.endswith('.exe'):
            require(not any(category == 'cpython-host-runtime' for _, category in requirements), 'native executable transitively depends on Python')
        else:
            require((python_dll, 'cpython-host-runtime') in requirements, 'native extension lacks expected CPython runtime')
        closures.append(dict(entry_point=root, bundled_images=sorted(visited),
                             external_requirements=[dict(name=name, kind=kind) for name, kind in sorted(requirements)]))
    return closures


def inspect_archive(path, gui=False, max_file_bytes=256*1024*1024, max_total_bytes=512*1024*1024, max_files=10000, max_dependency_edges=100000, native_archive=False):
    path = Path(path)
    require(1 <= max_file_bytes <= 256*1024*1024 and 1 <= max_total_bytes <= 1024*1024*1024 and
            1 <= max_files <= 10000 and 1 <= max_dependency_edges <= 100000, 'invalid manifest budgets')
    python_tag = python_dll = None
    if native_archive:
        require(path.name == 'qupathxx-windows-x64-candidate.zip', 'require named Windows native candidate archive')
    else:
        tags = re.fullmatch(r'qupathxx-[A-Za-z0-9_.+]+(?:-[A-Za-z0-9_.]+)?-(cp3[0-9]+)-\1-win_amd64\.whl', path.name)
        require(tags is not None, 'require qupathxx CPython Windows x64 wheel filename')
        python_tag = tags.group(1)
        python_dll = 'python'+python_tag[2:]+'.dll'
    files, binaries, names, directories = [], {}, set(), set()
    total = 0
    dependency_count = 0
    name_bytes = 0
    with path.open('rb') as source:
        before = version(os.fstat(source.fileno()))
        path_before = version(path.stat())
        require(path_before[:4] == before[:4], 'archive path and opened file differ')
        require(before[2] <= 1024*1024*1024, 'compressed archive exceeds byte budget')
        archive_hash = file_hash(source)
        source.seek(0)
        with zipfile.ZipFile(source) as archive:
            require(len(archive.infolist()) <= max_files*2, 'archive entry count exceeds budget')
            for item in archive.infolist():
                require(item.orig_filename == item.filename, 'archive name was normalized or contains NUL')
                directory = item.is_dir()
                name = canonical_name(item.filename, directory)
                name_bytes += len(name.encode('utf-8'))
                require(name_bytes <= 4*1024*1024, 'archive name bytes exceed budget')
                key = name.casefold()
                require(key not in names, 'duplicate or case-colliding archive path')
                names.add(key)
                mode = item.external_attr >> 16
                require(stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR), 'archive entry is not a regular file/directory')
                require(not (item.flag_bits & 1), 'encrypted archive entry is unsupported')
                if directory:
                    require(item.file_size == 0 and stat.S_IFMT(mode) != stat.S_IFREG, 'invalid archive directory entry')
                    directories.add(key)
                    continue
                require(stat.S_IFMT(mode) != stat.S_IFDIR, 'archive file has directory mode')
                require(len(files) < max_files and item.file_size <= max_file_bytes, 'archive file/count budget exceeded')
                require(item.file_size <= max_total_bytes-total, 'archive total byte budget exceeded')
                with archive.open(item) as content:
                    data = content.read(max_file_bytes+1)
                require(len(data) == item.file_size and len(data) <= max_file_bytes, 'archive file size differs')
                total += len(data)
                files.append(dict(path=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
                suffix = PurePosixPath(name).suffix.lower()
                require(not native_archive or suffix != '.pyd', 'Python extension in native archive')
                require(suffix not in ('.so', '.dylib'), 'non-Windows native binary in Windows wheel')
                if data.startswith(b'MZ') or suffix in ('.pyd', '.dll', '.exe'):
                    require(external_kind(PurePosixPath(name).name.lower(), python_dll) != 'cpython-host-runtime', 'bundled CPython runtime is unsupported')
                    binary = pe.inspect_pe(data)
                    if suffix in ('.pyd', '.dll'):
                        require(binary['kind'] == 'dll', 'DLL filename contains an executable image')
                    elif suffix == '.exe':
                        require(binary['kind'] == 'exe', 'executable filename contains a DLL image')
                    dependency_count += sum(len(binary[field]) for field in ['imports', 'delay_imports', 'export_forwarders'])
                    require(dependency_count <= max_dependency_edges, 'static dependency edge budget exceeded')
                    binaries[name] = binary
        require(file_hash(source) == archive_hash and version(os.fstat(source.fileno())) == before and
                version(path.stat()) == path_before, 'archive changed during manifest inspection')
    files.sort(key=lambda item: item['path'])
    by_case = {item['path'].casefold(): item['path'] for item in files}
    for item in files:
        for parent in PurePosixPath(item['path']).parents:
            if str(parent) != '.':
                require(str(parent).casefold() not in by_case, 'archive parent is also a file')
    for directory in directories:
        for parent in PurePosixPath(directory).parents:
            if str(parent) != '.':
                require(str(parent).casefold() not in by_case, 'archive directory parent is also a file')
    prefix = '' if native_archive else 'qupathxx/'
    roots = [prefix+'qupathxx_batch.exe', prefix+'qupathxx_region.exe']
    if not native_archive:
        roots.insert(0, prefix+f'_native.{python_tag}-win_amd64.pyd')
    if gui:
        roots.append(prefix+'qupathxx_app.exe')
    else:
        require(prefix+'qupathxx_app.exe' not in binaries, 'GUI app in headless package')
    require(all(name in binaries for name in roots), 'required native entry point is missing')
    edges, externals = [], set()
    for name, binary in sorted(binaries.items()):
        for kind, field in [('import', 'imports'), ('delay-import', 'delay_imports'), ('export-forwarder', 'export_forwarders')]:
            for dependency in binary[field]:
                sibling = str(PurePosixPath(name).parent/dependency)
                target = by_case.get(sibling.casefold())
                if target:
                    require(target in binaries and binaries[target]['kind'] == 'dll', 'bundled dependency is not a valid DLL')
                    require(external_kind(dependency, python_dll) is None, 'bundled host/system/runtime DLL requires an explicit reviewed policy')
                    edge = dict(source=name, dependency=dependency, mechanism=kind, resolution='bundled-sibling', target=target)
                else:
                    category = external_kind(dependency, python_dll)
                    require(category is not None, 'unresolved native dependency: '+name+' -> '+dependency)
                    if category == 'cpython-host-runtime':
                        require(not native_archive and dependency == python_dll, 'unexpected CPython runtime dependency')
                    externals.add((dependency, category))
                    edge = dict(source=name, dependency=dependency, mechanism=kind, resolution=category, target=None)
                edges.append(edge)
    closures = _dependency_closures(roots, edges, python_dll)
    return dict(schema=SCHEMA, archive=path.name, archive_sha256=archive_hash, archive_kind='native-zip' if native_archive else 'wheel', mode='gui' if gui else 'headless',
                platform='win_amd64', python_tag=python_tag, files=files, total_bytes=total,
                binaries=[dict(path=name, **binary) for name, binary in sorted(binaries.items())],
                dependency_edges=edges, entry_points=closures,
                external_requirements=[dict(name=name, kind=kind, availability_verified=False) for name, kind in sorted(externals)],
                static_dependency_closure_complete=True, native_executables_python_independent=True,
                dynamic_loading_exhaustively_verified=False, external_runtime_availability_verified=False,
                signing_verified=False, release_ready=False,
                scope='Complete archive file hashes and static PE32+ x64 import, delay-import and export-forwarder graph. Bundled DLLs must be siblings. External prerequisites are declarations, not availability checks. No binaries are extracted or loaded. This does not accept dynamic loader behavior, runtime probes, ABI semantics, signing, licensing, other binary formats or a supported release.')


def inspect_wheel(path, gui=False, **budgets):
    return inspect_archive(path, gui=gui, **budgets)


def canonical_json(data):
    output = io.BytesIO()
    for chunk in json.JSONEncoder(indent=2, sort_keys=True, allow_nan=False).iterencode(data):
        encoded = chunk.encode('utf-8')
        require(output.tell()+len(encoded)+1 <= 64*1024*1024, 'manifest JSON exceeds byte budget')
        output.write(encoded)
    output.write(b'\n')
    return output.getvalue()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate manifest JSON key')
        result[key] = value
    return result


def publish_new(path, data):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.'+path.name+'.qpx-tmp-', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    archive = parser.add_mutually_exclusive_group(required=True)
    archive.add_argument('--wheel', type=Path)
    archive.add_argument('--native-archive', type=Path)
    parser.add_argument('--gui', action='store_true')
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument('--output', type=Path)
    destination.add_argument('--verify', type=Path)
    args = parser.parse_args()
    try:
        input_path = args.wheel or args.native_archive
        result = inspect_archive(input_path, gui=args.gui, native_archive=bool(args.native_archive))
        if args.verify:
            require(args.verify.stat().st_size <= 64*1024*1024, 'input manifest exceeds byte budget')
            expected = json.loads(args.verify.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
            require(canonical_json(expected) == canonical_json(result), 'runtime manifest does not match archive, policy or static dependency graph')
        else:
            require(args.output.resolve() != input_path.resolve(), 'manifest output aliases the wheel')
            args.output.parent.mkdir(parents=True, exist_ok=True)
            # Never replace an earlier manifest or package artifact.
            publish_new(args.output, canonical_json(result))
        print(json.dumps(dict(schema=SCHEMA, files=len(result['files']), binaries=len(result['binaries']), verified=bool(args.verify), release_ready=False)))
    except (OSError, ValueError, zipfile.BadZipFile, zlib.error, RuntimeError) as error:
        print(json.dumps(dict(state='failed', error=str(error))), file=sys.stderr)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
