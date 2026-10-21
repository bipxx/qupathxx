import csv
import io
import json
from pathlib import Path

import pytest

import qupathxx as q

geo = pytest.mark.skipif(not q.has_interchange, reason="requires GeoJSON")


def document():
    return q.Document(q.Image.demo())


def feature(geometry, **properties):
    return {"type": "Feature", "id": "a", "geometry": geometry, "properties": properties}


def rectangle(x=0, y=0, width=10, height=10):
    return {"type": "Polygon", "coordinates": [[[x, y], [x+width, y], [x+width, y+height], [x, y+height], [x, y]]]}


@geo
def test_deterministic_roundtrip_remaps_hierarchy_and_class_colors_atomically():
    source = document()
    parent = source.add_rectangle(q.Rect(0, 0, 100, 100), 'Parent,"quoted"\nname')
    child = source.add_cell(q.Roi.ellipse(q.Rect(10, 10, 20, 20)), q.Roi.rectangle(q.Rect(15, 15, 3, 3)),
                            parent_id=parent, class_name="Tumor")
    source.set_class("Tumor", 0xff2030)
    source.set_class("Unused", 0x123456)
    with pytest.raises(ValueError, match="allow_losses"):
        q.export_geojson(source)
    exported = q.export_geojson(source, allow_losses=True)
    subset = q.export_geojson(source, [child, parent, child], allow_losses=True).text
    assert subset == q.export_geojson(source, [parent, child], allow_losses=True).text
    assert json.loads(subset)["features"] == json.loads(exported.text)["features"]
    assert json.loads(subset)["qupathxx"]["classes"] == [{"name": "Tumor", "color": 0xff2030}]
    assert exported.text == q.export_geojson(source, allow_losses=True).text
    assert [(i.code, i.count) for i in exported.issues] == [("ellipse_polygonized", 1)]
    target = document()
    existing = target.add_rectangle(q.Rect(200, 200, 1, 1))
    revision = target.revision
    imported = q.import_geojson(target, exported.text)
    assert imported.source_ids == [str(parent), str(child)] and imported.imported_ids == [existing+1, existing+2]
    new_parent, new_child = imported.imported_ids
    assert target.annotation(new_child).parent_id == new_parent
    assert target.annotation(new_child).geometry.kind == q.RoiKind.ELLIPSE
    assert target.annotation(new_child).nucleus.area == 9
    assert target.object_class("Tumor").color == 0xff2030 and target.object_class("Unused").color == 0x123456
    assert target.revision == revision+1 and target.undo_label == "Import annotations"
    assert target.undo() and target.object_count == 1 and not target.classes
    assert target.redo() and target.annotation(new_child).parent_id == new_parent


@geo
def test_untagged_pixel_assumption_dimensions_and_geographic_crs():
    target = document()
    data = feature({"type": "Point", "coordinates": [5, 6]})
    with pytest.raises(ValueError, match="assume_pixels"):
        q.import_geojson(target, json.dumps(data))
    result = q.import_geojson(target, json.dumps(data), assume_pixels=True)
    assert target.annotation(result.imported_ids[0]).geometry.contains(q.Point(5, 6))
    data["crs"] = {"type": "name", "properties": {"name": "EPSG:4326"}}
    with pytest.raises(ValueError, match="CRS"):
        q.import_geojson(target, json.dumps(data), assume_pixels=True)
    exported = json.loads(q.export_geojson(target).text)
    exported["qupathxx"]["imageWidth"] = 100
    with pytest.raises(ValueError, match="dimensions"):
        q.import_geojson(target, json.dumps(exported))


@geo
@pytest.mark.skipif(not q.has_geos, reason="requires polygons")
def test_qupath_source_shaped_fixture_cells_holes_and_nested_hierarchy():
    fixture = Path(__file__).parent / "fixtures/qupath-geojson-protocol.json"
    target = document()
    report = q.load_geojson(target, str(fixture), assume_pixels=True)
    parent, cell = report.imported_ids
    assert report.source_ids == ["35edb251-5f60-47e0-a544-339de32523bc", "ba21e2ba-176f-40b8-b33b-4e5fe48ab54b"]
    assert target.annotation(cell).parent_id == parent
    assert target.annotation(parent).geometry.area == 9600
    assert target.annotation(cell).kind == q.ObjectKind.CELL
    assert target.annotation(cell).nucleus.area == 25
    assert target.annotation(cell).geometry.kind == q.RoiKind.ELLIPSE
    assert target.object_class("Tumor").color == 0xff0000


