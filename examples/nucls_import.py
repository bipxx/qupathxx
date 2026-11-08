"""Preview/import matching NuCLS RGB8 PNG and CSV pairs through native services.

Omit --output for a read-only preview. Calibration and representation are explicit.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import qupathxx as q
from qupathxx._workflow_control import await_report

CLASS_COLUMNS = ('raw_classification', 'main_classification', 'super_classification')


def run(images: Path, annotations: Path, *, mpp_x: float, mpp_y: float,
        representation: str, names: list[str] | None = None,
        class_column: str = 'main_classification', output: Path | None = None) -> dict:
    options = q.NuclsImportOptions()
    options.mpp_x, options.mpp_y = mpp_x, mpp_y
    options.representation, options.class_column = representation, class_column
    options.names = names
    job = q.NuclsImportJob()
    job.run(str(images), str(annotations), options, '' if output is None else str(output))
    return json.loads(await_report(job, take=job.take_pairs, acknowledge=job.continue_pairs).json)


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
