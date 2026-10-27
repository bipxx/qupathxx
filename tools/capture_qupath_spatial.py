"""Capture a synthetic centroid-distance contract on authenticated QuPath 0.7.0.

This prepares an independent oracle; it does not qualify a native spatial API.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/qupath-spatial-0.7.0'
GENERATOR = ROOT / 'tools/upstream/qupath_spatial_reference.groovy'


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate_reference(contract, reference):
    if (reference.get('schema') != 'qpx.qupath-spatial-reference/1'
            or reference.get('qupath_version') != '0.7.0'
            or reference.get('upstream_revision') != contract['upstream_revision']
            or reference.get('synthetic') is not True):
        raise ValueError('reference identity differs')
    expected = contract['cases']
    observed = reference['cases']
    if [c['name'] for c in observed] != [c['name'] for c in expected]:
        raise ValueError('reference cases missing, reordered or duplicated')
    tolerance = contract['tolerance']
    for case, actual in zip(expected, observed):
        wanted = case['expected']
        source = next(obj for obj in case['objects'] if obj['id'] == case['source'])
        storage = 'FLOAT' if source.get('default_float') else source.get('storage', 'GENERAL')
        classes = {'GENERAL': 'qupath.lib.measurements.DefaultMeasurementList',
                   'DOUBLE': 'qupath.lib.measurements.NumericMeasurementList$DoubleList',
                   'FLOAT': 'qupath.lib.measurements.NumericMeasurementList$FloatList'}
        if actual['measurement_storage'] != classes[storage]:
            raise ValueError(case['name'] + ': measurement storage differs')
        if actual['measured'] is not (wanted['distance'] is not None):
            raise ValueError(case['name'] + ': missing/extra measurement')
        def close(a, b):
            return (isinstance(a, (int, float)) and not isinstance(a, bool)
                    and math.isfinite(a) and math.isclose(a, b,
                        abs_tol=tolerance['absolute'], rel_tol=tolerance['relative']))
        if wanted['distance'] is None:
            if actual['distance'] is not None:
                raise ValueError(case['name'] + ': absent result must be null')
        elif not close(actual['distance'], wanted['distance']):
            raise ValueError(case['name'] + ': distance differs')
        elif storage != 'GENERAL' and actual['distance'] != wanted['distance']:
            raise ValueError(case['name'] + ': stored rounding differs')
        for key in ('source_centroid', 'target_centroids'):
            values = [actual[key]] if key == 'source_centroid' else actual[key]
            targets = [wanted[key]] if key == 'source_centroid' else list(wanted[key])
            if key == 'target_centroids' and 'generated_targets' in case:
                grid = case['generated_targets']
                targets.extend([[grid['start'][axis] + i * grid['step'][axis]
                                 for axis in range(2)] for i in range(grid['count'])])
            if len(values) != len(targets) or any(len(a) != 2 or
                    any(not close(x, y) for x, y in zip(a, b))
                    for a, b in zip(values, targets)):
                raise ValueError(case['name'] + ': ' + key + ' differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    contract = read(FIXTURES / 'contract.json')
    spec = importlib.util.spec_from_file_location('qpx_spatial_fetch', ROOT / 'tools/fetch_qupath.py')
    fetcher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fetcher)
    executable = fetcher.fetch(args.runtime_cache)
    (output / 'contract.json').write_bytes((FIXTURES / 'contract.json').read_bytes())
    (output / 'generator.groovy').write_bytes(GENERATOR.read_bytes())
    command = [str(executable), 'script', '--args', str(output / 'contract.json'),
               '--args', str(output / 'qupath'), str(output / 'generator.groovy')]
    (output / 'command.json').write_text(json.dumps(command, indent=2) + '\n', encoding='utf-8')
    with (output / 'qupath.log').open('wb') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=120)
    if result.returncode:
        raise RuntimeError('QuPath capture failed; retained log: ' + str(output / 'qupath.log'))
    reference = read(output / 'qupath/reference.json')
    validate_reference(contract, reference)
    upstream = read(ROOT / 'tests/fixtures/qupath-0.7.0/contract.json')['upstream']
    report = dict(schema='qpx.spatial-oracle-preparation/1', state='reference_verified',
                  cases=len(contract['cases']), upstream=upstream,
                  contract_sha256=sha(FIXTURES / 'contract.json'),
                  generator_sha256=sha(GENERATOR),
                  reference_sha256=sha(output / 'qupath/reference.json'),
                  native_implemented=False, platform_accepted=False)
    (output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
