"""Check the native expansion kernel against pinned QuPath label maps."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve()
    fixtures = ROOT / 'tests/fixtures/qupath-cells-0.7.0'
    capture_tool = ROOT / 'tools/compare_qupath_cells.py'
    spec = importlib.util.spec_from_file_location('qpx_pinned_cell_capture', capture_tool)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    contract = capture.verify_capture(fixtures / 'capture')
    width, height = contract['width'], contract['height']
    environment = json.loads(subprocess.check_output([str(executable), '--environment'], text=True))
    report = dict(schema='qpx.native-cell-expansion-comparison/1',
                  environment=environment, native_sha256=sha(executable), tool_sha256=sha(Path(__file__)),
                  contract_sha256=sha(fixtures / 'contract.json'), cases=[],
                  capture_manifest_sha256=sha(fixtures / 'capture/capture-manifest.json'),
                  capture_verifier_sha256=sha(capture_tool),
                  production_recipe_integrated=False, segmentation_parity_accepted=False,
                  biological_validation=False, performance_accepted=False,
                  scope='Expansion-only comparison starting from pinned upstream nuclei. No detector, context, arbitrary resampling or production recipe acceptance.')
    with tempfile.TemporaryDirectory(prefix='qpx-expansion-') as temporary:
        source, output = Path(temporary)/'input.bin', Path(temporary)/'output.bin'
        for case in contract['cases']:
            if 'roi' in case or case.get('grid_equivalent') is False:
                report['cases'].append(dict(name=case['name'], state='excluded',
                                            reason='Context/ROI clipping or a different sampling grid requires separate production integration.'))
                continue
            folder = fixtures / 'capture' / case['name']
            nuclei_path, bodies_path = folder/'nucleus.u32le.gz', folder/'body.u32le.gz'
            nuclei = gzip.decompress(nuclei_path.read_bytes())
            expected = gzip.decompress(bodies_path.read_bytes())
            if len(nuclei) != width*height*4 or len(expected) != len(nuclei):
                raise RuntimeError('invalid pinned map dimensions')
            radius = contract['native']['cell_expansion']
            if radius != int(radius):
                raise RuntimeError('probe protocol requires an integer radius')
            source.write_bytes(b'QBE1' + struct.pack('<III', width, height, int(radius)) +
                               nuclei + bytes([1])*(width*height))
            subprocess.run([str(executable), str(source), str(output)], check=True, timeout=30)
            actual = output.read_bytes()
            if actual[:12] != b'QBR1' + struct.pack('<II', width, height):
                raise RuntimeError('invalid native expansion output')
            actual = actual[12:]
            if len(actual) != len(expected):
                raise RuntimeError('invalid output label dimensions')
            differences = int(np.count_nonzero(np.frombuffer(actual, dtype='<u4') !=
                                              np.frombuffer(expected, dtype='<u4')))
            report['cases'].append(dict(name=case['name'], state='compared',
                differing_label_pixels=differences, exact=actual == expected,
                input_nucleus_sha256=sha(nuclei_path), reference_body_sha256=sha(bodies_path),
                actual_label_sha256=hashlib.sha256(actual).hexdigest(),
                expected_label_sha256=hashlib.sha256(expected).hexdigest()))
    compared = [c for c in report['cases'] if c['state'] == 'compared']
    report['native_expansion_corpus_accepted'] = len(compared) == 7 and all(c['exact'] for c in compared)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes((json.dumps(report, indent=2)+'\n').encode())
    print(json.dumps(dict(compared=len(compared), accepted=report['native_expansion_corpus_accepted'])))
    return 0 if report['native_expansion_corpus_accepted'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
