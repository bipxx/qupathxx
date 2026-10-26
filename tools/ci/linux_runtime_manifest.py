"""Inventory Linux x86-64 wheel files and controlled static ELF dependencies.

No extraction or loading. Package policy requires direct $ORIGIN RUNPATHs for
bundled dependencies; external names and versions are unverified declarations.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import zipfile
import zlib


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    result = importlib.util.module_from_spec(spec)
    exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),result.__dict__)
    return result


common = module('qpx_manifest_common', 'runtime_manifest.py')
elf = module('qpx_manifest_elf', 'elf_dependencies.py')
require = common.require
SCHEMA = 'qpx.linux-package-manifest/1'
SYSTEM = frozenset(('libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 librt.so.1 '
                    'libutil.so.1 libresolv.so.2 libgcc_s.so.1 libstdc++.so.6 libz.so.1 '
                    'ld-linux-x86-64.so.2').split())
INTERPRETERS = frozenset(['/lib64/ld-linux-x86-64.so.2', '/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'])


def canonical_name(value, directory=False):
    require(isinstance(value, str) and bool(value), 'empty Linux archive path')
    if directory:
        require(value.endswith('/'), 'invalid directory name')
        value = value[:-1]
    require(all(part and part not in ('.', '..') for part in value.split('/')), 'noncanonical Linux archive path')
    require(not any(ord(c) < 32 or ord(c) == 127 or c == '\\' for c in value), 'invalid Linux archive path')
    require(len(value.encode('utf-8')) <= 32768, 'Linux archive path exceeds budget')
    return value


def search_directories(source, binary):
    # Restrict packaging to self-contained direct RUNPATH resolution. RPATH-only
    # inheritance needs a separate loader-context policy and is never guessed.
    require(binary['rpath'] is None or binary['runpath'] is not None, 'RPATH-only package requires reviewed inheritance policy')
    result = []
    for entry in binary['runpath'] or []:
        match = re.fullmatch(r'(?:\$ORIGIN|\$\{ORIGIN\})(/[^$\\\x00-\x1f\x7f]*)?', entry)
        require(match is not None, 'require nonempty origin-relative Linux RUNPATH')
        parts = list(PurePosixPath(source).parent.parts)
        for part in (match.group(1) or '').split('/')[1:]:
            if part == '.':
                continue
            if part == '..':
                require(bool(parts), 'RUNPATH escapes wheel root')
                parts.pop()
            else:
                require(bool(part), 'noncanonical Linux RUNPATH')
                parts.append(part)
        path = '/'.join(parts)
        if path not in result:
            result.append(path)
    return result


def external_kind(name, python_tag):
    if name in SYSTEM:
        return 'linux-system-runtime'
    if re.fullmatch(r'libpython3\.[0-9]+d?\.so(?:\.[0-9]+)*', name):
        expected = 'libpython3.'+python_tag[3:]+'.so'
        require(name in (expected, expected+'.1.0'), 'unexpected CPython runtime dependency')
        return 'cpython-host-runtime'
    return None


def closures(roots, edges):
    by_source = {}
    for edge in edges:
        by_source.setdefault(edge['source'], []).append(edge)
    result = []
    for root in roots:
        visited, pending, external = set(), [root], set()
        while pending:
            current = pending.pop()
            if current in visited:
                continue
            visited.add(current)
            for edge in by_source.get(current, []):
                if edge['target'] is not None:
                    pending.append(edge['target'])
                else:
                    external.add((edge['dependency'], edge['resolution']))
        if not root.endswith('.so'):
            require(not any(kind == 'cpython-host-runtime' for name, kind in external), 'native executable transitively depends on Python')
        result.append(dict(entry_point=root, bundled_images=sorted(visited),
                           external_requirements=[dict(name=name, kind=kind) for name, kind in sorted(external)]))
    return result


def inspect_wheel(path, max_file_bytes=256*1024*1024, max_total_bytes=512*1024*1024,
                  max_files=10000, max_dependency_edges=100000):
    path = Path(path)
    require(1 <= max_file_bytes <= 256*1024*1024 and 1 <= max_total_bytes <= 512*1024*1024
            and 1 <= max_files <= 10000 and 1 <= max_dependency_edges <= 100000, 'invalid Linux manifest budgets')
    tags = re.fullmatch(r'qupathxx-[A-Za-z0-9_.+]+(?:-[A-Za-z0-9_.]+)?-(cp3[0-9]+)-\1-(linux_x86_64|manylinux_2_28_x86_64)\.whl', path.name)
    require(tags is not None, 'require qupathxx CPython Linux x86-64 wheel filename')
    python_tag, platform = tags.groups()
    extension = f'qupathxx/_native.cpython-{python_tag[2:]}-x86_64-linux-gnu.so'
    roots = [extension, 'qupathxx/qupathxx_batch', 'qupathxx/qupathxx_region']
    files, binaries, names, directories = [], {}, set(), set()
    total = name_bytes = metadata_bytes = dependencies = 0
    with path.open('rb') as source:
        before, path_before = common.version(os.fstat(source.fileno())), common.version(path.stat())
        require(before[:4] == path_before[:4], 'archive path and opened file differ')
        require(before[2] <= 1024*1024*1024, 'compressed archive exceeds byte budget')
        archive_hash = common.file_hash(source)
        source.seek(0)
        with zipfile.ZipFile(source) as archive:
            require(len(archive.infolist()) <= max_files*2, 'archive entry count exceeds budget')
            for entry in archive.infolist():
                require(entry.orig_filename == entry.filename, 'archive name was normalized or contains NUL')
                directory = entry.is_dir()
                name = canonical_name(entry.filename, directory)
                name_bytes += len(name.encode('utf-8'))
                require(name_bytes <= 4*1024*1024, 'archive name bytes exceed budget')
                require(name not in names, 'duplicate Linux archive path')
                names.add(name)
                mode = entry.external_attr >> 16
                require(stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR), 'archive entry is not regular file/directory')
                require(not entry.flag_bits & 1, 'encrypted archive entry is unsupported')
                if directory:
                    require(entry.file_size == 0 and stat.S_IFMT(mode) != stat.S_IFREG, 'invalid archive directory')
                    directories.add(name)
                    continue
                require(stat.S_IFMT(mode) != stat.S_IFDIR, 'archive file has directory mode')
                require(len(files) < max_files and entry.file_size <= max_file_bytes, 'archive file/count budget exceeded')
                require(entry.file_size <= max_total_bytes-total, 'archive total byte budget exceeded')
                with archive.open(entry) as content:
                    data = content.read(max_file_bytes+1)
                require(len(data) == entry.file_size and len(data) <= max_file_bytes, 'archive file size differs')
                total += len(data)
                files.append(dict(path=name, bytes=len(data), mode=mode & 0o7777, sha256=hashlib.sha256(data).hexdigest()))
                native_name = bool(re.search(r'\.so(?:\.[0-9]+)*$', name)) or name in roots
                require(not data.startswith((b'MZ', b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf'))
                        and PurePosixPath(name).suffix.lower() not in ('.pyd', '.dll', '.exe', '.dylib'), 'non-Linux binary in Linux wheel')
                if data.startswith(b'\x7fELF') or native_name:
                    require(external_kind(PurePosixPath(name).name, python_tag) is None, 'bundled system/host runtime requires reviewed policy')
                    binary = elf.inspect_elf(data)
                    require(binary['soname'] is None or external_kind(binary['soname'], python_tag) is None,
                            'bundled ELF SONAME shadows system/host runtime')
                    require(binary['kind'] != 'executable' or name in roots[1:], 'unexpected executable in Linux wheel')
                    if name == extension:
                        require(binary['kind'] == 'shared-object', 'Python extension is not shared object')
                    if name in roots[1:]:
                        require(binary['kind'] == 'executable' and mode & 0o111, 'native CLI lacks executable image/mode')
                    require(not re.search(r'/_native\.cpython-', name) or name == extension, 'unexpected Python extension ABI')
                    dependencies += len(binary['needed'])+bool(binary['interpreter'])
                    require(dependencies <= max_dependency_edges, 'dependency edge budget exceeded')
                    metadata_bytes += len(common.canonical_json(binary))
                    require(metadata_bytes <= 32*1024*1024, 'ELF metadata exceeds aggregate budget')
                    binaries[name] = binary
        require(common.file_hash(source) == archive_hash and common.version(os.fstat(source.fileno())) == before
                and common.version(path.stat()) == path_before, 'archive changed during inspection')
    files.sort(key=lambda item: item['path'])
    file_names = {item['path'] for item in files}
    for name in file_names | directories:
        require(all(str(parent) not in file_names for parent in PurePosixPath(name).parents), 'archive parent is also file')
    require(all(root in binaries for root in roots), 'missing Linux extension/native entry point')
    require('qupathxx/qupathxx_app' not in binaries, 'GUI Linux package requires reviewed policy')
    edges, externals = [], set()
    for name, binary in sorted(binaries.items()):
        require(not any(binary[field] for field in ['auxiliary', 'filters', 'audit', 'depaudit']), 'ELF filter/audit dependencies require reviewed policy')
        search_paths = search_directories(name, binary)
        versions = {}
        for item in binary['version_requirements']:
            require(item['file'] in binary['needed'], 'ELF version dependency is absent from NEEDED')
            versions.setdefault(item['file'], []).extend(item['versions'])
        for dependency in binary['needed']:
            require(re.fullmatch(r'[A-Za-z0-9_+.-]+', dependency) is not None
                    and dependency not in ('.', '..'), 'require basename Linux dependency')
            target = None
            for directory in search_paths:
                candidate = (directory+'/' if directory else '')+dependency
                if candidate in file_names:
                    require(candidate in binaries and binaries[candidate]['kind'] == 'shared-object', 'bundled dependency is not ELF shared object')
                    target = candidate
                    break
            category = external_kind(dependency, python_tag)
            if target is not None:
                require(category is None, 'bundled host/system runtime shadowing requires reviewed policy')
                resolution = 'bundled-origin-runpath'
            else:
                require(category is not None, 'unresolved Linux dependency: '+name+' -> '+dependency)
                resolution = category
                externals.add((dependency, category))
            edges.append(dict(source=name, dependency=dependency, mechanism='DT_NEEDED', target=target,
                              resolution=resolution, required_versions=versions.get(dependency, []),
                              required_version_availability_verified=False))
        if binary['interpreter']:
            interpreter = binary['interpreter']
            require(interpreter in INTERPRETERS, 'unsupported Linux interpreter')
            externals.add((interpreter, 'linux-interpreter'))
            edges.append(dict(source=name, dependency=interpreter, mechanism='PT_INTERP', target=None,
                              resolution='linux-interpreter', required_versions=[], required_version_availability_verified=False))
    return dict(schema=SCHEMA, archive=path.name, archive_sha256=archive_hash, archive_kind='wheel',
                platform=platform, python_tag=python_tag, mode='headless', files=files, total_bytes=total,
                binaries=[dict(path=name, **binary) for name, binary in sorted(binaries.items())],
                dependency_edges=edges, entry_points=closures(roots, edges),
                external_requirements=[dict(name=name, kind=kind, availability_verified=False) for name, kind in sorted(externals)],
                static_dependency_closure_complete=True, native_executables_python_independent=True,
                dynamic_loading_exhaustively_verified=False, external_runtime_availability_verified=False,
                symbol_version_availability_verified=False, signing_verified=False, release_ready=False,
                scope='Complete wheel file hashes/modes and static ELF64 x86-64 NEEDED/interpreter graph under '
                      'direct origin RUNPATH packaging policy. Version requirements and external prerequisites are declarations. '
                      'No RPATH-only inheritance, filter/audit loading, environment search, symbol/ABI validity, dynamic loading, '
                      'manylinux compliance, runtime availability, signing, licensing or supported-release acceptance.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wheel', type=Path, required=True)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument('--output', type=Path)
    target.add_argument('--verify', type=Path)
    args = parser.parse_args()
    try:
        result = inspect_wheel(args.wheel)
        encoded = common.canonical_json(result)
        if args.verify:
            require(args.verify.stat().st_size <= 64*1024*1024, 'input manifest exceeds budget')
            expected = json.loads(args.verify.read_text(encoding='utf-8'), object_pairs_hook=common.unique_object)
            require(common.canonical_json(expected) == encoded, 'Linux manifest differs from archive or policy')
        else:
            require(args.output.resolve() != args.wheel.resolve(), 'manifest output aliases wheel')
            args.output.parent.mkdir(parents=True, exist_ok=True)
            common.publish_new(args.output, encoded)
        print(json.dumps(dict(schema=SCHEMA, files=len(result['files']), binaries=len(result['binaries']),
                              verified=bool(args.verify), release_ready=False)))
    except (OSError, ValueError, zipfile.BadZipFile, zlib.error, RuntimeError) as error:
        print(json.dumps(dict(state='failed', error=str(error))), file=sys.stderr)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
