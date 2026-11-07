"""Preview/import matching NuCLS RGB PNG and CSV pairs into a new QPX project.

Requires candidate4, Pillow and numpy. Omit --output for a read-only preview.
Calibration and representation are explicit; no pixels/annotations are repaired.
"""
from __future__ import annotations

import argparse
import csv
import importlib.util
import math
from pathlib import Path
import json
import warnings

import numpy as np
from PIL import Image
import qupathxx as q

_spec = importlib.util.spec_from_file_location(
    '_nucls_import_runner', Path(__file__).with_name('project_density.py'))
runner = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(runner)

MAX_IMAGES = 100
MAX_PIXELS = 16_000_000  # Aggregate decoded RGB retained by the new project.
MAX_ROWS = 100_000
MAX_VERTICES = 1_000_000
CLASS_COLUMNS = ('raw_classification', 'main_classification', 'super_classification')
REQUIRED = {'type', 'xmin', 'ymin', 'xmax', 'ymax', 'coords_x', 'coords_y'}


def _number(value: str) -> float:
    number = float(value)
    if not math.isfinite(number):
        raise ValueError('coordinate must be finite')
    return number


def _geometry(row: dict) -> tuple[object, tuple[float, float], int]:
    bounds = [_number(row[name]) for name in ('xmin', 'ymin', 'xmax', 'ymax')]
    x0, y0, x1, y1 = bounds
    if not x1 > x0 or not y1 > y0:
        raise ValueError('bounds require positive width and height')
    if row['type'] == 'rectangle':
        roi = q.Roi.rectangle(q.Rect(x0, y0, x1 - x0, y1 - y0))
        return roi, (x0 / 2 + x1 / 2, y0 / 2 + y1 / 2), 4
    if row['type'] != 'polyline':
        raise ValueError('NuCLS type must be rectangle or polyline (closed polygon)')
    xs = [_number(v) for v in row['coords_x'].split(',')]
    ys = [_number(v) for v in row['coords_y'].split(',')]
    if len(xs) != len(ys) or not 3 <= len(xs) <= 10000:
        raise ValueError('polygon requires 3..10000 paired coordinates')
    if min(xs) != x0 or max(xs) != x1 or min(ys) != y0 or max(ys) != y1:
        raise ValueError('polygon bounds disagree with coordinates')
    roi = q.Roi.polygon([q.Point(x, y) for x, y in zip(xs, ys)])
    # Native topology validation above; signed-area moments in a local frame
    # avoid cancellation from large absolute coordinate offsets.
    points = [(x - x0, y - y0) for x, y in zip(xs, ys)]
    pairs = list(zip(points, points[1:] + points[:1]))
    cross = [a[0] * b[1] - b[0] * a[1] for a, b in pairs]
    area2 = math.fsum(cross)
    if not area2 or not math.isfinite(area2):
        raise ValueError('polygon has undefined centroid')
    center = tuple(origin + math.fsum((a[k] + b[k]) * c for (a, b), c in
                   zip(pairs, cross)) / (3 * area2) for k, origin in enumerate((x0, y0)))
    if not all(math.isfinite(v) for v in center):
        raise ValueError('polygon has nonfinite centroid')
    return roi, center, len(xs)


