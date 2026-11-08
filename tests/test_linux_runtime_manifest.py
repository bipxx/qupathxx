"""Linux archive/layout/graph/publication controls with independent ELF bytes."""
import copy
import importlib.util
import json
from pathlib import Path
import stat
import subprocess
import sys
import zipfile

import pytest
from test_elf_dependencies import image, versioned_image

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_linux_manifest_tests', ROOT/'tools/ci/linux_runtime_manifest.py')
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


def wheel(tmp_path, replacements=None, remove=(), name='qupathxx-0.1.0-cp312-cp312-linux_x86_64.whl', mode_changes=None):
    contents = {
        'qupathxx/_native.cpython-312-x86_64-linux-gnu.so': image(needed=('bridge.so', 'libc.so.6'), runpath='$ORIGIN', soname=None),
        'qupathxx/bridge.so': image(needed=('libc.so.6', 'libm.so.6'), runpath='$ORIGIN', soname='bridge.so'),
        'qupathxx/qupathxx_batch': image(needed=('bridge.so',), runpath='$ORIGIN', soname=None,
            executable=True, interpreter='/lib64/ld-linux-x86-64.so.2'),
        'qupathxx/qupathxx_region': image(needed=('libc.so.6',), runpath='$ORIGIN', soname=None,
            executable=True, interpreter='/lib64/ld-linux-x86-64.so.2'),
        'qupathxx/readme.txt': b'ordinary package bytes\n',
    }
    contents.update(replacements or {})
    for key in remove:
        del contents[key]
    path = tmp_path/name
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in contents.items():
            entry = zipfile.ZipInfo(name)
            entry.create_system = 3
            entry.external_attr = (mode_changes or {}).get(name, stat.S_IFREG | 0o755) << 16
            archive.writestr(entry, data)
    return path


def test_complete_files_closure_modes_and_exact_interpreter_requirements(tmp_path):
    result = runtime.inspect_wheel(wheel(tmp_path))
    assert result['schema'] == 'qpx.linux-package-manifest/1'
    assert result['python_tag'] == 'cp312' and result['platform'] == 'linux_x86_64'
    assert len(result['files']) == 5 and len(result['binaries']) == 4
    assert all(row['mode'] == 0o755 and len(row['sha256']) == 64 for row in result['files'])
    batch = next(row for row in result['entry_points'] if row['entry_point'].endswith('qupathxx_batch'))
    assert batch['bundled_images'] == ['qupathxx/bridge.so', 'qupathxx/qupathxx_batch']
    assert batch['external_requirements'] == [
        dict(name='/lib64/ld-linux-x86-64.so.2', kind='linux-interpreter'),
        dict(name='libc.so.6', kind='linux-system-runtime'), dict(name='libm.so.6', kind='linux-system-runtime')]
    assert all(not result[key] for key in ['release_ready', 'dynamic_loading_exhaustively_verified',
        'external_runtime_availability_verified', 'symbol_version_availability_verified', 'signing_verified'])
    assert result['static_dependency_closure_complete'] and result['native_executables_python_independent']


def test_case_sensitive_dependencies_and_files_are_preserved(tmp_path):
    path = wheel(tmp_path, {'qupathxx/Bridge.so': image(needed=('libc.so.6',), soname='Bridge.so'),
        'qupathxx/_native.cpython-312-x86_64-linux-gnu.so': image(needed=('Bridge.so',), runpath='$ORIGIN')})
    result = runtime.inspect_wheel(path)
    edge = next(row for row in result['dependency_edges'] if row['source'].endswith('linux-gnu.so'))
    assert edge['target'] == 'qupathxx/Bridge.so'
    assert {'qupathxx/bridge.so', 'qupathxx/Bridge.so'} <= {row['path'] for row in result['files']}


def test_origin_subdirectory_and_parent_normalization(tmp_path):
    path = wheel(tmp_path, {'qupathxx/libs/bridge.so': image(needed=('libc.so.6',)),
        'qupathxx/_native.cpython-312-x86_64-linux-gnu.so': image(needed=('bridge.so',), runpath='${ORIGIN}/./libs/../libs:$ORIGIN')})
    result = runtime.inspect_wheel(path)
    edge = next(row for row in result['dependency_edges'] if row['source'].endswith('linux-gnu.so'))
    assert edge['target'] == 'qupathxx/libs/bridge.so'


def test_runpath_takes_precedence_over_inactive_rpath(tmp_path):
    path = wheel(tmp_path, {'qupathxx/_native.cpython-312-x86_64-linux-gnu.so': image(
        needed=('bridge.so',), runpath='$ORIGIN', rpath='/ignored/build/path')})
    assert runtime.inspect_wheel(path)['static_dependency_closure_complete']


