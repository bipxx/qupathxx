"""Explicit QuPath ellipse decoding retains raw/default and lossy-match policies."""
import json
import os
from pathlib import Path

import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
CASES = json.loads((ROOT/'tests/fixtures/qupath-0.7.0/upstream/edge-cases.json').read_text(encoding='utf8'))['cases']
THIN = CASES[0]
pytestmark = pytest.mark.skipif(not q.has_interchange, reason='GeoJSON required')
GEOS = pytest.mark.skipif(not q.has_geos, reason='QuPath precision reduction requires GEOS')


def document():
    return q.Document(q.Image(256, 256, [32]*256*256*3))


def bounds(roi):
    r = roi.bounds
    return [r.x, r.y, r.width, r.height]


def feature(coordinates):
    return json.dumps({'type': 'Feature', 'geometry': {'type': 'Polygon', 'coordinates': [coordinates], 'isEllipse': True},
                       'properties': {'name': 'Precision'}})


@GEOS
def test_default_preserves_raw_bounds_and_explicit_mode_matches_pinned_decoder(tmp_path):
    raw, matched = document(), document()
    q.import_geojson(raw, THIN['geojson'], assume_pixels=True)
    report = q.import_geojson(matched, THIN['geojson'], assume_pixels=True, allow_losses=True, qupath_ellipse_precision=True)
    assert bounds(raw.annotations[0].geometry) == pytest.approx([170, 220.45, 60, .1])
    assert bounds(matched.annotations[0].geometry) == THIN['decoded_geometry']['bounds']
    assert matched.annotations[0].geometry.area == pytest.approx(THIN['decoded_geometry']['area'], abs=1e-12)
    assert [(i.code, i.count) for i in report.issues] == [('ellipse_hint', 1), ('qupath_ellipse_precision_changed', 1)]
    assert matched.undo() and matched.object_count == 0 and not matched.classes
    assert matched.redo() and bounds(matched.annotations[0].geometry) == THIN['decoded_geometry']['bounds']
    if q.has_project:
        project = q.Project()
        project.add_document(matched)
        path = tmp_path/'precision.qpxproj'
        project.save(str(path))
        assert bounds(q.Project.open(str(path)).document(1).annotations[0].geometry) == THIN['decoded_geometry']['bounds']


@GEOS
def test_coordinate_changes_require_loss_permission_and_leave_atomic_state():
    doc = document()
    obj = doc.add_rectangle(q.Rect(0, 0, 4, 4), 'Existing')
    before = doc.revision
    with pytest.raises(ValueError, match='allow_losses'):
        q.import_geojson(doc, THIN['geojson'], assume_pixels=True, qupath_ellipse_precision=True)
    assert doc.revision == before and doc.object_count == 1 and not doc.classes
    assert doc.annotation(obj).name == 'Existing'
    assert doc.add_rectangle(q.Rect(5, 5, 4, 4)) == obj+1


@GEOS
def test_unchanged_integer_envelope_needs_no_loss_permission():
    doc = document()
    text = feature([[10, 10], [20, 10], [20, 30], [10, 30], [10, 10]])
    report = q.import_geojson(doc, text, assume_pixels=True, qupath_ellipse_precision=True)
    assert bounds(doc.annotations[0].geometry) == [10, 10, 10, 20]
    assert [i.code for i in report.issues] == ['ellipse_hint']


@GEOS
@pytest.mark.parametrize('coordinates', [
    [[10.001, 10.001], [10.002, 10.001], [10.002, 10.002], [10.001, 10.002], [10.001, 10.001]],
    [[1e39, 0], [2e39, 0], [2e39, 10], [1e39, 10], [1e39, 0]],
])
def test_collapsed_or_unrepresentable_matching_input_is_atomic(coordinates):
    doc = document()
    with pytest.raises(ValueError, match='collapses|float coordinate'):
        q.import_geojson(doc, feature(coordinates), assume_pixels=True, allow_losses=True, qupath_ellipse_precision=True)
    assert doc.object_count == doc.revision == 0 and not doc.classes


@GEOS
def test_native_analytic_hints_remain_exact_in_matching_mode():
    source = document()
    obj = source.add_roi(q.Roi.ellipse(q.Rect(170, 220.45, 60, .1)), 'Native')
    text = q.export_geojson(source, allow_losses=True).text
    target = document()
    report = q.import_geojson(target, text, qupath_ellipse_precision=True)
    assert bounds(target.annotations[0].geometry) == bounds(source.annotation(obj).geometry)
    assert not report.issues


@GEOS
def test_file_import_uses_same_precision_contract(tmp_path):
    path = tmp_path/'thin.geojson'
    path.write_text(THIN['geojson'], encoding='utf8')
    doc = document()
    report = q.load_geojson(doc, str(path), assume_pixels=True, allow_losses=True, qupath_ellipse_precision=True)
    assert report.imported_ids == [1] and bounds(doc.annotation(1).geometry) == THIN['decoded_geometry']['bounds']


@pytest.mark.skipif(q.has_geos, reason='GEOS-disabled configuration')
def test_precision_matching_rejects_without_geometry_backend():
    doc = document()
    with pytest.raises(RuntimeError, match='requires GEOS'):
        q.import_geojson(doc, THIN['geojson'], assume_pixels=True, allow_losses=True, qupath_ellipse_precision=True)
    assert doc.object_count == 0
    q.import_geojson(doc, THIN['geojson'], assume_pixels=True)
    assert bounds(doc.annotations[0].geometry) == pytest.approx([170, 220.45, 60, .1])


@GEOS
@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in ImGui import replay')
@pytest.mark.parametrize('losses', [False, True])
def test_gui_import_uses_explicit_matching_and_loss_choices(tmp_path, losses):
    from test_gui_tiles import replay
    path = tmp_path/'thin.geojson'
    path.write_text(THIN['geojson'], encoding='utf8')
    doc = document()
    options = q.GuiOptions()
    options.interchange_path = str(path)
    options.interchange_assume_pixels = options.qupath_ellipse_precision = True
    options.interchange_allow_losses = losses
    options.inputs = [replay(4, q.GuiInputKind.IMPORT_KEY)]
    report = q.show(doc, frame_limit=20, options=options)
    assert doc.object_count == (1 if losses else 0)
    assert report.selected_ids == ([1] if losses else [])
    if losses:
        assert bounds(doc.annotation(1).geometry) == THIN['decoded_geometry']['bounds']
