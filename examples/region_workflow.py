"""Run a bounded slide ROI through native analysis and a reopenable project.

Use an installed qupathxx with NumPy and a reader for the input format:
  python region_workflow.py slide.svs output --roi 12000 12000 512 512
Add --show to inspect the saved project in ImGui. Existing outputs are preserved.
"""
import argparse
import csv
import io
import json
import math
from fractions import Fraction
from pathlib import Path

import numpy as np
import qupathxx as q


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--roi', nargs=4, type=int, metavar=('X', 'Y', 'WIDTH', 'HEIGHT'),
                        help='level-zero rectangle; default is at most 512x512 at the origin')
    parser.add_argument('--show', action='store_true')
    args = parser.parse_args()
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error('choose a new or empty output directory')
    project = q.Project()
    image_id = project.add_image(str(args.image.resolve()))
    doc = project.document(image_id)
    metadata = doc.metadata
    x, y, width, height = args.roi or (0, 0, min(metadata.width, 512), min(metadata.height, 512))
    if (x < 0 or y < 0 or width <= 0 or height <= 0 or
            x + width > metadata.width or y + height > metadata.height):
        parser.error('ROI must be a positive rectangle within the level-zero image')
    if width * height > 1024 * 1024:
        parser.error('this bounded example supports at most 1,048,576 ROI pixels')
    obj = doc.add_rectangle(q.Rect(x, y, width, height), 'Scripted ROI')
    pixels = doc.server.read_region(q.RegionRequest(x=x, y=y, level=0, width=width, height=height))
    rgba = pixels.numpy()
    if rgba.dtype != np.uint8 or rgba.shape != (height, width, 4):
        raise RuntimeError('this workflow requires an RGBA8 region')
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
    request = q.AnalysisRequest.with_intensity_statistics(
        doc, obj, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS)
    result = q.analyze(request)
    analysis_id = doc.keep_analysis(request, result)
    tissue_request = q.TissueRequest(doc, obj, transform)
    tissue = q.detect_tissue(tissue_request)
    tissue_id = doc.keep_tissue(tissue_request, tissue)

    # An independent pixel check covers the real-slide reader/statistics plumbing.
    # Kernel and stain/tissue accuracy retain their existing fixture evidence.
    rgb = rgba[:, :, :3][rgba[:, :, 3] != 0].astype(np.int64)
    stats = result.intensity_statistics
    np.testing.assert_equal(stats.samples, len(rgb))
    if len(rgb):
        for field, expected in [('sum', rgb.sum(axis=0)), ('minimum', rgb.min(axis=0)),
                                ('maximum', rgb.max(axis=0))]:
            np.testing.assert_array_equal(getattr(stats.rgb, field), expected)
        if len(rgb) > 1:
            # Integer moments avoid a large floating reduction in the oracle.
            # Fraction preserves the exact sample variance until the final sqrt.
            n = len(rgb)
            sums, squares = rgb.sum(axis=0), (rgb * rgb).sum(axis=0)
            expected_sd = [math.sqrt(float(Fraction(n * int(ss) - int(s) ** 2, n * (n - 1))))
                           for s, ss in zip(sums, squares)]
            np.testing.assert_allclose(stats.rgb.sample_standard_deviation,
                                       expected_sd, rtol=0, atol=1e-10)

    args.output.mkdir(parents=True, exist_ok=True)
    project_path = args.output / 'study.qpxproj'
    project.save(str(project_path))
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
        text = q.measurements_csv(table)
        rows = list(csv.DictReader(io.StringIO(text)))
        expected_rows = 1 if kind == q.MeasurementTableKind.ROI else tissue.component_count
        if len(rows) != expected_rows:
            raise RuntimeError('measurement rows do not match retained results')
        if kind == q.MeasurementTableKind.TISSUE:
            if sum(int(row['component_samples']) for row in rows) != tissue.foreground_pixels:
                raise RuntimeError('component CSV does not account for the tissue mask')
        q.save_measurements_csv(table, str(args.output / name))
    environment = q.build_environment()
    summary = dict(source_revision=environment.git_revision,
                   source_tree_sha256=environment.source_tree_sha256,
                   source=str(args.image.resolve()), image_id=image_id, object_id=obj,
                   roi=[x, y, width, height], pixel_width_um=metadata.pixel_width_um,
                   pixel_height_um=metadata.pixel_height_um, samples=stats.samples,
                   tissue_foreground_pixels=tissue.foreground_pixels,
                   tissue_components=tissue.component_count,
                   analysis_record=analysis_id, tissue_record=tissue_id,
                   independent_rgb_statistics=True, saved_replay=True)
    (args.output / 'workflow.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    if args.show:
        q.show(reopened)
    print(json.dumps(summary, indent=2))
    print(project_path)


if __name__ == '__main__':
    main()
