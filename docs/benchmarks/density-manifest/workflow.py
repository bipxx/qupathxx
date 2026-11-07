"""Generate exact-class manifests for the preserved two-image reference project.

Usage: python -I workflow.py existing-study.qpxproj new-output-directory
The input deliberately repeats one reference crop, not independent specimens.
"""
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import qupathxx as q

root = Path(__file__).resolve().parents[3]
source = Path(sys.argv[1]).resolve(strict=True)
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=False)
before = hashlib.sha256(source.read_bytes()).hexdigest()
generator = root / 'examples/density_manifest.py'
runner = root / 'examples/project_density.py'
command = [sys.executable, '-I', str(generator), str(source),
           '--window-class', 'Density windows', '--tissue-class', 'Density tissue',
           '--exclusion-class', 'Density exclusion', '--all-cells']


def invoke(command):
    done = subprocess.run(command, capture_output=True, text=True, timeout=60)
    assert done.returncode == 0, done.stdout + done.stderr
    return json.loads(done.stdout)


preview = invoke(command)
assert preview['valid'] and not preview['written']
assert [i['counts'] for i in preview['images']] == [
    dict(windows=4, tissue=2, excluded=2, cells=506)] * 2
manifest = output / 'manifest.json'
published = invoke(command + ['--output', str(manifest)])
assert published['written'] and published['manifest'] == preview['manifest']
(output / 'preview.json').write_bytes(json.dumps(preview, indent=2).encode('utf-8'))
result = invoke([sys.executable, '-I', str(runner), str(manifest), str(output / 'run')])
assert result['state'] == 'completed' and result['computed'] == 2 and result['rows'] == 8
with Path(result['csv_path']).open(encoding='utf-8', newline='') as stream:
    rows = list(csv.DictReader(stream))
assert [int(row['count']) for row in rows] == [289, 121, 168, 0] * 2
assert [float(row['area_pixels']) for row in rows] == [40576, 17536, 23040, 0] * 2
assert hashlib.sha256(source.read_bytes()).hexdigest() == before
receipt = dict(native_revision=q.build_environment().git_revision,
               native_tree_sha256=q.build_environment().source_tree_sha256,
               generator_sha256=hashlib.sha256(generator.read_bytes()).hexdigest(),
               runner_sha256=hashlib.sha256(runner.read_bytes()).hexdigest(),
               source_project_sha256=before, selected_images=2,
               counts_per_image=dict(windows=4, tissue=2, excluded=2, cells=506),
               rows=8, expected_cell_counts_per_image=[289, 121, 168, 0],
               expected_area_pixels_per_image=[40576, 17536, 23040, 0],
               preview_read_only=True, source_unchanged=True,
               scope='Same preserved reference crop twice; selection/CLI integration only. '
                     'Candidate4 and batch runner unchanged; no native build, wheel, '
                     'segmentation, new dataset or speed claim.')
(output / 'workflow.json').write_bytes(json.dumps(receipt, indent=2).encode('utf-8'))
print(json.dumps(receipt, indent=2))
