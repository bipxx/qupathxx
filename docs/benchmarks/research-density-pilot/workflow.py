"""Bounded NuCLS pilot: downloaded files directory, new output directory.

Requires installed candidate4, existing Pillow/numpy, and the source files named
in sources.json. No download, resampling, segmentation or native rebuild here.
"""
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image
import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]


def centroid(row):
    if row['type'] == 'rectangle':
        return ((float(row['xmin']) + float(row['xmax'])) / 2,
                (float(row['ymin']) + float(row['ymax'])) / 2)
    assert row['type'] == 'polyline'
    points = list(zip(map(float, row['coords_x'].split(',')),
                      map(float, row['coords_y'].split(',')), strict=True))
    cross = [x * yy - xx * y for (x, y), (xx, yy) in zip(points, points[1:] + points[:1])]
    area2 = sum(cross)
    assert area2 != 0
    return tuple(sum((a[k] + b[k]) * c for a, b, c in
                     zip(points, points[1:] + points[:1], cross)) / (3 * area2)
                 for k in (0, 1))


def invoke(command):
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return json.loads(result.stdout)


def main():
    source = Path(sys.argv[1]).resolve(strict=True)
    output = Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=False)
    sources = json.loads(Path(__file__).with_name('sources.json').read_text(encoding='utf-8'))
    for asset in sources['files']:
        assert hashlib.sha256((source / asset['name']).read_bytes()).hexdigest() == asset['sha256']
    project, expected = q.Project(), []
    for case in sources['cases']:
        base = case['name']
        with Image.open(source / (base + '.png')) as rgb:
            assert rgb.mode == 'RGB'
            pixels = np.asarray(rgb)
        with Image.open(source / (base + '-mask.png')) as mask:
            labels = np.asarray(mask)[:, :, 0]
        height, width = pixels.shape[:2]
        yy, xx = np.where(labels == 253)  # Author's nucleus_GTcodes.csv: FOV.
        x0, y0, x1, y1 = int(xx.min()), int(yy.min()), int(xx.max()) + 1, int(yy.max()) + 1
        assert 0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height
        assert np.all(labels[y0:y1, x0:x1] != 0)
        # The chosen FOV rectangles lie inside both original image/mask extents.
        # No rescaling/cropping/repair of the one-pixel outer-extent difference.
        doc = q.Document(q.Image(width, height, pixels.reshape(-1).tolist()))
        doc.set_calibration(.2, .2)
        for name in ('Analysis windows', 'Annotated FOV support'):
            doc.set_class(name, 0x008800)
            obj = doc.add_rectangle(q.Rect(x0, y0, x1 - x0, y1 - y0))
            doc.classify_annotation(obj, name)
        with (source / (base + '.csv')).open(encoding='utf-8', newline='') as stream:
            annotations = list(csv.DictReader(stream))
        classes = sorted({row['main_classification'] for row in annotations})
        for name in classes:
            doc.set_class(name, 0x880000)
        counts = dict(rectangle=0, polyline=0)
        centers = []
        for row in annotations:
            counts[row['type']] += 1
            x, y = centroid(row)
            assert .5 <= x <= width - .5 and .5 <= y <= height - .5
            roi = q.Roi.rectangle(q.Rect(x - .5, y - .5, 1, 1))
            # Centroid-only representation: no claim that marker shapes are
            # nuclear outlines or whole-cell bodies. Keep original CSVs intact.
            doc.add_cell(roi, roi, class_name=row['main_classification'])
            centers.append((x, y))
        count = sum(x0 <= x <= x1 and y0 <= y <= y1 for x, y in centers)
        image_id = project.add_document(doc, base)
        project.set_image_metadata(image_id, dict(dataset='NuCLS corrected single-rater',
            license='CC0-1.0', units='annotated nuclei/mm2 of annotation FOV',
            annotation_representation='1px square Cell/nucleus markers at original annotation centroids; source CSVs preserved'))
        area = (x1 - x0) * (y1 - y0)
        expected.append(dict(image_id=image_id, case=base, image_size=[width, height],
            mask_size=[labels.shape[1], labels.shape[0]], fov=[x0, y0, x1, y1],
            annotation_count=len(annotations), annotation_types=counts, classes=classes,
            count=count, area_pixels=area, area_mm2=area * .2 * .2 / 1e6,
            density_per_mm2=count / (area * .2 * .2 / 1e6)))
    path = output / 'nucls.qpxproj'
    project.save_new(str(path))
    before = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = output / 'manifest.json'
    generator = ROOT / 'examples/density_manifest.py'
    runner = ROOT / 'examples/project_density.py'
    preview = invoke([sys.executable, '-I', str(generator), str(path),
        '--window-class', 'Analysis windows', '--tissue-class', 'Annotated FOV support', '--all-cells'])
    assert preview['valid'] and not preview['written']
    published = invoke([sys.executable, '-I', str(generator), str(path), '--output', str(manifest),
        '--window-class', 'Analysis windows', '--tissue-class', 'Annotated FOV support', '--all-cells'])
    assert published['written'] and published['manifest'] == preview['manifest']
    run = invoke([sys.executable, '-I', str(runner), str(manifest), str(output / 'run')])
    assert run['state'] == 'completed' and run['rows'] == run['computed'] == 2
    with Path(run['csv_path']).open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    for row, oracle in zip(rows, expected, strict=True):
        assert int(row['image_id']) == oracle['image_id']
        assert int(row['count']) == oracle['count']
        for name in ('area_pixels', 'area_mm2', 'density_per_mm2'):
            assert math.isclose(float(row[name]), oracle[name], rel_tol=1e-12)
    assert hashlib.sha256(path.read_bytes()).hexdigest() == before
    receipt = dict(dataset=sources['dataset'], source_files_verified=len(sources['files']),
        native_revision=q.build_environment().git_revision,
        native_tree_sha256=q.build_environment().source_tree_sha256,
        generator_sha256=hashlib.sha256(generator.read_bytes()).hexdigest(),
        runner_sha256=hashlib.sha256(runner.read_bytes()).hexdigest(),
        workflow_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        source_project_sha256=before, source_unchanged=True,
        preview_valid=True, computed_images=2, csv_rows=2, independent_oracles_match=True,
        expected=expected,
        limits='Public two-case annotated-nucleus FOV pilot, not tissue segmentation, whole-cell '
               'segmentation, independent clinical validation, cohort statistics or speed acceptance. '
               'Native Cells/nuclei are 1px square centroid markers, not imported outlines. '
               'Original RGB/mask outer extents differ by one pixel; support lies inside both. '
               'One source rectangle extends past RGB; all original centroid coordinates are '
               'preserved without clipping or resampling.')
    for name, value in [('preview.json', preview), ('workflow.json', receipt)]:
        (output / name).write_bytes(json.dumps(value, indent=2).encode('utf-8'))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
