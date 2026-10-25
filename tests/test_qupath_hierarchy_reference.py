"""Actually executed QuPath hierarchy, migration and explicit loss contract."""
import hashlib
import json
from pathlib import Path
import uuid

import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/qupath-hierarchy-0.7.0'
REFERENCE = json.loads((FIXTURE / 'reference.json').read_text(encoding='utf-8'))
NATIVE = pytest.mark.skipif(not (q.has_geos and q.has_interchange), reason='GEOS/GeoJSON hierarchy migration')


def read(name):
    return json.loads((FIXTURE / name).read_text(encoding='utf-8'))


def document():
    return q.Document(q.Image(256, 192, [255] * 256 * 192 * 3))


def bounds(roi):
    b = roi.bounds
    return [b.x, b.y, b.width, b.height]


def assert_objects(doc, ids):
    expected = REFERENCE['nested_decoded']
    mapping = {o['id']: i for o, i in zip(expected, ids, strict=True)}
    for ref, i in zip(expected, ids, strict=True):
        obj = doc.annotation(i)
        assert obj.parent_id == mapping.get(ref.get('parent'), 0)
        assert obj.name == ref['name']
        assert obj.kind == {'annotation': q.ObjectKind.ANNOTATION, 'detection': q.ObjectKind.DETECTION, 'cell': q.ObjectKind.CELL}[ref['kind']]
        assert obj.class_name == ref.get('classification', '')
        assert bounds(obj.geometry) == pytest.approx(ref['bounds'], abs=1e-6, rel=0)
        assert obj.geometry.area == pytest.approx(ref['area'], abs=1e-6, rel=0)
        if ref.get('nucleus_bounds'):
            assert bounds(obj.nucleus) == pytest.approx(ref['nucleus_bounds'], abs=1e-6, rel=0)
        else:
            assert obj.nucleus is None
        if ref['name'] == 'Points':
            assert obj.geometry.kind == q.RoiKind.POINTS
            assert [(p.x, p.y) for path in obj.geometry.paths for p in path] == [(215, 25), (220, 30)]
    assert doc.object_class('Tumor').color == 0xff2030


def test_captures_generator_contract_and_exporter_are_byte_bound():
    manifest = read('manifest.json')
    assert manifest['schema'] == 'qpx.qupath-hierarchy-fixtures/1'
    assert manifest['upstream'] == read('contract.json')['upstream']
    assert manifest['upstream']['revision'] == REFERENCE['upstream_revision'] == '04ccfa4fb7d43e9b566393e08e83690b72248d44'
    assert REFERENCE['qupath_version'] == '0.7.0'
    assert manifest['upstream']['runtime_files'] == 496
    assert manifest['native_exporter']['git_revision'] == 'f2e8c2ff5b044fd530c959c6d9c3b874d37a31b6'
    assert set(manifest['files']) == {'hierarchy.json', 'nested.geojson', 'flat.geojson', 'losses.geojson', 'metadata-raw.geojson', 'reference.json', 'capture.log', 'native-export.geojson', 'native-roundtrip.json', 'native-roundtrip.log'}
    for name, item in manifest['files'].items():
        data = (FIXTURE / name).read_bytes()
        assert len(data) == item['bytes']
        assert hashlib.sha256(data).hexdigest() == item['sha256']
    for name in ['generator', 'contract']:
        assert hashlib.sha256((ROOT / manifest[name]).read_bytes()).hexdigest() == manifest[name + '_sha256']
    for name in ['capture.log', 'native-roundtrip.log']:
        assert b'QPX_HIERARCHY_REFERENCE_OK 6' in (FIXTURE / name).read_bytes()


def test_actual_nested_capture_preserves_three_levels_and_flat_capture_loses_four_edges():
    contract = read('contract.json')
    created = REFERENCE['created']
    assert created == REFERENCE['nested_decoded']
    assert len(created) == contract['objects'] == 6
    assert sum(bool(o.get('parent')) for o in created) == contract['parent_edges'] == 4
    assert sum(not o.get('parent') for o in created) == contract['root_objects'] == 2
    assert len(read('nested.geojson')['features']) == 2
    assert created[0]['name'] == 'Parent \u03b1'
    assert created[2]['parent'] == created[1]['id']
    assert created[1]['parent'] == created[0]['id']
    depths = {}
    for obj in created:
        depths[obj['id']] = depths.get(obj.get('parent'), 0) + 1
    assert max(depths.values()) == contract['maximum_object_depth'] == 3
    flat = REFERENCE['flat_decoded']
    assert len(flat) == 6 and all(not o.get('parent') for o in flat)
    assert [{k: v for k, v in o.items() if k != 'parent'} for o in flat] == [{k: v for k, v in o.items() if k != 'parent'} for o in created]
    # The wrapper copies actual serialized children, not a hand-authored tree.
    assert read('nested.geojson')['features'] == read('hierarchy.json')['root']['properties']['childObjects']


