"""Additive nested export uses the same native geometry, loss and file path."""
import csv
import io
import json
import hashlib
import os
from pathlib import Path
import subprocess

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


def expected_with_explicit_class_color(legacy):
    envelope, delimiter, features = legacy.partition(',"features":')
    assert delimiter
    prefix, member, raw_metadata = envelope.partition(',"qupathxx":')
    assert member
    metadata = json.loads(raw_metadata)
    assert metadata['version'] == 1
    assert metadata['classes'] == [{'name': 'Tumor', 'color': 0xff2030}]
    metadata['version'] = 2
    metadata['classes'][0].update(parent='', argb=0xffff2030)
    # Change just the versioned metadata; keep archived feature bytes exact.
    return prefix + member + json.dumps(metadata, separators=(',', ':'), sort_keys=True) + delimiter + features


def test_nested_export_preserves_forest_and_does_not_change_flat_default():
    doc = scene()
    flat = q.export_geojson(doc)
    legacy = (FIXTURE / 'native-export.geojson').read_text(encoding='utf-8')
    assert flat.text == expected_with_explicit_class_color(legacy)
    legacy_target = document()
    legacy_import = q.import_geojson(legacy_target, legacy)
    assert_objects(legacy_target, legacy_import.imported_ids)
    assert q.export_geojson(legacy_target).text == legacy
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


@pytest.fixture
def project_cli(tmp_path):
    binary = Path(os.environ.get('QUPATHXX_TEST_FORMAT_IMPORT') or Path(q.__file__).with_name(
        'qupathxx_format_import.exe' if os.name == 'nt' else 'qupathxx_format_import'))
    if not binary.is_file():
        pytest.skip('native format CLI is unavailable')
    image = tmp_path / 'source.ppm'
    image.write_bytes(b'P6\n256 192\n255\n' + bytes([70, 110, 150]) * (256 * 192))
    project = tmp_path / 'base.qpx'

    def run(*args, error=None):
        result = subprocess.run([binary, *map(str, args)], cwd=tmp_path, capture_output=True,
                                encoding='utf-8', timeout=30)
        if error is not None:
            assert result.returncode == 2 and error in json.loads(result.stderr)['error'], result
            return None
        assert result.returncode == 0, result
        return json.loads(result.stdout)

    assert run('project-create', image, project)['image_id'] == 1
    return run, project, image


def test_project_cli_lists_optionless_reader_in_mixed_project(project_cli, tmp_path):
    configured = os.environ.get('QUPATHXX_TEST_PNM_MANIFEST')
    if not configured:
        pytest.skip('PNM reader manifest is unavailable')
    manifest = Path(configured).resolve()
    run, project, source = project_cli
    mixed = tmp_path / 'mixed-readers.qpx'
    flags = ('--reader-plugins', manifest)
    created = run(*flags, 'project-add-image', project, source, mixed,
                  '--reader', 'org.qupathxx.pnm', '--reader-options', '{}')
    before = mixed.read_bytes()
    [builtin, available] = run(*flags, 'project-images', mixed)['images']
    [builtin_offline, offline] = run('project-images', mixed)['images']
    assert builtin == builtin_offline and 'reader_plugin' not in builtin
    assert available['reader'] == 'org.qupathxx.pnm' and available['available']
    assert available['image_id'] == created['image_id']
    assert offline == {**available, 'available': False}
    assert available['reader_plugin']['options'] == {}
    assert available['reader_plugin']['binding'].startswith('org.qupathxx.pnm:plugin/1:')
    assert mixed.read_bytes() == before


