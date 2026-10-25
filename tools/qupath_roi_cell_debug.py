"""Capture/verify public QuPath debug stages for the immutable clipped ROI."""
import argparse
import gzip
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT/'tests/fixtures/qupath-cells-roi-debug-0.7.0'
CASE = 'roi_cut_hole'
FILES = {'contract.json', 'debug-probe.groovy', 'debug-reference.groovy',
         'debug-stack.json', 'debug-stack.tif', 'driver.log',
         *(CASE+'/'+name for name in ['input.png', 'reference.json', 'cells.geojson', 'nucleus.u32le.gz',
               'body.u32le.gz', 'nucleus-decoded.u32le.gz', 'body-decoded.u32le.gz'])}


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def context(case_name=CASE):
    comparison = load('qpx_roi_reference', ROOT/'tools/compare_qupath_cells.py')
    contract = comparison.verify_capture(comparison.FIXTURES/'capture')
    contract['cases'] = [c for c in contract['cases'] if c['name'] == case_name]
    assert len(contract['cases']) == 1
    upstream = comparison.read((comparison.FIXTURES/contract['upstream_contract']).resolve())['upstream']
    return comparison, contract, upstream


def debug_generator(comparison):
    text = comparison.GENERATOR.read_text(encoding='utf-8')
    needle = 'WatershedCellDetection.setDebugMode(false)'
    if text.count(needle) != 1:
        raise ValueError('unexpected ordinary generator debug switch')
    return text.replace(needle, 'WatershedCellDetection.setDebugMode(true)')


def identity(comparison, upstream, case_name=CASE):
    return dict(schema='qpx.qupath-roi-cell-debug/1', upstream=upstream, case=case_name,
        capture_manifest_sha256=comparison.sha(comparison.FIXTURES/'capture/capture-manifest.json'),
        base_generator_sha256=comparison.sha(comparison.GENERATOR),
        debug_probe_sha256=comparison.sha(ROOT/'tools/upstream/qupath_cell_debug_probe.groovy'))


def capture(folder, runtime_cache):
    if os.name != 'nt':
        raise ValueError('the pinned reference runtime capture requires Windows')
    comparison, contract, upstream = context()
    folder = Path(folder).resolve()
    folder.mkdir(parents=True, exist_ok=False)
    (folder/CASE).mkdir()
    (folder/'contract.json').write_bytes((json.dumps(contract, indent=2)+'\n').encode('utf-8'))
    shutil.copyfile(comparison.FIXTURES/'capture'/CASE/'input.png', folder/CASE/'input.png')
    (folder/'debug-reference.groovy').write_bytes(debug_generator(comparison).encode('utf-8'))
    shutil.copyfile(ROOT/'tools/upstream/qupath_cell_debug_probe.groovy', folder/'debug-probe.groovy')
    runtime = load('qpx_roi_runtime', ROOT/'tools/fetch_qupath.py').fetch(runtime_cache)
    command = [str(runtime), 'script', '--args', str(folder), '--args', 'unused',
               '--args', str(folder/'debug-reference.groovy'), str(folder/'debug-probe.groovy')]
    with (folder/'driver.log').open('wb') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                timeout=180, creationflags=subprocess.CREATE_NO_WINDOW)
    result.check_returncode()
    manifest = identity(comparison, upstream)
    manifest['files'] = {name: dict(bytes=(folder/name).stat().st_size, sha256=comparison.sha(folder/name))
                         for name in sorted(FILES)}
    (folder/'manifest.json').write_bytes((json.dumps(manifest, indent=2)+'\n').encode('utf-8'))
    return verify(folder)


