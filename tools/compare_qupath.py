"""Run a pinned independent QuPath reference, then check the installed native API.

Reference-only checks need NumPy/qupathxx; live regeneration additionally needs
Windows and the optional, hash-verified QuPath distribution. No fixture is
updated implicitly. All live output goes to a new directory under build/.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT/'tests/fixtures/qupath-0.7.0'
GENERATOR = ROOT/'tools/upstream/qupath_reference.groovy'
DATA_FILES = [f'{direction}/{name}' for direction in ['upstream', 'roundtrip']
              for name in ['objects.geojson', 'measurements.json', 'colors.json', 'roi-means.json', 'edge-cases.json']]
CAPTURE_FILES = DATA_FILES+['native-export.geojson', 'report.json']


def read(path):
    return json.loads(Path(path).read_text(encoding='utf8'))


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_capture(folder, contract):
    folder = Path(folder).resolve()
    manifest = read(folder/'capture-manifest.json')
    if (manifest['schema'] != 'qpx.qupath-capture/1' or manifest['upstream'] != contract['upstream']
            or manifest['generator_sha256'] != sha(GENERATOR)
            or manifest['contract_sha256'] != sha(FIXTURES/'contract.json')
            or set(manifest['files']) != set(CAPTURE_FILES)):
        raise ValueError('reference capture identity differs')
    for name, identity in manifest['files'].items():
        path = (folder/name).resolve()
        if (not path.is_relative_to(folder) or path.stat().st_size != identity['bytes']
                or sha(path) != identity['sha256']):
            raise ValueError('reference capture changed: '+name)
    return manifest


def capture_manifest(output, contract, report):
    manifest = dict(schema='qpx.qupath-capture/1', captured_utc=datetime.now(timezone.utc).isoformat(),
                    upstream=contract['upstream'], generator_sha256=sha(GENERATOR),
                    contract_sha256=sha(FIXTURES/'contract.json'), comparison_tool_sha256=sha(__file__),
                    native_sha256=report['native_sha256'], native_source_tree_sha256=report['source_tree_sha256'],
                    comparison_state=report['state'],
                    files={name: dict(bytes=(output/name).stat().st_size, sha256=sha(output/name)) for name in CAPTURE_FILES})
    (output/'capture-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')


def fetch_runtime(destination):
    spec = importlib.util.spec_from_file_location('qpx_fetch_qupath', ROOT/'tools/fetch_qupath.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.fetch(destination)


def pixels_document(contract):
    import numpy as np
    import qupathxx as q

    image = contract['image']
    y, x = np.mgrid[:image['height'], :image['width']]
    pixels = np.stack([(3*x+5*y)%256, (7*x+11*y)%256, (13*x+17*y)%256], axis=-1).astype(np.uint8)
    doc = q.Document(q.Image.from_numpy(pixels))
    doc.set_calibration(*image['calibration_um'])
    return doc


def document(folder,contract):
    import qupathxx as q
    doc=pixels_document(contract)
    imported=q.import_geojson(doc,(Path(folder)/'objects.geojson').read_text(encoding='utf8'),assume_pixels=True)
    return doc,imported



def sample_mask(doc, object_id, transform, nucleus, width, height):
    # Threshold zero with no cleanup exposes the analysis sampling domain, not
    # the closed-boundary geometry predicate used for picking/containment.
    import numpy as np
    import qupathxx as q
    parameters=q.TissueParameters()
    parameters.threshold,parameters.level=0,0
    result=q.detect_tissue(q.TissueRequest(doc,object_id,transform,parameters,nucleus))
    bounds=result.level_bounds
    mask=np.zeros((height,width),dtype=np.uint8)
    x,y=int(bounds.x),int(bounds.y)
    mask[y:y+result.height,x:x+result.width]=(result.labels_numpy()>0).astype(np.uint8)
    return mask.tobytes(),result.samples

def compare(folder, contract):
    import numpy as np
    import qupathxx as q

    folder = Path(folder)
    tolerance = contract['tolerances']
    reference = read(folder/'measurements.json')
    if reference['qupath_version'] != contract['upstream']['version']:
        raise ValueError('reference is not from the pinned QuPath version')
    doc, imported = document(folder, contract)
    objects = {a.name: a for a in doc.annotations}
    expected = {a['name']: a for a in reference['objects']}
    if len(objects) != contract['scope']['objects'] or objects.keys() != expected.keys():
        raise ValueError('reference object set differs')
    maximum_geometry_error = 0.
    ellipse_differences = []

    def close(actual, wanted, geometry=False):
        np.testing.assert_allclose(actual, wanted,
                                   atol=tolerance['geometry_absolute' if geometry else 'float_absolute'],
                                   rtol=tolerance['geometry_relative' if geometry else 'float_relative'])
        return float(np.max(np.abs(np.asarray(actual)-np.asarray(wanted))))

    for name, item in expected.items():
        obj = objects[name]
        kind = {q.ObjectKind.ANNOTATION: 'annotation', q.ObjectKind.DETECTION: 'detection', q.ObjectKind.CELL: 'cell'}[obj.kind]
        if kind != item['kind'] or obj.class_name != item.get('classification', ''):
            raise ValueError('object kind/class changed: '+name)
        if obj.class_name and doc.object_class(obj.class_name).color != item['class_color'] & 0xffffff:
            raise ValueError('class RGB changed: '+name)
        features = doc.shape_measurements(obj.id)
        for geometry, measurements, wanted in [(obj.geometry, features.roi, item['geometry']),
                                                (obj.nucleus, features.nucleus, item.get('nucleus'))]:
            if wanted is None:
                if geometry is not None:
                    raise ValueError('unexpected nucleus')
                continue
            if geometry is None or measurements is None:
                raise ValueError('missing geometry/measurements')
            bounds = geometry.bounds
            maximum_geometry_error = max(maximum_geometry_error,
                close([bounds.x, bounds.y, bounds.width, bounds.height], wanted['bounds'], True),
                close([measurements.area_px2, measurements.area_um2], [wanted['area'], wanted['area_um2']], True))
            if geometry.kind == q.RoiKind.ELLIPSE:
                for unit, actual, target in [('pixels', measurements.length_px, wanted['length']), ('um', measurements.length_um, wanted['length_um'])]:
                    np.testing.assert_allclose(actual, target, atol=tolerance['geometry_absolute'], rtol=tolerance['ellipse_length_relative'])
                    ellipse_differences.append(dict(name=name, compartment='nucleus' if wanted is item.get('nucleus') else 'body', unit=unit,
                                                    native=actual, qupath=target, relative_error=abs(actual-target)/target))
            else:
                maximum_geometry_error = max(maximum_geometry_error, close([measurements.length_px, measurements.length_um], [wanted['length'], wanted['length_um']], True))

    colors = read(folder/'colors.json')
    pixels = np.asarray(colors['pixels'], dtype=np.uint8)[None, :, :]
    server = q.Document(q.Image.from_numpy(pixels)).server
    buffer = server.read_region(q.RegionRequest(0, 0, 0, pixels.shape[1], 1))
    max_od_error, max_stain_error = 0., 0.
    for model in colors['models']:
        transform = q.StainTransform(colors['vectors'], model['white'])
        np.testing.assert_allclose(transform.stains, model['basis'], atol=tolerance['basis_absolute'], rtol=0)
        wanted = np.asarray(model['channels']).T
        max_od_error = max(max_od_error, close(transform.optical_densities(buffer).numpy().reshape(-1, 3), wanted[:, :3]))
        max_stain_error = max(max_stain_error, close(transform.deconvolve(buffer).numpy().reshape(-1, 3), wanted[:, 3:]))

    roi_reference = read(folder/'roi-means.json')
    width, height = roi_reference['width'], roi_reference['height']
    if (width, height) != (contract['image']['width'], contract['image']['height']):
        raise ValueError('reference image dimensions differ')
    transform = q.StainTransform(roi_reference['vectors'], roi_reference['white'])
    mean_error, analysis_ms = 0., 0.
    for region in roi_reference['regions']:
        obj = objects[region['name']]
        nucleus = region['compartment'] == 'nucleus'
        mask, count = sample_mask(doc, obj.id, transform, nucleus, width, height)
        if count != region['samples'] or hashlib.sha256(mask).hexdigest() != region['mask_sha256']:
            raise ValueError('pixel-center mask differs: '+region['name']+'/'+region['compartment'])
        for tile in [16, 64, 256]:
            for kind, key in [(q.AnalysisKind.OPTICAL_DENSITY_MEANS, 'od_means'), (q.AnalysisKind.STAIN_MEANS, 'stain_means')]:
                request = q.AnalysisRequest(doc, obj.id, transform, kind, nucleus, tile)
                start = time.perf_counter()
                result = q.analyze(request)
                analysis_ms += (time.perf_counter()-start)*1000
                if result.samples != region['samples']:
                    raise ValueError('sample count differs: '+region['name'])
                mean_error = max(mean_error, close(result.means, region[key]))
    if len(roi_reference['regions']) != contract['scope']['area_compartments']:
        raise ValueError('reference region count differs')
    return dict(objects=len(objects), area_compartments=len(roi_reference['regions']),
                mask_mismatches=0, analysis_jobs=len(roi_reference['regions'])*6,
                geometry_max_absolute_error=maximum_geometry_error, ellipse_length_differences=ellipse_differences, od_max_absolute_error=max_od_error,
                stain_max_absolute_error=max_stain_error, roi_mean_max_absolute_error=mean_error,
                analysis_jobs_wall_ms=analysis_ms, import_notices=[dict(code=i.code, count=i.count) for i in imported.issues])


def serialization_changes(folder):
    data = read(Path(folder)/'measurements.json')
    original = {o['name']: o['geometry'] for o in data['created_geometry']}
    regions = read(Path(folder)/'roi-means.json')
    before = {(r['name'], r['compartment']): r for r in regions['created_regions']}
    changes = []
    for item in data['objects']:
        name = item['name']
        old, new = original[name], item['geometry']
        key = name, 'body'
        samples_before = before[key]['samples'] if key in before else None
        after = next((r['samples'] for r in regions['regions'] if (r['name'], r['compartment']) == key), None)
        if old != new or samples_before != after:
            changes.append(dict(name=name, created_area=old['area'], decoded_area=new['area'],
                                bounds_max_absolute_error=max(abs(a-b) for a, b in zip(old['bounds'], new['bounds'])),
                                created_samples=samples_before, decoded_samples=after))
    return changes



def compare_edges(folder, contract):
    import numpy as np
    import qupathxx as q
    result=[]
    means=read(Path(folder)/'roi-means.json')
    transform=q.StainTransform(means['vectors'],means['white'])
    for case in read(Path(folder)/'edge-cases.json')['cases']:
        doc=pixels_document(contract)
        q.import_geojson(doc,case['geojson'],assume_pixels=True)
        roi=doc.annotations[0].geometry
        bounds=roi.bounds
        native=[bounds.x,bounds.y,bounds.width,bounds.height]
        decoded=case['decoded_geometry']['bounds']
        mask,count=sample_mask(doc,doc.annotations[0].id,q.StainTransform([[1,0,0],[0,1,0]]),False,256,256)
        mean_error=0.
        for kind,key in [(q.AnalysisKind.OPTICAL_DENSITY_MEANS,'od_means'),(q.AnalysisKind.STAIN_MEANS,'stain_means')]:
            actual=q.analyze(q.AnalysisRequest(doc,doc.annotations[0].id,transform,kind,tile_size=64))
            np.testing.assert_allclose(actual.means,case['decoded_region'][key],atol=contract['tolerances']['float_absolute'],rtol=contract['tolerances']['float_relative'])
            mean_error=max(mean_error,float(np.max(np.abs(np.asarray(actual.means)-case['decoded_region'][key]))))
        bounds_error=max(abs(a-b) for a,b in zip(native,decoded))
        area_error=abs(roi.area-case['decoded_geometry']['area'])
        mask_equal=hashlib.sha256(mask).hexdigest()==case['decoded_region']['mask_sha256']
        gap=(bounds_error>contract['tolerances']['geometry_absolute'] or area_error>contract['tolerances']['geometry_absolute'] or not mask_equal)
        result.append(dict(name=case['name'],state='known_parity_gap' if gap else 'passed',native_bounds=native,qupath_decoded_bounds=decoded,
                           bounds_max_absolute_error=bounds_error,
                           native_area=roi.area,qupath_decoded_area=case['decoded_geometry']['area'],
                           created_samples=case['created_region']['samples'],decoded_samples=case['decoded_region']['samples'],
                           native_samples=count,mask_equal=mask_equal,roi_mean_max_absolute_error=mean_error))
        if gap and not any(gate['case']==case['name'] for gate in contract['unmet_gates']):
            raise ValueError('unregistered edge discrepancy: '+case['name'])
    if len(result)!=contract['scope']['retained_edge_cases']:
        raise ValueError('edge case count differs')
    return result


def run_reference(executable, output, source=None):
    args = [str(executable), 'script', '--args', str(output.resolve())]
    if source is not None:
        args += ['--args', str(source.resolve())]
    args.append(str(GENERATOR))
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    output.with_suffix('.log').write_bytes(result.stdout)
    if result.returncode != 0 or ('QPX_REFERENCE_OK '+str(read(FIXTURES/'contract.json')['scope']['objects'])).encode() not in result.stdout:
        raise RuntimeError('QuPath reference failed; see '+str(output.with_suffix('.log')))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime-cache', type=Path, default=ROOT/'build/upstream/qupath')
    parser.add_argument('--reference-root', type=Path, help='check captured references without launching QuPath')
    parser.add_argument('--output', type=Path, default=ROOT/'build/qupath-comparison')
    args = parser.parse_args()
    contract = read(FIXTURES/'contract.json')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    reference = args.reference_root.resolve() if args.reference_root else output
    if args.reference_root:
        verify_capture(reference, contract)
    if not args.reference_root:
        if os.name != 'nt':
            parser.error('live generation currently uses the pinned Windows distribution; use --reference-root elsewhere')
        executable = fetch_runtime(args.runtime_cache)
        run_reference(executable, output/'upstream')
        import qupathxx as q
        doc, _ = document(output/'upstream', contract)
        q.save_geojson(doc, str(output/'native-export.geojson'), allow_losses=True)
        run_reference(executable, output/'roundtrip', output/'native-export.geojson')
    import qupathxx as q
    from qupathxx import _native
    export_doc, _ = document(reference/'upstream', contract)
    export_report = q.export_geojson(export_doc, allow_losses=True)
    edges=compare_edges(reference/'upstream',contract)
    unmet=[gate for gate in contract['unmet_gates'] if any(edge['name']==gate['case'] and edge['state']!='passed' for edge in edges)]
    report = dict(schema='qpx.qupath-comparison/1', state='partial' if unmet else 'passed', unmet_gates=unmet, platform=platform.platform(),
                  native_sha256=sha(_native.__file__), source_tree_sha256=q.build_environment().source_tree_sha256,
                  generator_sha256=sha(GENERATOR), contract_sha256=sha(FIXTURES/'contract.json'),
                  upstream=contract['upstream'], tolerances=contract['tolerances'],
                  original=compare(reference/'upstream', contract), roundtrip=compare(reference/'roundtrip', contract),
                  upstream_serialization_changes=serialization_changes(reference/'upstream'),
                  retained_edge_cases=edges,
                  native_export_notices=[dict(code=i.code, count=i.count) for i in export_report.issues],
                  identity_loss='Native numeric IDs are replaced with QuPath UUIDs; object comparisons use unique names.',
                  performance_scope='analysis_jobs_wall_ms times 54 native jobs per direction only; setup, masks, QuPath and file IO excluded')
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    if not args.reference_root:
        capture_manifest(output, contract, report)
    print(json.dumps(report, indent=2))
    if unmet:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