def test_project_cli_nested_geojson_round_trip(project_cli, tmp_path):
    run, project, image = project_cli
    source = FIXTURE / 'nested.geojson'
    original = {path: path.read_bytes() for path in (source, project, image)}
    saved = tmp_path / 'with-hierarchy.qpx'
    run('geojson-import', project, 1, source, saved, error='assume_pixels')
    assert not saved.exists()
    imported = run('geojson-import', project, 1, source, saved, '--assume-pixels')
    assert imported['features'] == 6 and imported['issues'] == []
    assert [row['object_id'] for row in imported['objects']] == list(range(1, 7))
    [entry] = run('project-images', saved)['images']
    assert (entry['image_id'], entry['objects'], entry['available']) == (1, 6, True)

    flat, nested = tmp_path / 'flat.geojson', tmp_path / 'nested.geojson'
    assert run('geojson-export', saved, 1, flat)['features'] == 6
    exported = json.loads(flat.read_bytes())
    reference = json.loads((FIXTURE / 'native-export.geojson').read_bytes())
    assert exported['features'] == reference['features']
    assert exported['qupathxx']['classes'] == [dict(reference['qupathxx']['classes'][0], argb=0xffff2030, parent='')]
    assert run('geojson-export', saved, 1, nested, '--nested-hierarchy')['issues'] == []
    forest = [(feature['id'], parent) for feature, parent in flatten(json.loads(nested.read_bytes()))]
    assert forest == [('1', None), ('2', '1'), ('3', '2'), ('4', '1'), ('5', None), ('6', '5')]

    restored = tmp_path / 'restored.qpx'
    assert run('geojson-import', project, 1, nested, restored)['features'] == 6
    output = tmp_path / 'restored.geojson'
    run('geojson-export', restored, 1, output, '--nested-hierarchy')
    assert output.read_bytes() == nested.read_bytes()
    assert {path: path.read_bytes() for path in original} == original


def test_project_cli_geojson_refusals_and_reviewed_loss(project_cli, tmp_path):
    run, project, image = project_cli
    source = tmp_path / 'lossy.geojson'
    lossy = json.loads((FIXTURE / 'losses.geojson').read_bytes())
    lossy['unknown_extension'] = 'requires review'
    source.write_text(json.dumps(lossy), encoding='utf-8')
    output = tmp_path / 'reviewed.qpx'
    run('geojson-import', project, 1, source, output, '--assume-pixels', error='allow_losses')
    assert not output.exists()
    report = run('geojson-import', project, 1, source, output, '--assume-pixels', '--allow-losses')
    assert report['features'] == 1 and any(issue['code'] == 'collection_members_dropped' for issue in report['issues'])

    linked = tmp_path / 'source-alias.geojson'
    os.link(image, linked)
    image_before = image.read_bytes()
    run('geojson-export', output, 1, linked, error='aliases its input')
    assert linked.read_bytes() == image_before and image.read_bytes() == image_before
    run('geojson-export', output, 1, output, error='must not replace its input')

    wire = tmp_path / 'existing.geojson'
    wire.write_bytes(b'user-owned')
    run('geojson-export', output, 1, wire, error='already exists')
    assert wire.read_bytes() == b'user-owned'
    wrong = tmp_path / 'wrong.geojson'
    run('geojson-export', output, 1, wrong, '--nested-hierarchy', '--nested-hierarchy', error='Duplicate')
    assert not wrong.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))


