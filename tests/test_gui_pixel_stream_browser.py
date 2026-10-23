"""Historical block preview uses its own local canvas and never mutates the slide."""
import os
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q
from test_gui_pixel_classifier import click,ready,ppm
from test_pixel_stream_read_job import FIXTURE

pytestmark=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='requires GUI and QUPATHXX_TEST_GUI=1')


def workspace(index=0):
    doc=q.Document(q.Image.from_numpy(np.full((64,96,3),80,np.uint8)))
    o=q.GuiOptions();o.pixel_open=True;o.pixel_stream_read_path=str(FIXTURE);o.pixel_stream_read_index=index
    o.autosave_interval_seconds=0;o.replay_timeout_seconds=20
    return doc,o


def open_inputs():
    return [*click(4,q.GuiInputTarget.PIXEL_STREAM_BROWSER),*click(8,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(12,q.GuiInputWait.PIXEL_STREAM_READ_READY)]


@pytest.mark.parametrize('dimensions,scale',[((1440,900),1.),((960,720),1.25)])
@pytest.mark.parametrize('index',[0,11])
def test_real_browser_opens_historical_blocks_and_has_exact_rendered_colors(tmp_path,dimensions,scale,index):
    doc,o=workspace(index);o.width,o.height=dimensions;o.ui_scale=scale;o.inputs=open_inputs();revision=doc.revision
    capture=tmp_path/'browser.ppm';report=q.show(doc,frame_limit=40,capture_path=str(capture),options=o);s=report.pixel_stream_read
    assert report.replay_complete and s.state=='ready' and not s.error and not s.verified and s.block_index==index
    reference=q.PixelStreamFile(str(FIXTURE)).read_block(index);assert s.block.sha256==reference.sha256
    assert report.pixel_stream_read_preview_ready and report.pixel_stream_read_gpu_bytes==s.block.width*s.block.height*4
    assert report.pixel_gpu_bytes==0 and report.pixel_result is None and report.pixel_historical_result is None and report.pixel_model is None
    assert doc.revision==revision and doc.object_count==0 and not report.pixel_contours_published and report.ui_overflow_items==0,report.ui_overflow_label
    pixels=ppm(capture);bounds=report.pixel_stream_read_preview_bounds;labels=s.block.labels_numpy()
    colors=[(76,120,168),(245,133,24)]
    for row in range(s.block.height):
        for col in range(s.block.width):
            x=round((bounds.x+(col+.5)*bounds.width/s.block.width)*report.framebuffer_scale)
            y=round((bounds.y+(row+.5)*bounds.height/s.block.height)*report.framebuffer_scale)
            expected=colors[int(labels[row,col])-1] if labels[row,col] else (30,32,36)
            np.testing.assert_array_equal(pixels[y,x],expected)


def test_real_browser_navigates_verifies_and_releases_preview_on_close():
    doc,o=workspace();o.inputs=[*open_inputs(),*click(16,q.GuiInputTarget.PIXEL_STREAM_NEXT),ready(20,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(24,q.GuiInputTarget.PIXEL_STREAM_PREVIOUS),ready(28,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(32,q.GuiInputTarget.PIXEL_STREAM_VERIFY),ready(36,q.GuiInputWait.PIXEL_STREAM_VERIFIED),*click(40,q.GuiInputTarget.PIXEL_STREAM_CLOSE)]
    report=q.show(doc,frame_limit=65,options=o)
    assert report.replay_complete and not report.pixel_error and report.pixel_stream_read.state=='idle'
    assert report.pixel_stream_read.block is None and not report.pixel_stream_read_preview_ready and report.pixel_stream_read_gpu_bytes==0
    assert 0<report.pixel_stream_read_peak_gpu_bytes<=1024*1024 and doc.object_count==0


def test_real_browser_verifies_whole_file_without_live_document_authority():
    doc,o=workspace(11);o.pixel_open=False;o.pixel_stream_browser_open=True
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(12,q.GuiInputTarget.PIXEL_STREAM_VERIFY),ready(16,q.GuiInputWait.PIXEL_STREAM_VERIFIED)]
    report=q.show(doc,frame_limit=40,options=o);s=report.pixel_stream_read
    assert report.replay_complete and s.verified and s.block_index==11 and s.block.width==8 and s.block.height==8
    assert report.pixel_stream_read_preview_ready and report.pixel_result is None and doc.object_count==0


def test_real_browser_reports_corrupt_file_without_showing_previous_values(tmp_path):
    doc,o=workspace();path=tmp_path/'broken.qps';data=bytearray(FIXTURE.read_bytes());data[-1]^=1;path.write_bytes(data)
    o.pixel_stream_read_path=str(path);o.pixel_open=False;o.pixel_stream_browser_open=True
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_FINISHED)]
    report=q.show(doc,frame_limit=35,options=o)
    assert report.replay_complete and report.pixel_stream_read.state=='failed' and report.pixel_stream_read.error
    assert report.pixel_stream_read.block is None and report.pixel_stream_read_gpu_bytes==0 and not report.pixel_stream_read_preview_ready


def test_real_browser_reopens_loaded_file_and_replaces_preview():
    doc,o=workspace(11);o.inputs=[*open_inputs(),*click(16,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(20,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(24,q.GuiInputTarget.PIXEL_STREAM_READ),ready(28,q.GuiInputWait.PIXEL_STREAM_READ_READY)]
    report=q.show(doc,frame_limit=45,options=o)
    assert report.replay_complete and report.pixel_stream_read.state=='ready' and report.pixel_stream_read.block_index==11
    assert report.pixel_stream_read.generation==3 and report.pixel_stream_read_preview_ready and not report.pixel_error


def test_real_browser_cancel_interrupts_complete_scan_and_preserves_file(tmp_path):
    import hashlib
    model=q.PixelStreamFile(str(FIXTURE)).read_block(0).model
    source=q.Document(q.Image.from_numpy(np.full((2048,2048,3),80,np.uint8)));resolution=model.resolution
    if resolution.pixel_width_um is not None:source.set_calibration(resolution.pixel_width_um,resolution.pixel_height_um)
    root=source.add_roi(q.Roi.rectangle(q.Rect(0,0,2048,2048)));options=q.PixelStreamOptions();options.block_size=16
    path=tmp_path/'many-blocks.qps';q.save_pixel_stream(q.PixelPredictionStream(source,root,model,options),str(path));before=hashlib.sha256(path.read_bytes()).hexdigest()
    doc,o=workspace();o.pixel_open=False;o.pixel_stream_browser_open=True;o.pixel_stream_read_path=str(path)
    o.inputs=[*click(4,q.GuiInputTarget.PIXEL_STREAM_OPEN),ready(8,q.GuiInputWait.PIXEL_STREAM_READ_READY),
        *click(12,q.GuiInputTarget.PIXEL_STREAM_VERIFY),*click(14,q.GuiInputTarget.PIXEL_STREAM_CANCEL),ready(18,q.GuiInputWait.PIXEL_STREAM_READ_FINISHED)]
    report=q.show(doc,frame_limit=45,options=o)
    assert report.replay_complete and report.pixel_stream_read.state=='cancelled' and not report.pixel_stream_read.verified
    assert report.pixel_stream_read.block is None and report.pixel_stream_read_gpu_bytes==0 and not report.pixel_error
    assert hashlib.sha256(path.read_bytes()).hexdigest()==before;q.PixelStreamFile(str(path)).verify()
