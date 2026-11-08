"""Thin Python-to-ImGui example reuses native sampling and owned documents."""
import importlib.util
import os
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'examples/rgb_nominal_preview.py'
spec=importlib.util.spec_from_file_location('rgb_preview_example',path)
example=importlib.util.module_from_spec(spec)
exec(compile(path.read_bytes(),str(path),'exec',dont_inherit=True),example.__dict__)


def test_preview_preserves_native_pixels_nominal_calibration_and_owned_lifetime():
    rgb=np.arange(17*13*3,dtype=np.uint8).reshape(13,17,3)
    document=q.Document(q.Image.from_numpy(rgb))
    document.set_calibration(.5,1.2)
    request=q.RgbNominalRequest(0,0,17,13,1.5)
    expected=q.read_rgb_nominal_region(document.server,request)
    preview=example.make_preview(document,request)
    assert preview.server is not document.server
    assert preview.pixel_width_um==pytest.approx(.75)
    assert preview.pixel_height_um==pytest.approx(1.8)
    assert (expected.width,expected.height)==(11,9)
    document.server.close()
    actual=preview.server.read_region(q.RegionRequest(0,0,0,expected.width,expected.height))
    assert actual.data==expected.data


def test_preview_keeps_unknown_calibration_unknown():
    document=q.Document(q.Image.demo())
    preview=example.make_preview(document,q.RgbNominalRequest(0,0,17,13,1.5))
    assert preview.pixel_width_um is None and preview.pixel_height_um is None


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in RGB nominal preview')
def test_rgb_nominal_preview_renders_native_pixels_in_imgui(tmp_path):
    from PIL import Image
    document=q.Document(q.Image(256,192,[35,60,90]*256*192))
    preview=example.make_preview(document,q.RgbNominalRequest(40,30,120,80,1.5))
    document.server.close()
    options=q.GuiOptions();options.width=900;options.height=700
    options.docked_workspace=False;options.settle_tiles=True;options.autosave_interval_seconds=0
    capture=tmp_path/'preview.ppm'
    report=q.show(preview,frame_limit=16,capture_path=str(capture),options=options)
    assert report.canvas_visible and report.tiles_settled and report.uploaded_tiles>0
    with Image.open(capture) as frame:
        assert frame.getpixel((int(frame.width*.67),frame.height//2))==(35,60,90)