@geo
def test_loss_reporting_subset_parents_and_unknown_properties():
    doc = document()
    parent = doc.add_rectangle(q.Rect(0, 0, 100, 100))
    child = doc.add_detection(q.Roi.rectangle(q.Rect(10, 10, 10, 10)), parent_id=parent)
    with pytest.raises(ValueError, match="Parent omitted"):
        q.export_geojson(doc, [child])
    result = q.export_geojson(doc, [child], allow_losses=True)
    assert result.issues[0].code == "parent_not_exported"
    assert json.loads(result.text)["features"][0]["properties"]["qupathxx"]["parentId"] == "0"
    data = feature({"type": "Point", "coordinates": [1, 2]}, measurements=[{"name": "Area", "value": 3}], isLocked=True)
    target = document()
    with pytest.raises(ValueError, match="Unsupported properties"):
        q.import_geojson(target, json.dumps(data), assume_pixels=True)
    assert target.object_count == 0
    imported = q.import_geojson(target, json.dumps(data), assume_pixels=True, allow_losses=True)
    assert [(i.code, i.count) for i in imported.issues] == [("properties_dropped", 2)]


@geo
@pytest.mark.parametrize("broken", [
    '{"type":"Feature","type":"Feature"}',
    '{"type":"Feature","geometry":{"type":"Point","coordinates":[1e400,0]}}',
    '{"type":"Feature","geometry":{"type":"Point","coordinates":[1,2,3]}}',
    '{"type":"Feature","geometry":{"type":"Point","coordinates":[1,2],"plane":{"z":1}}}',
    '{"type":"Feature","geometry":{"type":"Point","coordinates":[1,2],"plane":{"z":4294967296}}}',
    '{"type":"Feature","geometry":{"type":"GeometryCollection","coordinates":[]}}',
    '{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1]]]}}',
])
def test_invalid_geometry_and_json_preserve_existing_document(broken):
    doc = document()
    obj = doc.add_rectangle(q.Rect(10, 10, 10, 10))
    doc.rename_annotation(obj, "Undo")
    assert doc.undo()
    revision, history, redo = doc.revision, doc.history_bytes, doc.redo_label
    with pytest.raises(ValueError):
        q.import_geojson(doc, broken, assume_pixels=True)
    assert (doc.revision, doc.history_bytes, doc.redo_label) == (revision, history, redo)
    assert doc.object_count == 1 and not doc.classes


@geo
def test_conflicting_classes_duplicate_ids_cycles_and_budget_are_atomic():
    target = document()
    target.set_class("Tumor", 0x123456)
    data = feature({"type": "Point", "coordinates": [1, 1]}, classification={"name": "Tumor", "color": [255, 0, 0]})
    revision = target.revision
    with pytest.raises(ValueError, match="conflicts"):
        q.import_geojson(target, json.dumps(data), assume_pixels=True)
    assert target.revision == revision and target.object_count == 0
    data["properties"] = {"qupathxx": {"parentId": "a"}}
    with pytest.raises(ValueError, match="cycle"):
        q.import_geojson(target, json.dumps(data), assume_pixels=True)
    data["properties"] = {}
    duplicate = {"type": "FeatureCollection", "features": [data, data]}
    with pytest.raises(ValueError, match="duplicate"):
        q.import_geojson(target, json.dumps(duplicate), assume_pixels=True)
    target.set_history_limits(128, 1)
    with pytest.raises(ValueError, match="budget"):
        q.import_geojson(target, json.dumps(data), assume_pixels=True)
    assert target.revision == revision and target.object_count == 0
    target.set_history_limits(128, 64*1024*1024)
    assert q.import_geojson(target, json.dumps(data), assume_pixels=True).imported_ids == [1]


def test_csv_units_unknown_values_quotes_and_independent_rgb_means():
    doc = q.Document(q.Image(10, 10, [v for y in range(10) for x in range(10) for v in [x+2*y]*3]))
    obj = doc.add_cell(q.Roi.rectangle(q.Rect(0, 0, 10, 10)), q.Roi.rectangle(q.Rect(2, 2, 2, 2)), name='Name,"quoted"\nnext')
    doc.add_roi(q.Roi.polyline([q.Point(0, 0), q.Point(3, 4)]))
    rows = list(csv.DictReader(io.StringIO(q.measurements_csv(doc, include_intensity=True))))
    assert rows[0]["name"] == 'Name,"quoted"\nnext'
    assert rows[0]["area_um2"] == "" and rows[0]["nucleus_area_um2"] == ""
    assert float(rows[0]["mean_rgb"]) == 13.5 and float(rows[0]["nucleus_mean_rgb"]) == 7.5
    assert rows[1]["length_px"] == "5" and rows[1]["mean_rgb"] == ""
    doc.set_calibration(.5, .25)
    rows = list(csv.DictReader(io.StringIO(q.measurements_csv(doc, [obj, obj]))))
    assert len(rows) == 1 and rows[0]["area_um2"] == "12.5" and rows[0]["nucleus_area_um2"] == "0.5"
    assert rows[0]["mean_rgb"] == ""
    assert q.measurements_csv(doc) == q.measurements_csv(doc)