def test_required_versions_are_attached_to_exact_needed_edges(tmp_path):
    path = wheel(tmp_path, {'qupathxx/_native.cpython-312-x86_64-linux-gnu.so': versioned_image()})
    result = runtime.inspect_wheel(path)
    edge = next(row for row in result['dependency_edges'] if row['source'].endswith('linux-gnu.so'))
    assert edge['dependency'] == 'libc.so.6'
    assert [row['name'] for row in edge['required_versions']] == ['GLIBC_2.28', 'GLIBC_2.2.5']
    assert not edge['required_version_availability_verified']


@pytest.mark.parametrize('failure', ['missing_cli', 'missing_extension', 'bad_extension', 'unresolved', 'case',
    'no_runpath', 'rpath_only', 'absolute_search', 'empty_search', 'cwd_search', 'escape_search',
    'environment_search', 'path_dependency', 'system_bundle', 'system_soname', 'python_transitive',
    'python_abi', 'no_exec_mode', 'cli_shared', 'extra_executable', 'interpreter',
    'auxiliary', 'filter', 'audit', 'depaudit', 'version_without_needed', 'fake_so', 'pe', 'mach'])
def test_layout_dependency_search_and_runtime_failures(tmp_path, failure):
    extension = 'qupathxx/_native.cpython-312-x86_64-linux-gnu.so'
    changes, removed, modes = {}, [], {}
    if failure == 'missing_cli': removed = ['qupathxx/qupathxx_region']
    elif failure == 'missing_extension': removed = [extension]
    elif failure == 'bad_extension':
        removed = [extension];changes['qupathxx/_native.cpython-311-x86_64-linux-gnu.so'] = image()
    elif failure in ['unresolved', 'case', 'path_dependency']:
        dependency = {'unresolved': 'libmissing.so', 'case': 'BRIDGE.so', 'path_dependency': '/build/bridge.so'}[failure]
        changes[extension] = image(needed=(dependency,), runpath='$ORIGIN')
    elif failure in ['no_runpath', 'rpath_only', 'absolute_search', 'empty_search', 'cwd_search', 'escape_search', 'environment_search']:
        runpath = {'no_runpath': None, 'rpath_only': None, 'absolute_search': '/tmp',
            'empty_search': '', 'cwd_search': '$ORIGIN:', 'escape_search': '$ORIGIN/../..',
            'environment_search': '$LIB:$ORIGIN'}[failure]
        changes[extension] = image(needed=('bridge.so',), runpath=runpath, rpath='$ORIGIN' if failure == 'rpath_only' else None)
    elif failure == 'system_bundle':
        changes['qupathxx/libc.so.6'] = image(needed=(), soname='libc.so.6')
    elif failure == 'system_soname': changes['qupathxx/disguised.so'] = image(needed=(), soname='libc.so.6')
    elif failure == 'python_transitive':
        changes['qupathxx/bridge.so'] = image(needed=('libpython3.12.so.1.0',))
    elif failure == 'python_abi': changes[extension] = image(needed=('libpython3.11.so.1.0',))
    elif failure == 'no_exec_mode': modes['qupathxx/qupathxx_batch'] = stat.S_IFREG | 0o644
    elif failure == 'cli_shared': changes['qupathxx/qupathxx_batch'] = image()
    elif failure == 'extra_executable': changes['qupathxx/unexpected'] = image(executable=True)
    elif failure == 'interpreter': changes['qupathxx/qupathxx_batch'] = image(executable=True, interpreter='/tmp/custom-loader')
    elif failure in ['auxiliary', 'filter', 'audit', 'depaudit']:
        tag = {'auxiliary': 0x7ffffffd, 'filter': 0x7fffffff, 'audit': 0x6ffffefc, 'depaudit': 0x6ffffefb}[failure]
        changes[extension] = image(extra=[(tag, 'libextra.so')])
    elif failure == 'version_without_needed':
        data = bytearray(versioned_image());data[0x310:0x318] = (4).to_bytes(8, 'little', signed=True);changes[extension] = bytes(data)
    elif failure == 'fake_so': changes['qupathxx/other.so'] = b'not ELF'
    elif failure == 'pe': changes['qupathxx/blob'] = b'MZ'+b'\0'*100
    elif failure == 'mach': changes['qupathxx/blob'] = b'\xcf\xfa\xed\xfe'+b'\0'*100
    with pytest.raises(ValueError):
        runtime.inspect_wheel(wheel(tmp_path, changes, removed, mode_changes=modes))