def test_project_cli_exports_saved_object_measurements(project_cli, tmp_path):
    import numpy as np
    import tifffile

    run, project, other_image = project_cli
    image = tmp_path / 'planes.ome.tif'
    tifffile.imwrite(image, np.zeros((2, 2, 2, 32, 40), dtype=np.uint16),
                     ome=True, photometric='minisblack',
                     metadata={'axes': 'TZCYX', 'PhysicalSizeX': .25, 'PhysicalSizeY': .5})
    with_image = tmp_path / 'two-images.qpx'
    image_id = run('project-add-image', project, image, with_image, '--reader', 'tiff')['image_id']

    collection = json.loads((FIXTURE / 'losses.geojson').read_bytes())
    annotation = collection['features'][0]
    annotation['geometry']['plane'] = {'c': 1, 'z': 1, 't': 1}
    annotation['properties']['metadata'] = {'note': 'alpha, "quoted"\nline'}
    collection['features'].append({
        'type': 'Feature', 'id': 'unpositioned',
        'geometry': {'type': 'Point', 'coordinates': [2, 3]},
        'properties': {'objectType': 'annotation', 'name': 'Point'},
    })
    source = tmp_path / 'objects.geojson'
    source.write_text(json.dumps(collection), encoding='utf-8')
    saved = tmp_path / 'saved.qpx'
    report = run('geojson-import', with_image, image_id, source, saved, '--assume-pixels')
    assert report['issues'] == []
    polygon, point = [row['object_id'] for row in report['objects']]
    protected = {path: path.read_bytes() for path in (saved, image, other_image)}

    output = tmp_path / 'objects.csv'
    report = run('measurements-export', saved, image_id, output)
    data = output.read_bytes()
    assert (report['state'], report['image_id'], report['rows'], report['bytes']) == ('saved', image_id, 2, len(data))
    rows = list(csv.DictReader(io.StringIO(data.decode('utf-8'), newline='')))
    assert [int(row['object_id']) for row in rows] == [polygon, point]
    assert rows[0] == {
        'object_id': str(polygon), 'parent_id': '0', 'name': 'Unsupported properties',
        'object_type': 'annotation', 'class_name': 'Tumor', 'roi_type': 'polygon',
        'x_px': '10', 'y_px': '10', 'width_px': '20', 'height_px': '20',
        'area_px2': '400', 'length_px': '80', 'area_um2': '50',
        'nucleus_area_px2': '', 'nucleus_area_um2': '', 'mean_rgb': '', 'nucleus_mean_rgb': '',
        'metadata:note': 'alpha, "quoted"\nline', 'imported:Fixture measurement': '12.5',
        'roi_locked': 'true', 'object_color_rgb': str(0x0C2238), 'class_probability': '',
        'plane_c': '1', 'plane_z': '1', 'plane_t': '1',
    }
    assert tuple(rows[1][key] for key in ('plane_c', 'plane_z', 'plane_t')) == ('-1', '0', '0')

    selected = tmp_path / 'selected.csv'
    assert run('measurements-export', saved, image_id, selected, '--object', point, '--object', point)['rows'] == 1
    selected_rows = list(csv.DictReader(io.StringIO(selected.read_text(encoding='utf-8'))))
    assert selected_rows == [{key: rows[1][key] for key in list(rows[0])[:17]}]
    empty = tmp_path / 'empty.csv'
    assert run('measurements-export', saved, 1, empty)['rows'] == 0
    assert empty.read_bytes() == selected.read_bytes().split(b'\r\n', 1)[0] + b'\r\n'

    invalid = tmp_path / 'invalid.csv'
    run('measurements-export', saved, image_id, invalid, '--object', 99999, error='export object does not exist')
    run('measurements-export', saved, image_id, invalid, '--object', '0', error='positive native object ID')
    run('measurements-export', saved, image_id, invalid, '--object', error='requires an object ID')
    run('measurements-export', saved, image_id, output, error='already exists')
    run('measurements-export', saved, image_id, saved, error='must not replace its input')
    alias = tmp_path / 'alias.csv'
    os.link(other_image, alias)
    run('measurements-export', saved, image_id, alias, error='aliases its input')
    assert not invalid.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))
    assert output.read_bytes() == data
    assert {path: path.read_bytes() for path in protected} == protected


def test_project_cli_adoption_protects_missing_original_link(project_cli, tmp_path):
    run, project, image = project_cli
    saved = tmp_path / 'saved.qpx'
    run('geojson-import', project, 1, FIXTURE / 'nested.geojson', saved, '--assume-pixels')
    project_bytes, image_bytes = saved.read_bytes(), image.read_bytes()
    online = tmp_path / 'online.csv'
    run('measurements-export', saved, 1, online)
    original_geojson = tmp_path / 'online.geojson'
    run('geojson-export', saved, 1, original_geojson, '--nested-hierarchy')

    relocated = image.rename(tmp_path / 'relocated.ppm')
    offline = tmp_path / 'offline.csv'
    run('measurements-export', saved, 1, offline)
    assert offline.read_bytes() == online.read_bytes()
    offline_geojson = tmp_path / 'offline.geojson'
    run('geojson-export', saved, 1, offline_geojson, '--nested-hierarchy')
    assert offline_geojson.read_bytes() == original_geojson.read_bytes()
    review = run('project-review-source', saved, 1, relocated)['review_sha256']
    run('project-adopt-source', saved, 1, relocated, review, image, error='must not replace its input')
    sidecar = saved.with_name(saved.name + '-wal')
    run('project-adopt-source', saved, 1, relocated, review, sidecar, error='must not replace its input')
    assert not image.exists() and not sidecar.exists()

    adopted = tmp_path / 'adopted.qpx'
    assert run('project-adopt-source', saved, 1, relocated, review, adopted)['state'] == 'saved'
    assert run('project-review-source', adopted, 1, relocated)['comparison'] == 'matching_bytes'
    csv_after = tmp_path / 'adopted.csv'
    run('measurements-export', adopted, 1, csv_after)
    assert csv_after.read_bytes() == online.read_bytes()
    assert saved.read_bytes() == project_bytes and relocated.read_bytes() == image_bytes
    assert not list(tmp_path.glob('.qpx-save-new-*'))


