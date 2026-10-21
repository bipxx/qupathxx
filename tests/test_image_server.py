import gc
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess

import pytest

import qupathxx as q
import qupathxx._native as native


def test_dense_padding_and_closed_lifetime():
    server = q.dense_image_server(q.Image(2, 1, [10, 20, 30, 100, 110, 120]))
    metadata = server.metadata
    assert metadata.width == 2
    assert metadata.levels[0].downsample == 1
    assert metadata.pixel_width_um is None
    assert metadata.pixel_type == q.PixelType.UINT8
    assert metadata.layout == q.PixelLayout.RGBA
    region = server.read_region(q.RegionRequest(-1, 0, 0, 4, 1))
    assert region.row_stride == 16
    assert region.data == bytes([0, 0, 0, 0, 10, 20, 30, 255, 100, 110, 120, 255, 0, 0, 0, 0])
    doc = q.Document(server)
    server.close()
    server.close()
    assert server.closed
    with pytest.raises(RuntimeError, match="closed"):
        server.read_region(q.RegionRequest(0, 0, 0, 1, 1))
    with pytest.raises(ValueError, match="open image server"):
        q.Document(server)
    del doc, server
    gc.collect()
    assert region.data[4:8] == bytes([10, 20, 30, 255])
    assert metadata.levels[0].width == 2


@pytest.mark.parametrize("region_request,error", [
    (q.RegionRequest(0, 0, -1, 1, 1), IndexError),
    (q.RegionRequest(0, 0, 1, 1, 1), IndexError),
    (q.RegionRequest(0, 0, 0, 0, 1), ValueError),
    (q.RegionRequest(0, 0, 0, 1, -1), ValueError),
    (q.RegionRequest(0, 0, 0, 1 << 40, 1 << 40), ValueError),
    (q.RegionRequest((1 << 63) - 1, 0, 0, 8, 1), ValueError),
])
def test_region_validation(region_request, error):
    server = q.dense_image_server(q.Image.demo())
    with pytest.raises(error):
        server.read_region(region_request)
    assert server.read_region(q.RegionRequest(0, 0, 0, 1, 1)).data[-1] == 255


def test_unknown_and_anisotropic_calibration():
    doc = q.Document(q.Image.demo())
    annotation = doc.add_rectangle(q.Rect(0, 0, 10, 20))
    assert doc.pixel_width_um is None and doc.pixel_height_um is None
    with pytest.raises(RuntimeError, match="unknown"):
        doc.area_um2(annotation)
    doc.set_calibration(0.25, 0.5)
    assert doc.area_um2(annotation) == 25
    with pytest.raises(ValueError):
        doc.set_calibration(1, float("nan"))
    assert doc.pixel_width_um == 0.25
    doc.clear_calibration()
    assert doc.pixel_width_um is None


def test_server_retained_by_document():
    server = q.dense_image_server(q.Image(1, 1, [10, 20, 30]))
    doc = q.Document(server)
    del server
    gc.collect()
    annotation = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    assert doc.mean_intensity(annotation) == 20
    assert doc.server.metadata.width == 1


def test_close_serializes_with_reads():
    server = q.dense_image_server(q.Image.demo())
    def read(_):
        try:
            return len(server.read_region(q.RegionRequest(0, 0, 0, 512, 512)).data)
        except RuntimeError as exc:
            assert "closed" in str(exc)
            return None
    with ThreadPoolExecutor(max_workers=4) as pool:
        jobs = [pool.submit(read, i) for i in range(12)]
        server.close()
        assert all(job.result() in (None, 512 * 512 * 4) for job in jobs)
    assert server.closed


def test_native_cli_matches_python(tmp_path):
    executable = Path(native.__file__).with_name("qupathxx_region.exe" if sys.platform == "win32" else "qupathxx_region")
    assert executable.is_file(), "region CLI must ship with the installed package"
    image = tmp_path / "sample.ppm"
    image.write_bytes(b"P6\n2 1\n255\n" + bytes([10, 20, 30, 100, 110, 120]))
    output = tmp_path / "output.pam"
    subprocess.run([str(executable), str(image), "-1", "0", "0", "4", "1", str(output)], check=True)
    expected = q.dense_image_server(q.Image.read_ppm(str(image))).read_region(q.RegionRequest(-1, 0, 0, 4, 1))
    assert output.read_bytes().split(b"ENDHDR\n", 1)[1] == expected.data
    original = image.read_bytes()
    rejected = subprocess.run([str(executable), str(image), "0", "0", "0", "1", "1", str(image)], capture_output=True)
    assert rejected.returncode != 0
    assert b"overwrite the source" in rejected.stderr
    assert image.read_bytes() == original
