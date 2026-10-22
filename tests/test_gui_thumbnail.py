"""Real GL publication, stale source rejection and separate preview budget."""
import os
import numpy as np
import pytest
import qupathxx as q
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native GUI')


def config():
    result = q.GuiOptions()
    result.settle_tiles = True
    return result


@pytest.mark.parametrize('blue', [False, True])
def test_overview_is_complete_bounded_and_captured(tmp_path, blue):
    from PIL import Image
    doc = q.Document(q.Image(1024, 768, [30, 120, 210]*1024*768))
    capture = tmp_path/'overview.ppm'
    options = config()
    if blue:
        settings = q.DisplaySettings()
        settings.mode = q.DisplayMode.BLUE
        options.display = settings
    report = q.show(doc, frame_limit=20, capture_path=str(capture), options=options)
    assert report.tiles_settled and report.overview_state == 'succeeded' and not report.overview_error
    result = report.overview_result
    assert result.plan.width == 200 and result.plan.height == 150
    np.testing.assert_array_equal(result.pixels.numpy(), np.broadcast_to([30, 120, 210, 255], (150, 200, 4)))
    assert report.overview_gpu_bytes == report.overview_peak_gpu_bytes == 200*150*4
    with Image.open(capture) as image:
        pixels = np.asarray(image)
        # Inspector is left of the main canvas. Check that it contains the
        # actual image color, including both sides of the reduced preview.
        mask = np.all(pixels[:, :300] == ([210]*3 if blue else [30, 120, 210]), axis=2)
        assert mask.sum() > 20000
        _, xs = np.nonzero(mask)
        assert xs.max()-xs.min() >= 195


def test_source_switch_publishes_only_current_complete_overview():
    project = q.Project()
    project.add_document(q.Document(q.Image(512, 512, [200, 10, 20]*512*512)))
    second = project.add_document(q.Document(q.Image(768, 512, [10, 200, 30]*768*512)))
    project.active_image = project.images[0].id
    options = config()
    options.inputs = [replay(4, q.GuiInputKind.NEXT_IMAGE_KEY), replay(7, q.GuiInputKind.PREVIOUS_IMAGE_KEY), replay(10, q.GuiInputKind.NEXT_IMAGE_KEY)]
    report = q.show(project, frame_limit=20, options=options)
    assert report.active_image == second and report.image_switches == 3
    assert report.overview_state == 'succeeded' and report.overview_result.plan.source_width == 768
    np.testing.assert_array_equal(report.overview_result.pixels.numpy(), np.broadcast_to([10, 200, 30, 255], (150, 225, 4)))
    assert report.overview_gpu_bytes <= 250*150*4 and report.overview_peak_gpu_bytes <= 250*150*4


def test_empty_workspace_settles_without_thumbnail():
    report = q.show(q.Project(), frame_limit=4, options=config())
    assert report.tiles_settled and report.overview_state == 'idle'
    assert report.overview_result is None and report.overview_gpu_bytes == 0


def test_inadequate_pyramid_reports_unavailable_without_partial_preview():
    # Single natural level exceeds the overview source budget. Main viewport
    # tiles are still usable; preview failure is explicit and settles normally.
    doc = q.Document(q.Image(2049, 2048, [50, 100, 150]*(2049*2048)))
    report = q.show(doc, frame_limit=12, options=config())
    assert report.tiles_settled and report.overview_state == 'failed'
    assert 'source pixel limit' in report.overview_error
    assert report.overview_result is None and report.overview_gpu_bytes == 0
