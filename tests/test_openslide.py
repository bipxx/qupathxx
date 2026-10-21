import gc
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

import qupathxx as q
import qupathxx._native as native

pytestmark = pytest.mark.skipif(not q.has_openslide, reason="OpenSlide build disabled")


@pytest.fixture
def pyramid(tmp_path):
    np = pytest.importorskip("numpy")
    tifffile = pytest.importorskip("tifffile")
    y, x = np.indices((384, 512))
    rgb = np.stack((x % 256, y % 256, (x + y) % 256), axis=-1).astype("uint8")
    path = tmp_path / "pyramid.tif"
    with tifffile.TiffWriter(path) as writer:
        writer.write(rgb, tile=(64, 64), photometric="rgb", metadata=None)
        writer.write(rgb[::2, ::2], tile=(64, 64), photometric="rgb", subfiletype=1, metadata=None)
    return path, rgb


def test_synthetic_pyramid_against_exact_pixels(pyramid):
    np = pytest.importorskip("numpy")
    path, rgb = pyramid
    server = q.open_slide(str(path))
    metadata = server.metadata
    assert metadata.vendor == "generic-tiff"
    assert [(l.width, l.height, l.downsample) for l in metadata.levels] == [(512, 384, 1), (256, 192, 2)]
    assert metadata.pixel_width_um is None
    for level in range(2):
        region = server.read_region(q.RegionRequest(64, 80, level, 20, 17))
        actual = np.frombuffer(region.data, dtype="uint8").reshape(17, 20, 4)
        scale = 1 << level
        assert np.array_equal(actual[:, :, :3], rgb[80:80 + 17*scale:scale, 64:64 + 20*scale:scale])
        assert (actual[:, :, 3] == 255).all()
    outside = server.read_region(q.RegionRequest(-2, -2, 0, 4, 4))
    actual = np.frombuffer(outside.data, dtype="uint8").reshape(4, 4, 4)
    assert not actual[:2].any()
    assert not actual[:, :2].any()
    assert np.array_equal(actual[2:, 2:, :3], rgb[:2, :2])
    doc = q.Document(server)
    annotation = doc.add_rectangle(q.Rect(64, 80, 20, 17))
    assert doc.mean_intensity(annotation) == pytest.approx(float(rgb[80:97, 64:84].mean()))
    with pytest.raises(RuntimeError, match="unknown"):
        doc.area_um2(annotation)
    server.close()
    del doc, server
    gc.collect()
    assert outside.data == actual.tobytes()


def test_openslide_invalid_request_does_not_poison_handle(pyramid):
    server = q.open_slide(str(pyramid[0]))
    with pytest.raises(IndexError):
        server.read_region(q.RegionRequest(0, 0, 5, 1, 1))
    assert len(server.read_region(q.RegionRequest(0, 0, 0, 2, 2)).data) == 16
    server.close()
    with pytest.raises(RuntimeError, match="closed"):
        server.read_region(q.RegionRequest(0, 0, 0, 1, 1))


def test_unrecognized_input(tmp_path):
    path = tmp_path / "bad.svs"
    path.write_bytes(b"not a slide")
    with pytest.raises(RuntimeError, match="cannot open or recognize"):
        q.open_slide(str(path))


def test_vendor_slide_against_independent_binding(tmp_path):
    path = os.environ.get("QUPATHXX_APERIO_FIXTURE")
    if not path:
        pytest.skip("set QUPATHXX_APERIO_FIXTURE to the manifest's Aperio slide")
    fixture = json.loads((Path(__file__).parent / "fixtures/manifest.json").read_text())["downloads"][0]
    assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == fixture["sha256"]
    # Native module import has loaded the SDK DLL; make it discoverable to ctypes too.
    dll_directory = os.add_dll_directory(str(Path(native.__file__).parent)) if sys.platform == "win32" else None
    try:
        reference_api = pytest.importorskip("openslide")
        with reference_api.OpenSlide(path) as reference:
            server = q.open_slide(path)
            assert server.metadata.vendor == "aperio"
            assert (server.metadata.width, server.metadata.height) == reference.dimensions
            assert server.metadata.pixel_width_um == pytest.approx(float(reference.properties["openslide.mpp-x"]))
            for level in range(reference.level_count):
                for x, y in [(0, 0), (1000, 1200), (-8, -8), (reference.dimensions[0]-10, reference.dimensions[1]-10)]:
                    expected = reference.read_region((x, y), level, (32, 24)).tobytes()
                    assert server.read_region(q.RegionRequest(x, y, level, 32, 24)).data == expected
            output = tmp_path / "region.pam"
            exe = Path(native.__file__).with_name("qupathxx_region.exe" if sys.platform == "win32" else "qupathxx_region")
            subprocess.run([str(exe), path, "1000", "1200", "0", "32", "24", str(output)], check=True)
            assert output.read_bytes().split(b"ENDHDR\n", 1)[1] == reference.read_region((1000, 1200), 0, (32, 24)).tobytes()
            document = q.Document(server)
            assert document.pixel_height_um == server.metadata.pixel_height_um
            server.close()
    finally:
        if dll_directory:
            dll_directory.close()
