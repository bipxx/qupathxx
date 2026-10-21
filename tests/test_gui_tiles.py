"""Opt-in real OpenGL checks. Run with QUPATHXX_TEST_GUI=1 on a desktop."""
import os

import pytest

import qupathxx as q

pytestmark = pytest.mark.skipif(
    not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1",
    reason="requires GUI build and QUPATHXX_TEST_GUI=1",
)


def test_real_tiled_render_and_cleanup(tmp_path):
    from PIL import Image
    # Exact constant pixels test framebuffer orientation, channel order and
    # actual draw publication instead of accepting a merely nonempty window.
    doc = q.Document(q.Image(512, 512, [30, 120, 210] * (512 * 512)))
    capture = tmp_path / "tiles.ppm"
    report = q.show(doc, frame_limit=60, capture_path=str(capture))
    assert report.uploaded_tiles == 4
    assert report.pending_tiles == 0
    assert report.peak_gpu_bytes == 512 * 512 * 4
    with Image.open(capture) as frame:
        width, height = frame.size
        assert frame.getpixel((int(width * 0.65), height // 2)) == (30, 120, 210)
    doc.set_calibration(0.25, 0.5)
    doc.add_rectangle(q.Rect(10, 10, 50, 60))
    repeated = q.show(doc, frame_limit=30)
    assert repeated.uploaded_tiles == 4 and repeated.pending_tiles == 0


def test_real_aperio_view(tmp_path):
    path = os.environ.get("QUPATHXX_APERIO_FIXTURE")
    if not path or not q.has_openslide:
        pytest.skip("requires identified Aperio fixture and OpenSlide")
    doc = q.Document(q.open_slide(path))
    report = q.show(doc, frame_limit=120, capture_path=str(tmp_path / "aperio.ppm"))
    assert report.uploaded_tiles > 0
    assert report.pending_tiles == 0
    assert report.peak_gpu_bytes <= 64 * 1024 * 1024
    assert report.cpu_frame_p95_ms > 0
    doc.server.close()