@pytest.mark.parametrize('name', ['/absolute', '../escape', 'qupathxx/../escape', 'qupathxx//double',
    'qupathxx/./dot', 'qupathxx/control\x01', 'qupathxx/readme.txt/child'])
def test_noncanonical_and_conflicting_archive_paths_fail(tmp_path, name):
    with pytest.raises(ValueError): runtime.inspect_wheel(wheel(tmp_path, {name: b'bad'}))


@pytest.mark.parametrize('changes', [dict(max_files=1), dict(max_file_bytes=10), dict(max_total_bytes=100),
                                  dict(max_dependency_edges=1), dict(max_files=10001), dict(max_file_bytes=0)])
def test_file_and_edge_quotas(tmp_path, changes):
    with pytest.raises(ValueError, match='budget'): runtime.inspect_wheel(wheel(tmp_path), **changes)


def test_cycles_do_not_rescan_or_repeat_images(tmp_path):
    path = wheel(tmp_path, {'qupathxx/bridge.so': image(needed=('other.so',), runpath='$ORIGIN'),
        'qupathxx/other.so': image(needed=('bridge.so', 'libc.so.6'), runpath='$ORIGIN')})
    result = runtime.inspect_wheel(path)
    batch = next(row for row in result['entry_points'] if row['entry_point'].endswith('qupathxx_batch'))
    assert batch['bundled_images'] == ['qupathxx/bridge.so', 'qupathxx/other.so', 'qupathxx/qupathxx_batch']


def test_symlinks_duplicates_normalized_names_and_fake_first_search_result(tmp_path):
    path = wheel(tmp_path, mode_changes={'qupathxx/readme.txt': stat.S_IFLNK | 0o777})
    with pytest.raises(ValueError): runtime.inspect_wheel(path)
    path = wheel(tmp_path)
    with zipfile.ZipFile(path, 'a') as archive:
        with pytest.warns(UserWarning): archive.writestr('qupathxx/readme.txt', b'duplicate')
    with pytest.raises(ValueError): runtime.inspect_wheel(path)
    path = wheel(tmp_path, {'qupathxx/xXdata': b'payload'})
    path.write_bytes(path.read_bytes().replace(b'qupathxx/xXdata', b'qupathxx/x\0data'))
    with pytest.raises(ValueError): runtime.inspect_wheel(path)
    path = wheel(tmp_path, {'qupathxx/first/bridge.so': b'not native',
        'qupathxx/_native.cpython-312-x86_64-linux-gnu.so': image(needed=('bridge.so',), runpath='$ORIGIN/first:$ORIGIN')})
    with pytest.raises(ValueError): runtime.inspect_wheel(path)


def test_cli_new_publication_exact_verification_and_json_controls(tmp_path):
    path = wheel(tmp_path)
    output = tmp_path/'manifest.json'
    command = [sys.executable, '-I', str(ROOT/'tools/ci/linux_runtime_manifest.py'), '--wheel', str(path)]
    assert subprocess.run(command+['--output', str(output)], capture_output=True).returncode == 0
    original = output.read_bytes()
    assert subprocess.run(command+['--verify', str(output)], capture_output=True).returncode == 0
    assert subprocess.run(command+['--output', str(output)], capture_output=True).returncode != 0
    assert output.read_bytes() == original
    expected = json.loads(original)
    for kind in ['file_hash', 'missing_file', 'mode', 'edge', 'closure', 'release', 'bool_type', 'version_claim', 'duplicate_key']:
        changed = copy.deepcopy(expected)
        if kind == 'file_hash': changed['files'][0]['sha256'] = '0'*64
        elif kind == 'missing_file': changed['files'].pop()
        elif kind == 'mode': changed['files'][0]['mode'] = 0o777
        elif kind == 'edge': changed['dependency_edges'].pop()
        elif kind == 'closure': changed['entry_points'][0]['bundled_images'].pop()
        elif kind == 'release': changed['release_ready'] = True
        elif kind == 'bool_type': changed['release_ready'] = 0
        elif kind == 'version_claim': changed['symbol_version_availability_verified'] = True
        encoded = runtime.common.canonical_json(changed)
        if kind == 'duplicate_key': encoded = encoded.replace(b'"release_ready": false', b'"release_ready": false, "release_ready": false')
        output.write_bytes(encoded)
        assert subprocess.run(command+['--verify', str(output)], capture_output=True).returncode != 0, kind
    output.write_bytes(original)
    assert subprocess.run(command+['--verify', str(output)], capture_output=True).returncode == 0
    assert not list(tmp_path.glob('.*.qpx-tmp-*'))
