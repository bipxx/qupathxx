"""One actual active cancellation/retry interaction for the new spatial mode."""
import os

import pytest
import qupathxx as q
from test_gui_pixel_classifier import click, ready

pytestmark = pytest.mark.skipif(
    not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='opt-in actual neighborhood desktop')


def options(sources, targets, run=False):
    value = q.GuiOptions()
    value.width, value.height = 1280, 1000
    value.autosave_interval_seconds = 0
    value.replay_timeout_seconds = 20
    spatial = q.GuiSpatialOptions()
    spatial.open, spatial.run = True, run
    spatial.operation, spatial.radius = q.SpatialOperation.RADIUS_COUNT, 1
    spatial.sources, spatial.targets = sources, targets
    analysis = spatial.analysis
    analysis.max_distance_tests = 1 << 40
    spatial.analysis = analysis
    value.spatial = spatial
    return value


def test_active_cancel_then_retry_keeps_only_complete_counts():
    doc = q.Document(q.Image(64, 64, [80] * 64 * 64 * 3))
    point = q.Roi.points([q.Point(32, 32)])
    ids = doc.execute([q.ObjectEdit.create_geometry(point) for _ in range(20000)])
    project = q.Project()
    project.add_document(doc, 'Coincident neighborhood cancellation')
    revision = doc.revision
    value = options(ids, ids)
    value.inputs = [*click(4, q.GuiInputTarget.SPATIAL_RUN),
                    *click(8, q.GuiInputTarget.SPATIAL_CANCEL)]
    report = q.show(project, frame_limit=18, options=value)
    state = report.spatial
    assert report.replay_complete and state.state == 'cancelled' and not state.busy
    assert state.neighborhood_result is None and state.result is None
    assert state.analysis.distance_tests > 0
    assert doc.revision == revision and doc.neighborhood_record_count == 0
    retry = options([ids[0]], [ids[1]], run=True)
    retry.inputs = [ready(4, q.GuiInputWait.SPATIAL_READY),
                    *click(8, q.GuiInputTarget.SPATIAL_KEEP),
                    ready(12, q.GuiInputWait.SPATIAL_KEPT)]
    report = q.show(project, frame_limit=22, options=retry)
    assert report.replay_complete and report.spatial.neighborhood_result.row(0).count == 1
    assert report.spatial.result is None and doc.neighborhood_record_count == 1