@geo
def test_atomic_files_unicode_replacement_and_failed_export(tmp_path):
    doc = document()
    doc.add_rectangle(q.Rect(0, 0, 10, 10))
    path = tmp_path / "\u6807\u6ce8.geojson"
    first = q.save_geojson(doc, str(path)).text
    assert path.read_text(encoding="utf-8") == first
    doc.add_roi(q.Roi.ellipse(q.Rect(20, 20, 10, 10)))
    with pytest.raises(ValueError, match="allow_losses"):
        q.save_geojson(doc, str(path))
    assert path.read_text(encoding="utf-8") == first
    q.save_geojson(doc, str(path), allow_losses=True)
    assert json.loads(path.read_text(encoding="utf-8"))["type"] == "FeatureCollection"
    assert not list(tmp_path.glob("*.qpx-tmp-*"))
    destination = tmp_path / "directory"
    destination.mkdir()
    with pytest.raises(RuntimeError, match="replacement"):
        q.save_geojson(doc, str(destination), allow_losses=True)
    assert destination.is_dir() and not list(tmp_path.glob("*.qpx-tmp-*"))
    csv_path = tmp_path / "measurements.csv"
    q.save_measurements_csv(doc, str(csv_path))
    assert csv_path.read_bytes() == q.measurements_csv(doc).encode()


@geo
def test_unknown_geometry_and_collection_members_require_loss_permission():
    data = {"type": "FeatureCollection", "custom": 1, "features": [feature({"type": "Point", "coordinates": [1, 2], "custom": 2})]}
    doc = document()
    with pytest.raises(ValueError):
        q.import_geojson(doc, json.dumps(data), assume_pixels=True)
    assert doc.object_count == 0
    result = q.import_geojson(doc, json.dumps(data), assume_pixels=True, allow_losses=True)
    assert {i.code for i in result.issues} == {"collection_members_dropped", "geometry_members_dropped"}
    own = json.loads(q.export_geojson(doc).text)
    own["qupathxx"]["classes"] = {}
    with pytest.raises(ValueError, match="array"):
        q.import_geojson(doc, json.dumps(own))


@geo
def test_invalid_class_colors_do_not_wrap_or_drop_alpha():
    doc = document()
    for color in [18446744073709551615, -2147483648, [255, 0, 0, 128]]:
        data = feature({"type": "Point", "coordinates": [1, 2]}, classification={"name": "Tumor", "color": color})
        with pytest.raises(ValueError, match="color"):
            q.import_geojson(doc, json.dumps(data), assume_pixels=True, allow_losses=True)
    assert doc.object_count == 0 and not doc.classes


@geo
def test_points_lines_and_analytic_hints_are_validated():
    doc = document()
    doc.add_roi(q.Roi.points([q.Point(2, 3), q.Point(4, 5)]))
    doc.add_roi(q.Roi.polyline([q.Point(0, 0), q.Point(3, 4)]))
    doc.add_rectangle(q.Rect(10, 10, 5, 6))
    exported = q.export_geojson(doc).text
    target = document()
    ids = q.import_geojson(target, exported).imported_ids
    assert [target.annotation(i).geometry.kind for i in ids] == [q.RoiKind.POINTS, q.RoiKind.POLYLINE, q.RoiKind.RECTANGLE]
    forged = json.loads(exported)
    forged["features"][-1]["geometry"]["qupathxx"]["bounds"][2] = 7
    revision = target.revision
    with pytest.raises(ValueError, match="disagrees"):
        q.import_geojson(target, json.dumps(forged))
    assert target.revision == revision and target.object_count == 3


@geo
@pytest.mark.skipif(not q.has_geos, reason="requires polygons")
def test_polygon_holes_and_multipart_roundtrip_and_winding():
    def ring(x, y, w, h):
        return [q.Point(x, y), q.Point(x+w, y), q.Point(x+w, y+h), q.Point(x, y+h)]
    shape = q.Roi.multipolygon([q.PolygonPart(ring(0, 0, 50, 50), [ring(10, 10, 10, 10)]),
                               q.PolygonPart(ring(60, 60, 20, 20), [])])
    source = document()
    source.add_roi(shape)
    exported = q.export_geojson(source).text
    coordinates = json.loads(exported)["features"][0]["geometry"]["coordinates"]
    def signed_area(r):
        return sum(a[0]*b[1]-b[0]*a[1] for a, b in zip(r, r[1:])) / 2
    assert all(signed_area(part[0]) > 0 and all(signed_area(hole) < 0 for hole in part[1:]) for part in coordinates)
    target = document()
    obj = q.import_geojson(target, exported).imported_ids[0]
    restored = target.annotation(obj).geometry
    # Export normalizes winding; structural path equality is intentionally stricter.
    assert restored.covers(shape) and shape.covers(restored)
    assert restored.area == 2800
    assert restored.contains(q.Point(5, 5)) and not restored.contains(q.Point(15, 15))
    assert restored.contains(q.Point(65, 65)) and not restored.contains(q.Point(55, 55))
