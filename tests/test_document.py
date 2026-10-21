import gc
import math

import pytest

from qupathxx import Document, Image, Point, Rect, Viewport


def test_measurements_and_snapshot_lifetime():
    doc = Document(Image(2, 1, [0, 30, 60, 120, 150, 180]))
    annotation_id = doc.add_rectangle(Rect(0, 0, 2, 1), "Tissue")
    doc.set_calibration(0.5, 0.25)
    assert doc.area_um2(annotation_id) == 0.25
    assert doc.mean_intensity(annotation_id) == 90
    snapshot = doc.annotations
    snapshot[0].roi.width = 100  # Returned ROI is a value, not mutable document state.
    assert doc.area_um2(annotation_id) == 0.25
    assert doc.remove_annotation(annotation_id)
    del doc
    gc.collect()
    assert snapshot[0].name == "Tissue"
    assert snapshot[0].roi.width == 2


def test_rejected_changes_preserve_document():
    doc = Document(Image.demo())
    for roi in [Rect(-1, 0, 2, 2), Rect(0, 0, 0, 2), Rect(0, 0, math.nan, 2), Rect(1023, 0, 2, 2)]:
        with pytest.raises(ValueError):
            doc.add_rectangle(roi)
    assert doc.annotations == []
    assert doc.add_rectangle(Rect(0, 0, 1, 1)) == 1
    with pytest.raises(ValueError):
        doc.set_calibration(2, -1)
    assert doc.pixel_width_um is None
    with pytest.raises(IndexError):
        doc.area_um2(999)


def test_fractional_pixel_center_rule():
    doc = Document(Image(2, 1, [0, 30, 60, 120, 150, 180]))
    assert doc.mean_intensity(doc.add_rectangle(Rect(0.5, 0, 1, 1))) == 30
    tiny = doc.add_rectangle(Rect(0, 0, 0.1, 0.1))
    with pytest.raises(ValueError, match="no pixel centers"):
        doc.mean_intensity(tiny)


def test_zoom_preserves_mouse_anchor():
    view = Viewport()
    view.fit(1000, 500, 800, 600)
    cursor = Point(313, 225)
    before = view.screen_to_image(cursor)
    view.zoom_at(cursor, 3)
    after = view.screen_to_image(cursor)
    assert after.x == pytest.approx(before.x)
    assert after.y == pytest.approx(before.y)
    assert view.image_to_screen(after).y == pytest.approx(cursor.y)


def test_ppm_binary_whitespace_and_errors(tmp_path):
    path = tmp_path / "sample.ppm"
    path.write_bytes(b"P6\n# comment\n1 1\n255\n\x0a\x20\xff")
    doc = Document(Image.read_ppm(str(path)))
    assert doc.mean_intensity(doc.add_rectangle(Rect(0, 0, 1, 1))) == 99
    path.write_bytes(b"P6\n999999 999999\n255\n")
    with pytest.raises(RuntimeError, match="truncated"):
        Image.read_ppm(str(path))
