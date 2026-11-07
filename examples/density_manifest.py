"""Preview exact class selections and write a project_density.py manifest.

Runs against installed candidate4; omit --output for a read-only preview.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
from typing import Sequence

import qupathxx as q

# Resolve this trusted sibling explicitly, including when invoked with python -I.
_spec = importlib.util.spec_from_file_location(
    '_density_manifest_runner', Path(__file__).with_name('project_density.py'))
runner = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(runner)


def _names(values: Sequence[str], role: str, required: bool = False) -> set[str]:
    if isinstance(values, str) or any(not isinstance(v, str) or not v for v in values):
        raise ValueError(f'{role} requires nonempty exact class names')
    result = set(values)
    if required and not result:
        raise ValueError(f'{role} requires at least one class')
    return result


def generate(project: Path, *, window_classes: Sequence[str],
             tissue_classes: Sequence[str], exclusion_classes: Sequence[str] = (),
             cell_classes: Sequence[str] = (), all_cells: bool = False,
             image_ids: Sequence[int] = (), prefer_nucleus: bool = True,
             polygonize_curves: bool = False, output: Path | None = None) -> dict:
    """Return preview; publish explicit IDs only if all images pass native capture.

    Class matching is exact, case-sensitive and does not expand descendants.
    A requested class must have objects in every selected image. With all_cells,
    zero Cells is legitimate. No exclusion selector means an empty exclusion set.
    Native capture validates the selected geometry, planes, budgets/calibration;
    this does not execute Boolean area/count computation or save the project.
    """
    roles = dict(windows=_names(window_classes, 'windows', True),
                 tissue=_names(tissue_classes, 'tissue', True),
                 excluded=_names(exclusion_classes, 'excluded'),
                 cells=_names(cell_classes, 'cells'))
    for value in (all_cells, prefer_nucleus, polygonize_curves):
        if type(value) is not bool:
            raise ValueError('selection/options flags must be booleans')
    if all_cells == bool(roles['cells']):
        raise ValueError('choose either cell classes or all_cells')
    selected = {runner._integer(i, 'image_id') for i in image_ids}
    if len(selected) != len(image_ids):
        raise ValueError('duplicate image_id')
    target = Path(output).resolve() if output is not None else None
    if target is not None and target.exists():
        raise FileExistsError(f'manifest already exists: {target}')
    guard = runner.FileGuard(Path(project), 1024 * runner.MIB, project=True)
    source = q.Project.open_without_journal_recovery(str(guard.path))
    images = sorted(source.images, key=lambda image: image.id)
    unknown = selected - {image.id for image in images}
    if unknown:
        raise ValueError(f'unknown image IDs: {sorted(unknown)}')
    images = [image for image in images if not selected or image.id in selected]
    if not 1 <= len(images) <= 1000:
        raise ValueError('select 1..1000 images')
    options = q.SpatialDensityOptions()
    options.prefer_nucleus = prefer_nucleus
    options.polygonize_curves = polygonize_curves
    entries, preview = [], []
    for image in images:
        guard.check()
        item = dict(image_id=image.id, **{role: [] for role in roles})
        row = dict(image_id=image.id, name=image.name, counts={}, calibration_um=None,
                   issues=[])
        try:
            doc = source.document(image.id)
            row['calibration_um'] = [doc.pixel_width_um, doc.pixel_height_um]
            found = {role: set() for role in roles}
            for obj in doc.annotations:
                for role, names in roles.items():
                    if obj.class_name in names:
                        found[role].add(obj.class_name)
                        item[role].append(obj.id)
                if all_cells and obj.kind == q.ObjectKind.CELL:
                    item['cells'].append(obj.id)
            for role, names in roles.items():
                missing = names - found[role]
                if missing:
                    row['issues'].append(f'{role}: no objects for classes {sorted(missing)}')
                item[role].sort()
                cap = 100000 if role == 'cells' else 10000
                if len(item[role]) > cap:
                    row['issues'].append(f'{role}: exceeds {cap} objects')
            if not row['issues']:
                q.SpatialDensityRequest(doc, item['windows'], item['tissue'],
                                        item['excluded'], item['cells'], options)
        except Exception as error:
            row['issues'].append(str(error))
        row['counts'] = {role: len(item[role]) for role in roles}
        entries.append(item)
        preview.append(row)
    manifest = dict(schema=runner.SCHEMA, project=str(guard.path), images=entries,
                    options=dict(prefer_nucleus=prefer_nucleus,
                                 polygonize_curves=polygonize_curves))
    valid = all(not row['issues'] for row in preview)
    report = dict(valid=valid, project=str(guard.path), project_sha256=guard.sha256,
                  selectors={role: sorted(names) for role, names in roles.items()},
                  all_cells=all_cells, images=preview, manifest=manifest,
                  written=False)
    guard.check(rehash=True)
    if target is not None and valid:
        runner._atomic_json(target, manifest)
        report.update(written=True, output=str(target))
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('--output', type=Path, help='new manifest; omit for preview')
    parser.add_argument('--image', action='append', type=int, default=[], help='image ID; repeat; default all')
    parser.add_argument('--window-class', action='append', required=True)
    parser.add_argument('--tissue-class', action='append', required=True)
    parser.add_argument('--exclusion-class', action='append', default=[])
    cells = parser.add_mutually_exclusive_group(required=True)
    cells.add_argument('--cell-class', action='append', default=[])
    cells.add_argument('--all-cells', action='store_true')
    parser.add_argument('--body-centroids', action='store_true')
    parser.add_argument('--polygonize-curves', action='store_true')
    args = parser.parse_args()
    try:
        report = generate(args.project, window_classes=args.window_class,
                          tissue_classes=args.tissue_class,
                          exclusion_classes=args.exclusion_class,
                          cell_classes=args.cell_class, all_cells=args.all_cells,
                          image_ids=args.image, prefer_nucleus=not args.body_centroids,
                          polygonize_curves=args.polygonize_curves, output=args.output)
        print(json.dumps(report, indent=2, ensure_ascii=True, allow_nan=False))
        return 0 if report['valid'] else 1
    except Exception as error:
        print(json.dumps({'error': str(error)}, ensure_ascii=True))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
