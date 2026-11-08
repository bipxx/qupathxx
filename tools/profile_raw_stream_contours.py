"""Fresh-process Windows raw contour scaling with fixed native/cache budgets."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT/'tests/fixtures/raw-stream-resources-v1/contract.json'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            value.update(block)
    return value.hexdigest()


def memory():
    # Reuse the existing admitted Windows counter implementation, not tracemalloc
    # or a Python allocation estimate of native/decoder/GEOS memory.
    path = ROOT/'tools/check_tma_large_pyramid.py'
    spec = importlib.util.spec_from_file_location('raw_resource_memory', path)
    module = importlib.util.module_from_spec(spec)
    exec(compile(path.read_bytes(), str(path), 'exec'), module.__dict__)
    return module.process_memory()


def lattice(q, size):
    return q.ScientificRawLattice(size, size, q.ScientificGridAxis(0, size, size),
                                  q.ScientificGridAxis(0, size, size))


def reader(q, path, contract):
    options = q.TiffOpenOptions()
    options.cache_bytes = contract['reader_cache_bytes']
    options.decode_bytes = contract['reader_decode_bytes']
    return q.open_tiff(str(path), options)


def matches_rectangle(geometry, label, size):
    if label not in [1, 2]:
        return False
    left, right = (0, size/2) if label == 1 else (size/2, size)
    paths = geometry.paths
    return len(paths) == 1 and len(paths[0]) == 4 and {
        (point.x, point.y) for point in paths[0]} == {(left, 0), (right, 0), (right, size), (left, size)}


def prepare(q, work, size, contract):
    # Generation/training run outside the measured child. Only two tile buffers
    # and 64 training rows exist; never construct a complete image or label mask.
    import numpy as np
    import tifffile
    folder = work/str(size)
    folder.mkdir()
    source, model_path = folder/'source.tif', folder/'model.qrm'
    tile_size = contract['source_tile']
    tiles = [np.full((tile_size, tile_size), value, dtype=np.uint16)
             for value in [1000, 50000]]
    count = size//tile_size
    tifffile.imwrite(source, (tiles[int(x >= count//2)] for y in range(count) for x in range(count)),
                     shape=(size, size), dtype=np.uint16, tile=(tile_size, tile_size),
                     photometric='minisblack', compression=None, metadata=None)
    server = reader(q, source, contract)
    try:
        selection = q.ScientificRawOptions()
        features = q.RawPixelFeatureOptions()
        features.offsets, features.scales = [0], [1]
        features.sigmas = [q.RawPixelFeatureScale(0, 0)]
        recipe = q.RawPixelRecipe(server.raw_metadata, lattice(q, size), selection, features)
        require(len(recipe.feature_schema) == 1, 'probe requires exactly one raw feature')
        values = np.repeat(np.array([[1000], [50000]], np.float32), 32, axis=0)
        labels = np.repeat(np.arange(2, dtype=np.uint32), 32)
        parameters = q.ForestParameters()
        parameters.trees, parameters.max_depth, parameters.seed = 5, 3, 1729
        forest = q.train_forest(values, labels, recipe.feature_schema, ['A', 'B'], parameters)
        q.save_raw_pixel_classifier(q.RawPixelClassifier(forest, recipe), str(model_path))
    finally:
        server.close()
    spec = dict(size=size, source=str(source), model=str(model_path),
                source_sha256=digest(source), model_sha256=digest(model_path),
                source_bytes=source.stat().st_size, contract=contract)
    path = folder/'case.json'
    path.write_text(json.dumps(spec, indent=2)+'\n', encoding='utf-8')
    return path


def observe(case):
    spec = json.loads(case.read_text(encoding='utf-8'))
    c, size = spec['contract'], spec['size']
    result = dict(size=size, pid=os.getpid(), memory_before_import=memory())
    output = case.parent/'observation.json'
    server = None
    try:
        import qupathxx as q
        result.update(runtime_source=q.build_environment().git_revision,
                      runtime_fingerprint=q.build_environment().source_tree_sha256,
                      package=str(Path(q.__file__).resolve()))
        for key in ['source', 'model']:
            require(digest(Path(spec[key])) == spec[key+'_sha256'], 'changed '+key+' input')
        result['memory_after_import_and_hash'] = memory()
        start = time.perf_counter()
        server = reader(q, Path(spec['source']), c)
        doc = q.Document(server)
        model = q.load_raw_pixel_classifier(spec['model'])
        stream = q.RawPixelStreamOptions()
        stream.tile_width = stream.tile_height = c['stream_tile']
        stream.work_bytes = c['stream_work_bytes']
        request = q.RawPixelTiledPredictionRequest(doc, model, lattice(q, size), stream)
        options = q.RawPixelStreamContourOptions()
        options.work_bytes, options.boundary_work_bytes = c['contour_work_bytes'], c['boundary_work_bytes']
        geometry = q.TissueContourOptions()
        geometry.work_bytes = c['geometry_work_bytes']
        options.contours = geometry
        result['capture_ms'] = (time.perf_counter()-start)*1000
        result['memory_before_trace'] = memory()
        start = time.perf_counter()
        contours = q.trace_raw_pixel_stream(request, options)
        result['trace_ms'] = (time.perf_counter()-start)*1000
        result['memory_after_trace'] = memory()
        stats = server.tiff_read_stats
        result['reader'] = {key: getattr(stats, key) for key in
                            ['decoded_blocks', 'decoded_bytes', 'cache_bytes', 'peak_cache_bytes']}
        result.update(completed_samples=contours.completed_samples, valid_samples=contours.valid_samples,
                      completed_windows=contours.completed_windows, expected_windows=request.total_windows,
                      class_counts=contours.class_counts, estimated_work_bytes=contours.estimated_work_bytes,
                      payload_bytes=contours.payload_bytes)
        components = contours.contours.components
        result['geometry'] = [dict(label=v.label, pixels=v.pixels, area=v.geometry.area,
                                   perimeter=v.geometry.perimeter) for v in components]
        result['geometry_wkt'] = [v.geometry.wkt for v in components]
        result['exact_rectangles'] = len(components) == 2 and all(
            matches_rectangle(v.geometry, v.label, size) for v in components)
        result['input_identities'] = {key: spec[key] for key in ['source_sha256', 'model_sha256', 'source_bytes']}
    except Exception as error:
        result['error'] = type(error).__name__+': '+str(error)
        raise
    finally:
        if server is not None:
            server.close()
        result['memory_final'] = memory()
        output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')


def check(contract, rows):
    require(sorted(row['size'] for row in rows) == contract['sizes'], 'missing/duplicate size')
    require(len({row['pid'] for row in rows}) == len(rows), 'not fresh processes')
    require(len({(row['runtime_source'], row['runtime_fingerprint'], row['package']) for row in rows}) == 1,
            'mixed runtime identities')
    peaks = []
    for row in rows:
        require(not row.get('error'), 'child observation failed')
        n = row['size']
        require(row['completed_samples'] == row['valid_samples'] == n*n, 'incomplete sample coverage')
        require(row['class_counts'] == [n*n//2, n*n//2], 'wrong labels/counts')
        require(row['completed_windows'] == row['expected_windows'] == (n//contract['stream_tile'])**2,
                'incomplete window coverage')
        require(row['exact_rectangles'] and sorted(row['geometry'], key=lambda v: v['label']) ==
                [dict(label=i, pixels=n*n//2, area=n*n/2, perimeter=3*n) for i in [1, 2]], 'wrong geometry')
        require(0 < row['payload_bytes'] <= row['estimated_work_bytes'] <= contract['contour_work_bytes'],
                'native admission budget')
        require(0 < row['reader']['peak_cache_bytes'] <= contract['reader_cache_bytes'], 'reader cache budget')
        require(row['reader']['decoded_bytes'] == n*n*2, 'unexpected decoded coverage')
        require(math.isfinite(row['trace_ms']) and row['trace_ms'] > 0, 'missing timing')
        peak = row['memory_final']['peak_working_set_bytes']
        require(0 < peak <= contract['process_peak_bytes'], 'process working set ceiling')
        require(peak >= row['memory_before_trace']['peak_working_set_bytes'], 'invalid OS high-water sequence')
        peaks.append(peak)
    require(max(peaks)-min(peaks) <= contract['process_peak_spread_bytes'], 'process peak spread')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--work', type=Path)
    mode.add_argument('--child', type=Path)
    args = parser.parse_args()
    require(sys.platform == 'win32', 'Windows OS working-set counter required')
    if args.child:
        observe(args.child.resolve())
        return
    work = args.work.resolve()
    require(not work.exists(), 'work destination already exists')
    c = json.loads(CONTRACT.read_text(encoding='utf-8'))
    work.mkdir(parents=True)
    (work/'contract.json').write_bytes(CONTRACT.read_bytes())
    import qupathxx as q
    require(q.has_tiff and q.has_geos and q.has_classifier_training, 'TIFF/GEOS/actual forest backend required')
    report = dict(schema='qpx.raw-stream-resource-observation/1', contract=c,
                  machine=dict(platform=platform.platform(), processor=platform.processor(),
                               logical_cpus=os.cpu_count(), python=sys.version),
                  runtime_source=q.build_environment().git_revision,
                  runtime_fingerprint=q.build_environment().source_tree_sha256,
                  package=str(Path(q.__file__).resolve()), rows=[], commands=[],
                  source_hashes={str(p.relative_to(ROOT)): digest(p) for p in
                                 [Path(__file__).resolve(), CONTRACT, ROOT/'tools/check_tma_large_pyramid.py']},
                  scope=c['scope'], preparation_outside_measured_children=True,
                  cold_storage_claim=False, hardware_acceptance=False)
    try:
        for size in c['sizes']:
            case = prepare(q, work, size, c)
            command = [sys.executable, '-I', str(Path(__file__).resolve()), '--child', str(case)]
            start = time.perf_counter()
            with (case.parent/'child.log').open('w', encoding='utf-8') as log:
                done = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                      timeout=c['child_timeout_seconds'])
            report['commands'].append(dict(command=command, returncode=done.returncode,
                                           process_wall_ms=(time.perf_counter()-start)*1000))
            observation = case.parent/'observation.json'
            if observation.exists():
                report['rows'].append(json.loads(observation.read_text(encoding='utf-8')))
            require(done.returncode == 0, 'child failed; see '+str(case.parent/'child.log'))
        check(c, report['rows'])
        require(all(row['runtime_source'] == report['runtime_source'] and
                    row['runtime_fingerprint'] == report['runtime_fingerprint'] and
                    row['package'] == report['package'] for row in report['rows']), 'parent/child runtime mismatch')
        report['local_engineering_guard_passed'] = True
    finally:
        (work/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
