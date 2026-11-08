"""Characterize pinned public QuPath RGB reads without changing native sampling.

The float64 bilinear candidates below are diagnostic mathematical models, not
an implementation of Java rendering or a new quantitative sampling policy.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/qupath-rgb-resampling-0.7.0'
GENERATOR = ROOT / 'tools/upstream/qupath_rgb_resampling.groovy'
UPSTREAM = ROOT / 'tests/fixtures/qupath-0.7.0/contract.json'


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def generator_sha():
    return hashlib.sha256(GENERATOR.read_text(encoding='utf-8').encode('utf-8')).hexdigest()


def patterns():
    import numpy as np
    from PIL import Image
    y, x = np.mgrid[:13, :17]
    return {
        'gradient': np.stack((7*x+3*y, 3*x+11*y, 13*x+2*y), axis=2).astype(np.uint8),
        'checker': np.stack((255*((x+y)%2), 255*(x%2), 255*(y%2)), axis=2).astype(np.uint8),
        'noise': np.random.default_rng(286).integers(0, 256, size=(13, 17, 3), dtype=np.uint8),
        'cells': np.asarray(Image.open(ROOT / 'tests/fixtures/qupath-cells-0.7.0/capture/resampled_reference/input.png').convert('RGB')),
    }


def payloads(contract):
    return {'contract.json', 'reference.json',
            *(case['name'] + '/input.png' for case in contract['cases']),
            *(case['name'] + '/' + request['name'] + '.png'
              for case in contract['cases'] for request in case['requests'])}


def verify_capture(folder=FIXTURE):
    folder = Path(folder)
    contract = read(FIXTURE / 'contract.json')
    manifest = read(folder / 'manifest.json')
    expected = payloads(contract)
    if (manifest['schema'] != 'qpx.qupath-rgb-resampling-manifest/1'
            or manifest['upstream'] != read(UPSTREAM)['upstream']
            or manifest['generator_sha256'] != generator_sha()
            or manifest['contract_sha256'] != sha(FIXTURE / 'contract.json')
            or set(manifest['files']) != expected
            or {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()} != expected | {'manifest.json'}):
        raise ValueError('RGB resampling capture identity differs')
    for name, entry in manifest['files'].items():
        path = folder / name
        if path.stat().st_size != entry['bytes'] or sha(path) != entry['sha256']:
            raise ValueError('RGB resampling capture bytes changed: ' + name)
    if read(folder / 'contract.json') != contract:
        raise ValueError('RGB resampling contract differs')
    reference = read(folder / 'reference.json')
    identities = [(case['name'], request['name']) for case in contract['cases'] for request in case['requests']]
    if (reference['schema'] != 'qpx.qupath-rgb-resampling-capture/1'
            or reference['qupath_version'] != '0.7.0' or reference['java_version'] != '25.0.2'
            or [(v['case_name'], v['request_name']) for v in reference['cases']] != identities):
        raise ValueError('RGB resampling reference identities differ')
    return contract, reference


def capture(folder, runtime_cache):
    if os.name != 'nt':
        raise ValueError('the pinned optional reference runtime capture requires Windows')
    from PIL import Image
    folder = Path(folder).resolve()
    folder.mkdir(parents=True, exist_ok=False)
    contract = read(FIXTURE / 'contract.json')
    (folder / 'contract.json').write_bytes((FIXTURE / 'contract.json').read_bytes())
    for name, rgb in patterns().items():
        (folder / name).mkdir()
        Image.fromarray(rgb).save(folder / name / 'input.png')
    spec = importlib.util.spec_from_file_location('qpx_rgb_runtime', ROOT / 'tools/fetch_qupath.py')
    fetch = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fetch)
    runtime = fetch.fetch(runtime_cache)
    command = [str(runtime), 'script', '--args', str(folder), str(GENERATOR)]
    log = folder.parent / (folder.name + '-runtime.log')
    with log.open('xb') as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                                timeout=180, creationflags=subprocess.CREATE_NO_WINDOW)
    result.check_returncode()
    manifest = dict(schema='qpx.qupath-rgb-resampling-manifest/1', upstream=read(UPSTREAM)['upstream'],
        generator_sha256=generator_sha(), contract_sha256=sha(folder / 'contract.json'),
        files={name: dict(bytes=(folder / name).stat().st_size, sha256=sha(folder / name))
               for name in sorted(payloads(contract))})
    (folder / 'manifest.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode('utf-8'))
    verify_capture(folder)


def bilinear_model(source, width, height, step_x, step_y, half_up=True):
    """Float64 center model with edge clamping; deliberately no Java assumptions."""
    import numpy as np
    h, w = source.shape[:2]
    x = np.clip((np.arange(width)+.5)*step_x-.5, 0, w-1)
    y = np.clip((np.arange(height)+.5)*step_y-.5, 0, h-1)
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    wx, wy = (x-ix)[None, :, None], (y-iy)[:, None, None]
    source = source.astype(np.float64)
    values = (source[iy[:, None], ix[None, :]]*(1-wx)*(1-wy)
        + source[iy[:, None], np.minimum(ix+1, w-1)[None, :]]*wx*(1-wy)
        + source[np.minimum(iy+1, h-1)[:, None], ix[None, :]]*(1-wx)*wy
        + source[np.minimum(iy+1, h-1)[:, None], np.minimum(ix+1, w-1)[None, :]]*wx*wy)
    return (np.floor(values+.5) if half_up else np.rint(values)).astype(np.uint8)


def differences(actual, expected):
    import numpy as np
    delta = np.abs(actual.astype(int)-expected.astype(int))
    return dict(changed_components=int(np.count_nonzero(delta)), maximum_error=int(delta.max()))


def compare(folder=FIXTURE, native=False):
    import numpy as np
    from PIL import Image
    contract, reference = verify_capture(folder)
    folder = Path(folder)
    inputs = patterns()
    requests = {(case['name'], request['name']): request for case in contract['cases'] for request in case['requests']}
    for name, expected in inputs.items():
        actual = np.asarray(Image.open(folder / name / 'input.png').convert('RGB'))
        np.testing.assert_array_equal(actual, expected)
    rows = []
    for item in reference['cases']:
        name, request_name = item['case_name'], item['request_name']
        request = requests[name, request_name]
        if item['bounds'] != request['bounds'] or item['downsample'] != request['downsample']:
            raise ValueError('RGB read request differs from capture contract')
        x, y, w, h = request['bounds']
        source = inputs[name][y:y+h, x:x+w]
        actual = np.asarray(Image.open(folder / name / (request_name + '.png')).convert('RGB'))
        height, width = actual.shape[:2]
        if (width, height) != (item['width'], item['height']):
            raise ValueError('RGB capture dimensions differ from reference')
        models = {}
        for mode, sx, sy in [('nominal', request['downsample'], request['downsample']),
                             ('realized', w/width, h/height)]:
            ix = np.clip(np.floor((np.arange(width)+.5)*sx).astype(int), 0, w-1)
            iy = np.clip(np.floor((np.arange(height)+.5)*sy).astype(int), 0, h-1)
            models[mode + '_nearest'] = differences(actual, source[iy[:, None], ix[None, :]])
            for rounding in ('half_up', 'ties_even'):
                model = bilinear_model(source, width, height, sx, sy, rounding == 'half_up')
                models[mode + '_' + rounding] = differences(actual, model)
        row = dict(**item, nominal_step=[request['downsample']]*2, realized_step=[w/width, h/height],
                   shape_round_half_up=[int(np.floor(w/request['downsample']+.5)), int(np.floor(h/request['downsample']+.5))],
                   shape_ceil=[int(np.ceil(w/request['downsample'])), int(np.ceil(h/request['downsample']))], models=models)
        if native:
            import qupathxx as q
            rgba = np.concatenate((source, np.full((*source.shape[:2], 1), 255, dtype=np.uint8)), axis=2)
            pixels = q.resample_area(q.PixelBuffer.from_numpy(rgba), width, height).numpy()
            row['native_rgba8_area'] = differences(actual, pixels[:, :, :3])
        rows.append(row)
    report = dict(schema='qpx.qupath-rgb-resampling-models/1', state='characterized-partial',
        scope='Opaque RGB WrappedBufferedImageServer public readRegion on four synthetic inputs and nine in-bounds requests each.',
        capture_manifest_sha256=sha(folder / 'manifest.json'), comparison_tool_sha256=sha(__file__),
        cases=rows, native_sampling_changed=False, sampler_equivalence_accepted=False,
        biological_validation=False, general_speed_claim=False)
    if native:
        import qupathxx as q
        report['native_source'] = q.build_environment().git_revision
        report['native_source_fingerprint'] = q.build_environment().source_tree_sha256
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, default=FIXTURE)
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--runtime-cache', type=Path, default=ROOT / 'build/upstream/qupath')
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.capture:
        capture(args.capture, args.runtime_cache)
    report = compare(args.capture or args.reference, args.native)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes((json.dumps(report, indent=2, allow_nan=False) + '\n').encode('utf-8'))
    print(json.dumps(dict(state=report['state'], cases=len(report['cases']), report=str(args.report))))


if __name__ == '__main__':
    main()
