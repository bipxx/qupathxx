import shutil
import os
import sqlite3
import subprocess
import sys
from pathlib import Path

import pytest

import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason="requires native project storage")


def ppm(path, width=8, height=8):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + bytes([30, 60, 90])*width*height)


def rect(x, y, w, h):
    return q.Roi.rectangle(q.Rect(x, y, w, h))


def test_rename_image_preserves_document_and_survives_restart(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    image = project.add_document(doc, "Original")
    path = tmp_path / "renamed.qpxproj"
    project.save(str(path))
    revision = doc.revision
    project.rename_image(image, "Renamed \u6807\u6ce8")
    assert project.dirty and project.document(image) is doc and doc.revision == revision
    for name in ["", "\0bad", "x"*4097]:
        with pytest.raises(ValueError):
            project.rename_image(image, name)
    assert project.images[0].name == "Renamed \u6807\u6ce8"
    project.save(str(path))
    project.rename_image(image, "Renamed \u6807\u6ce8")
    assert not project.dirty
    assert q.Project.open(str(path)).images[0].name == "Renamed \u6807\u6ce8"


def state(doc):
    return [(a.id, a.name, a.parent_id, a.class_name, a.revision, a.kind, a.geometry.kind,
             a.geometry.wkt, a.nucleus.wkt if a.nucleus else None) for a in doc.annotations]


def test_multi_image_roundtrip_preserves_state_pixels_and_identity(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    first = project.add_document(doc, "Embedded")
    with pytest.raises(ValueError, match="already belongs"):
        project.add_document(doc, "Duplicate")
    parent = doc.add_roi(rect(0, 0, 200, 200))
    cell = doc.add_cell(q.Roi.ellipse(q.Rect(10, 10, 30, 20)), rect(15, 15, 5, 5),
                        name="Cell", parent_id=parent, class_name="Tumor'; DROP TABLE objects;--")
    discarded = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    doc.remove_annotation(discarded)
    doc.set_class("Tumor'; DROP TABLE objects;--", 0xff2030)
    doc.set_calibration(.5, .25)
    file = tmp_path / "images" / "source.ppm"
    ppm(file)
    second = project.add_image(str(file), name="Linked")
    linked = project.document(second)
    linked.add_roi(q.Roi.points([q.Point(1, 1), q.Point(2, 2)]))
    project.active_image = second
    before, revision = state(doc), doc.revision
    path = tmp_path / "study.qpxproj"
    project.save(str(path))
    assert project.generation == 1 and not project.dirty
    loaded = q.Project.open(str(path))
    restored = loaded.document(first)
    assert loaded.identity == project.identity and loaded.active_image == second
    assert [i.name for i in loaded.images] == ["Embedded", "Linked"]
    assert all(i.available for i in loaded.images) and not loaded.dirty
    assert state(restored) == before and restored.revision == revision
    assert not restored.can_undo and restored.annotation(cell).geometry.kind == q.RoiKind.ELLIPSE
    assert restored.area_um2(cell) == doc.area_um2(cell)
    assert restored.nucleus_mean_intensity(cell) == doc.nucleus_mean_intensity(cell)
    assert restored.add_rectangle(q.Rect(0, 0, 1, 1)) > discarded
    assert loaded.dirty
    assert loaded.document(second).server.read_region(q.RegionRequest(0, 0, 0, 1, 1)).data == bytes([30, 60, 90, 255])
    with sqlite3.connect(path) as database:
        assert database.execute("PRAGMA user_version").fetchone()[0] == 7
        assert database.execute("SELECT source FROM images WHERE id=?", (str(second),)).fetchone()[0] == "images/source.ppm"
        assert database.execute("PRAGMA quick_check").fetchone()[0] == "ok"


@pytest.mark.skipif(not q.has_geos, reason="requires polygon support")
def test_holes_multipart_line_points_and_nucleus_exact_roundtrip(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    doc = project.document(image)
    ring = lambda x: [q.Point(x, 0), q.Point(x+10, 0), q.Point(x+10, 10), q.Point(x, 10)]
    polygon = q.Roi.polygon(ring(0), [[q.Point(2, 2), q.Point(8, 2), q.Point(5, 8)]])
    multipart = q.Roi.multipolygon([polygon.polygon_parts[0], q.PolygonPart(ring(20))])
    for roi in (polygon, multipart, q.Roi.polyline([q.Point(0, 0), q.Point(3, 4)]),
                q.Roi.points([q.Point(7, 9)]), q.Roi.ellipse(q.Rect(40, 0, 30, 20))):
        doc.add_roi(roi)
    doc.add_cell(multipart, polygon)
    path = tmp_path / "geometry.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert state(doc) == state(restored)
    for before, after in zip(doc.annotations, restored.annotations):
        assert before.geometry.equals(after.geometry)
        assert before.geometry.area == after.geometry.area


def test_failed_save_rolls_back_previous_valid_project(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, 10, 10), "Saved")
    path = tmp_path / "atomic.qpxproj"
    project.save(str(path))
    doc.rename_annotation(obj, "Unsaved")
    with sqlite3.connect(path) as database:
        database.execute("CREATE TRIGGER save_fault BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected write failure'); END")
    with pytest.raises(RuntimeError, match="injected write failure"):
        project.save(str(path))
    assert project.generation == 1 and project.dirty and doc.annotation(obj).name == "Unsaved"
    assert q.Project.open(str(path)).document(image).annotation(obj).name == "Saved"
    with sqlite3.connect(path) as database:
        assert database.execute("PRAGMA quick_check").fetchone()[0] == "ok"
        database.execute("DROP TRIGGER save_fault")
    project.save(str(path))
    assert project.generation == 2 and not project.dirty
    assert q.Project.open(str(path)).document(image).annotation(obj).name == "Unsaved"


def test_stale_writers_unknown_schema_and_other_database_are_rejected(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    obj = project.document(image).add_rectangle(q.Rect(0, 0, 1, 1))
    path = tmp_path / "generation.qpxproj"
    project.save(str(path))
    other = q.Project.open(str(path))
    project.document(image).rename_annotation(obj, "First writer")
    project.save(str(path))
    other.document(image).rename_annotation(obj, "Second writer")
    with pytest.raises(RuntimeError, match="changed on disk"):
        other.save(str(path))
    assert other.dirty and other.generation == 1
    with pytest.raises(ValueError, match="different project"):
        q.Project().save(str(path))
    assert q.Project.open(str(path)).document(image).annotation(obj).name == "First writer"
    with sqlite3.connect(path) as database:
        database.execute("PRAGMA user_version=999")
    before = path.read_bytes()
    with pytest.raises(ValueError, match="schema"):
        q.Project.open(str(path))
    with pytest.raises(ValueError, match="schema"):
        project.save(str(path))
    assert path.read_bytes() == before


def test_moved_project_missing_images_and_verified_relink(tmp_path):
    root = tmp_path / "original"
    file = root / "images" / "source.ppm"
    ppm(file)
    project = q.Project()
    image = project.add_image(str(file))
    obj = project.document(image).add_rectangle(q.Rect(1, 1, 2, 2))
    path = root / "move.qpxproj"
    project.save(str(path))
    moved = tmp_path / "moved"
    shutil.copytree(root, moved)
    loaded = q.Project.open(str(moved / path.name))
    doc = loaded.document(image)
    assert doc.mean_intensity(obj) == 60
    missing_file = moved / "images" / "source.ppm"
    doc.server.close()
    missing_file.unlink()
    loaded = q.Project.open(str(moved / path.name))
    assert not loaded.images[0].available
    assert loaded.document(image).annotation(obj).roi.width == 2
    with pytest.raises(RuntimeError):
        loaded.document(image).mean_intensity(obj)
    bad = tmp_path / "bad.ppm"
    ppm(bad, width=4)
    with pytest.raises(ValueError, match="match"):
        loaded.relink(image, str(bad))
    loaded.relink(image, str(file))
    assert loaded.images[0].available and loaded.dirty
    assert loaded.document(image).mean_intensity(obj) == 60
    loaded.save(str(moved / path.name))
    assert q.Project.open(str(moved / path.name)).document(image).mean_intensity(obj) == 60


def test_unicode_paths_full_uint64_ids_and_corrupt_geometry(tmp_path):
    file = tmp_path / "\u56fe\u50cf.ppm"
    ppm(file)
    project = q.Project()
    image = project.add_image(str(file), name="\u7814\u7a76")
    project.document(image).add_rectangle(q.Rect(1, 1, 2, 2))
    path = tmp_path / "\u9879\u76ee.qpxproj"
    project.save(str(path))
    huge = 2**63 + 123
    with sqlite3.connect(path) as database:
        database.execute("UPDATE objects SET id=?", (str(huge),))
        database.execute("UPDATE images SET next_object=?", (str(huge+1),))
    doc = q.Project.open(str(path)).document(image)
    assert doc.annotation(huge).id == huge
    assert doc.add_rectangle(q.Rect(0, 0, 1, 1)) == huge+1
    with sqlite3.connect(path) as database:
        database.execute("UPDATE objects SET geometry=?", (b"QRO\x01\x04\xff\xff\xff\xff",))
    with pytest.raises(ValueError, match="vertex|ROI|geometry"):
        q.Project.open(str(path))


def test_fresh_process_and_document_lifetime_after_project_removal(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(1, 1, 2, 2), "Restart")
    path = tmp_path / "restart.qpxproj"
    project.save(str(path))
    code = "import qupathxx as q,sys; p=q.Project.open(sys.argv[1]); assert p.document(1).annotation(1).name=='Restart'; assert not p.dirty"
    subprocess.run([sys.executable, "-I", "-c", code, str(path)], check=True)
    project.remove_image(image)
    assert doc.annotation(obj).name == "Restart" and project.dirty
    assert project.add_document(q.Document(q.Image.demo())) > image


def test_slide_link_keeps_explicit_unknown_calibration_and_pixels(tmp_path):
    fixture = os.environ.get("QUPATHXX_APERIO_FIXTURE")
    if not q.has_openslide or not fixture:
        pytest.skip("requires identified OpenSlide fixture")
    project = q.Project()
    image = project.add_image(fixture)
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, 20, 20))
    doc.clear_calibration()
    path = tmp_path / "slide.qpxproj"
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert restored.pixel_width_um is None and restored.pixel_height_um is None
    assert restored.mean_intensity(obj) == doc.mean_intensity(obj)
    assert restored.metadata.width == doc.metadata.width


def test_large_pyramid_is_a_small_project_link(tmp_path):
    fixture = Path(__file__).resolve().parents[1] / "build/fixtures/large-pyramid.tif"
    if not q.has_openslide or not fixture.exists():
        pytest.skip("requires generated large pyramid fixture")
    project = q.Project()
    image = project.add_image(str(fixture))
    doc = project.document(image)
    assert doc.metadata.width >= 100000
    obj = doc.add_rectangle(q.Rect(100000, 100, 20, 20))
    path = tmp_path / "large.qpxproj"
    project.save(str(path))
    assert path.stat().st_size < 256*1024
    restored = q.Project.open(str(path)).document(image)
    assert restored.metadata.width == doc.metadata.width
    assert restored.mean_intensity(obj) == pytest.approx(280/3)
    with sqlite3.connect(path) as database:
        assert database.execute("SELECT rgb IS NULL FROM images").fetchone()[0] == 1
