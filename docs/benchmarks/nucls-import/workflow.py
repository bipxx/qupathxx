"""Installed CLI import -> saved project -> existing density consumers.

Arguments: preserved NuCLS download directory, new output directory.
Reuses the prior public pilot's pinned inputs and independent count/area oracles.
"""
import csv
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image
import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]
PRIOR = ROOT / 'docs/benchmarks/research-density-pilot'


def invoke(command, expected=0):
    completed = subprocess.run(command, capture_output=True, text=True, timeout=60)
    assert completed.returncode == expected, completed.stdout + completed.stderr
    return json.loads(completed.stdout)


def main():
    source, output = Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=False)
    sources = json.loads((PRIOR / 'sources.json').read_text(encoding='utf-8'))
    prior = json.loads((PRIOR / 'workflow.json').read_text(encoding='utf-8'))
    for asset in sources['files']:
        assert hashlib.sha256((source / asset['name']).read_bytes()).hexdigest() == asset['sha256']
    script = ROOT / 'examples/nucls_import.py'
    command = [sys.executable, '-I', str(script), str(source), str(source), '--mpp-x', '.2', '--mpp-y', '.2']
    for case in sources['cases']:
        command += ['--name', case['name']]
    outlines = invoke(command + ['--representation', 'outlines'], expected=1)
    assert not outlines['valid'] and outlines['images'][1]['source_bounds_outside'] == 1
    markers = command + ['--representation', 'centroids']
    preview = invoke(markers)
    assert preview['valid'] and not preview['written']
    assert [row['imported'] for row in preview['images']] == [32, 28]
    original = output / 'imported.qpxproj'
    published = invoke(markers + ['--output', str(original)])
    assert published['written']
    before = hashlib.sha256(original.read_bytes()).hexdigest()
    project = q.Project.open_without_journal_recovery(str(original))
    for image, oracle in zip(project.images, prior['expected'], strict=True):
        doc = project.document(image.id)
        assert image.name == oracle['case']
        assert doc.pixel_width_um == doc.pixel_height_um == .2
        with Image.open(source / (image.name + '.png')) as rgb:
            assert np.array_equal(doc.image.numpy(), np.asarray(rgb))
        assert len(doc.annotations) == oracle['annotation_count']
        assert sorted({obj.class_name for obj in doc.annotations}) == oracle['classes']
        assert all(obj.properties.metadata['representation'] == 'centroids' for obj in doc.annotations)
        assert all(obj.geometry.area == 1 for obj in doc.annotations)
        # Reuse the pinned, already checked FOV support. Importer itself does
        # not invent analysis windows, tissue masks or exclusion regions.
        x0, y0, x1, y1 = oracle['fov']
        for name in ('Analysis windows', 'Annotated FOV support'):
            doc.set_class(name, 0x008800)
            obj = doc.add_rectangle(q.Rect(x0, y0, x1 - x0, y1 - y0))
            doc.classify_annotation(obj, name)
    study = output / 'analysis.qpxproj'
    project.save_new(str(study))
    manifest = output / 'manifest.json'
    selected = invoke([sys.executable, '-I', str(ROOT / 'examples/density_manifest.py'), str(study),
        '--window-class', 'Analysis windows', '--tissue-class', 'Annotated FOV support',
        '--all-cells', '--output', str(manifest)])
    assert selected['written']
    result = invoke([sys.executable, '-I', str(ROOT / 'examples/project_density.py'),
                     str(manifest), str(output / 'density')])
    assert result['state'] == 'completed' and result['computed'] == result['rows'] == 2
    with Path(result['csv_path']).open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    for row, expected in zip(rows, prior['expected'], strict=True):
        assert int(row['count']) == expected['count']
        for field in ('area_pixels', 'area_mm2', 'density_per_mm2'):
            assert math.isclose(float(row[field]), expected[field], rel_tol=1e-12)
    assert hashlib.sha256(original.read_bytes()).hexdigest() == before
    receipt = dict(native_revision=q.build_environment().git_revision,
        native_tree_sha256=q.build_environment().source_tree_sha256,
        importer_sha256=hashlib.sha256(script.read_bytes()).hexdigest(),
        workflow_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        imported_project_sha256=before, imported_project_unchanged=True,
        original_pixels_exact=True, source_files_verified=7, imported_pairs=2,
        imported_annotations=[32, 28], all_classes_preserved=True,
        outlines_reject_outside_source=True, centroids_explicit=True,
        density_counts=[int(row['count']) for row in rows],
        density_areas_pixels=[float(row['area_pixels']) for row in rows],
        prior_pilot_oracles_match=True,
        python_dependencies={name:dict(version=importlib.metadata.version(name),
            license=importlib.metadata.metadata(name).get('License-Expression') or
                    importlib.metadata.metadata(name).get('License', '').splitlines()[0])
            for name in ('Pillow', 'numpy')},
        limits='NuCLS PNG/CSV only; Cell body/nucleus markers are annotated nuclei, '
               'not whole cells. FOV support/oracles reused from the prior pilot. '
               'No new native build, package/install, download or scientific acceptance.')
    for name, value in [('workflow.json', receipt), ('preview.json', preview),
                        ('outlines-preview.json', outlines)]:
        (output / name).write_bytes(json.dumps(value, indent=2).encode('utf-8'))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
