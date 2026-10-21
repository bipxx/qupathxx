"""Immutable object blocks must reproduce the canonical graph at capture time."""
import random
import sqlite3

import pytest
import qupathxx as q

from test_project import state

pytestmark = pytest.mark.skipif(not q.has_project, reason="project storage disabled")


@pytest.mark.parametrize("count", [0, 1, 255, 256, 257, 1025])
def test_capture_retains_block_boundaries_during_edits_and_undo(tmp_path, count):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    doc.execute([q.ObjectEdit.create(q.Rect(i % 512, i//512, 1, 1), f"Object {i}") for i in range(count)])
    expected = state(doc)
    path = tmp_path/"boundary.qpxproj"
    save = q.ProjectSaveJob()
    assert save.request(project, str(path))
    if count:
        ids = [a.id for a in doc.annotations]
        for index in set([0, min(254, count-1), min(255, count-1), min(256, count-1), count-1]):
            doc.rename_annotation(ids[index], "After capture "+"x"*200)
        doc.remove_objects(ids)
        assert doc.undo() and doc.object_count == count
    else:
        doc.add_rectangle(q.Rect(1, 1, 2, 2))
    assert save.wait().applied and project.dirty
    restored = q.Project.open(str(path)).document(image)
    assert state(restored) == expected
    later = state(doc)
    assert save.request(project, str(path)) and save.wait().applied
    assert state(q.Project.open(str(path)).document(image)) == later


def test_aggregate_payload_reclaims_empty_blocks_and_accounts_long_names(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    path = tmp_path/"budget.qpxproj"
    project.save(str(path))
    save = q.ProjectSaveJob()
    assert save.request(project, str(path)) and save.wait().applied
    empty_bytes = save.stats.snapshot_bytes
    ids = doc.execute([q.ObjectEdit.create(q.Rect(i % 512, i//512, 1, 1)) for i in range(513)])
    assert save.request(project, str(path)) and save.wait().applied
    short_bytes = save.stats.snapshot_bytes
    doc.rename_annotation(ids[255], "x"*4096)
    assert save.request(project, str(path)) and save.wait().applied
    assert save.stats.snapshot_bytes-short_bytes == 4096-len("Annotation")
    limit = q.ProjectSaveJob(short_bytes)
    revision = doc.revision
    with pytest.raises((ValueError, RuntimeError, IndexError), match="snapshot exceeds memory budget"):
        limit.request(project, str(path))
    assert doc.revision == revision and doc.annotation(ids[255]).name == "x"*4096
    doc.remove_objects(ids)
    assert save.request(project, str(path)) and save.wait().applied
    assert save.stats.snapshot_bytes == empty_bytes
    assert q.Project.open(str(path)).document(image).object_count == 0


def test_mixed_edits_use_current_blocks_and_failures_publish_nothing(tmp_path):
    rng = random.Random(17)
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    root = q.Roi.rectangle(q.Rect(0, 0, 512, 512))
    parents = [doc.add_roi(root, "Parent A"), doc.add_roi(root, "Parent B")]
    doc.set_class("Tumor", 0x123456)
    doc.set_class("Immune", 0x654321)
    ids = doc.execute([q.ObjectEdit.create_geometry(q.Roi.ellipse(q.Rect(i % 256, i//256, 2, 2)),
                        f"Cell {i}", parents[0], "Tumor", q.ObjectKind.CELL,
                        q.Roi.rectangle(q.Rect(i % 256+.75, i//256+.75, .5, .5))) for i in range(770)])
    path = tmp_path/"mixed.qpxproj"
    save = q.ProjectSaveJob()
    for iteration in range(12):
        expected = state(doc)
        assert save.request(project, str(path))
        obj = rng.choice(ids)
        doc.rename_annotation(obj, f"Edit {iteration}")
        doc.classify_annotation(obj, "Immune" if iteration % 2 else "Tumor")
        doc.reparent_annotation(obj, parents[iteration % 2])
        doc.update_nucleus(obj, None if iteration % 2 else q.Roi.rectangle(q.Rect(doc.annotation(obj).roi.x+.75, doc.annotation(obj).roi.y+.75, .5, .5)))
        doc.set_calibration(.1+iteration*.01, .2)
        before = state(doc)
        with pytest.raises(ValueError):
            doc.execute([q.ObjectEdit.replace_geometry(obj, root, "Would change"), q.ObjectEdit.create(q.Rect(-1, 0, 1, 1))])
        assert state(doc) == before
        assert save.wait().applied
        restored = q.Project.open(str(path)).document(image)
        assert state(restored) == expected
    expected = state(doc)
    assert save.request(project, str(path)) and save.wait().applied
    restored = q.Project.open(str(path))
    # Loaded documents initialize save blocks before the first background save.
    assert save.request(restored, str(path)) and save.wait().applied
    assert state(q.Project.open(str(path)).document(image)) == expected


def test_sparse_unsigned_ids_do_not_allocate_intervening_blocks(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    doc.add_rectangle(q.Rect(1, 1, 2, 2), "Low")
    doc.add_rectangle(q.Rect(5, 5, 2, 2), "High")
    path = tmp_path/"sparse.qpxproj"
    project.save(str(path))
    high = 2**64-2
    with sqlite3.connect(path) as db:
        db.execute("UPDATE objects SET id=? WHERE id='2'", (str(high),))
        db.execute("UPDATE images SET next_object=?", (str(high+1),))
    loaded = q.Project.open(str(path))
    current = loaded.document(image)
    expected = state(current)
    save = q.ProjectSaveJob(4*1024*1024)
    assert save.request(loaded, str(path))
    current.rename_annotation(high, "Later high edit")
    assert save.wait().applied and save.stats.snapshot_bytes < 4*1024*1024
    assert state(q.Project.open(str(path)).document(image)) == expected
