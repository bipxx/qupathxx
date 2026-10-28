"""Derive native RGB golden hashes from independent pinned QuPath PNG captures.

This generator performs no interpolation. Native expected RGBA hashes come only
from captured outputs; input bytes are decoded from captured source PNG files.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOLDOUT = ROOT / 'tests/fixtures/qupath-rgb-resampling-holdout-0.7.0'
BASELINE = ROOT / 'tests/fixtures/qupath-rgb-resampling-0.7.0'
FIXTURE = ROOT / 'tests/fixtures/rgb-nominal-v1'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_holdout(folder=HOLDOUT):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    contract = json.loads((HOLDOUT / 'contract.json').read_text())
    expected = {'contract.json', 'reference.json',
        *(case['name'] + '/input.png' for case in contract['cases']),
        *(case['name'] + '/' + request['name'] + '.png' for case in contract['cases'] for request in case['requests'])}
    upstream = json.loads((ROOT / 'tests/fixtures/qupath-0.7.0/contract.json').read_text())['upstream']
    generator = (ROOT / 'tools/upstream/qupath_rgb_resampling.groovy').read_text(encoding='utf-8').encode()
    if (manifest['schema'] != 'qpx.qupath-rgb-holdout-manifest/1' or manifest['upstream'] != upstream
            or manifest['generator_sha256'] != hashlib.sha256(generator).hexdigest()
            or manifest['contract_sha256'] != sha(HOLDOUT / 'contract.json')
            or set(manifest['files']) != expected):
        raise ValueError('RGB holdout identity differs')
    for name, entry in manifest['files'].items():
        path = folder / name
        if path.stat().st_size != entry['bytes'] or sha(path) != entry['sha256']:
            raise ValueError('RGB holdout bytes changed: ' + name)
    reference = json.loads((folder / 'reference.json').read_text())
    identities = [(case['name'], request['name']) for case in contract['cases'] for request in case['requests']]
    if (len(identities) != 63 or reference['qupath_version'] != '0.7.0' or reference['java_version'] != '25.0.2'
            or [(v['case_name'], v['request_name']) for v in reference['cases']] != identities):
        raise ValueError('RGB holdout reference differs')
    return reference


def generate(destination):
    import importlib.util
    import numpy as np
    from PIL import Image
    spec = importlib.util.spec_from_file_location('qpx_rgb_baseline', ROOT / 'tools/qupath_rgb_resampling.py')
    baseline = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(baseline)
    _, first = baseline.verify_capture()
    second = verify_holdout()
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    (destination / 'sources').mkdir()
    lines = ['qpx.rgb-nominal-golden/1']
    for prefix, folder, reference in [('baseline', BASELINE, first), ('holdout', HOLDOUT, second)]:
        inputs = {}
        for item in reference['cases']:
            name = item['case_name']
            source_file = 'sources/' + prefix + '_' + name + '.rgb'
            if name not in inputs:
                array = np.asarray(Image.open(folder / name / 'input.png').convert('RGB'))
                inputs[name] = array
                (destination / source_file).write_bytes(array.tobytes())
            source = inputs[name]
            output = np.asarray(Image.open(folder / name / (item['request_name'] + '.png')).convert('RGB'))
            if output.shape[:2] != (item['height'], item['width']):
                raise ValueError('RGB capture shape differs')
            rgba = np.concatenate((output, np.full((*output.shape[:2], 1), 255, dtype=np.uint8)), axis=2)
            fields = [prefix + '_' + name + '_' + item['request_name'], source_file,
                source.shape[1], source.shape[0], *item['bounds'], repr(float(item['downsample'])),
                item['width'], item['height'], hashlib.sha256(rgba.tobytes()).hexdigest()]
            lines.append(' '.join(map(str, fields)))
    if len(lines) != 100:
        raise ValueError('Native RGB golden case count differs')
    (destination / 'index.txt').write_bytes(('\n'.join(lines) + '\n').encode())
    manifest = dict(schema='qpx.rgb-nominal-golden-manifest/1', cases=99,
        baseline_manifest_sha256=sha(BASELINE / 'manifest.json'), holdout_manifest_sha256=sha(HOLDOUT / 'manifest.json'),
        generator_sha256=sha(__file__), interpolation_performed=False,
        files={p.relative_to(destination).as_posix(): dict(bytes=p.stat().st_size, sha256=sha(p))
               for p in sorted(destination.rglob('*')) if p.is_file()})
    (destination / 'manifest.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode())
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    print(json.dumps(generate(parser.parse_args().output), indent=2))
