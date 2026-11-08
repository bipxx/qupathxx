"""Independently check saved region pixels/statistics and CSV accounting.

Development oracle only: python -I tools/validate_region_workflow.py OUTPUT_DIRECTORY
"""
import csv
from fractions import Fraction
import json
import math
from pathlib import Path
import sys

import numpy as np
import qupathxx as q


def validate(output: Path):
    summary = json.loads((output / 'workflow.json').read_text(encoding='utf-8'))
    project = q.Project.open_without_journal_recovery(str(output / 'study.qpxproj'))
    doc = project.document(summary['image_id'])
    x, y, width, height = summary['roi']
    rgba = doc.server.read_region(q.RegionRequest(x, y, 0, width, height)).numpy()
    assert rgba.dtype == np.uint8 and rgba.shape == (height, width, 4)
    rgb = rgba[:, :, :3][rgba[:, :, 3] == 255].astype(np.int64)
    stats = doc.analysis_record(summary['analysis_record']).result.intensity_statistics
    np.testing.assert_equal(stats.samples, len(rgb))
    if len(rgb):
        for field, expected in [('sum', rgb.sum(axis=0)), ('minimum', rgb.min(axis=0)),
                                ('maximum', rgb.max(axis=0))]:
            np.testing.assert_array_equal(getattr(stats.rgb, field), expected)
        if len(rgb) > 1:
            n = len(rgb)
            sums, squares = rgb.sum(axis=0), (rgb * rgb).sum(axis=0)
            expected_sd = [math.sqrt(float(Fraction(n * int(ss) - int(s) ** 2, n * (n - 1))))
                           for s, ss in zip(sums, squares)]
            np.testing.assert_allclose(stats.rgb.sample_standard_deviation,
                                       expected_sd, rtol=0, atol=1e-10)
    tissue = doc.tissue_record(summary['tissue_record']).result
    for name, count in [('measurements.csv', 1), ('tissue.csv', max(1, tissue.component_count))]:
        with (output / name).open(encoding='utf-8', newline='') as stream:
            rows = list(csv.DictReader(stream))
        assert len(rows) == count
        if name == 'tissue.csv':
            assert sum(int(row['component_samples'] or 0) for row in rows) == tissue.foreground_pixels
    return dict(independent_rgb_statistics=True, csv_accounting=True, samples=len(rgb))


if __name__ == '__main__':
    print(json.dumps(validate(Path(sys.argv[1]))))
