"""Actual density cancellation/retry; numerical topology is tested natively."""
import os

import pytest
import qupathxx as q
from test_gui_pixel_classifier import click, ready

pytestmark = pytest.mark.skipif(
    not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='opt-in actual density desktop')


def options(windows, tissue, cells, run=False):
    value = q.GuiOptions()
    value.width, value.height = 1280, 1000
    value.autosave_interval_seconds = 0
    value.replay_timeout_seconds = 20
    spatial = q.GuiSpatialOptions()
    spatial.open, spatial.run = True, run
    spatial.operation = q.SpatialOperation.TISSUE_DENSITY
    spatial.sources, spatial.targets, spatial.tissue = windows, cells, tissue
    value.spatial = spatial
    return value


def test_active_cancel_then_retry_keeps_only_complete_density():
    doc = q.Document(q.Image(64, 64, [80] * 64 * 64 * 3))
    doc.set_calibration(1, 1)
    body = q.Roi.rectangle(q.Rect(31, 31, 2, 2))
    cells = doc.execute([q.ObjectEdit.create_geometry(body, kind=q.ObjectKind.CELL)
                         for _ in range(20000)])
    window = q.Roi.rectangle(q.Rect(1, 1, 60, 60))
    windows = doc.execute([q.ObjectEdit.create_geometry(window) for _ in range(1000)])
    tissue = doc.add_rectangle(q.Rect(0, 0, 64, 64))
    revision = doc.revision
    value = options(windows, [tissue], cells)
    value.inputs = [*click(4, q.GuiInputTarget.SPATIAL_RUN),
                    *click(12, q.GuiInputTarget.SPATIAL_CANCEL)]
    report = q.show(doc, frame_limit=24, options=value)
    state = report.spatial
    assert report.replay_complete and state.state == 'cancelled' and not state.busy
    assert state.density_result is state.result is state.neighborhood_result is None
    assert state.density_analysis.membership_tests > 0
    assert doc.revision == revision and doc.density_record_count == 0
    retry = options([windows[0]], [tissue], cells[:2], run=True)
    retry.inputs = [ready(4, q.GuiInputWait.SPATIAL_READY),
                    *click(8, q.GuiInputTarget.SPATIAL_KEEP),
                    ready(12, q.GuiInputWait.SPATIAL_KEPT)]
    report = q.show(doc, frame_limit=24, options=retry)
    assert report.replay_complete and report.spatial.density_result.row(0).count == 2
    assert report.spatial.density_result.row(0).area_pixels == 3600
    assert report.spatial.result is report.spatial.neighborhood_result is None
    assert doc.density_record_count == 1