def test_actual_native_to_qupath_capture_records_identity_remapping_and_hierarchy_loss():
    native = read('native-export.geojson')
    decoded = read('native-roundtrip.json')['native_decoded']
    assert [o['properties']['qupathxx']['parentId'] for o in native['features']] == ['0', '1', '2', '1', '0', '5']
    assert len(decoded) == 6 and all(not o.get('parent') for o in decoded)
    assert len({o['id'] for o in decoded}) == 6
    for old, new in zip(REFERENCE['created'], decoded, strict=True):
        uuid.UUID(new['id'])
        for key in ['name', 'kind', 'bounds', 'area', 'classification', 'class_color', 'nucleus_bounds']:
            assert old.get(key) == new.get(key)


def test_duplicate_metadata_is_retained_in_raw_upstream_evidence():
    pairs = []
    json.loads((FIXTURE / 'metadata-raw.geojson').read_text(encoding='utf-8'), object_pairs_hook=lambda value: pairs.append(value) or dict(value))
    assert any(sum(k == 'metadata' for k, _ in obj) == 2 for obj in pairs)
    properties = read('losses.geojson')['features'][0]['properties']
    assert properties['color'] == [12, 34, 56] and properties['isLocked'] is True
    assert properties['measurements']


@NATIVE
def test_nested_import_remaps_ids_and_is_one_atomic_history_step():
    doc = document()
    retained = doc.add_rectangle(q.Rect(240, 170, 5, 5), 'retained')
    revision = doc.revision
    report = q.load_geojson(doc, str(FIXTURE / 'nested.geojson'), assume_pixels=True)
    assert report.features == 6 and not report.issues
    assert report.source_ids == [o['id'] for o in REFERENCE['created']]
    assert report.imported_ids == list(range(retained + 1, retained + 7))
    assert doc.revision == revision + 1 and doc.undo_label == 'Import annotations'
    assert_objects(doc, report.imported_ids)
    assert doc.undo() and doc.object_count == 1 and not doc.classes
    assert doc.redo()
    assert_objects(doc, report.imported_ids)


@NATIVE
@pytest.mark.skipif(not q.has_project, reason='project-enabled migration')
def test_restarted_project_and_native_geojson_keep_hierarchy_calibration_and_nucleus(tmp_path):
    doc = document()
    doc.set_calibration(.5, .25)
    report = q.load_geojson(doc, str(FIXTURE / 'nested.geojson'), assume_pixels=True)
    project = q.Project()
    image_id = project.add_document(doc, 'Migrated hierarchy')
    path = tmp_path / 'migration.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image_id)
    assert (restored.pixel_width_um, restored.pixel_height_um) == (.5, .25)
    assert_objects(restored, report.imported_ids)
    exported = q.export_geojson(restored)
    assert not exported.issues and exported.features == 6
    target = document()
    result = q.import_geojson(target, exported.text)
    assert not result.issues
    assert_objects(target, result.imported_ids)


@NATIVE
def test_actual_unsupported_properties_require_permission_and_count_all_losses():
    doc = document()
    revision = doc.revision
    with pytest.raises(ValueError, match='Unsupported properties'):
        q.load_geojson(doc, str(FIXTURE / 'losses.geojson'), assume_pixels=True)
    assert doc.revision == revision and doc.object_count == 0 and not doc.classes
    report = q.load_geojson(doc, str(FIXTURE / 'losses.geojson'), assume_pixels=True, allow_losses=True)
    assert [(i.code, i.count) for i in report.issues] == [('properties_dropped', 3)]
    assert doc.annotation(report.imported_ids[0]).name == 'Unsupported properties'
    assert doc.undo() and doc.object_count == 0 and not doc.classes


@NATIVE
@pytest.mark.parametrize('allow_losses', [False, True])
def test_actual_duplicate_metadata_never_bypasses_atomic_validation(allow_losses):
    doc = document()
    doc.add_rectangle(q.Rect(240, 170, 5, 5), 'retained')
    baseline = (doc.revision, doc.history_bytes, doc.undo_label)
    with pytest.raises(ValueError, match='duplicate JSON member'):
        q.load_geojson(doc, str(FIXTURE / 'metadata-raw.geojson'), assume_pixels=True, allow_losses=allow_losses)
    assert (doc.revision, doc.history_bytes, doc.undo_label) == baseline
    assert doc.object_count == 1 and not doc.classes


@NATIVE
@pytest.mark.parametrize('mutation', ['duplicate_id', 'conflicting_parent', 'invalid_child'])
def test_nested_failure_after_staged_parent_preserves_document(mutation):
    data = read('nested.geojson')
    parent = data['features'][0]
    child = parent['properties']['childObjects'][0]
    if mutation == 'duplicate_id':
        child['id'] = parent['id']
    elif mutation == 'conflicting_parent':
        child['properties']['qupathxx'] = {'parentId': parent['id']}
    else:
        child['geometry']['coordinates'] = []
    doc = document()
    doc.add_rectangle(q.Rect(240, 170, 5, 5), 'retained')
    baseline = (doc.revision, doc.history_bytes, doc.undo_label)
    with pytest.raises(ValueError):
        q.import_geojson(doc, json.dumps(data), assume_pixels=True)
    assert (doc.revision, doc.history_bytes, doc.undo_label) == baseline
    assert doc.object_count == 1 and not doc.classes
