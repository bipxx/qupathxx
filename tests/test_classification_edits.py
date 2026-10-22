"""Class-only transactions share the same owner/history/save contracts as geometry edits."""
import pytest
import qupathxx as q


def test_classification_batch_is_atomic_and_preserves_redo():
    doc = q.Document(q.Image.demo())
    a = doc.add_rectangle(q.Rect(1, 1, 10, 10))
    b = doc.add_cell(q.Roi.rectangle(q.Rect(20, 20, 10, 10)),
                     q.Roi.rectangle(q.Rect(22, 22, 2, 2)))
    doc.clear_history()
    doc.apply_classifications([(b, 'B'), (a, 'A')], 'Batch labels')
    assert doc.undo_label == 'Batch labels'
    assert doc.annotation(b).nucleus.area == 4
    assert doc.undo()
    revision, history = doc.revision, doc.history_bytes
    for assignments in [[(a, 'A'), (999999, 'B')], [(a, ''), (a, '')], [(a, 'x'*4097)]]:
        with pytest.raises((ValueError, IndexError)):
            doc.apply_classifications(assignments)
        assert doc.revision == revision and doc.history_bytes == history and doc.can_redo
        assert doc.annotation(a).class_name == ''
    doc.apply_classifications([(a, '')])
    assert doc.revision == revision and doc.can_redo and doc.redo()
    assert doc.annotation(a).class_name == 'A' and doc.annotation(b).class_name == 'B'


@pytest.mark.skipif(not q.has_project, reason='project storage disabled')
def test_classification_snapshots_survive_mixed_history_and_block_boundaries(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc, 'Class snapshots')
    doc.set_history_limits(0, 0)
    ids = doc.execute([q.ObjectEdit.create(q.Rect(1, 1, 2, 2), str(i)) for i in range(520)])
    doc.set_history_limits(128, 64*1024*1024)
    path = tmp_path/'classes.qpxproj'
    project.save(str(path))
    chosen = [ids[0], ids[254], ids[255], ids[256], ids[-1]]
    doc.apply_classifications([(i, 'Tumor') for i in reversed(chosen)])
    revisions = {i: doc.annotation(i).revision for i in chosen}
    saved_job = q.ProjectSaveJob()
    assert saved_job.request(project, str(path))
    assert doc.undo()
    doc.rename_annotation(ids[1], 'Live name')
    untouched_revision = doc.annotation(ids[1]).revision
    doc.apply_classifications([(chosen[0], 'Other')])
    assert doc.undo() and doc.annotation(ids[1]).revision == untouched_revision
    assert saved_job.wait().applied
    captured = q.Project.open(str(path)).document(image)
    for i in chosen:
        assert captured.annotation(i).class_name == 'Tumor'
        assert captured.annotation(i).revision == revisions[i]
        assert doc.annotation(i).class_name == ''
    assert captured.annotation(ids[1]).name == '1'
    assert project.dirty
    # Saving live state catches stale block rows after a mixed undo sequence.
    project.save(str(path))
    live = q.Project.open(str(path)).document(image)
    assert live.annotation(ids[1]).name == 'Live name'
    assert live.annotation(ids[1]).revision == untouched_revision
    assert all(live.annotation(i).class_name == '' for i in chosen)
    assert doc.redo()
    project.save(str(path))
    assert q.Project.open(str(path)).document(image).annotation(chosen[0]).class_name == 'Other'
