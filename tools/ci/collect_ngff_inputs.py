"""Record actual native translation units, headers, flags and retained products.

Development provenance only. MSBuild source markers begin with '^'; target log
basenames may be shortened, so locate them under the exact target .dir folder.
"""
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
    sources, groups, read_logs, commands = msvc_inputs(root, cache,
        lambda target: target.startswith('ngff_') or target in ('qpx_ngff_labels_plugin', 'qupathxx_labels'))
    required = {'ngff_native': 'cpp/label_workflow.cpp',
                'qpx_ngff_labels_plugin': 'plugins/ngff_labels/ngff_labels.cpp',
                'ngff_ngff_label_workflow_tests': 'cpp/tests/ngff_label_workflow_tests.cpp',
                'ngff_json_tree_tests': 'cpp/tests/json_tree_tests.cpp',
                'ngff_label_consumer': 'examples/ngff_label_plugin/main.cpp'}
    for target, source in required.items():
        assert source in groups.get(target, []), (target, source)
    products = {}
    for path in (cache / 'Release').iterdir():
        if path.is_file() and path.suffix in ('.exe', '.dll', '.lib', '.json') and (
            path.name.startswith('ngff_') or path.name in (
                'qupathxx_labels.exe', 'qpx_ngff_labels_plugin.dll', 'ngff-label-reader-plugins.json')):
            products[path.relative_to(root).as_posix()] = sha(path)
    result = {'schema': 'qpx.ngff-native-inputs/2', 'sources': dict(sorted(sources.items())),
              'groups': groups, 'read_logs': read_logs, 'commands': commands, 'products': products,
              'profile': {'msvc': 1944, 'compiler': '19.44.35225.0', 'toolset': '14.44.35207',
                          'configuration': 'Release', 'runtime': 'MD', 'cpp': 17},
              'base_core_sha256': sha(root / 'build/format-family-plugins/sdk/lib/qupathxx/qupathxx_core.lib'),
              'source_capture': 'Translation-unit markers and header reads; exact target folders; resolved on-disk path case'}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    print(json.dumps({'source_inputs': len(sources), 'targets': len(groups), 'products': len(products)}))


if __name__ == '__main__':
    main()
