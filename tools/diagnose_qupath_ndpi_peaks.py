"""Retain fixed, one-factor real-slide diagnostics without choosing new defaults."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def diagnose(reference, image):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    from qupathxx import _native

    comparison = load('qpx_ndpi_diagnostic_reference', 'compare_qupath_ndpi_cells.py')
    metrics = load('qpx_ndpi_diagnostic_metrics', 'label_comparison.py')
    contract, upstream = comparison.verify_capture(reference)
    comparison.identify_image(image, contract)
    x, y, width, height = contract['roi']
    labels = np.frombuffer(gzip.decompress((reference / 'nucleus.u32le.gz').read_bytes()),
                           dtype='<u4').reshape(height, width)
    # Prespecified one-factor changes: never select a winner or change production defaults.
    cases = [('baseline', {}), ('spacing_0', {'min_seed_distance': 0.}),
             ('spacing_2', {'min_seed_distance': 2.}),
             ('radius_0', {'min_peak_radius': 0.}),
             ('minimum_area_1', {'min_nucleus_pixels': 1}),
             ('maximum_area_unlimited', {'max_nucleus_pixels': 0}),
             ('prominence_025', {'log_prominence': .25}),
             ('prominence_1', {'log_prominence': 1.})]
    report = dict(schema='qpx.qupath-ndpi-peak-diagnostics/1',
        build_git_revision=q.build_environment().git_revision,
        source_tree_sha256=q.build_environment().source_tree_sha256,
        native_sha256=comparison.sha(Path(_native.__file__)),
        tool_sha256=comparison.sha(Path(__file__)),
        comparison_tool_sha256=comparison.sha(ROOT / 'tools/compare_qupath_ndpi_cells.py'),
        metrics_tool_sha256=comparison.sha(ROOT / 'tools/label_comparison.py'),
        capture_manifest_sha256=comparison.sha(reference / 'capture-manifest.json'),
        reference_count=upstream['count'], fixture_sha256=contract['fixture']['sha256'],
        segmentation_parity_accepted=False, biological_validation=False,
        performance_accepted=False, production_defaults_changed=False, cases=[])
    server = q.open_slide(str(image))
    try:
        pixels = server.read_region(q.RegionRequest(x, y, 0, width, height)).numpy()
        expected = np.asarray(Image.open(reference / 'input.png').convert('RGB'))
        if not np.array_equal(pixels[..., :3], expected) or not np.all(pixels[..., 3] == 255):
            raise ValueError('NDPI pixels differ from the QuPath capture')
        report['source_pixels_equal'] = True
        doc = q.Document(server)
        target = doc.add_rectangle(q.Rect(*contract['roi']))
        transform = q.StainTransform(contract['vectors'], contract['white'])
        for name, changes in cases:
            p = q.CellParameters()
            for key, value in contract['native'].items():
                setattr(p, key, value)
            for key, value in changes.items():
                setattr(p, key, value)
            p.algorithm = q.CellAlgorithm.LOG_WATERSHED_BODIES
            p.tile_size = 64
            result = q.detect_cells(q.CellRequest(doc, target, transform, p))
            native = comparison.local_labels(result.nuclei, contract['roi'])
            record = dict(name=name, changes=changes, cells=result.cell_count,
                raw_peaks=result.raw_peaks, selected_markers=len(result.markers),
                nucleus_sha256=hashlib.sha256(native.astype('<u4', copy=False).tobytes()).hexdigest(),
                segmentation=metrics.metrics(labels, native, contract['comparison']))
            report['cases'].append(record)
            print(json.dumps({k: record[k] for k in ['name', 'cells', 'raw_peaks', 'selected_markers']}), flush=True)
    finally:
        server.close()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = diagnose(args.reference, args.image)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes((json.dumps(report, indent=2, allow_nan=False) + '\n').encode())


if __name__ == '__main__':
    main()
