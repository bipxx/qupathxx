"""Owning SDK surfaces and class-specific persistence boundaries (CLASS1)."""
import hashlib
import json
import shutil
import sqlite3
from pathlib import Path

import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/qupath-class-hierarchy-0.7.0'


def definitions():
    return [q.ObjectClass('Parent', 0x80123456),
            q.ObjectClass('Parent: Positive', None, parent='Parent')]


def values(owner):
    return {c.name: (c.parent, c.color_argb, c.color) for c in owner.classes}


def test_class_values_are_owning_and_argb_is_exact():
    positive = q.ObjectClass('Parent: Leaf', -2146290602, parent='Parent')
    assert (positive.name, positive.parent, positive.leaf_name) == ('Parent: Leaf', 'Parent', 'Leaf')
    assert positive.color_argb == 0x80123456 and positive.color == 0x123456
    assert positive == q.ObjectClass('Parent: Leaf', 0x80123456, parent='Parent')
    flat = q.ObjectClass('Parent: Literal', None)
    assert flat.parent == '' and flat.leaf_name == flat.name and flat.color_argb is None
    with pytest.raises(AttributeError):
        positive.parent = ''
    doc = q.Document(q.Image.demo())
    supplied = definitions()
    doc.replace_class_registry(supplied)
    supplied.clear()
    returned = doc.classes
    retained = returned[1]
    returned.clear()
    assert doc.class_path('Parent: Positive') == ['Parent', 'Positive']
    del doc
    assert retained.parent == 'Parent' and retained.color_argb is None


@pytest.mark.parametrize('raw', [-2147483649, 4294967296, 1.5, 'red'])
def test_class_constructor_rejects_invalid_argb(raw):
    with pytest.raises((TypeError, ValueError, OverflowError)):
        q.ObjectClass('Invalid', raw)


def test_document_definition_commands_and_atomic_invalid_graph():
    doc = q.Document(q.Image.demo())
    doc.replace_class_registry(definitions())
    obj = doc.add_detection(q.Roi.rectangle(q.Rect(1, 2, 3, 4)), class_name='Parent: Positive')
    retained = doc.object_class('Parent: Positive')
    revision = doc.revision
    doc.set_class_definition(retained)
    assert doc.revision == revision
    with pytest.raises(ValueError):
        doc.replace_class_registry([q.ObjectClass('Absent: Leaf', None, parent='Absent')])
    assert doc.revision == revision and doc.object_class('Parent: Positive') == retained
    doc.rename_class('Parent', 'Renamed')
    assert doc.class_path('Renamed: Positive') == ['Renamed', 'Positive']
    assert doc.annotation(obj).class_name == 'Renamed: Positive' and retained.parent == 'Parent'
    assert doc.undo() and doc.object_class('Parent: Positive') == retained
    doc.set_class('Parent', 0xabcdef)
    assert doc.object_class('Parent').color_argb == 0x80abcdef


