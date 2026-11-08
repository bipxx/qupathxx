"""Verify a pinned resampled input and its nominal geometry separately.

Captured-input detector agreement does not establish equivalence of the native
area sampler, a general Java resize implementation, or biological accuracy.
"""
import argparse
from fractions import Fraction
import gzip
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT/'tests/fixtures/qupath-cells-resampled-debug-0.7.0'
CASE = 'resampled_reference'
FILES = {'contract.json', 'debug-probe.groovy', 'debug-reference.groovy',
         'debug-stack.json', 'debug-stack.tif', 'driver.log',
         *(CASE+'/'+n for n in ['input.png', 'reference.json', 'cells.geojson', 'nucleus.u32le.gz',
              'body.u32le.gz', 'nucleus-decoded.u32le.gz', 'body-decoded.u32le.gz',
              'processed-input.png', 'processed-input.json'])}
RGB_PROBE = '''        double probeDownsample=params.requestedPixelSizeMicrons/pixelSize
        def probeRequest=qupath.lib.regions.RegionRequest.createInstance(server.getPath(),probeDownsample,
            *(bounds.collect {it as int}))
        def probeImage=server.readRegion(probeRequest)
        if(!ImageIO.write(probeImage,'PNG',folder.resolve('processed-input.png').toFile()))
            throw new IllegalStateException('Cannot save processed input')
        Files.writeString(folder.resolve('processed-input.json'),GsonTools.getInstance(true).toJson(
            [downsample:probeDownsample,request:bounds,width:probeImage.width,height:probeImage.height,
             bufferedImageType:probeImage.type])+'\\n')
'''


