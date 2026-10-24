"""Mac build-host snapshots and text oracle controls; no fixture execution."""
import copy
import importlib.util
from pathlib import Path
import stat
import struct
import zipfile

import pytest
from test_macho_dependencies import thin

spec=importlib.util.spec_from_file_location('qpx_mac_capture_tests',Path(__file__).resolve().parents[1]/'tools/ci/macos_native_inventory.py')
runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime)


def inputs(tmp_path,arch='arm64'):
    native=tmp_path/'native';geos=tmp_path/'geos';opencv=tmp_path/'opencv'
    native.mkdir();geos.mkdir();opencv.mkdir()
    cpu=0x0100000c if arch=='arm64' else 0x01000007
    for key in ['qupathxx_batch','qupathxx_region']:(native/key).write_bytes(thin(cpu=cpu,kind=2))
    for sdk in [geos,opencv]:
        (sdk/'share/licenses/sdk').mkdir(parents=True)
        (sdk/'share/licenses/sdk/LICENSE').write_bytes(b'fixture notice\n')
        (sdk/'lib').mkdir()
        (sdk/'lib/fixture.dylib').write_bytes(thin(cpu=cpu))
    return native,dict(geos=geos,opencv=opencv)


def capture(tmp_path):
    native,sdks=inputs(tmp_path)
    archive=tmp_path/'mac-native.zip';manifest=tmp_path/'manifest.json'
    result,sources=runtime.capture(native,sdks,'arm64',archive,manifest)
    return result,sources,archive,manifest


def test_complete_snapshot_reinspection_and_original_bytes(tmp_path):
    result,sources,archive,manifest=capture(tmp_path)
    assert len(result['files'])==6 and len(result['binaries'])==4
    assert all(row['mode']==0o755 for row in result['files'] if row['path'].startswith('native/'))
    assert manifest.read_bytes()==runtime.common.canonical_json(runtime.inspect_archive(archive,'arm64'))
    with zipfile.ZipFile(archive) as snapshot:
        assert all(snapshot.read(key)==source.read_bytes() for key,source in sources.items())
    assert result['native_cli_modes_normalized']
    assert not any(result[key] for key in ['runtime_resolution_complete','relocated_package_validated',
        'external_runtime_availability_verified','dynamic_loading_exhaustively_verified','signing_verified','release_ready'])


@pytest.mark.parametrize('failure',['architecture','native_kind','sdk_kind','notice','missing_native','invalid_dylib'])
def test_capture_failure_never_publishes_partial_outputs(tmp_path,failure):
    native,sdks=inputs(tmp_path)
    if failure=='architecture':(native/'qupathxx_region').write_bytes(thin(kind=2))
    elif failure=='native_kind':(native/'qupathxx_batch').write_bytes(thin(cpu=0x0100000c))
    elif failure=='sdk_kind':(sdks['geos']/'lib/fixture.dylib').write_bytes(thin(cpu=0x0100000c,kind=2))
    elif failure=='notice':(sdks['opencv']/'share/licenses/sdk/LICENSE').unlink()
    elif failure=='missing_native':(native/'qupathxx_batch').unlink()
    elif failure=='invalid_dylib':(sdks['geos']/'lib/fixture.dylib').write_bytes(b'not Mach-O')
    archive=tmp_path/'new.zip';manifest=tmp_path/'new.json'
    with pytest.raises((ValueError,OSError)):runtime.capture(native,sdks,'arm64',archive,manifest)
    assert not archive.exists() and not manifest.exists()
    assert not list(tmp_path.glob('.*.qpx-tmp-*'))


def test_existing_output_is_preserved(tmp_path):
    native,sdks=inputs(tmp_path)
    output=tmp_path/'prior.json';output.write_bytes(b'prior evidence')
    with pytest.raises(ValueError):runtime.capture(native,sdks,'arm64',tmp_path/'new.zip',output)
    assert output.read_bytes()==b'prior evidence' and not (tmp_path/'new.zip').exists()


@pytest.mark.parametrize('failure',['duplicate','parent','traversal','symlink','mode','foreign_arch','missing_cli','unexpected'])
def test_changed_archive_layout_modes_and_binaries_fail(tmp_path,failure):
    result,sources,archive,manifest=capture(tmp_path)
    with zipfile.ZipFile(archive) as original:
        entries=[(copy.copy(info),original.read(info)) for info in original.infolist()]
    if failure=='duplicate':entries.append(entries[0])
    elif failure=='parent':entries.append((zipfile.ZipInfo('sdk/geos/lib/fixture.dylib/child'),b'bad'))
    elif failure=='traversal':entries.append((zipfile.ZipInfo('../escape'),b'bad'))
    elif failure=='symlink':entries[0][0].external_attr=(stat.S_IFLNK|0o777)<<16
    elif failure=='mode':entries[0][0].external_attr=(stat.S_IFREG|0o644)<<16
    elif failure=='foreign_arch':entries[0]=(entries[0][0],thin(kind=2))
    elif failure=='missing_cli':entries=entries[1:]
    elif failure=='unexpected':entries.append((zipfile.ZipInfo('unexpected'),b'bad'))
    target=tmp_path/'changed.zip'
    with zipfile.ZipFile(target,'w') as changed:
        for info,data in entries:
            if not info.external_attr:info.external_attr=(stat.S_IFREG|0o644)<<16
            if failure=='duplicate':
                import warnings
                with warnings.catch_warnings():warnings.simplefilter('ignore',UserWarning);changed.writestr(info,data)
            else:changed.writestr(info,data)
    with pytest.raises(ValueError):runtime.inspect_archive(target,'arm64')


def test_otool_names_spaces_versions_and_duplicates():
    text='binary:\n\t@rpath/lib with spaces.dylib (compatibility version 1.0.0, current version 3.15.2)\n'
    result=runtime.parse_otool(text+text.split('\n',1)[1])
    assert result==[dict(name='@rpath/lib with spaces.dylib',compatibility_version=65536,current_version=0x030f02)]*2


@pytest.mark.parametrize('text',['unrecognized output','binary:\n\tlib (current version 1.0.0)',
    'binary:\n\tlib (compatibility version 1.256.0, current version 1.0.0)',
    'binary:\n\tlib (compatibility version 1.0.0, current version 65536.0.0)'])
def test_bad_otool_output_rejects(text):
    with pytest.raises(ValueError):runtime.parse_otool(text)


def test_expected_links_include_dylib_identity_and_preserve_versions():
    binary=dict(slices=[dict(dylib_id=dict(name='identity',current_version=2,compatibility_version=1),
                            dependencies=[dict(name='dependency',current_version=3,compatibility_version=2,mechanism='weak')])])
    assert runtime.expected_links(binary)==[dict(name='identity',current_version=2,compatibility_version=1),
                                            dict(name='dependency',current_version=3,compatibility_version=2)]
