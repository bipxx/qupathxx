"""Record the exact local MSVC inputs and products for the AnnData Zarr addon."""
import argparse
import json
from pathlib import Path

from native_build_inputs import msvc_inputs, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    cache = args.cache.resolve()
    expected = {
        'qupathxx_anndata_zarr_native': 'cpp/format_import.cpp',
        'qpx_anndata_zarr_plugin': 'plugins/anndata_zarr/anndata_zarr.cpp',
        'anndata_format_cli': 'cpp/format_import_main.cpp',
        'anndata_zarr_consumer': 'examples/anndata_zarr_plugin/main.cpp',
        'anndata_zarr_tests': 'cpp/tests/anndata_zarr_tests.cpp',
        'anndata_zarr_offline_tests': 'cpp/tests/anndata_zarr_tests.cpp',
        'anndata_matrix_control': 'cpp/tests/matrix_store_test_module.cpp',
        'anndata_c_header_tests': 'cpp/tests/format_plugin_c_header_test.c',
        'anndata_legacy_host_tests': 'cpp/tests/format_plugin_tests.cpp',
        'anndata_legacy_import_tests': 'cpp/tests/format_import_tests.cpp',
        'anndata_loom_tests': 'cpp/tests/loom_tests.cpp',
    }
    sources, groups, logs, commands = msvc_inputs(root, cache, lambda name: name in expected)
    for name, source in expected.items():
        assert source in groups.get(name, ()), (name, source)
    products = {}
    for directory in ('anndata-zarr/Release', 'anndata-zarr-host/Release', 'anndata-zarr-provider/Release'):
        for path in (cache / directory).iterdir():
            if path.is_file() and path.suffix.lower() in ('.exe', '.dll', '.lib', '.json'):
                products[path.relative_to(root).as_posix()] = sha(path)
    result = {'schema': 'qpx.anndata-zarr-native-inputs/1', 'sources': dict(sorted(sources.items())),
              'groups': groups, 'read_logs': logs, 'commands': commands, 'products': products,
              'profile': {'msvc': 1944, 'compiler': '19.44.35225.0', 'toolset': '14.44.35207',
                          'configuration': 'Release', 'runtime': 'MD', 'cpp': 17}}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(sources)} inputs, {len(groups)} targets, {len(products)} products')


if __name__ == '__main__':
    main()
