"""Versioned native workspace views and transactional v1 migration."""
import hashlib
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason="requires project storage")
V1 = Path(__file__).parent / "fixtures/project-v1.qpxproj"
V1_SHA = "bb79fc7a8e47896bc8793e10ed99efed9e9ffb7887a1b8a110cbdd003547a82b"


def view():
    value = q.ProjectView()
    value.center, value.zoom = q.Point(-10, 300), 8.75
    display = q.DisplaySettings()
    display.mode, display.black, display.white, display.gamma = q.DisplayMode.GREEN, 10, 220, 2.4
    value.display = display
    return value


def values(value):
    return (value.center.x, value.center.y, value.zoom, value.display.mode,
            value.display.black, value.display.white, value.display.gamma)


def test_exact_views_dirty_tracking_snapshot_isolation_and_restart(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc)
    second = project.add_document(q.Document(q.Image.demo()))
    path = tmp_path / "views.qpxproj"
    project.save(str(path))
    assert project.view(image) is None
    revision = doc.revision
    expected = view()
    project.set_view(image, expected)
    assert project.dirty and doc.revision == revision
    expected.zoom = 100
    assert project.view(image).zoom == 8.75
    snapshot = project.view(image)
    snapshot.zoom = 200
    assert project.view(image).zoom == 8.75
    project.save(str(path))
    project.set_view(image, view())
    assert not project.dirty
    loaded = q.Project.open(str(path))
    assert values(loaded.view(image)) == values(view()) and loaded.view(second) is None and not loaded.dirty
    code = "import qupathxx as q,sys; p=q.Project.open(sys.argv[1]); v=p.view(1); assert v.zoom==8.75 and v.center.x==-10 and v.display.gamma==2.4 and not p.dirty"
    subprocess.run([sys.executable, "-I", "-c", code, str(path)], check=True)
    loaded.clear_view(image)
    assert loaded.dirty and loaded.view(image) is None
    loaded.set_view(second, view())
    loaded.remove_image(second)
    loaded.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("SELECT count(*) FROM views").fetchone()[0] == 0


@pytest.mark.parametrize("field,bad", [("zoom", 0), ("zoom", -1), ("zoom", float("inf")),
                                       ("zoom", float("nan")), ("zoom", 5e-324), ("zoom", 1e100),
                                       ("center", q.Point(float("inf"), 0)), ("center", q.Point(1e308, 0))])
def test_invalid_views_leave_saved_project_unchanged(tmp_path, field, bad):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    project.set_view(image, view())
    path = tmp_path / "valid.qpxproj"
    project.save(str(path))
    invalid = view()
    setattr(invalid, field, bad)
    with pytest.raises(ValueError):
        project.set_view(image, invalid)
    assert not project.dirty and values(project.view(image)) == values(view())


def test_actual_v1_fixture_read_only_open_then_in_place_migration(tmp_path):
    assert hashlib.sha256(V1.read_bytes()).hexdigest() == V1_SHA
    path = tmp_path / "legacy.qpxproj"
    shutil.copyfile(V1, path)
    before = path.read_bytes()
    project = q.Project.open(str(path))
    assert path.read_bytes() == before and not project.dirty
    assert len(project.images) == 2 and project.view(1) is None and project.view(2) is None
    cell = project.document(1).annotation(2)
    assert cell.geometry.kind == q.RoiKind.ELLIPSE and cell.nucleus.area == .0625
    assert cell.parent_id == 1 and project.document(1).object_class("Tumor").color == 0x123456
    project.set_view(1, view())
    project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 13
        assert db.execute("SELECT count(*) FROM views").fetchone()[0] == 1
        assert db.execute("PRAGMA quick_check").fetchone()[0] == "ok"
    restored = q.Project.open(str(path))
    assert restored.generation == 2 and not restored.dirty
    assert values(restored.view(1)) == values(view())
    assert restored.document(1).annotation(2).revision == cell.revision
    assert restored.document(1).annotation(2).geometry.kind == cell.geometry.kind
    assert restored.document(1).add_rectangle(q.Rect(0, 0, .1, .1)) == 3


def test_failed_save_rolls_back_schema_migration_and_previous_data(tmp_path):
    path = tmp_path / "failed.qpxproj"
    shutil.copyfile(V1, path)
    project = q.Project.open(str(path))
    project.set_view(1, view())
    with sqlite3.connect(path) as db:
        db.execute("CREATE TRIGGER fail_migration BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected migration failure'); END")
    with pytest.raises(RuntimeError, match="injected"):
        project.save(str(path))
    assert project.dirty and project.generation == 1 and values(project.view(1)) == values(view())
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 1
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='views'").fetchone()[0] == 0
        assert db.execute("SELECT count(*) FROM objects").fetchone()[0] == 2
        assert db.execute("PRAGMA quick_check").fetchone()[0] == "ok"
        db.execute("DROP TRIGGER fail_migration")
    assert q.Project.open(str(path)).document(1).object_count == 2
    project.save(str(path))
    assert q.Project.open(str(path)).view(1).zoom == 8.75


def test_save_as_migrates_new_destination_without_rewriting_v1_source(tmp_path):
    source = tmp_path / "old.qpxproj"
    shutil.copyfile(V1, source)
    project = q.Project.open(str(source))
    destination = tmp_path / "new.qpxproj"
    project.save(str(destination))
    assert hashlib.sha256(source.read_bytes()).hexdigest() == V1_SHA
    with sqlite3.connect(destination) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 13


@pytest.mark.parametrize("assignment,parameters", [("zoom=0", ()), ("zoom=?", (float("inf"),)),
                                                   ("mode=99", ()), ("gamma=0", ()), ("white=black", ())])
def test_malformed_persisted_views_are_rejected(tmp_path, assignment, parameters):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    project.set_view(image, view())
    path = tmp_path / "corrupt.qpxproj"
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("UPDATE views SET " + assignment, parameters)
    with pytest.raises(ValueError):
        q.Project.open(str(path))


def test_orphan_view_is_rejected(tmp_path):
    path = tmp_path / "orphan.qpxproj"
    project = q.Project()
    project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("INSERT INTO views VALUES('999',0,0,1,0,0,255,1)")
    with pytest.raises(ValueError, match="orphan"):
        q.Project.open(str(path))
