"""Measure cells/mm2 of explicit tissue within selected ROI windows.

Use existing Cell IDs; no segmentation is run. Example:
python density_workflow.py cells.qpxproj output --image 1 --window 10 --tissue 11 --cell 12
"""
import argparse
import json
from pathlib import Path

import qupathxx as q


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--image', type=int, required=True)
    parser.add_argument('--window', type=int, action='append', required=True)
    parser.add_argument('--tissue', type=int, action='append', required=True)
    parser.add_argument('--exclude', type=int, action='append', default=[])
    parser.add_argument('--cell', type=int, action='append', default=[])
    parser.add_argument('--body', action='store_true')
    parser.add_argument('--polygonize-curves', action='store_true')
    args = parser.parse_args()
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error('choose a new or empty output directory')
    project = q.Project.open(str(args.project.resolve()))
    doc = project.document(args.image)
    options = q.SpatialDensityOptions()
    options.prefer_nucleus = not args.body
    options.polygonize_curves = args.polygonize_curves
    request = q.SpatialDensityRequest(doc, args.window, args.tissue, args.exclude, args.cell, options)
    result = q.measure_spatial_density(request)
    record = doc.keep_density(request, result)
    args.output.mkdir(parents=True, exist_ok=True)
    q.save_spatial_density_csv(result, str(args.output / 'density.csv'))
    saved = args.output / 'study.qpxproj'
    project.save_new(str(saved))
    restored = q.Project.open(str(saved)).document(args.image)
    replay = q.SpatialDensityRequest.replay(restored, record)
    if not restored.verify_density(record, replay, q.measure_spatial_density(replay)):
        raise RuntimeError('saved density result did not replay exactly')
    summary = dict(source_revision=q.build_environment().git_revision,
                   algorithm=result.algorithm, geometry_policy=result.geometry_policy,
                   image_id=args.image, record_id=record, windows=result.window_count,
                   tissue=result.tissue_count, exclusions=result.exclusion_count, cells=result.cell_count,
                   units='cells/mm2', x_scale=result.x_scale, y_scale=result.y_scale,
                   input_sha256=result.input_sha256, result_sha256=result.sha256,
                   snapshot_bytes=request.snapshot_bytes, estimated_work_bytes=request.required_work_bytes,
                   membership_tests=result.membership_tests, geometry_operations=result.geometry_operations,
                   saved_replay=True)
    (args.output / 'workflow.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