@pytest.mark.skipif(not q.has_project, reason='project capability')
def test_project_defaults_owning_explicit_application_and_noop(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    supplied = definitions()
    defaults = ['Parent: Positive', 'Parent', 'Parent: Positive']
    project.set_class_registry(supplied, defaults)
    supplied.clear(); defaults.clear()
    snapshot = values(project)
    returned = project.classes; returned.clear()
    returned_defaults = project.class_defaults; returned_defaults.clear()
    assert values(project) == snapshot and not doc.classes
    assert project.class_defaults == ['Parent: Positive', 'Parent', 'Parent: Positive']
    path = tmp_path / 'defaults.qpxproj'
    project.save(str(path))
    project.set_class_registry(list(reversed(project.classes)), project.class_defaults)
    assert not project.dirty
    with pytest.raises(ValueError):
        project.set_class_registry(project.classes, ['Missing'])
    assert not project.dirty and values(project) == snapshot
    revision = doc.revision
    project.apply_class_defaults(image)
    assert doc.revision == revision + 1 and values(doc) == snapshot
    assert doc.undo() and not doc.classes
    assert q.Project.open(str(path)).class_defaults == project.class_defaults


@pytest.mark.skipif(not q.has_project, reason='project capability')
def test_class_snapshot_budget_and_recovery_ownership(tmp_path):
    project = q.Project(); project.set_class_registry(definitions(), ['Parent: Positive'] * 2)
    before = values(project)
    path = tmp_path / 'snapshot.qpxproj'
    writer = q.ProjectSaveJob()
    assert writer.request(project, str(path))
    project.set_class_registry([q.ObjectClass('Later', 1)], ['Later'])
    assert writer.wait().applied
    saved = q.Project.open(str(path))
    assert values(saved) == before and saved.class_defaults == ['Parent: Positive'] * 2 and project.dirty
    project.set_class_registry([q.ObjectClass('x' * 3000 + str(i), None) for i in range(200)])
    original = path.read_bytes()
    with pytest.raises((ValueError, RuntimeError), match='budget'):
        q.ProjectSaveJob(100000).request(project, str(path))
    assert path.read_bytes() == original
    project.set_class_registry(definitions(), ['Parent: Positive'] * 2)
    recovery = tmp_path / 'classes.qpxrecover'
    autosave = q.ProjectAutosave()
    assert autosave.request(project, str(recovery)) and autosave.wait().state == 'ready'
    restored = q.Project.recover(str(recovery))
    assert values(restored) == before and restored.class_defaults == project.class_defaults
    project.set_class_registry([])
    project.restore_recovery(str(recovery))
    assert values(project) == before and project.class_defaults == ['Parent: Positive'] * 2


@pytest.mark.skipif(not q.has_project, reason='project capability')
def test_document_class_snapshot_and_recovery_are_owning(tmp_path):
    project = q.Project(); doc = q.Document(q.Image(2, 2, [0] * 12))
    image = project.add_document(doc)
    doc.replace_class_registry(definitions())
    project.set_class_registry([q.ObjectClass('Global only', None)], ['Global only'])
    before = values(doc)
    path = tmp_path / 'document-snapshot.qpxproj'; job = q.ProjectSaveJob()
    assert job.request(project, str(path))
    doc.rename_class('Parent', 'Later')
    assert job.wait().applied
    saved = q.Project.open(str(path))
    assert values(saved.document(image)) == before and saved.class_defaults == ['Global only'] and project.dirty
    assert doc.undo()
    recovery = tmp_path / 'document-classes.qpxrecover'; writer = q.ProjectAutosave()
    assert writer.request(project, str(recovery))
    doc.rename_class('Parent', 'Later')
    assert writer.wait().state == 'ready'
    recovered = q.Project.recover(str(recovery))
    assert values(recovered.document(image)) == before and recovered.class_defaults == ['Global only']
    project.restore_recovery(str(recovery))
    assert values(project.document(image)) == before
    assert doc.class_path('Later: Positive') == ['Later', 'Positive']


@pytest.mark.skipif(not q.has_project, reason='project capability')
@pytest.mark.parametrize('damage', ['orphan-parent', 'argb-negative', 'argb-overflow', 'default-gap', 'default-missing', 'doc-cache', 'doc-parent'])
def test_malformed_class_store_refuses(tmp_path, damage):
    project = q.Project(); doc = q.Document(q.Image.demo()); project.add_document(doc)
    project.set_class_registry(definitions(), ['Parent: Positive'])
    doc.replace_class_registry(definitions())
    path = tmp_path / 'bad.qpxproj'; project.save(str(path))
    with sqlite3.connect(path) as db:
        if damage == 'orphan-parent': db.execute("UPDATE project_classes SET parent_name='Absent' WHERE name='Parent: Positive'")
        elif damage == 'argb-negative': db.execute('UPDATE project_classes SET color_argb=-1')
        elif damage == 'argb-overflow': db.execute('UPDATE project_classes SET color_argb=4294967296')
        elif damage == 'default-gap': db.execute('UPDATE project_class_defaults SET position=7')
        elif damage == 'default-missing': db.execute("UPDATE project_class_defaults SET class_name='Missing'")
        elif damage == 'doc-cache': db.execute("UPDATE classes SET color=1 WHERE name='Parent'")
        else: db.execute("UPDATE class_semantics SET parent_name='Absent' WHERE class_name='Parent: Positive'")
    with pytest.raises((ValueError, RuntimeError)):
        q.Project.open(str(path))


@pytest.mark.skipif(not q.has_project, reason='project capability')
def test_failed_class_write_keeps_committed_project(tmp_path):
    project = q.Project(); project.set_class_registry(definitions(), ['Parent: Positive'])
    path = tmp_path / 'atomic.qpxproj'; project.save(str(path)); original = values(project)
    with sqlite3.connect(path) as db:
        db.execute("CREATE TRIGGER fail_classes BEFORE INSERT ON project_classes BEGIN SELECT RAISE(ABORT,'class write fault'); END")
    before = path.read_bytes(); project.set_class_registry([q.ObjectClass('Later', None)], ['Later'])
    with pytest.raises(RuntimeError, match='class write fault'):
        project.save(str(path))
    assert path.read_bytes() == before and values(q.Project.open(str(path))) == original


def foreign(tmp_path, label, route):
    directory = tmp_path / 'foreign'; shutil.copytree(FIXTURE / label, directory)
    path = directory / 'project.qpproj'; manifest = json.loads(path.read_bytes())
    if manifest['images']:
        source = directory / 'source.ome.tif'
        shutil.copyfile(ROOT / 'tests/fixtures/qupath-root-facts-0.7.0/source.ome.tif', source)
        manifest['images'][0]['serverBuilder']['uri'] = source.as_uri()
        path.write_text(json.dumps(manifest), encoding='utf-8')
    if route != 'manifest':
        digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
        out = directory / 'qupathxx-exchange'; out.mkdir()
        images = []
        if manifest['images']:
            item = {'entry_id': '1', 'qpdata_sha256': digest(directory / 'data/1/data.qpdata')}
            if route == 'v2-native': item['representation'] = 'native-saved-data'
            else:
                shutil.copyfile(directory / 'objects.geojson', out / 'objects.geojson')
                item.update(file='objects.geojson', sha256=digest(out / 'objects.geojson'), pixel_width_um=None, pixel_height_um=None, issues=[])
                if route == 'v2-geojson': item['representation'] = 'geojson'
            images.append(item)
        bundle = {'schema': 'qpx.qupath-project-exchange/' + ('1' if route == 'v1' else '2'), 'qupath_version': '0.7.0', 'project_sha256': digest(path),
                  'files': {name: digest(directory / name) for name in ['classifiers/classes.json', 'data/metadata.json']}, 'images': images}
        (out / 'index.json').write_text(json.dumps(bundle), encoding='utf-8')
    return path


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff, reason='project/TIFF/interchange')
@pytest.mark.parametrize('label', ['rgb', 'rgba', 'null-color', 'color-conflict', 'empty'])
@pytest.mark.parametrize('route', ['manifest', 'v1', 'v2-native', 'v2-geojson'])
def test_captured_classes_through_installed_project_api(tmp_path, label, route):
    path = foreign(tmp_path, label, route)
    expected = json.loads((path.parent / 'classifiers/classes.json').read_bytes())['pathClasses']
    preview = q.preview_qupath_project(str(path))
    assert not any(issue.code in ['classification_hierarchy_flattened', 'project_classes_dropped'] for issue in preview.issues)
    target = tmp_path / 'saved.qpxproj'; project = preview.import_new(str(target), allow_losses=True)
    restored = q.Project.open(str(target))
    assert restored.class_defaults == [row['name'] for row in expected] and values(restored) == values(project)
    classes = {c.name: c for c in restored.classes}
    for row in expected:
        assert classes[row['name']].color_argb == (row['color'] & 0xffffffff if 'color' in row else None)
    if label == 'empty': assert not restored.images
    else:
        doc = restored.document(restored.images[0].id)
        assert doc.object_count == (6 if label == 'rgb' else 1)
        for c in restored.classes: assert doc.object_class(c.name) == c
        if label == 'rgb': assert doc.class_path('Tumor α: Positive: High') == ['Tumor α', 'Positive', 'High']
