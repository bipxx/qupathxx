"""Count selected object centroids within a calibrated radius and retain/replay.

Uses existing objects in a native project; it does not run cell segmentation.
Example: python neighborhood_workflow.py cells.qpxproj output --image 1
         --source 10 --source 11 --target 12 --radius 25 --units um
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
    parser.add_argument('--source', type=int, action='append', required=True)
    parser.add_argument('--target', type=int, action='append', default=[])
    parser.add_argument('--radius', type=float, required=True)
    parser.add_argument('--units', choices=['px', 'um'], required=True)
    parser.add_argument('--include-self', action='store_true')
    parser.add_argument('--body', action='store_true', help='use cell-body centroid instead of an available nucleus')
    args = parser.parse_args()
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error('choose a new or empty output directory')
    project = q.Project.open(str(args.project.resolve()))
    doc = project.document(args.image)
    options = q.SpatialNeighborhoodOptions()
    options.radius = args.radius
    options.units = q.SpatialDistanceUnits.PIXELS if args.units == 'px' else q.SpatialDistanceUnits.MICROMETRES
    options.exclude_self, options.prefer_nucleus = not args.include_self, not args.body
    request = q.SpatialNeighborhoodRequest(doc, args.source, args.target, options)
    result = q.measure_spatial_neighborhoods(request)
    record = doc.keep_neighborhood(request, result)
    args.output.mkdir(parents=True, exist_ok=True)
    csv_path, saved = args.output / 'neighborhoods.csv', args.output / 'study.qpxproj'
    q.save_spatial_neighborhood_csv(result, str(csv_path))
    project.save_new(str(saved))
    restored = q.Project.open(str(saved)).document(args.image)
    replay = q.SpatialNeighborhoodRequest.replay(restored, record)
    if not restored.verify_neighborhood(record, replay, q.measure_spatial_neighborhoods(replay)):
        raise RuntimeError('saved neighborhood result did not replay')
    summary = dict(source_revision=q.build_environment().git_revision, algorithm=result.algorithm,
                   image_id=args.image, record_id=record, sources=result.source_count,
                   targets=result.target_count, radius=options.radius, units=args.units,
                   x_scale=result.x_scale, y_scale=result.y_scale, rows=result.row_count,
                   input_sha256=result.input_sha256, result_sha256=result.sha256,
                   snapshot_bytes=request.snapshot_bytes, estimated_work_bytes=request.required_work_bytes,
                   distance_tests=result.distance_tests, saved_replay=True)
    (args.output / 'workflow.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