@pytest.mark.parametrize('command,provider,manifest_var,table,extent', [
    ('points-import', 'org.qupathxx.point-tsv', 'QUPATHXX_TEST_POINT_MANIFEST',
     'id\tx\ty\tc\tz\tt\tclass\n0001\t1.5\t2.5\t-1\t0\t0\t\n', ('0', '0')),
    ('annotations-import', 'org.qupathxx.geometry-tsv', 'QUPATHXX_TEST_GEOMETRY_MANIFEST',
     'id\tkind\tgeometry\tc\tz\tt\tclass\n0001\trectangle\t1.5,2.5,3,4\t-1\t0\t0\t\n', ('3', '4')),
])
def test_project_cli_annotation_plugins_require_verified_source(project_cli, tmp_path, command, provider,
                                                               manifest_var, table, extent):
    manifest = os.environ.get(manifest_var)
    if not manifest:
        pytest.skip(f'{provider} manifest is unavailable')
    run, project, image = project_cli
    original_image = image.read_bytes()
    verified = tmp_path / 'verified.qpx'
    review = run('project-review-source', project, 1, image)
    run('project-adopt-source', project, 1, image, review['review_sha256'], verified)
    original_project = verified.read_bytes()
    annotations = tmp_path / 'objects.tsv'
    annotations.write_text(table, encoding='utf-8')
    args = ('--format-plugins', Path(manifest).resolve(), command, provider)
    before = tmp_path / 'before.qpx'
    assert run(*args, verified, 1, annotations, before)['objects'] == [{'object_id': 1, 'source_id': '0001'}]

    replacement = tmp_path / 'replacement.ppm'
    replacement.write_bytes(original_image[:-1] + bytes([original_image[-1] + 1]))
    replacement.replace(image)
    denied = tmp_path / 'denied.qpx'
    run(*args, verified, 1, annotations, denied, error='differs from saved source verification')
    assert not denied.exists() and not list(tmp_path.glob('.qpx-save-new-*'))

    updated, imported = tmp_path / 'updated.qpx', tmp_path / 'imported.qpx'
    review = run('project-review-source', verified, 1, image)
    assert review['comparison'] == 'changed_bytes'
    run('project-adopt-source', verified, 1, image, review['review_sha256'], updated, '--accept-changed')
    assert run(*args, updated, 1, annotations, imported)['objects'] == [{'object_id': 1, 'source_id': '0001'}]
    report = tmp_path / 'measurements.csv'
    run('measurements-export', imported, 1, report)
    [row] = list(csv.DictReader(io.StringIO(report.read_text(encoding='utf-8'))))
    assert (row['name'], row['x_px'], row['y_px'], row['width_px'], row['height_px']) == ('0001', '1.5', '2.5', *extent)
    assert verified.read_bytes() == original_project


def test_rebuilt_export_preserves_wire_features_from_pinned_qupath_capture():
    root = Path(__file__).resolve().parents[1]
    folder = root / 'docs/benchmarks/annotation-migration-recovery-local/python-roundtrip'
    report = json.loads((folder / 'report.json').read_text(encoding='utf-8'))
    assert report['state'] == 'passed' and report['comparison']['parent_links'] == 4
    for name in ['native-nested.geojson', 'qupath/reference.json', 'qupath.log']:
        data = (folder / name).read_bytes()
        assert len(data) == report['files'][name]['bytes']
        assert hashlib.sha256(data).hexdigest() == report['files'][name]['sha256']
    legacy = (folder / 'native-nested.geojson').read_bytes().decode('utf-8')
    assert q.export_geojson(scene(), nested_hierarchy=True).text == expected_with_explicit_class_color(legacy)
    legacy_target = document()
    legacy_import = q.import_geojson(legacy_target, legacy)
    assert_objects(legacy_target, legacy_import.imported_ids)
    assert q.export_geojson(legacy_target, nested_hierarchy=True).text == legacy


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
