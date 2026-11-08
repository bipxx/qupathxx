"""Additive nested export uses the same native geometry, loss and file path."""
import json
import hashlib
from pathlib import Path

import pytest
import qupathxx as q

from test_qupath_hierarchy_reference import FIXTURE, assert_objects, document

pytestmark = pytest.mark.skipif(not (q.has_interchange and q.has_geos), reason='GeoJSON/GEOS nested export')


def scene():
    doc = document()
    q.load_geojson(doc, str(FIXTURE / 'nested.geojson'), assume_pixels=True)
    return doc


def flatten(data):
    result = []
    def visit(feature, parent):
        result.append((feature, parent))
        for child in feature['properties']['childObjects']:
            visit(child, feature['id'])
    for root in data['features']:
        visit(root, None)
    return result


def test_nested_export_preserves_forest_and_does_not_change_flat_default():
    doc = scene()
    flat = q.export_geojson(doc)
    assert flat.text == (FIXTURE / 'native-export.geojson').read_text(encoding='utf-8')
    assert flat.text == q.export_geojson(doc, nested_hierarchy=False).text
    nested = q.export_geojson(doc, nested_hierarchy=True)
    assert nested.features == 6 and not nested.issues
    data = json.loads(nested.text)
    assert len(data['features']) == 2
    nodes = flatten(data)
    assert [(obj['id'], parent) for obj, parent in nodes] == [('1', None), ('2', '1'), ('3', '2'), ('4', '1'), ('5', None), ('6', '5')]
    assert all('qupathxx' not in obj['properties'] for obj, _ in nodes)
    target = document()
    imported = q.import_geojson(target, nested.text)
    assert not imported.issues
    assert_objects(target, imported.imported_ids)
    assert nested.text == q.export_geojson(doc, [6, 3, 1, 5, 2, 4, 1], nested_hierarchy=True).text


def test_rebuilt_export_matches_wire_actually_imported_by_pinned_qupath():
    root = Path(__file__).resolve().parents[1]
    folder = root / 'docs/benchmarks/annotation-migration-recovery-local/python-roundtrip'
    report = json.loads((folder / 'report.json').read_text(encoding='utf-8'))
    assert report['state'] == 'passed' and report['comparison']['parent_links'] == 4
    for name in ['native-nested.geojson', 'qupath/reference.json', 'qupath.log']:
        data = (folder / name).read_bytes()
        assert len(data) == report['files'][name]['bytes']
        assert hashlib.sha256(data).hexdigest() == report['files'][name]['sha256']
    assert q.export_geojson(scene(), nested_hierarchy=True).text.encode('utf-8') == (folder / 'native-nested.geojson').read_bytes()


def test_subset_parent_loss_is_explicit_and_selected_children_become_roots():
    doc = scene()
    with pytest.raises(ValueError, match='Parent omitted'):
        q.export_geojson(doc, [2, 3], nested_hierarchy=True)
    report = q.export_geojson(doc, [2, 3], allow_losses=True, nested_hierarchy=True)
    assert report.features == 2
    assert [(i.code, i.count) for i in report.issues] == [('parent_not_exported', 1)]
    nodes = flatten(json.loads(report.text))
    assert [(obj['id'], parent) for obj, parent in nodes] == [('2', None), ('3', '2')]
    target = document()
    imported = q.import_geojson(target, report.text)
    assert target.annotation(imported.imported_ids[1]).parent_id == imported.imported_ids[0]


def test_omitted_intermediate_parent_never_reparents_to_selected_ancestor():
    result = q.export_geojson(scene(), [1, 3], allow_losses=True, nested_hierarchy=True)
    assert [(i.code, i.count) for i in result.issues] == [('parent_not_exported', 1)]
    assert [(obj['id'], parent) for obj, parent in flatten(json.loads(result.text))] == [('1', None), ('3', None)]


def test_parent_created_after_child_exports_once_and_remaps_in_tree_order():
    doc = document()
    child = doc.add_rectangle(q.Rect(10, 10, 5, 5), 'child')
    parent = doc.add_rectangle(q.Rect(0, 0, 100, 100), 'parent')
    doc.reparent_annotation(child, parent)
    result = q.export_geojson(doc, nested_hierarchy=True)
    assert [(obj['id'], owner) for obj, owner in flatten(json.loads(result.text))] == [(str(parent), None), (str(child), str(parent))]
    target = document()
    imported = q.import_geojson(target, result.text)
    assert imported.source_ids == [str(parent), str(child)]
    assert target.annotation(imported.imported_ids[1]).parent_id == imported.imported_ids[0]


def chain(levels):
    doc = document()
    previous = 0
    for _ in range(levels):
        previous = doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 100, 100)), parent_id=previous)
    return doc


def test_depth_boundary_is_readable_and_overflow_keeps_existing_file(tmp_path):
    doc = chain(32)
    output = tmp_path / 'nested.geojson'
    q.save_geojson(doc, str(output), nested_hierarchy=True)
    target = document()
    assert q.load_geojson(target, str(output)).features == 32
    assert target.annotation(32).parent_id == 31
    original = output.read_bytes()
    deep = chain(33)
    with pytest.raises((ValueError, RuntimeError), match='32 object levels'):
        q.save_geojson(deep, str(output), nested_hierarchy=True)
    assert output.read_bytes() == original
    assert q.export_geojson(deep).features == 33
    assert sorted(p.name for p in tmp_path.iterdir()) == ['nested.geojson']


def test_empty_forest_is_valid_and_invalid_ids_do_not_touch_destination(tmp_path):
    doc = document()
    result = q.export_geojson(doc, nested_hierarchy=True)
    assert result.features == 0 and json.loads(result.text)['features'] == []
    output = tmp_path / 'preserved.geojson'
    output.write_bytes(b'keep')
    with pytest.raises((IndexError, ValueError)):
        q.save_geojson(doc, str(output), [99], nested_hierarchy=True)
    assert output.read_bytes() == b'keep'


def test_ellipse_loss_and_analytic_nucleus_contract_is_shared():
    doc = document()
    parent = doc.add_rectangle(q.Rect(0, 0, 100, 100))
    doc.add_cell(q.Roi.ellipse(q.Rect(10, 10, 20, 20)), q.Roi.ellipse(q.Rect(15, 15, 5, 5)), parent_id=parent)
    with pytest.raises(ValueError, match='allow_losses'):
        q.export_geojson(doc, nested_hierarchy=True)
    result = q.export_geojson(doc, allow_losses=True, nested_hierarchy=True)
    assert [(i.code, i.count) for i in result.issues] == [('ellipse_polygonized', 2)]
    target = document()
    imported = q.import_geojson(target, result.text)
    cell = target.annotation(imported.imported_ids[1])
    assert cell.parent_id == imported.imported_ids[0]
    assert cell.geometry.kind == cell.nucleus.kind == q.RoiKind.ELLIPSE
