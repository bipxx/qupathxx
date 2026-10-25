"""Real ImGui migration using the pinned upstream hierarchy and loss fixtures."""
import os

import pytest
import qupathxx as q

from test_gui_tiles import replay
from test_qupath_hierarchy_reference import FIXTURE, assert_objects, document

pytestmark = pytest.mark.skipif(
    not (q.has_gui and q.has_project and q.has_geos and q.has_interchange)
    or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='opt-in project/GEOS/GeoJSON desktop migration',
)


def options(path, allow_losses=False):
    result = q.GuiOptions()
    result.width, result.height = 1280, 1100
    result.interchange_path = str(path)
    result.interchange_assume_pixels = True
    result.interchange_allow_losses = allow_losses
    result.inputs = [replay(4, q.GuiInputKind.IMPORT_KEY)]
    return result


def test_actual_upstream_hierarchy_import_history_project_restart_and_export(tmp_path):
    source_bytes = (FIXTURE / 'nested.geojson').read_bytes()
    doc = document()
    doc.set_calibration(.5, .25)
    project = q.Project()
    image_id = project.add_document(doc, 'Hierarchy migration')
    o = options(FIXTURE / 'nested.geojson')
    capture = tmp_path / 'hierarchy-import.ppm'
    report = q.show(project, frame_limit=24, capture_path=str(capture), options=o)
    assert report.ui_overflow_items == 0 and capture.is_file()
    ids = list(range(1, 7))
    assert doc.object_count == 6
    assert_objects(doc, ids)
    doc.rename_annotation(ids[2], 'Edited cell')
    assert doc.undo() and doc.annotation(ids[2]).name == 'Cell'
    assert doc.undo() and doc.object_count == 0
    assert doc.redo()
    assert_objects(doc, ids)
    path = tmp_path / 'migration.qpxproj'
    project.save(str(path))
    restarted = q.Project.open(str(path))
    assert_objects(restarted.document(image_id), ids)
    assert restarted.document(image_id).pixel_width_um == .5
    output = tmp_path / 'native-roundtrip.geojson'
    o = options(output)
    o.inputs = [replay(4, q.GuiInputKind.EXPORT_KEY)]
    q.show(restarted, frame_limit=20, options=o)
    target = document()
    imported = q.load_geojson(target, str(output))
    assert not imported.issues
    assert_objects(target, imported.imported_ids)
    assert (FIXTURE / 'nested.geojson').read_bytes() == source_bytes


def test_upstream_unsupported_properties_require_desktop_loss_opt_in(tmp_path):
    doc = document()
    q.show(doc, frame_limit=20, options=options(FIXTURE / 'losses.geojson'))
    assert doc.object_count == 0 and not doc.classes
    capture = tmp_path / 'hierarchy-losses.ppm'
    report = q.show(doc, frame_limit=24, capture_path=str(capture), options=options(FIXTURE / 'losses.geojson', True))
    assert report.ui_overflow_items == 0 and capture.is_file()
    assert doc.object_count == 1 and doc.annotations[0].name == 'Unsupported properties'
    assert doc.undo() and doc.object_count == 0


def test_upstream_duplicate_metadata_rejects_on_desktop_even_with_loss_opt_in():
    doc = document()
    baseline = doc.revision
    q.show(doc, frame_limit=20, options=options(FIXTURE / 'metadata-raw.geojson', True))
    assert doc.object_count == 0 and doc.revision == baseline and not doc.classes
