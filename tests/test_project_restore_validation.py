"""Bulk native reconstruction preserves scientific state and rejects invalid graphs."""
import sqlite3

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='requires native project storage')


def fixture(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image(32, 32, [40, 80, 160]*32*32))
    image = project.add_document(doc)
    doc.set_class('Tumor', 0x123456)
    obj = doc.add_rectangle(q.Rect(0, 0, 10, 10), 'Template')
    doc.classify_annotation(obj, 'Tumor')
    path = tmp_path/'study.qpxproj'
    project.save(str(path))
    return path, image


def chain(path, depth, reverse=False):
    db = sqlite3.connect(path)
    try:
        geometry = db.execute('SELECT geometry FROM objects LIMIT 1').fetchone()[0]
        image = db.execute('SELECT id FROM images').fetchone()[0]
        db.execute('DELETE FROM objects')
        rows = []
        for i in range(1, depth+1):
            parent = (i+1 if i < depth else 0) if reverse else i-1
            rows.append((image, str(i), f'Object {i}', str(parent), 'Tumor', str(i), 0, geometry, None))
        db.executemany('INSERT INTO objects VALUES(?,?,?,?,?,?,?,?,?)', rows)
        db.execute('UPDATE images SET next_object=?, revision=?', (str(depth+10), str(depth+10)))
        db.commit()
    finally:
        db.close()


@pytest.mark.parametrize('reverse', [False, True])
def test_full_depth_hierarchy_counts_revisions_history_and_indexes_roundtrip(tmp_path, reverse):
    path, image = fixture(tmp_path)
    chain(path, 256, reverse)
    project = q.Project.open(str(path))
    doc = project.document(image)
    assert doc.object_count == doc.class_count('Tumor') == 256
    assert doc.object_class('Tumor').color == 0x123456 and doc.revision == 266
    assert not doc.can_undo and not doc.can_redo and not project.dirty
    for i in range(1, 257):
        expected_parent = (i+1 if i < 256 else 0) if reverse else i-1
        assert doc.annotation(i).parent_id == expected_parent and doc.annotation(i).revision == i
    root, leaf = (256, 1) if reverse else (1, 256)
    assert doc.children(root) == [root-1 if reverse else root+1]
    assert len(doc.query_region(q.Rect(0, 0, 10, 10))) == 256
    doc.remove_annotation(root)
    assert doc.object_count == doc.class_count('Tumor') == 0
    assert doc.can_undo and doc.undo()
    assert doc.object_count == doc.class_count('Tumor') == 256
    assert doc.annotation(leaf).name == f'Object {leaf}'
    created = doc.add_rectangle(q.Rect(0, 0, 1, 1), 'Next')
    assert created == 266
    project.save(str(path))
    reopened = q.Project.open(str(path)).document(image)
    assert reopened.object_count == 257 and reopened.class_count('Tumor') == 256


@pytest.mark.parametrize('sql,match', [
    ("UPDATE objects SET parent=id", 'cycle'),
    ("UPDATE objects SET parent='999'", 'parent'),
    ("UPDATE objects SET class_name='Unregistered'", 'registered'),
    ("UPDATE objects SET revision='0'", 'revision'),
    ("UPDATE objects SET revision='999'", 'revision'),
    ("UPDATE images SET next_object='1'", 'identity'),
    ("UPDATE classes SET color=16777216", 'color'),
    ("UPDATE classes SET name=''", 'empty'),
    ("UPDATE objects SET name=char(0)", 'NUL'),
    ("UPDATE objects SET class_name=char(0)", 'NUL'),
    ("UPDATE classes SET name=char(0)", 'NUL'),
])
def test_bulk_restore_rejects_invalid_canonical_rows_without_writing_source(tmp_path, sql, match):
    path, _ = fixture(tmp_path)
    db = sqlite3.connect(path)
    try:
        db.execute(sql)
        db.commit()
    finally:
        db.close()
    before = path.read_bytes()
    with pytest.raises((ValueError, RuntimeError), match=match):
        q.Project.open(str(path))
    assert path.read_bytes() == before


@pytest.mark.parametrize('reverse', [False, True])
def test_bulk_restore_rejects_over_depth_even_when_parent_rows_follow_children(tmp_path, reverse):
    path, _ = fixture(tmp_path)
    chain(path, 257, reverse=reverse)
    before = path.read_bytes()
    with pytest.raises(ValueError, match='256 levels'):
        q.Project.open(str(path))
    assert path.read_bytes() == before


def test_bulk_restore_rejects_disconnected_cycle_after_valid_root(tmp_path):
    path, _ = fixture(tmp_path)
    chain(path, 3)
    db = sqlite3.connect(path)
    try:
        db.execute("UPDATE objects SET parent='3' WHERE id='2'")
        db.commit()
    finally:
        db.close()
    with pytest.raises(ValueError, match='cycle'):
        q.Project.open(str(path))


@pytest.mark.parametrize('kind,match', [('outside-parent', 'contain'), ('nonarea-cell', 'area'), ('nucleus-on-annotation', 'only cells')])
def test_bulk_restore_uses_live_roi_and_object_kind_validation(tmp_path, kind, match):
    project = q.Project()
    doc = q.Document(q.Image(32, 32, [40, 80, 160]*32*32))
    project.add_document(doc)
    if kind == 'outside-parent':
        doc.add_rectangle(q.Rect(0, 0, 10, 10))
        doc.add_rectangle(q.Rect(11, 11, 2, 2))
        sql = "UPDATE objects SET parent='1' WHERE id='2'"
    elif kind == 'nonarea-cell':
        doc.add_roi(q.Roi.points([q.Point(1, 1)]))
        sql = 'UPDATE objects SET kind=2'
    else:
        doc.add_cell(q.Roi.rectangle(q.Rect(0, 0, 10, 10)), q.Roi.ellipse(q.Rect(1, 1, 2, 2)))
        sql = 'UPDATE objects SET kind=0'
    path = tmp_path/'invalid.qpxproj'
    project.save(str(path))
    db = sqlite3.connect(path)
    try:
        db.execute(sql)
        db.commit()
    finally:
        db.close()
    before = path.read_bytes()
    with pytest.raises(ValueError, match=match):
        q.Project.open(str(path))
    assert path.read_bytes() == before
