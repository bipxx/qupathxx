"""Frozen archive blocks preserve the save baseline while live slots change."""
import sqlite3
import subprocess
import sys

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason="project storage disabled")


def archive(tmp_path, count=700):
    project = q.Project()
    doc = q.Document(q.Image(2, 1, [10, 20, 30, 40, 50, 60]))
    image = project.add_document(doc)
    obj = doc.add_rectangle(q.Rect(0, 0, 2, 1))
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    result = q.analyze(request)
    ids = [doc.keep_analysis(request, result) for _ in range(count)]
    path = tmp_path/"blocks.qpxproj"
    project.save(str(path))
    return project, doc, image, obj, ids, result, path


@pytest.mark.parametrize("mode", ["partial_reuse", "clear_reuse"])
def test_explicit_save_keeps_frozen_blocks_during_slot_reuse(tmp_path, mode):
    project, doc, image, obj, ids, original, path = archive(tmp_path)
    lock = sqlite3.connect(path)
    lock.execute("BEGIN IMMEDIATE")
    job = q.ProjectSaveJob()
    try:
        assert job.request(project, str(path)) and job.busy
        removed = ids if mode == "clear_reuse" else ids[:100]+ids[250:270]+ids[510:530]
        for record in removed:
            assert doc.remove_analysis(record)
        request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]], [220, 230, 240]))
        fresh = q.analyze(request)
        new_ids = [doc.keep_analysis(request, fresh) for _ in range(300)]
        assert not set(new_ids).intersection(ids)
        assert doc.analysis_record_count == len(ids)-len(removed)+300
    finally:
        lock.rollback()
        lock.close()
    assert job.wait().applied and project.dirty
    restored = q.Project.open(str(path)).document(image)
    records = restored.analysis_records
    assert [record.id for record in records] == ids
    assert all(record.result.means == original.means for record in records)
    assert all(record.result.white == original.white for record in records)
    assert restored.analysis_status(ids[-1]) == "unverified"
    assert job.request(project, str(path)) and job.wait().applied and not project.dirty
    current = q.Project.open(str(path)).document(image)
    assert [record.id for record in current.analysis_records] == sorted(set(ids)-set(removed)) + new_ids
    assert current.analysis_record(new_ids[-1]).result.means == fresh.means


def test_recovery_uses_the_same_frozen_archive_blocks(tmp_path):
    project, doc, image, obj, ids, original, _ = archive(tmp_path)
    project.rename_image(image, "Recovery capture")
    job = q.ProjectAutosave()
    checkpoint = tmp_path/"blocks.qpxrecover"
    assert job.request(project, str(checkpoint))
    for record in ids:
        assert doc.remove_analysis(record)
    request = q.AnalysisRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), tile_size=16)
    new_id = doc.keep_analysis(request, q.analyze(request))
    assert job.wait().completed == 1
    restored = q.Project.recover(str(checkpoint)).document(image)
    assert [record.id for record in restored.analysis_records] == ids
    assert restored.analysis_record(ids[-1]).result.means == original.means
    assert doc.analysis_record_count == 1 and doc.latest_analysis(obj).id == new_id


def test_budget_accounts_for_partly_empty_retained_blocks(tmp_path):
    project, doc, image, _, ids, _, path = archive(tmp_path, 1000)
    for record in ids[:-1]:
        doc.remove_analysis(record)
    job = q.ProjectSaveJob(15000)
    with pytest.raises((ValueError, RuntimeError, IndexError), match="snapshot exceeds memory budget"):
        job.request(project, str(path))
    compact = q.Project()
    compact_doc = q.Document(q.Image(2, 1, [10, 20, 30, 40, 50, 60]))
    compact_image = compact.add_document(compact_doc)
    obj = compact_doc.add_rectangle(q.Rect(0, 0, 2, 1))
    request = q.AnalysisRequest(compact_doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]))
    compact_doc.keep_analysis(request, q.analyze(request))
    assert job.request(compact, str(tmp_path/"compact.qpxproj")) and job.wait().applied
    assert q.Project.open(str(path)).document(image).analysis_record_count == 1000
    assert q.Project.open(str(tmp_path/"compact.qpxproj")).document(compact_image).analysis_record_count == 1


def test_snapshot_teardown_after_last_live_archive_owner_is_removed(tmp_path):
    code = '''
import gc, sqlite3, sys
import qupathxx as q
project=q.Project()
doc=q.Document(q.Image(2,1,[10,20,30,40,50,60]))
image=project.add_document(doc)
obj=doc.add_rectangle(q.Rect(0,0,2,1))
request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
result=q.analyze(request)
for _ in range(700): doc.keep_analysis(request,result)
project.save(sys.argv[1])
lock=sqlite3.connect(sys.argv[1]);lock.execute("BEGIN IMMEDIATE")
job=q.ProjectSaveJob();assert job.request(project,sys.argv[1])
project.remove_image(image)
del doc, request, result
gc.collect()
lock.rollback();lock.close()
assert job.wait().applied
restored=q.Project.open(sys.argv[1]).document(image)
assert restored.analysis_record_count==700
del job, restored, project
gc.collect()
print("retired")
'''
    completed = subprocess.run([sys.executable, "-I", "-c", code, str(tmp_path/"retired.qpxproj")], capture_output=True, text=True, timeout=20)
    assert completed.returncode == 0, completed.stderr
    assert "retired" in completed.stdout
