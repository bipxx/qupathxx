"""Inclusive physical tissue filters share the frozen native pixel-count recipe."""
import csv
import io
import json
import math
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_tissue_levels import pyramid_levels
from test_viewer_session import GUI, completed, run


def scene(calibration=(.5, .25)):
    rgb = np.full((4, 8, 3), 255, dtype=np.uint8)
    for x, y in [(0, 0), (3, 0), (4, 0), (0, 2), (1, 2), (0, 3), (1, 3)]:
        rgb[y, x] = 0
    doc = q.Document(q.Image.from_numpy(rgb))
    if calibration is not None:
        doc.set_calibration(*calibration)
    obj = doc.add_rectangle(q.Rect(0, 0, 8, 4))
    return doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]])


@pytest.mark.parametrize('minimum,maximum,pixels', [
    (None, None, 7), (0., None, 7), (.25, .25, 2), (.25, .5, 6),
    (math.nextafter(.25, 1.), None, 4), (None, math.nextafter(.25, 0.), 1),
    (None, 0., 0), (.50001, None, 0), (1000., None, 0),
])
def test_inclusive_calibrated_component_filter(minimum, maximum, pixels):
    doc, obj, transform = scene()
    request = q.TissueRequest.with_component_area_limits(doc, obj, transform, minimum, maximum)
    result = q.detect_tissue(request)
    assert result.foreground_pixels == pixels
    assert all(c.pixels in (1, 2, 4) for c in result.components)
    assert result.samples == 32


@pytest.mark.parametrize('minimum,maximum', [(-1., None), (None, -1.),
    (math.nan, None), (None, math.inf), (2., 1.)])
def test_invalid_bounds_and_viewer_admission(tmp_path, minimum, maximum):
    doc, obj, transform = scene()
    with pytest.raises(ValueError):
        q.TissueRequest.with_component_area_limits(doc, obj, transform, minimum, maximum)
    session = q.ViewerSession(str(tmp_path / 'unopened.qpxproj'))
    with pytest.raises(ValueError):
        session.tissue_with_component_area_limits(1, obj, transform, minimum, maximum)
    assert session.stats.tickets == session.stats.queued_bytes == 0


def test_unknown_calibration_and_empty_intersection():
    doc, obj, transform = scene(None)
    assert q.detect_tissue(q.TissueRequest.with_component_area_limits(doc, obj, transform)).foreground_pixels == 7
    with pytest.raises(ValueError, match='both pixel calibration axes'):
        q.TissueRequest.with_component_area_limits(doc, obj, transform, 0.)
    doc.set_calibration(.5, .25)
    p = q.TissueParameters()
    p.min_pixels = 3
    request = q.TissueRequest.with_component_area_limits(doc, obj, transform, maximum_um2=.25, parameters=p)
    assert request.parameters.min_pixels == 33 and request.parameters.max_pixels == 0
    assert q.detect_tissue(request).foreground_pixels == 0
    p.min_pixels = 1
    p.max_pixels = 1
    assert q.detect_tissue(q.TissueRequest.with_component_area_limits(doc, obj, transform, .25, parameters=p)).foreground_pixels == 0


def test_capture_staleness_ownership_and_saved_count_recipe(tmp_path):
    doc, obj, transform = scene()
    request = q.TissueRequest.with_component_area_limits(doc, obj, transform, .25, .25)
    result = q.detect_tissue(request)
    record = doc.keep_tissue(request, result)
    draft = request.parameters
    draft.min_pixels = 99
    assert request.parameters.min_pixels == 2
    doc.set_calibration(1., 1.)
    assert not request.matches(doc, obj)
    with pytest.raises(ValueError):
        doc.keep_tissue(request, result)
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'frozen.qpxproj'
    project.save(str(source))
    restored = q.Project.open(str(source)).document(image)
    replay = q.TissueRequest.replay(restored, record)
    assert (replay.parameters.min_pixels, replay.parameters.max_pixels) == (2, 2)
    assert q.detect_tissue(replay).labels == result.labels
    assert restored.tissue_record(record).result.provenance.effective_pixel_width_um == .5


def test_selected_level_and_anisotropy(pyramid_levels):
    server, _ = pyramid_levels
    doc = q.Document(server)
    doc.set_calibration(.5, .25)
    obj = doc.add_rectangle(q.Rect(0, 0, 300, 240))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    p = q.TissueParameters()
    p.level = 1
    exact = q.TissueRequest.with_component_area_limits(doc, obj, transform, 2.25, 2.25, p)
    assert exact.downsample == 1.5
    assert (exact.parameters.min_pixels, exact.parameters.max_pixels) == (8, 8)
    base = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    filtered = q.detect_tissue(exact)
    keep = {c.label for c in base.components if c.pixels == 8}
    original = np.frombuffer(base.labels, dtype=np.uint32)
    actual = np.frombuffer(filtered.labels, dtype=np.uint32)
    np.testing.assert_array_equal(actual != 0, np.isin(original, list(keep)))