def verify_envelope(folder, comparison, contract, expected_identity, files, generator, label='ROI debug'):
    """Shared byte/recipe checks; domain-specific stage comparators stay separate."""
    case_name = expected_identity['case']
    manifest = comparison.read(folder/'manifest.json')
    if ({k: manifest.get(k) for k in expected_identity} != expected_identity
            or set(manifest.get('files', {})) != files):
        raise ValueError(label+' capture identity differs')
    for name, saved in manifest['files'].items():
        path = (folder/name).resolve()
        if (not path.is_relative_to(folder) or path.stat().st_size != saved['bytes']
                or comparison.sha(path) != saved['sha256']):
            raise ValueError(label+' capture bytes changed: '+name)
    if (comparison.read(folder/'contract.json') != contract
            or (folder/'debug-reference.groovy').read_text(encoding='utf-8') != generator
            or (folder/'debug-probe.groovy').read_bytes() != (ROOT/'tools/upstream/qupath_cell_debug_probe.groovy').read_bytes()):
        raise ValueError(label+' capture recipe differs')
    ordinary = comparison.FIXTURES/'capture'/case_name
    if ((folder/case_name/'input.png').read_bytes() != (ordinary/'input.png').read_bytes()
            or comparison.read(folder/case_name/'reference.json') != comparison.read(ordinary/'reference.json')):
        raise ValueError(label+' ordinary reference differs')
    for name in ['nucleus', 'body', 'nucleus-decoded', 'body-decoded']:
        if gzip.decompress((folder/case_name/(name+'.u32le.gz')).read_bytes()) != gzip.decompress((ordinary/(name+'.u32le.gz')).read_bytes()):
            raise ValueError(label+' ordinary labels differ')
    return ordinary


def verify(folder=FIXTURE):
    import numpy as np
    from PIL import Image
    comparison, contract, upstream = context()
    folder = Path(folder).resolve()
    ordinary = verify_envelope(folder, comparison, contract, identity(comparison, upstream),
                               FILES, debug_generator(comparison))
    x, y, w, h = contract['cases'][0]['roi']
    metadata = comparison.read(folder/'debug-stack.json')
    names = ['Input image', 'Laplacian of Gaussian filtered', 'Watershed labels', 'Binary', 'Labeled ROIs']
    if metadata != dict(width=w, height=h, slices=names):
        raise ValueError('unexpected ROI debug processing extent/stages')
    stages = []
    with Image.open(folder/'debug-stack.tif') as image:
        if image.n_frames != len(names):
            raise ValueError('unexpected ROI debug frame count')
        for i in range(image.n_frames):
            image.seek(i)
            array = np.asarray(image).copy()
            if array.shape != (h, w) or array.dtype != np.float32 or not np.all(np.isfinite(array)):
                raise ValueError('invalid ROI debug stage array')
            stages.append(array)
    with Image.open(ordinary/'input.png') as image:
        rgb = np.asarray(image.convert('RGB'))[y:y+h, x:x+w]
    expected_od = np.maximum(0, np.log10(np.asarray(contract['white'])/np.maximum(rgb.astype(float), 1))).sum(axis=2)
    error = float(np.max(np.abs(stages[0]-expected_od)))
    if error > 1e-6:
        raise ValueError('ROI debug input optical density differs')
    if np.any(stages[-1] < 0) or np.any(stages[-1] != np.floor(stages[-1])):
        raise ValueError('invalid ROI debug labels')
    labels = np.frombuffer(gzip.decompress((ordinary/'nucleus.u32le.gz').read_bytes()),
                           dtype='<u4').reshape(contract['height'], contract['width'])[y:y+h, x:x+w]
    metric = comparison.metrics(labels, stages[-1].astype(np.uint32), contract['comparison'])
    if (metric['reference_count'] != 3 or metric['native_count'] != 3
            or metric['foreground_iou'] != 1 or metric['unmatched_reference'] or metric['unmatched_native']
            or any(m['iou'] != 1 or m['centroid_error_px'] != 0 for m in metric['matches'])):
        raise ValueError('ROI debug stage nuclei differ from ordinary capture')
    points = [(32, 68), (33, 68), (32, 81), (33, 81)]
    return dict(schema='qpx.qupath-roi-cell-debug-verification/1',
        processing_bounds=[x, y, w, h], optical_density_max_absolute_error=error,
        ordinary_labels_exact=True, debug_nucleus_masks_exact=True, nuclei=3,
        boundary_probes=[dict(x=px, y=py, log_response=float(stages[1][py-y, px-x]),
                             label=int(stages[-1][py-y, px-x])) for px, py in points],
        stages=[dict(name=name, positive_pixels=int(np.count_nonzero(a > 0))) for name, a in zip(names, stages)],
        manifest_sha256=comparison.sha(folder/'manifest.json'),
        biological_validation=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, help='New output directory; never overwrites the pinned fixture')
    parser.add_argument('--runtime-cache', type=Path, default=ROOT/'build/upstream/qupath')
    parser.add_argument('--reference', type=Path, default=FIXTURE)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = capture(args.capture, args.runtime_cache) if args.capture else verify(args.reference)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes((json.dumps(report, indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(nuclei=report['nuclei'], processing_bounds=report['processing_bounds'], report=str(args.report))))


if __name__ == '__main__':
    main()
