"""Capture only the actual M05 host/provider/consumer build inputs and products."""
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
        'qupathxx_loom_matrix_native': 'cpp/expression_matrix_metadata.cpp',
        'qpx_loom_plugin': 'plugins/loom/loom.cpp',
        'loom_format_cli': 'cpp/format_import_main.cpp',
        'loom_matrix_consumer': 'examples/loom_matrix_plugin/main.cpp',
        'loom_tests': 'cpp/tests/loom_tests.cpp',
        'loom_offline_tests': 'cpp/tests/loom_tests.cpp',
        'loom_matrix_control': 'cpp/tests/matrix_typed_test_module.cpp',
        'loom_c_header_tests': 'cpp/tests/format_plugin_c_header_test.c',
        'loom_legacy_host_tests': 'cpp/tests/format_plugin_tests.cpp',
        'loom_legacy_import_tests': 'cpp/tests/format_import_tests.cpp',
    }
    sources, groups, logs, commands = msvc_inputs(root, cache, lambda target: target in expected)
    for target, source in expected.items():
        assert source in groups.get(target, []), (target, source)
    products = {}
    for directory in (cache / 'loom/Release', cache / 'loom-host/Release', cache / 'loom-provider/Release'):
        for path in directory.iterdir():
            if path.is_file() and path.suffix in ('.exe', '.dll', '.lib', '.json'):
                products[path.relative_to(root).as_posix()] = sha(path)
    for path in (cache / 'Release').glob('loom_matrix_control.*'):
        if path.suffix == '.lib':
            products[path.relative_to(root).as_posix()] = sha(path)
    sdk = root / 'build/format-family-plugins/sdk'
    inherited = root / 'build/gui-aggregation/catalogue-closure/ngff-labels/package-final'
    result = {'schema': 'qpx.loom-native-inputs/1', 'sources': dict(sorted(sources.items())),
              'groups': groups, 'read_logs': logs, 'commands': commands, 'products': products,
              'profile': {'msvc': 1944, 'compiler': '19.44.35225.0', 'toolset': '14.44.35207',
                          'configuration': 'Release', 'runtime': 'MD', 'cpp': 17},
              'base_core_sha256': sha(sdk / 'lib/qupathxx/qupathxx_core.lib'),
              'retained_application_addon_sha256': sha(inherited / 'native/ngff_native.lib'),
              'source_capture': 'Shared exact-target MSBuild translation-unit/header/command authority; current M05 products only'}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    print(json.dumps({'source_inputs': len(sources), 'targets': len(groups), 'products': len(products)}))


if __name__ == '__main__':
    main()
