"""Live pinned QuPath check of native nested export; each output must be new."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/qupath-hierarchy-0.7.0'
GENERATOR = ROOT / 'tools/upstream/qupath_hierarchy_reference.groovy'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare(expected, actual, tolerance):
    if len(actual) != len(expected) or len({o['name'] for o in actual}) != len(actual):
        raise ValueError('missing, duplicate or extra upstream objects')
    wanted = {o['name']: o for o in expected}
    found = {o['name']: o for o in actual}
    if wanted.keys() != found.keys():
        raise ValueError('upstream object names differ')
    def parents(items):
        names = {o['id']: o['name'] for o in items}
        if len(names) != len(items):
            raise ValueError('duplicate upstream identities')
        return {o['name']: names[o['parent']] if o.get('parent') else None for o in items}
    if parents(expected) != parents(actual):
        raise ValueError('upstream parent links differ')
    for name, old in wanted.items():
        new = found[name]
        uuid.UUID(new['id'])
        for key in ['kind', 'classification', 'class_color']:
            if old.get(key) != new.get(key):
                raise ValueError(f'{name}: upstream {key} differs')
        if sorted(old.get('point_coordinates', [])) != sorted(new.get('point_coordinates', [])):
            raise ValueError(f'{name}: upstream point coordinates differ')
        for key in ['bounds', 'nucleus_bounds']:
            a, b = old.get(key), new.get(key)
            if (a is None) != (b is None) or (a is not None and (len(a) != len(b) or any(not math.isclose(x, y, abs_tol=tolerance, rel_tol=0) for x, y in zip(a, b)))):
                raise ValueError(f'{name}: upstream {key} differs')
        if not math.isclose(old['area'], new['area'], abs_tol=tolerance, rel_tol=0):
            raise ValueError(f'{name}: upstream area differs')
    return {'objects': len(actual), 'parent_links': sum(bool(o.get('parent')) for o in actual), 'geometry_absolute_tolerance': tolerance}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime-cache', type=Path, default=ROOT / 'build/upstream/qupath')
    parser.add_argument('--native-input', type=Path, help='Inspect a real GUI-produced nested export instead of creating one')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {'schema': 'qpx.qupath-hierarchy-roundtrip/1', 'state': 'failed'}
    try:
        import qupathxx as q
        from qupathxx import _native
        manifest = read(FIXTURES / 'manifest.json')
        contract = read(FIXTURES / 'contract.json')
        if sha(GENERATOR) != manifest['generator_sha256'] or sha(FIXTURES / 'contract.json') != manifest['contract_sha256']:
            raise ValueError('reference generator or contract identity differs')
        for name, item in manifest['files'].items():
            path = FIXTURES / name
            if path.stat().st_size != item['bytes'] or sha(path) != item['sha256']:
                raise ValueError('reference fixture identity differs: ' + name)
        spec = importlib.util.spec_from_file_location('qpx_hierarchy_fetch', ROOT / 'tools/fetch_qupath.py')
        fetcher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fetcher)
        executable = fetcher.fetch(args.runtime_cache.resolve())
        if args.native_input is None:
            doc = q.Document(q.Image(256, 192, [255] * 256 * 192 * 3))
            imported = q.load_geojson(doc, str(FIXTURES / 'nested.geojson'), assume_pixels=True)
            if imported.features != contract['objects'] or imported.issues:
                raise ValueError('unexpected native import result')
            q.save_geojson(doc, str(output / 'native-nested.geojson'), nested_hierarchy=True)
        else:
            # Snapshot bytes once. The checker never rewrites the user's export.
            if args.native_input.stat().st_size > 256 * 1024 * 1024:
                raise ValueError('native input exceeds GeoJSON byte limit')
            with args.native_input.open('rb') as stream:
                data = stream.read(256 * 1024 * 1024 + 1)
            if len(data) > 256 * 1024 * 1024:
                raise ValueError('native input exceeds GeoJSON byte limit')
            (output / 'native-nested.geojson').write_bytes(data)
        command = [str(executable), 'script', '--args', str(output / 'qupath'), '--args', str(output / 'native-nested.geojson'), str(GENERATOR)]
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        except subprocess.TimeoutExpired as error:
            (output / 'qupath.log').write_bytes(error.stdout or b'')
            raise
        (output / 'qupath.log').write_bytes(result.stdout)
        if result.returncode or b'QPX_HIERARCHY_REFERENCE_OK 6' not in result.stdout:
            raise ValueError('QuPath capture failed; see qupath.log')
        captured = read(output / 'qupath/reference.json')
        expected = read(FIXTURES / 'reference.json')['created']
        if captured['qupath_version'] != contract['upstream']['version'] or captured['upstream_revision'] != contract['upstream']['revision']:
            raise ValueError('upstream version differs')
        report['comparison'] = compare(expected, captured['native_decoded'], contract['geometry_absolute_tolerance'])
        environment = q.build_environment()
        report.update(state='passed', upstream=contract['upstream'], generator_sha256=sha(GENERATOR), contract_sha256=sha(FIXTURES / 'contract.json'), native={'git_revision': environment.git_revision, 'source_tree_sha256': environment.source_tree_sha256, 'module_sha256': sha(Path(_native.__file__))}, input_kind='gui-export' if args.native_input else 'native-python-export')
        report['native_engine_role'] = 'checker only; external GUI producer identity belongs in its originating receipt' if args.native_input else 'checker and exporter'
    except Exception as error:
        report['error'] = str(error)
    report['files'] = {p.relative_to(output).as_posix(): {'bytes': p.stat().st_size, 'sha256': sha(p)} for p in sorted(output.rglob('*')) if p.is_file()}
    (output / 'report.json').write_bytes((json.dumps(report, indent=2) + '\n').encode('utf-8'))
    print(report['state'], report.get('comparison', report.get('error')))
    return 0 if report['state'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