def common():
    spec = importlib.util.spec_from_file_location('qpx_processing_debug', ROOT/'tools/qupath_roi_cell_debug.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def generator(comparison):
    text = common().debug_generator(comparison)
    needle = '        if(!QP.runPlugin('
    if text.count(needle) != 1:
        raise ValueError('unexpected ordinary plugin invocation')
    return text.replace(needle, RGB_PROBE+needle)


def identity(comparison, upstream):
    value = common().identity(comparison, upstream, CASE)
    value['schema'] = 'qpx.qupath-resampled-cell-debug/1'
    return value


def capture(folder, runtime_cache):
    if os.name != 'nt':
        raise ValueError('the pinned reference runtime capture requires Windows')
    shared = common()
    comparison, contract, upstream = shared.context(CASE)
    folder = Path(folder).resolve()
    folder.mkdir(parents=True, exist_ok=False)
    (folder/CASE).mkdir()
    (folder/'contract.json').write_bytes((json.dumps(contract, indent=2)+'\n').encode('utf-8'))
    shutil.copyfile(comparison.FIXTURES/'capture'/CASE/'input.png', folder/CASE/'input.png')
    (folder/'debug-reference.groovy').write_bytes(generator(comparison).encode('utf-8'))
    shutil.copyfile(ROOT/'tools/upstream/qupath_cell_debug_probe.groovy', folder/'debug-probe.groovy')
    runtime = shared.load('qpx_processing_runtime', ROOT/'tools/fetch_qupath.py').fetch(runtime_cache)
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


def processing_labels(features, key, width, height, step_x=1.5, step_y=1.5):
    """Independent center raster for the captured rectilinear pixel polygons."""
    import numpy as np
    if (width, height) != (171, 128) or len(features) != 3:
        raise ValueError('unexpected processing label dimensions/count')
    y, x = np.mgrid[:height, :width]
    x, y = x+.5, y+.5
    output = np.zeros((height, width), dtype=np.uint32)
    used = set()
    for feature in features:
        label = int(feature['properties']['name'].removeprefix('cell_'))
        if label not in (1, 2, 3) or label in used or feature[key]['type'] != 'Polygon':
            raise ValueError('unexpected processing geometry labels/type')
        used.add(label)
        inside = np.zeros((height, width), dtype=bool)
        for ring in feature[key]['coordinates']:
            points = np.asarray(ring)/[step_x, step_y]
            if (len(points) < 4 or points.shape[1:] != (2,) or not np.all(np.isfinite(points))
                    or not np.array_equal(points[0], points[-1]) or np.any(points != np.rint(points))):
                raise ValueError('processing geometry is not on the declared integer lattice')
            for a, b in zip(points[:-1], points[1:]):
                if a[0] != b[0] and a[1] != b[1]:
                    raise ValueError('processing capture requires rectilinear pixel polygons')
                if a[1] != b[1]:
                    inside ^= ((a[1] > y) != (b[1] > y)) & (x < a[0])
        if np.any(inside & (output > 0)):
            raise ValueError('processing geometry overlaps')
        output[inside] = label
    return output


def project_nominal(labels, width=256, height=192, step=Fraction(3, 2)):
    """Exact inverse mapping of level-zero pixel centers to the nominal grid."""
    import numpy as np
    if labels.shape != (128, 171) or (width, height) != (256, 192) or step != Fraction(3, 2):
        raise ValueError('unexpected nominal projection contract')
    x = ((2*np.arange(width)+1)*step.denominator)//(2*step.numerator)
    y = ((2*np.arange(height)+1)*step.denominator)//(2*step.numerator)
    return labels[y[:, None], x[None, :]]


def bilinear_model(rgb, step_x=1.5, step_y=1.5, half_up=True):
    """Diagnostic center-bilinear model for this capture, not a product reader."""
    import numpy as np
    x = np.clip((np.arange(171)+.5)*step_x-.5, 0, 255)
    y = np.clip((np.arange(128)+.5)*step_y-.5, 0, 191)
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    wx, wy = (x-ix)[None, :, None], (y-iy)[:, None, None]
    source = rgb.astype(float)
    values = (source[iy[:, None], ix[None, :]]*(1-wx)*(1-wy)
        + source[iy[:, None], np.minimum(ix+1, 255)[None, :]]*wx*(1-wy)
        + source[np.minimum(iy+1, 191)[:, None], ix[None, :]]*(1-wx)*wy
        + source[np.minimum(iy+1, 191)[:, None], np.minimum(ix+1, 255)[None, :]]*wx*wy)
    return (np.floor(values+.5) if half_up else np.rint(values)).astype(np.uint8)


def verified_data(folder):
    import numpy as np
    from PIL import Image
    shared = common()
    comparison, contract, upstream = shared.context(CASE)
    folder = Path(folder).resolve()
    ordinary = shared.verify_envelope(folder, comparison, contract, identity(comparison, upstream),
                                      FILES, generator(comparison), label='resampled debug')
    metadata = comparison.read(folder/CASE/'processed-input.json')
    if metadata != dict(downsample=1.5, request=[0, 0, 256, 192], width=171, height=128, bufferedImageType=1):
        raise ValueError('unexpected resampled input request/mapping')
    names = ['Input image', 'Laplacian of Gaussian filtered', 'Watershed labels', 'Binary', 'Labeled ROIs']
    if comparison.read(folder/'debug-stack.json') != dict(width=171, height=128, slices=names):
        raise ValueError('unexpected resampled debug dimensions/stages')
    stages = []
    with Image.open(folder/'debug-stack.tif') as image:
        if image.n_frames != 5:
            raise ValueError('unexpected resampled debug frame count')
        for i in range(5):
            image.seek(i)
            array = np.asarray(image).copy()
            if array.shape != (128, 171) or array.dtype != np.float32 or not np.all(np.isfinite(array)):
                raise ValueError('invalid resampled debug stage array')
            stages.append(array)
    with Image.open(folder/CASE/'processed-input.png') as image:
        if image.mode != 'RGB' or image.size != (171, 128):
            raise ValueError('unexpected resampled RGB input format/dimensions')
        rgb = np.asarray(image).copy()
    with Image.open(ordinary/'input.png') as image:
        source = np.asarray(image.convert('RGB')).copy()
    expected_od = np.maximum(0, np.log10(255/np.maximum(rgb.astype(float), 1))).sum(axis=2)
    error = float(np.max(np.abs(expected_od-stages[0])))
    if error > 1e-6:
        raise ValueError('resampled RGB does not reproduce debug optical densities')
    features = comparison.read(folder/CASE/'cells.geojson')['features']
    original_features = comparison.read(ordinary/'cells.geojson')['features']
    geometry = lambda f: (f['properties']['name'], f['geometry'], f['nucleusGeometry'])
    if [geometry(f) for f in features] != [geometry(f) for f in original_features]:
        raise ValueError('resampled ordinary exported geometry differs')
    masks = {name: processing_labels(features, key, 171, 128)
             for name, key in [('nucleus', 'nucleusGeometry'), ('body', 'geometry')]}
    if not np.array_equal(stages[-1], masks['nucleus']):
        raise ValueError('resampled debug nuclei differ from nominal exported geometry')
    for name, labels in masks.items():
        wanted = np.frombuffer(gzip.decompress((ordinary/(name+'.u32le.gz')).read_bytes()), dtype='<u4').reshape(192, 256)
        if not np.array_equal(project_nominal(labels), wanted):
            raise ValueError('nominal geometry projection differs from ordinary labels')
    return comparison, contract, rgb, source, masks, error


def verify(folder=FIXTURE):
    import numpy as np
    comparison, _, rgb, source, masks, error = verified_data(folder)
    nominal = bilinear_model(source)
    realized = bilinear_model(source, step_x=256/171)
    even = bilinear_model(source, half_up=False)
    return dict(schema='qpx.qupath-resampled-cell-debug-verification/1',
        source_dimensions=[256, 192], processing_dimensions=[171, 128],
        nominal_step=[3, 2], realized_x_step=[256, 171],
        optical_density_max_absolute_error=error, ordinary_labels_and_geometry_exact=True,
        processing_nucleus_pixels=int(np.count_nonzero(masks['nucleus'])),
        processing_body_pixels=int(np.count_nonzero(masks['body'])),
        nominal_bilinear_half_up_different_components=int(np.count_nonzero(nominal != rgb)),
        realized_width_bilinear_different_components=int(np.count_nonzero(realized != rgb)),
        ties_even_bilinear_different_components=int(np.count_nonzero(even != rgb)),
        model_generalized=False, sampler_equivalence_accepted=False,
        manifest_sha256=comparison.sha(Path(folder)/'manifest.json'), biological_validation=False)


def compare_native(folder=FIXTURE):
    import numpy as np
    import qupathxx as q
    import qupathxx._native as native
    comparison, contract, rgb, original, reference, _ = verified_data(folder)
    report = verify(folder)
    doc = q.Document(q.Image.from_numpy(rgb))
    doc.set_calibration(.75, .75)
    obj = doc.add_rectangle(q.Rect(0, 0, 171, 128))
    transform = q.StainTransform(contract['vectors'], contract['white'])
    parameters = q.GridCellParameters()
    parameters.threshold = contract['native']['threshold']
    parameters.sigma = contract['native']['sigma']/1.5
    parameters.cell_expansion = contract['native']['cell_expansion']/1.5
    parameters.min_nucleus_area = contract['native']['min_nucleus_pixels']/1.5**2
    parameters.max_nucleus_area = contract['native']['max_nucleus_pixels']/1.5**2
    grid = q.ScientificGrid(171, 128, q.ScientificGridAxis(0, 171, 171), q.ScientificGridAxis(0, 128, 128))
    request = q.CellRequest(doc, obj, transform, grid, parameters)
    result = q.detect_cells(request)
    masks = {name: getattr(result, attr).labels_numpy()
             for name, attr in [('nucleus', 'nuclei'), ('body', 'cells')]}
    metrics = {name: comparison.metrics(reference[name], labels, contract['comparison']) for name, labels in masks.items()}
    od = np.maximum(0, np.log10(255/np.maximum(rgb.astype(float), 1)))
    feature_checks = []
    for feature in result.features:
        calibrated = result.calibrated_feature(feature.label)
        for name, prefix in [('nucleus', 'nucleus'), ('body', 'cell')]:
            selected = reference[name] == feature.label
            count = int(selected.sum())
            tolerance = contract['comparison']
            feature_checks.append(dict(label=feature.label, compartment=name,
                sample_count_exact=getattr(feature, prefix+'_pixels') == count,
                physical_area_exact=getattr(calibrated, prefix+'_sample_area_um2') == count*.75**2,
                rgb_accepted=bool(np.allclose(getattr(feature, prefix+'_rgb'), rgb[selected].mean(axis=0),
                    atol=tolerance['feature_absolute'], rtol=tolerance['feature_relative'])),
                od_accepted=bool(np.allclose(getattr(feature, prefix+'_channels'), od[selected].mean(axis=0),
                    atol=tolerance['feature_absolute'], rtol=tolerance['feature_relative']))))
    server = q.dense_image_server(q.Image.from_numpy(original))
    area_grid = q.ScientificGrid(256, 192, q.ScientificGridAxis(0, 256, 171), q.ScientificGridAxis(0, 192, 128))
    area_rgb = q.read_scientific_rgb(server, area_grid).numpy()
    report.update(native_sha256=comparison.sha(native.__file__), build_git_revision=q.build_environment().git_revision,
        source_tree_sha256=q.build_environment().source_tree_sha256, algorithm=result.algorithm,
        native_processing_parameters=dict(units='processing_pixels', calibration_um=[.75, .75],
            threshold=parameters.threshold, sigma=parameters.sigma, cell_expansion=parameters.cell_expansion,
            min_nucleus_area=parameters.min_nucleus_area, max_nucleus_area=parameters.max_nucleus_area,
            log_prominence=parameters.log_prominence, min_peak_radius=parameters.min_peak_radius,
            min_seed_distance=parameters.min_seed_distance),
        segmentation=metrics, feature_checks=feature_checks, input_sha256=result.nuclei.input_sha256,
        original_source_area_max_absolute_rgb_difference=float(np.max(np.abs(area_rgb-rgb))),
        original_source_area_different_components=int(np.count_nonzero(area_rgb != rgb)),
        original_source_replay_authority=False, segmentation_parity_accepted=False,
        scope='Detector/feature comparison on independently captured processed RGB, not native original-source sampling equivalence')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, help='New output directory; Windows pinned runtime only')
    parser.add_argument('--runtime-cache', type=Path, default=ROOT/'build/upstream/qupath')
    parser.add_argument('--reference', type=Path, default=FIXTURE)
    parser.add_argument('--compare-native', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    folder = args.capture or args.reference
    if args.capture:
        capture(folder, args.runtime_cache)
    report = compare_native(folder) if args.compare_native else verify(folder)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes((json.dumps(report, indent=2, allow_nan=False)+'\n').encode('utf-8'))
    print(json.dumps(dict(processing_dimensions=report['processing_dimensions'], report=str(args.report))))


if __name__ == '__main__':
    main()
