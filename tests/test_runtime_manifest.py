"""Independent PE byte fixtures and package-manifest corruption controls."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import stat
import struct
import subprocess
import sys
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_runtime_manifest', ROOT/'tools/ci/runtime_manifest.py')
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


def image(imports=(), delayed=(), forwarders=(), dll=True, legacy_delay=False):
    # One file-backed section. RVAs are deliberately different from file offsets.
    data = bytearray(0x4200)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 1, 0, 0, 0, 240, 2 | (0x2000 if dll else 0))
    struct.pack_into('<H', data, 0x98, 0x20b)
    base = 0x400000 if legacy_delay else 0x180000000
    struct.pack_into('<Q', data, 0xb0, base)
    struct.pack_into('<I', data, 0xd4, 0x200)
    struct.pack_into('<I', data, 0x104, 16)
    section = 0x188
    data[section:section+8] = b'.rdata\0\0'
    struct.pack_into('<IIII', data, section+8, 0x4000, 0x1000, 0x4000, 0x200)
    offset = lambda rva: rva-0x1000+0x200
    def directory(index, rva, size):
        struct.pack_into('<II', data, 0x108+index*8, rva, size)
    def name(rva, value):
        encoded = value.encode('ascii')+b'\0'
        data[offset(rva):offset(rva)+len(encoded)] = encoded
    if imports:
        directory(1, 0x1100, (len(imports)+1)*20)
        for at, value in enumerate(imports):
            struct.pack_into('<IIIII', data, offset(0x1100)+at*20, 0, 0, 0, 0x1800+at*80, 0)
            name(0x1800+at*80, value)
    if delayed:
        directory(13, 0x1300, (len(delayed)+1)*32)
        for at, value in enumerate(delayed):
            address = 0x1c00+at*80
            struct.pack_into('<IIIIIIII', data, offset(0x1300)+at*32, 0 if legacy_delay else 1,
                             address+base if legacy_delay else address, 0, 0, 0, 0, 0, 0)
            name(address, value)
    if forwarders:
        directory(0, 0x1500, 0x200)
        struct.pack_into('<II', data, offset(0x1500)+20, len(forwarders), 0)
        struct.pack_into('<I', data, offset(0x1500)+28, 0x1540)
        for at, value in enumerate(forwarders):
            address = 0x1580+at*80
            struct.pack_into('<I', data, offset(0x1540)+at*4, address)
            name(address, value)
    return bytes(data)


def wheel(tmp_path, files=None):
    path = tmp_path/'qupathxx-0.1.0-cp312-cp312-win_amd64.whl'
    content = {'qupathxx/_native.cp312-win_amd64.pyd': image(['python312.dll', 'bridge.dll']),
               'qupathxx/qupathxx_batch.exe': image(['bridge.dll'], dll=False),
               'qupathxx/qupathxx_region.exe': image(['KERNEL32.dll'], dll=False),
               'qupathxx/bridge.dll': image(['kernel32.dll', 'vcruntime140.dll']),
               'qupathxx/readme.txt': b'package bytes\n'}
    if files:
        content.update(files)
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, value in content.items():
            archive.writestr(name, value)
    return path


def test_pe_import_delay_and_forwarder_rvas_are_not_file_offsets():
    result = runtime.pe.inspect_pe(image(['KERNEL32.dll', 'bridge.dll'], ['late.dll'], ['forward.Target', 'ordinal.#7']))
    assert result == dict(format='PE32+', machine='x86_64', kind='dll', imports=['bridge.dll', 'kernel32.dll'],
                          delay_imports=['late.dll'], export_forwarders=['forward.dll', 'ordinal.dll'])
    assert runtime.pe.inspect_pe(image(delayed=['late.dll'], legacy_delay=True))['delay_imports'] == ['late.dll']


@pytest.mark.parametrize('failure', ['dos', 'pe', 'machine', 'magic', 'sections', 'headers', 'directories',
                                     'section_bounds', 'rva_padding', 'import_terminator', 'delay_attributes',
                                     'dll_path', 'forwarder', 'directory_overflow', 'truncated'])
def test_pe_rejects_corrupt_offsets_counts_and_names(failure):
    data = bytearray(image(['bridge.dll'], ['late.dll'], ['forward.Target']))
    if failure == 'dos': data[:2] = b'XX'
    elif failure == 'pe': data[0x80:0x84] = b'NONE'
    elif failure == 'machine': struct.pack_into('<H', data, 0x84, 0xaa64)
    elif failure == 'magic': struct.pack_into('<H', data, 0x98, 0x10b)
    elif failure == 'sections': struct.pack_into('<H', data, 0x86, 97)
    elif failure == 'headers': struct.pack_into('<I', data, 0xd4, 100)
    elif failure == 'directories': struct.pack_into('<I', data, 0x104, 129)
    elif failure == 'section_bounds': struct.pack_into('<I', data, 0x188+20, len(data))
    elif failure == 'rva_padding': struct.pack_into('<I', data, 0x300+12, 0x6000)
    elif failure == 'import_terminator': struct.pack_into('<I', data, 0x108+8+4, 20)
    elif failure == 'delay_attributes': struct.pack_into('<I', data, 0x500, 3)
    elif failure == 'dll_path': data[0xa00:0xa0c] = b'../evil.dll\0'
    elif failure == 'forwarder': data[0x780:0x780+15] = b'invalidsymbol\0\0'
    elif failure == 'directory_overflow': struct.pack_into('<II', data, 0x108+8, 0xfffffff0, 32)
    elif failure == 'truncated': data = data[:0x190]
    with pytest.raises(ValueError): runtime.pe.inspect_pe(bytes(data))


def test_manifest_hashes_every_file_and_closes_bundled_dependencies(tmp_path):
    path = wheel(tmp_path)
    result = runtime.inspect_wheel(path)
    assert len(result['files']) == 5 and len(result['binaries']) == 4
    assert result['archive_sha256'] == hashlib.sha256(path.read_bytes()).hexdigest()
    assert result == runtime.inspect_wheel(path)
    with zipfile.ZipFile(path) as archive:
        assert {f['path']: (f['bytes'], f['sha256']) for f in result['files']} == {
            name: (len(archive.read(name)), hashlib.sha256(archive.read(name)).hexdigest()) for name in archive.namelist()}
    batch = next(root for root in result['entry_points'] if root['entry_point'].endswith('batch.exe'))
    assert batch['bundled_images'] == ['qupathxx/bridge.dll', 'qupathxx/qupathxx_batch.exe']
    assert {'name': 'vcruntime140.dll', 'kind': 'msvc-runtime'} in batch['external_requirements']
    assert not result['release_ready'] and not result['dynamic_loading_exhaustively_verified']


@pytest.mark.parametrize('name', ['../escape.txt', '/absolute.txt', 'qupathxx//empty.txt', 'qupathxx/./dot.txt',
                                  'qupathxx/a:stream', 'qupathxx/end.', 'qupathxx/end ', 'qupathxx/CON.txt',
                                  'qupathxx/COM¹.txt', 'qupathxx/a\\b.txt', 'qupathxx/new\nline'])
def test_manifest_rejects_unportable_archive_paths(tmp_path, name):
    if '\\' in name:
        # ZipInfo on Windows normalizes separators while writing; mutate both
        # raw ZIP filename fields so this is an actual malformed input archive.
        placeholder = name.replace('\\', '_')
        path = wheel(tmp_path, {placeholder: b'bad'})
        data = path.read_bytes()
        path.write_bytes(data.replace(placeholder.encode('utf-8'), name.encode('utf-8')))
    else:
        path = wheel(tmp_path, {name: b'bad'})
    with pytest.raises(ValueError): runtime.inspect_wheel(path)


@pytest.mark.parametrize('failure', ['case_collision', 'parent_file', 'symlink', 'missing_dll', 'fake_dll',
                                     'unknown_dll', 'python_direct', 'python_transitive', 'python_forwarder',
                                     'python_delay', 'wrong_host_abi', 'gui_mode', 'file_budget', 'total_budget'])
def test_manifest_rejects_incomplete_or_wrong_native_closure(tmp_path, failure):
    files, kwargs = {}, {}
    if failure == 'case_collision': files['qupathxx/BRIDGE.DLL'] = image()
    elif failure == 'parent_file': files['qupathxx'] = b'not a directory'
    elif failure == 'missing_dll': files['qupathxx/bridge.dll'] = image(['absent.dll'])
    elif failure == 'fake_dll': files['qupathxx/bridge.dll'] = b'not an image'
    elif failure == 'unknown_dll': files['qupathxx/bridge.dll'] = image(['unknown-runtime.dll'])
    elif failure == 'python_direct': files['qupathxx/qupathxx_batch.exe'] = image(['python312.dll'], dll=False)
    elif failure == 'python_transitive': files['qupathxx/bridge.dll'] = image(['python312.dll'])
    elif failure == 'python_forwarder': files['qupathxx/bridge.dll'] = image(forwarders=['python312.Function'])
    elif failure == 'python_delay': files['qupathxx/bridge.dll'] = image(delayed=['python312.dll'])
    elif failure == 'wrong_host_abi': files['qupathxx/_native.cp312-win_amd64.pyd'] = image(['python311.dll'])
    elif failure == 'gui_mode': kwargs['gui'] = True
    elif failure == 'file_budget': kwargs['max_file_bytes'] = 64
    elif failure == 'total_budget': kwargs['max_total_bytes'] = 64
    path = wheel(tmp_path, files)
    if failure == 'symlink':
        with zipfile.ZipFile(path, 'a') as archive:
            entry = zipfile.ZipInfo('qupathxx/link');entry.create_system = 3;entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(entry, 'bridge.dll')
    with pytest.raises(ValueError): runtime.inspect_wheel(path, **kwargs)


def test_manifest_cli_verifies_types_duplicates_and_preserves_files(tmp_path):
    path = wheel(tmp_path)
    manifest = tmp_path/'manifest.json'
    command = [sys.executable, '-I', str(ROOT/'tools/ci/runtime_manifest.py'), '--wheel', str(path)]
    assert subprocess.run(command+['--output', str(manifest)], capture_output=True).returncode == 0
    original = manifest.read_bytes()
    assert subprocess.run(command+['--verify', str(manifest)], capture_output=True).returncode == 0
    assert subprocess.run(command+['--output', str(manifest)], capture_output=True).returncode == 1
    assert manifest.read_bytes() == original and not list(tmp_path.glob('.*.qpx-tmp-*'))
    # JSON booleans and numbers must not compare equal through Python equality.
    manifest.write_bytes(original.replace(b'"release_ready": false', b'"release_ready": 0'))
    assert subprocess.run(command+['--verify', str(manifest)], capture_output=True).returncode == 1
    manifest.write_bytes(original.replace(b'"release_ready": false', b'"release_ready": true, "release_ready": false'))
    assert subprocess.run(command+['--verify', str(manifest)], capture_output=True).returncode == 1
    before = path.read_bytes()
    assert subprocess.run(command+['--output', str(path)], capture_output=True).returncode == 1
    assert path.read_bytes() == before


def test_manifest_dependency_budget_and_cycle(tmp_path):
    path = wheel(tmp_path, {'qupathxx/bridge.dll': image(['cycle.dll']),
                            'qupathxx/cycle.dll': image(['bridge.dll'])})
    result = runtime.inspect_wheel(path)
    batch = next(root for root in result['entry_points'] if root['entry_point'].endswith('batch.exe'))
    assert batch['bundled_images'] == ['qupathxx/bridge.dll', 'qupathxx/cycle.dll', 'qupathxx/qupathxx_batch.exe']
    with pytest.raises(ValueError, match='edge budget'):
        runtime.inspect_wheel(path, max_dependency_edges=1)


def test_manifest_rejects_raw_nul_names_and_directory_file_conflicts(tmp_path):
    path = wheel(tmp_path, {'qupathxx/xXdata': b'payload'})
    path.write_bytes(path.read_bytes().replace(b'qupathxx/xXdata', b'qupathxx/x\0data'))
    with pytest.raises(ValueError): runtime.inspect_wheel(path)
    path = wheel(tmp_path, {'qupathxx/readme.txt/child/': b''})
    with pytest.raises(ValueError, match='parent is also a file'): runtime.inspect_wheel(path)


def test_manifest_refuses_system_dll_shadowing(tmp_path):
    path = wheel(tmp_path, {'qupathxx/kernel32.dll': image()})
    with pytest.raises(ValueError, match='explicit reviewed policy'): runtime.inspect_wheel(path)


def test_manifest_publication_failure_leaves_no_partial_artifact(tmp_path, monkeypatch):
    target = tmp_path/'new.json'
    def refuse_link(*args):
        raise OSError('publication control')
    monkeypatch.setattr(runtime.os, 'link', refuse_link)
    with pytest.raises(OSError): runtime.publish_new(target, b'complete JSON')
    assert not list(tmp_path.iterdir())


def native_zip(tmp_path, **changes):
    source = wheel(tmp_path)
    destination = tmp_path/'qupathxx-windows-x64-candidate.zip'
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        content = {name.removeprefix('qupathxx/'): original.read(name) for name in original.namelist() if not name.endswith('.pyd')}
        content.update(changes)
        for name, data in content.items(): archive.writestr(name, data)
    return destination


def test_native_archive_inventory_has_no_interpreter_entrypoint(tmp_path):
    path = native_zip(tmp_path)
    result = runtime.inspect_archive(path, native_archive=True)
    assert result['archive_kind'] == 'native-zip' and result['python_tag'] is None
    assert len(result['files']) == 4 and len(result['binaries']) == 3
    assert {entry['entry_point'] for entry in result['entry_points']} == {'qupathxx_batch.exe', 'qupathxx_region.exe'}
    assert all(item['kind'] != 'cpython-host-runtime' for item in result['external_requirements'])
    command = [sys.executable, '-I', str(ROOT/'tools/ci/runtime_manifest.py'), '--native-archive', str(path)]
    target = tmp_path/'native-manifest.json'
    assert subprocess.run(command+['--output', str(target)], capture_output=True).returncode == 0
    assert subprocess.run(command+['--verify', str(target)], capture_output=True).returncode == 0


@pytest.mark.parametrize('failure', ['extension', 'runtime', 'transitive', 'missing_cli', 'gui', 'wrong_name'])
def test_native_archive_rejects_python_and_wrong_layout(tmp_path, failure):
    changes = {}
    if failure == 'extension': changes['_native.cp312-win_amd64.pyd'] = image(['python312.dll'])
    elif failure == 'runtime': changes['python312.dll'] = image()
    elif failure == 'transitive': changes['bridge.dll'] = image(['python312.dll'])
    elif failure == 'missing_cli': changes['qupathxx_batch.exe'] = b'missing'
    path = native_zip(tmp_path, **changes)
    if failure == 'wrong_name':
        other = tmp_path/'renamed.zip';path.rename(other);path = other
    with pytest.raises(ValueError): runtime.inspect_archive(path, native_archive=True, gui=failure == 'gui')


def test_dependency_walk_is_linear_in_source_edge_inspections():
    count = 512
    touches = [0]
    class Edge(dict):
        def __getitem__(self, key):
            if key == 'source':
                touches[0] += 1
                assert touches[0] <= 2*(count+4), 'dependency traversal rescans the global edge table'
            return super().__getitem__(key)
    roots = ['batch.exe', 'region.exe']
    names = [f'module-{i:04}.dll' for i in range(count)]
    edges = [Edge(source=root, dependency=names[0], target=names[0], resolution='bundled-sibling') for root in roots]
    edges += [Edge(source=name, dependency=names[(i+1) % count], target=names[(i+1) % count], resolution='bundled-sibling') for i, name in enumerate(names)]
    edges += [Edge(source=names[-1], dependency='kernel32.dll', target=None, resolution='windows-system'),
              Edge(source=names[0], dependency='vcruntime140.dll', target=None, resolution='msvc-runtime')]
    before = [dict(edge) for edge in edges]
    result = runtime._dependency_closures(roots, edges, None)
    assert touches[0] <= 2*len(edges)
    assert [dict(edge) for edge in edges] == before
    for row, root in zip(result, roots):
        assert row == dict(entry_point=root, bundled_images=sorted([root, *names]), external_requirements=[
            dict(name='kernel32.dll', kind='windows-system'), dict(name='vcruntime140.dll', kind='msvc-runtime')])