def run(images: Path, annotations: Path, *, mpp_x: float, mpp_y: float,
        representation: str, names: list[str] | None = None,
        class_column: str = 'main_classification', output: Path | None = None) -> dict:
    """Preview all selected pairs, publishing only if all are valid.

    Every source nucleus becomes a native Cell, with the supplied outline or
    a 1px square centroid marker in both body/nucleus slots. This does not infer
    whole-cell boundaries. The selected class and source class columns survive.
    """
    if representation not in ('outlines', 'centroids') or class_column not in CLASS_COLUMNS:
        raise ValueError('invalid representation or class column')
    if any(type(v) not in (int, float) or not math.isfinite(v) or v <= 0 for v in (mpp_x, mpp_y)):
        raise ValueError('explicit X/Y calibration must be positive finite micrometers/pixel')
    images, annotations = Path(images).resolve(strict=True), Path(annotations).resolve(strict=True)
    if not images.is_dir() or not annotations.is_dir():
        raise ValueError('images and annotations must be directories')
    image_files = {p.stem: p for p in images.glob('*.png') if p.is_file()}
    csv_files = {p.stem: p for p in annotations.glob('*.csv') if p.is_file()}
    if names is None:
        selected = sorted(image_files.keys() | csv_files.keys())
    else:
        if not isinstance(names, list) or any(not isinstance(n, str) or not n or
                n in ('.', '..') or any(c in n for c in '/\\:') for n in names):
            raise ValueError('names must be plain file stems')
        if len(set(names)) != len(names):
            raise ValueError('duplicate selected name')
        selected = sorted(names)
    if not 1 <= len(selected) <= MAX_IMAGES:
        raise ValueError(f'select 1..{MAX_IMAGES} pairs')
    target = Path(output).resolve() if output is not None else None
    if target is not None and target.exists():
        raise FileExistsError(f'output already exists: {target}')
    project, reports, guards = q.Project(), [], []
    pixels_used = rows_used = vertices_used = 0
    for name in selected:
        image_path, csv_path = images / (name + '.png'), annotations / (name + '.csv')
        report = dict(name=name, image=str(image_path), annotations=str(csv_path),
                      rows=0, imported=0, classes={}, source_bounds_outside=0,
                      issue_count=0, issues=[])
        reports.append(report)

        def issue(message):
            report['issue_count'] += 1
            if len(report['issues']) < 50:
                report['issues'].append(message)

        try:
            for path, files in ((image_path, image_files), (csv_path, csv_files)):
                if name not in files:
                    issue(f'missing file: {path}')
            if report['issue_count']:
                continue
            image_guard = runner.FileGuard(image_path, 32 * runner.MIB)
            csv_guard = runner.FileGuard(csv_path, 16 * runner.MIB)
            guards.extend((image_guard, csv_guard))
            report['image_sha256'], report['csv_sha256'] = image_guard.sha256, csv_guard.sha256
            with warnings.catch_warnings():
                warnings.simplefilter('error', Image.DecompressionBombWarning)
                with Image.open(image_guard.path) as image:
                    if image.format != 'PNG' or image.mode != 'RGB':
                        raise ValueError('requires an RGB PNG; no automatic color conversion')
                    width, height = image.size
                    pixels_used += width * height
                    if pixels_used > MAX_PIXELS:
                        raise ValueError(f'exceeds aggregate {MAX_PIXELS} pixels')
                    doc = q.Document(q.Image.from_numpy(np.asarray(image)))
            doc.set_calibration(mpp_x, mpp_y)
            report.update(width=width, height=height, calibration_um=[mpp_x, mpp_y])
            with csv_guard.path.open(encoding='utf-8-sig', newline='') as stream:
                reader = csv.DictReader(stream, strict=True)
                header = reader.fieldnames or []
                if len(set(header)) != len(header) or not REQUIRED.union({class_column}) <= set(header):
                    raise ValueError('CSV requires unique NuCLS coordinate/type/class columns')
                for ordinal, row in enumerate(reader, 1):
                    rows_used += 1
                    report['rows'] += 1
                    if rows_used > MAX_ROWS:
                        raise ValueError(f'exceeds aggregate {MAX_ROWS} annotation rows')
                    try:
                        if None in row or any(v is None for v in row.values()):
                            raise ValueError('CSV row width differs from header')
                        class_name = row[class_column]
                        if not class_name or class_name != class_name.strip():
                            raise ValueError('class must be nonempty with no surrounding whitespace')
                        roi, (cx, cy), vertices = _geometry(row)
                        vertices_used += vertices
                        if vertices_used > MAX_VERTICES:
                            raise ValueError(f'exceeds aggregate {MAX_VERTICES} vertices')
                        bounds = roi.bounds
                        outside = bounds.x < 0 or bounds.y < 0 or bounds.x + bounds.width > width or bounds.y + bounds.height > height
                        report['source_bounds_outside'] += int(outside)
                        if representation == 'centroids':
                            if not .5 <= cx <= width - .5 or not .5 <= cy <= height - .5:
                                raise ValueError('1px centroid marker does not fit image')
                            roi = q.Roi.rectangle(q.Rect(cx - .5, cy - .5, 1, 1))
                        if class_name not in report['classes']:
                            doc.set_class(class_name, 0x008800)
                        obj = doc.add_cell(roi, roi, class_name=class_name)
                        metadata = {key: row[key] for key in CLASS_COLUMNS if key in row}
                        metadata.update(source_row=str(ordinal), source_type=row['type'],
                                        representation=representation, class_column=class_column)
                        doc.set_object_properties(obj, q.ObjectProperties(metadata=metadata))
                        report['classes'][class_name] = report['classes'].get(class_name, 0) + 1
                        report['imported'] += 1
                    except (ValueError, RuntimeError, OverflowError) as error:
                        issue(f'annotation {ordinal} (CSV line {reader.line_num}): {error}')
                    if vertices_used > MAX_VERTICES:
                        break
            if not report['issue_count']:
                image_id = project.add_document(doc, name)
                project.set_image_metadata(image_id, dict(importer='qpx.nucls/1',
                    representation=representation, class_column=class_column,
                    calibration_source='explicit caller X/Y micrometers/pixel',
                    image_path=str(image_guard.path), csv_path=str(csv_guard.path),
                    image_sha256=image_guard.sha256, csv_sha256=csv_guard.sha256,
                    object_semantics='annotated nuclei in native Cell body/nucleus slots; not whole-cell boundaries'))
        except (OSError, ValueError, RuntimeError, csv.Error,
                Image.DecompressionBombWarning, Image.DecompressionBombError) as error:
            issue(str(error))
        if rows_used > MAX_ROWS or vertices_used > MAX_VERTICES or pixels_used > MAX_PIXELS:
            break
    valid = len(reports) == len(selected) and all(not r['issue_count'] for r in reports)
    report = dict(valid=valid, representation=representation, class_column=class_column,
                  calibration_um=[mpp_x, mpp_y], selected_pairs=len(selected),
                  inspected_pairs=len(reports), images=reports, written=False)
    for guard in guards:
        guard.check(rehash=True)
    if target is not None and valid:
        project.save_new(str(target), snapshot_bytes=512 * runner.MIB, file_bytes=1024 * runner.MIB)
        report.update(written=True, output=str(target))
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('images', type=Path)
    parser.add_argument('annotations', type=Path)
    parser.add_argument('--name', action='append', help='select a file stem; repeat; default union of PNG/CSV stems')
    parser.add_argument('--mpp-x', type=float, required=True)
    parser.add_argument('--mpp-y', type=float, required=True)
    parser.add_argument('--representation', choices=('outlines', 'centroids'), required=True)
    parser.add_argument('--class-column', choices=CLASS_COLUMNS, default='main_classification')
    parser.add_argument('--output', type=Path, help='new QPX; omit for preview')
    args = parser.parse_args()
    try:
        result = run(args.images, args.annotations, mpp_x=args.mpp_x, mpp_y=args.mpp_y,
                     representation=args.representation, names=args.name,
                     class_column=args.class_column, output=args.output)
        print(json.dumps(result, indent=2, allow_nan=False))
        return 0 if result['valid'] else 1
    except Exception as error:
        print(json.dumps(dict(error=str(error))))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
