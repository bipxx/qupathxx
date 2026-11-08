"""Preview exact native class selections and optionally save a density manifest."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

import qupathxx as q
from qupathxx._workflow_control import await_report


def generate(project: Path, *, window_classes: Sequence[str],
             tissue_classes: Sequence[str], exclusion_classes: Sequence[str] = (),
             cell_classes: Sequence[str] = (), all_cells: bool = False,
             image_ids: Sequence[int] = (), prefer_nucleus: bool = True,
             polygonize_curves: bool = False, output: Path | None = None) -> dict:
    recipe = q.ProjectDensityRecipe()
    recipe.classes = (window_classes, tissue_classes, exclusion_classes, cell_classes)
    recipe.images = image_ids
    recipe.all_images, recipe.all_cells = not image_ids, all_cells
    recipe.options.prefer_nucleus = prefer_nucleus
    recipe.options.polygonize_curves = polygonize_curves
    job = q.ProjectDensityJob()
    job.generate_manifest(str(project), recipe, '' if output is None else str(output))
    return json.loads(await_report(job, result='manifest_result').json)


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
