"""Run a bounded slide ROI through native analysis and a reopenable project.

Use an installed qupathxx with a reader for the input format:
  python region_workflow.py slide.svs output --roi 12000 12000 512 512
Add --show to inspect the saved project in ImGui. Existing outputs are preserved.
"""
import argparse
import json
from pathlib import Path

import qupathxx as q


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--roi', nargs=4, type=int, metavar=('X', 'Y', 'WIDTH', 'HEIGHT'),
                        help='level-zero rectangle; default is at most 512x512 at the origin')
    parser.add_argument('--show', action='store_true')
    parser.add_argument('--whole-slide-tissue', action='store_true',
                        help='stable tissue snapshot of a native pyramid level; ROI statistics remain bounded')
    parser.add_argument('--tissue-level', type=int,
                        help='native level above zero for whole-slide tissue; default is the coarsest level')
    parser.add_argument('--tissue-memory-mib', type=int,
                        help='native tissue working-memory limit, 1..512 MiB (default: 64 ROI / 128 whole slide)')
    args = parser.parse_args()
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error('choose a new or empty output directory')
    project = q.Project()
    image_id = project.add_image(str(args.image.resolve()))
    doc = project.document(image_id)
    metadata = doc.metadata
    tissue_parameters = q.TissueParameters()
    if args.tissue_level is not None and not args.whole_slide_tissue:
        parser.error('--tissue-level requires --whole-slide-tissue')
    tissue_memory_mib = args.tissue_memory_mib
    if tissue_memory_mib is None:
        tissue_memory_mib = 128 if args.whole_slide_tissue else 64
    if not 1 <= tissue_memory_mib <= 512:
        parser.error('--tissue-memory-mib must be between 1 and 512')
    tissue_parameters.work_bytes = tissue_memory_mib * 1024 * 1024
    tissue_level = args.tissue_level if args.tissue_level is not None else len(metadata.levels) - 1
    if args.whole_slide_tissue:
        if not hasattr(tissue_parameters, 'canonical_pyramid'):
            parser.error('whole-slide tissue requires a qupathxx build with stable pyramid snapshots')
        if tissue_level <= 0 or tissue_level >= len(metadata.levels):
            parser.error('whole-slide tissue requires an available native pyramid level above zero')
        tissue_parameters.level = tissue_level
        tissue_parameters.canonical_pyramid = True
    x, y, width, height = args.roi or (0, 0, min(metadata.width, 512), min(metadata.height, 512))
    if (x < 0 or y < 0 or width <= 0 or height <= 0 or
            x + width > metadata.width or y + height > metadata.height):
        parser.error('ROI must be a positive rectangle within the level-zero image')
    if width * height > 1024 * 1024:
        parser.error('this bounded example supports at most 1,048,576 ROI pixels')
    obj = doc.add_rectangle(q.Rect(x, y, width, height), 'Scripted ROI')
    tissue_object = (doc.add_rectangle(q.Rect(0, 0, metadata.width, metadata.height),
                                      f'Whole-slide tissue (level {tissue_level})')
                     if args.whole_slide_tissue else obj)
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
    request = q.AnalysisRequest.with_intensity_statistics(
        doc, obj, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS)
    result = q.analyze(request)
    analysis_id = doc.keep_analysis(request, result)
    tissue_request = q.TissueRequest(doc, tissue_object, transform, tissue_parameters)
    tissue = q.detect_tissue(tissue_request)
    tissue_id = doc.keep_tissue(tissue_request, tissue)

    stats = result.intensity_statistics

    args.output.mkdir(parents=True, exist_ok=True)
    project_path = args.output / 'study.qpxproj'
    project.save_new(str(project_path))
    reopened = q.Project.open(str(project_path))
    restored = reopened.document(image_id)
    replay = q.AnalysisRequest.replay(restored, analysis_id)
    if not restored.verify_analysis(analysis_id, replay, q.analyze(replay)):
        raise RuntimeError('saved region analysis did not replay')
    replay_tissue = q.TissueRequest.replay(restored, tissue_id)
    if not restored.verify_tissue(tissue_id, replay_tissue, q.detect_tissue(replay_tissue)):
        raise RuntimeError('saved tissue analysis did not replay')
    for kind, name, record in [(q.MeasurementTableKind.ROI, 'measurements.csv', analysis_id),
                               (q.MeasurementTableKind.TISSUE, 'tissue.csv', tissue_id)]:
        table = q.MeasurementTableRequest(restored, kind, [record])
        q.save_measurements_csv(table, str(args.output / name))
    environment = q.build_environment()
    summary = dict(source_revision=environment.git_revision,
                   source_tree_sha256=environment.source_tree_sha256,
                   source=str(args.image.resolve()), image_id=image_id, object_id=obj,
                   roi=[x, y, width, height], pixel_width_um=metadata.pixel_width_um,
                   pixel_height_um=metadata.pixel_height_um, samples=stats.samples,
                   tissue_foreground_pixels=tissue.foreground_pixels,
                   tissue_components=tissue.component_count,
                   tissue_object_id=tissue_object, tissue_level=tissue.level,
                   tissue_width=tissue.width, tissue_height=tissue.height,
                   tissue_samples=tissue.samples,
                   tissue_work_bytes=tissue_request.estimated_work_bytes,
                   whole_slide_tissue=args.whole_slide_tissue,
                   analysis_record=analysis_id, tissue_record=tissue_id,
                   saved_replay=True)
    (args.output / 'workflow.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    if args.show:
        q.show(reopened)
    print(json.dumps(summary, indent=2))
    print(project_path)


if __name__ == '__main__':
    main()