@pytest.mark.parametrize('calibration', [(1e-300, 1e-300), (1e308, 1e-308), (1e308, 1e308)])
def test_extreme_calibration(calibration):
    doc, obj, transform = scene(calibration)
    assert q.detect_tissue(q.TissueRequest.with_component_area_limits(doc, obj, transform, maximum_um2=0.)).foreground_pixels == 0
    if calibration == (1e308, 1e-308):
        assert q.detect_tissue(q.TissueRequest.with_component_area_limits(doc, obj, transform, 1.5, 2.5)).foreground_pixels == 2


def test_batch_recipe_resume_csv_and_native_cli(tmp_path):
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'input.qpxproj'
    project.save(str(source))
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.TISSUE
    options.tissue_min_component_area_um2 = .25
    options.tissue_max_component_area_um2 = .25
    request = q.ProjectBatchRequest(project, transform, options=options)
    assert json.loads(request.parameters_json)['tissue_area_limits'] == {'minimum_um2': .25, 'maximum_um2': .25}
    output = tmp_path / 'sdk'
    first = q.run_project_batch(request, str(output))
    assert first.state == 'succeeded' and first.saved == 1
    again = q.run_project_batch(request, str(output))
    assert again.reused == 1 and again.saved == 0
    result = q.Project.open(first.images[0].path).document(image).latest_tissue(obj).result
    assert result.foreground_pixels == 2 and result.parameters.min_pixels == 2
    rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(first.images[0].path))))
    assert len(rows) == 1 and float(rows[0]['component_area_um2']) == .25
    options.tissue_max_component_area_um2 = .5
    changed = q.ProjectBatchRequest(project, transform, options=options)
    assert changed.parameters_sha256 != request.parameters_sha256
    changed_result = q.run_project_batch(changed, str(output))
    assert changed_result.state == 'succeeded' and changed_result.saved == 1 and changed_result.reused == 0
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'tissue',
               '--tissue-min-area-um2', '.25', '--tissue-max-area-um2', '.25']
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    terminal = json.loads(done.stdout.splitlines()[-1])
    assert terminal['saved'] == 1
    checkpoints = list((tmp_path / 'cli').glob('*.qpxproj'))
    assert len(checkpoints) == 1
    assert q.Project.open(str(checkpoints[0])).document(image).latest_tissue(obj).result.foreground_pixels == 2
    repeated = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert repeated.returncode == 0 and json.loads(repeated.stdout.splitlines()[-1])['reused'] == 1
    for bad in [command + ['--kind', 'cells'], command + ['--tissue-min-area-um2', '1']]:
        assert subprocess.run(bad, capture_output=True, timeout=30).returncode != 0


@GUI
def test_managed_viewer_uses_the_same_physical_filter(tmp_path):
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    source, output = tmp_path / 'source.qpxproj', tmp_path / 'output.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))
    def producer():
        event = completed(session, session.tissue_with_component_area_limits(image, obj, transform,
                          .25, .25, retain_result=True))
        assert event.committed and event.record_id and event.tissue.foreground_pixels == 2
        assert event.tissue.parameters.min_pixels == event.tissue.parameters.max_pixels == 2
        completed(session, session.save(str(output)))
        completed(session, session.close(True))
    run(session, producer)
    assert q.Project.open(str(output)).document(image).latest_tissue(obj).result.foreground_pixels == 2


@GUI
def test_desktop_tissue_uses_physical_options(tmp_path):
    doc, _, _ = scene()
    options = q.GuiOptions()
    options.tissue_min_component_area_um2 = options.tissue_max_component_area_um2 = .25
    inputs = []
    for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (8, q.GuiInputKind.DETECT_TISSUE_KEY)]:
        event = q.GuiInput()
        event.frame, event.kind = frame, kind
        inputs.append(event)
    options.inputs = inputs
    report = q.show(doc, frame_limit=60, capture_path=str(tmp_path / 'physical-tissue.ppm'), options=options)
    assert report.tissue_state == 'ready', report.tissue_error
    assert report.tissue_components == 1 and report.tissue_foreground_pixels == 2
    assert report.tissue_preview_ready and not report.tissue_error
