"""Opt-in real native ImGui raw reader and live Python endpoint workflows."""
import os
import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_gui_tma import click
from test_gui_tiles import replay
from test_tiff_reader import request
from test_tiff_project import scene
from test_viewer_session import completed,run

pytestmark=pytest.mark.skipif(not q.has_gui or not q.has_tiff or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in raw desktop')


def options():
    o=q.GuiOptions();o.width=1280;o.height=1100;o.raw_open=True;o.replay_timeout_seconds=20
    return o


def test_apply_native_display_view(tmp_path):
    p,image,values=scene(tmp_path);o=options();preset=q.TiffOpenOptions()
    preset.view.z=2;preset.view.t=1;preset.view.channels=[1,0,-1];preset.view.lower=[0,0,0];preset.view.upper=[5000,5000,5000]
    o.tiff_options=preset;o.inputs=click(4,q.GuiInputTarget.TIFF_APPLY)
    done=replay(8,q.GuiInputKind.MOVE);done.wait_for=q.GuiInputWait.TIFF_VIEW_APPLIED;o.inputs += [done]
    report=q.show(p,frame_limit=20,options=o)
    assert not report.raw_error and report.tiff_view_changes==1
    assert p.document(image).server.tiff_view.z==2
    assert report.ui_overflow_items==0


@pytest.mark.parametrize('operation',['measure','export'])
def test_async_raw_measure_and_numpy_export(tmp_path,operation):
    p,image,values=scene(tmp_path);o=options();o.raw_tab=1
    o.raw_request=request(24,16,c=2,z=1,t=1);output=tmp_path/'raw.npy';o.raw_numpy_path=str(output)
    target=q.GuiInputTarget.RAW_MEASURE if operation=='measure' else q.GuiInputTarget.RAW_EXPORT
    o.inputs=click(5,target);done=replay(10,q.GuiInputKind.MOVE);done.wait_for=q.GuiInputWait.RAW_TASK_FINISHED;o.inputs += [done]
    report=q.show(p,frame_limit=25,options=o)
    assert not report.raw_error,report.raw_error
    expected=values[1,1].transpose(1,2,0)
    if operation=='measure':
        assert report.raw_task_state=='succeeded' and report.raw_statistics.valid_pixels==24*16
        np.testing.assert_allclose(report.raw_statistics.means,expected.mean(axis=(0,1)))
    else:
        assert report.raw_task_state=='saved'
        np.testing.assert_array_equal(np.load(output),expected)
        assert report.raw_export_bytes==output.stat().st_size
    assert report.ui_overflow_items==0


def test_managed_raw_read_view_change_and_persistence(tmp_path):
    p,image,values=scene(tmp_path);path=tmp_path/'source.qpxproj';output=tmp_path/'saved.qpxproj';p.save(str(path))
    session=q.ViewerSession(str(path));held={}
    def producer():
        meta=completed(session,session.inspect_raw(image));assert meta.raw_metadata.channels==2
        read=completed(session,session.read_raw(image,request(24,16,c=2,z=2,t=1)));held['array']=read.raw_pixels.numpy()
        v=q.TiffView();v.z=2;v.t=1;v.channels=[1,0,-1];v.lower=[0,0,0];v.upper=[5000,5000,5000]
        changed=completed(session,session.set_tiff_view(image,v));assert changed.committed
        completed(session,session.save(str(output)))
        completed(session,session.close())
    run(session,producer)
    np.testing.assert_array_equal(held['array'],values[1,2].transpose(1,2,0))
    assert q.Project.open(str(output)).document(image).server.tiff_view.z==2


def test_open_plain_tiff_series_as_an_independent_project_image(tmp_path):
    source=tmp_path/'pages.tif';values=np.arange(16*24,dtype='uint16').reshape(16,24)
    with tifffile.TiffWriter(source) as writer:
        writer.write(values,metadata=None);writer.write(values+1000,metadata=None)
    project=q.Project();first=project.add_tiff_image(str(source));o=options()
    preset=q.TiffOpenOptions();preset.view.series=1;o.tiff_options=preset
    o.inputs=click(5,q.GuiInputTarget.TIFF_ADD_SERIES)
    done=replay(10,q.GuiInputKind.MOVE);done.wait_for=q.GuiInputWait.TIFF_VIEW_APPLIED;o.inputs += [done]
    report=q.show(project,frame_limit=25,options=o)
    assert not report.raw_error and report.tiff_view_changes==1 and len(project.images)==2
    assert project.document(first).server.tiff_view.series==0
    second=next(i.id for i in project.images if i.id!=first)
    np.testing.assert_array_equal(project.document(second).server.read_raw(request(24,16)).numpy()[...,0],values+1000)


def test_gui_export_preserves_existing_file(tmp_path):
    p,_,_=scene(tmp_path);o=options();o.raw_tab=1;o.raw_request=request(24,16,c=2)
    output=tmp_path/'existing.npy';output.write_bytes(b'keep original');o.raw_numpy_path=str(output)
    o.inputs=click(5,q.GuiInputTarget.RAW_EXPORT)
    done=replay(10,q.GuiInputKind.MOVE);done.wait_for=q.GuiInputWait.RAW_TASK_FINISHED;o.inputs += [done]
    report=q.show(p,frame_limit=25,options=o)
    assert report.raw_task_state=='failed' and report.raw_error and output.read_bytes()==b'keep original'
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def test_managed_raw_budget_uses_actual_dtype_and_reclaims_failed_result(tmp_path):
    p,image,values=scene(tmp_path);path=tmp_path/'source.qpxproj';p.save(str(path))
    limits=q.ViewerLimits();limits.region_bytes=1024;session=q.ViewerSession(str(path),limits)
    def producer():
        ticket=session.read_raw(image,request(24,16))
        failure=session.wait(ticket,15);assert failure.state=='failed' and 'byte limit' in failure.error
        session.take(ticket)
        assert session.stats.retained_bytes==0
        small=completed(session,session.read_raw(image,request(1,1)))
        np.testing.assert_array_equal(small.raw_pixels.numpy()[...,0],values[0,0,0,:1,:1])
        completed(session,session.close())
    run(session,producer)
