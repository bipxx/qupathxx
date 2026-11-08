"""Strict sampled-pixel cutoffs and saved replay at the no-growth boundary."""
import math

import numpy as np
import pytest
import qupathxx as q

from test_log_body_cells import body_parameters
from test_sampled_cells import sampled_parameters
from test_log_cells import scene


@pytest.mark.parametrize('sampled', [False, True], ids=['legacy-bodies', 'sampled-bodies'])
@pytest.mark.parametrize('radius', [0., .5, 1., math.nextafter(1., math.inf)],
                         ids=['zero', 'fractional', 'one', 'just-above-one'])
def test_strict_unit_radius_labels_measurements_and_saved_replay(tmp_path, sampled, radius):
    doc, obj, transform = scene()
    parameters = sampled_parameters() if sampled else body_parameters()
    parameters.cell_expansion = radius
    request = q.CellRequest(doc, obj, transform, parameters)
    result = q.detect_cells(request)
    nuclei = result.nuclei.labels_numpy()
    expected = nuclei.copy()
    if radius > 1:
        # The next representable radius above one admits exactly the four
        # cardinal neighbors. These isolated nuclei cannot compete for a pixel.
        expected[1:] = np.maximum(expected[1:], nuclei[:-1])
        expected[:-1] = np.maximum(expected[:-1], nuclei[1:])
        expected[:, 1:] = np.maximum(expected[:, 1:], nuclei[:, :-1])
        expected[:, :-1] = np.maximum(expected[:, :-1], nuclei[:, 1:])
        assert np.count_nonzero(expected) > np.count_nonzero(nuclei)
    np.testing.assert_array_equal(result.cells.labels_numpy(), expected)
    height, width = nuclei.shape
    rgb = doc.server.read_region(q.RegionRequest(0, 0, 0, width, height)).numpy()[..., :3]
    for feature in result.features:
        pixels = rgb[expected == feature.label]
        assert feature.cell_pixels == len(pixels)
        np.testing.assert_allclose(feature.cell_rgb, pixels.mean(axis=0), rtol=0, atol=1e-10)
        if radius <= 1:
            assert feature.cell_pixels == feature.nucleus_pixels
            assert feature.cytoplasm_rgb is None and feature.cytoplasm_channels is None
    if q.has_project:
        record = doc.keep_cells(request, result)
        project = q.Project()
        image = project.add_document(doc)
        path = tmp_path/'unit-radius.qpxproj'
        project.save(str(path))
        restored = q.Project.open(str(path)).document(image)
        np.testing.assert_array_equal(restored.cell_record(record).result.cells.labels_numpy(), expected)
        replay = q.CellRequest.replay(restored, record)
        assert restored.verify_cells(record, replay, q.detect_cells(replay))
