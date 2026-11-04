"""Source adoption must keep current object planes representable atomically."""
import os
import shutil

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_project_source import fingerprint,retain
from test_tiff_project import scene
from test_tiff_reader import request

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='TIFF/project')


def replacement(tmp_path,axis,count=1):
    shape=[2,3,2,16,24];shape[{'t':0,'z':1,'c':2}[axis]]=count
    path=tmp_path/f'replacement-{axis}-{count}.ome.tif'
    tifffile.imwrite(path,np.full(shape,13,dtype='uint16'),ome=True,photometric='minisblack',metadata={'axes':'TZCYX'})
    return path


def nondefault(axis):
    return {'z':q.ObjectPlane(-1,2,0),'t':q.ObjectPlane(-1,0,1),'c':q.ObjectPlane(1,0,0)}[axis]


def state(project,image):
    doc=project.document(image);baseline=project.source_verification(image)
    return (project.images[0].source_path,q.reader_source_json(doc.server.reader_source),project.dirty,
            q.source_file_association_json(baseline) if baseline else None,
            [(a.id,a.revision,a.plane.c,a.plane.z,a.plane.t) for a in doc.annotations])


@pytest.mark.parametrize('route',['relink','review'])
@pytest.mark.parametrize('axis',['z','t','c'])
def test_reduced_axes_refuse_without_publishing_reader_path_or_baseline(tmp_path,route,axis):
    project,image,_=scene(tmp_path);doc=project.document(image);obj=doc.add_rectangle(q.Rect(1,2,5,6));doc.set_object_plane(obj,nondefault(axis))
    source=tmp_path/'source.ome.tif'
    if route=='review':retain(project,image,source)
    saved=tmp_path/'original.qpxproj';project.save(str(saved));before=state(project,image);reader=doc.server;bytes_before=saved.read_bytes()
    path=replacement(tmp_path,axis)
    if route=='review':
        reviewed=project.review_source_verification(image,fingerprint(path))
        with pytest.raises(ValueError,match='retained object plane'):project.apply_source_review(reviewed,accept_changed=True)
        assert not reviewed.consumed
    else:
        with pytest.raises(ValueError,match='retained object plane'):project.relink(image,str(path))
    assert state(project,image)==before and doc.server is reader and saved.read_bytes()==bytes_before
    assert doc.server.raw_metadata.z_planes==3 and doc.server.raw_metadata.timepoints==2 and doc.server.raw_metadata.channels==2


@pytest.mark.parametrize('route',['relink','review'])
@pytest.mark.parametrize('axis',['z','t','c'])
def test_compatible_reduction_keeps_reader_handoff_planes_and_saved_replay(tmp_path,route,axis):
    project,image,values=scene(tmp_path);doc=project.document(image);obj=doc.add_rectangle(q.Rect(1,2,5,6));doc.set_object_plane(obj,q.ObjectPlane(0 if axis=='c' else -1,1,0))
    old=doc.server;old_request=request(1,1,z=2)
    expected=old.read_raw(old_request).numpy(copy=True)
    if route=='review':retain(project,image,tmp_path/'source.ome.tif')
    path=replacement(tmp_path,axis,2 if axis=='z' else 1)
    if route=='relink':project.relink(image,str(path))
    else:
        reviewed=project.review_source_verification(image,fingerprint(path));project.apply_source_review(reviewed,accept_changed=True);assert reviewed.consumed
    raw=doc.server.raw_metadata;assert {'z':raw.z_planes,'t':raw.timepoints,'c':raw.channels}[axis]==(2 if axis=='z' else 1) and doc.server.tiff_read_stats.decoded_blocks==0
    assert (doc.annotation(obj).plane.z,doc.annotation(obj).plane.t)==(1,0)
    np.testing.assert_array_equal(old.read_raw(old_request).numpy(),expected)
    capture=q.RawAnalysisRequest(doc,request(1,1,z=1),obj,tile_size=16);result=q.analyze_raw(capture)
    assert result.statistics.means==[13.0] and result.statistics.valid_pixels==30
    record=doc.keep_raw_analysis(capture,result);saved=tmp_path/'saved.qpxproj';project.save(str(saved));restored=q.Project.open(str(saved)).document(image)
    assert restored.annotation(obj).plane.z==1 and q.analyze_raw(q.RawAnalysisRequest.replay(restored,record)).input_sha256==result.input_sha256


def test_plane_edit_after_review_is_checked_at_apply_and_refusal_does_not_consume_review(tmp_path):
    project,image,_=scene(tmp_path);doc=project.document(image);obj=doc.add_rectangle(q.Rect(1,2,5,6));retain(project,image,tmp_path/'source.ome.tif')
    path=replacement(tmp_path,'z',2);reviewed=project.review_source_verification(image,fingerprint(path))
    doc.set_object_plane(obj,q.ObjectPlane(-1,2,0));before=state(project,image);reader=doc.server
    with pytest.raises(ValueError,match='retained object plane'):project.apply_source_review(reviewed,accept_changed=True)
    assert state(project,image)==before and doc.server is reader and not reviewed.consumed
    doc.set_object_plane(obj,q.ObjectPlane(-1,1,0));project.apply_source_review(reviewed,accept_changed=True)
    assert reviewed.consumed and doc.server.raw_metadata.z_planes==2 and doc.annotation(obj).plane.z==1


def test_undo_restored_plane_is_checked_before_source_adoption(tmp_path):
    project,image,_=scene(tmp_path);doc=project.document(image);obj=doc.add_rectangle(q.Rect(1,2,5,6));doc.set_object_plane(obj,q.ObjectPlane(-1,2,0))
    doc.set_object_plane(obj,q.ObjectPlane());path=replacement(tmp_path,'z',2)
    reviewed=project.review_source_verification(image,fingerprint(path));assert doc.undo();before=state(project,image)
    with pytest.raises(ValueError,match='retained object plane'):project.apply_source_review(reviewed,accept_changed=True)
    assert state(project,image)==before and not reviewed.consumed


def test_matching_byte_move_and_review_noop_preserve_plane_and_decode_work(tmp_path):
    project,image,_=scene(tmp_path);doc=project.document(image);obj=doc.add_rectangle(q.Rect(1,2,5,6));doc.set_object_plane(obj,q.ObjectPlane(-1,2,1))
    retain(project,image,tmp_path/'source.ome.tif');path=tmp_path/'saved.qpxproj';project.save(str(path));reader=doc.server;before=state(project,image)
    reviewed=project.review_source_verification(image,fingerprint(tmp_path/'source.ome.tif'));project.apply_source_review(reviewed)
    assert state(project,image)==before and doc.server is reader
    moved=tmp_path/'moved.ome.tif';shutil.copyfile(tmp_path/'source.ome.tif',moved)
    reviewed=project.review_source_verification(image,fingerprint(moved));assert reviewed.comparison=='matching_bytes';project.apply_source_review(reviewed)
    assert doc.annotation(obj).plane.z==2 and doc.annotation(obj).plane.t==1 and doc.server.tiff_read_stats.decoded_blocks==0


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in source replacement desktop')
@pytest.mark.parametrize('compatible',[False,True])
def test_desktop_reviewed_source_axes_are_checked_before_apply_and_save(tmp_path,compatible):
    from test_gui_replay import event
    project,image,_=scene(tmp_path);doc=project.document(image);obj=doc.add_rectangle(q.Rect(1,2,5,6));doc.set_object_plane(obj,q.ObjectPlane(-1,1 if compatible else 2,0))
    retain(project,image,tmp_path/'source.ome.tif');saved=tmp_path/'source.qpxproj';project.save(str(saved));before=state(project,image);baseline=saved.read_bytes();path=replacement(tmp_path,'z',2)
    options=q.GuiOptions();options.file_fingerprint_path=str(path);options.settle_tiles=True;options.width=1280;options.height=1100
    options.inputs=[event(4,q.GuiInputKind.FINGERPRINT_FILE_KEY),event(6,q.GuiInputKind.REVIEW_SOURCE_VERIFICATION_KEY,wait=q.GuiInputWait.FILE_FINGERPRINT_READY),
        event(8,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.ACCEPT_SOURCE_REPLACEMENT,q.GuiInputWait.TARGET_READY),event(9,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.ACCEPT_SOURCE_REPLACEMENT),
        event(12,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.APPLY_SOURCE_VERIFICATION,q.GuiInputWait.TARGET_READY),event(13,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.APPLY_SOURCE_VERIFICATION)]
    if compatible:options.inputs.append(event(24,q.GuiInputKind.SAVE_KEY))
    report=q.show(project,frame_limit=50,capture_path=str(tmp_path/'source-axis-review.ppm'),options=options)
    assert report.replay_complete and report.file_fingerprint_state=='succeeded'
    if compatible:
        assert not report.file_fingerprint_error and not project.dirty and doc.server.raw_metadata.z_planes==2
        assert q.Project.open(str(saved)).document(image).annotation(obj).plane.z==1
        result=q.analyze_raw(q.RawAnalysisRequest(doc,request(1,1,z=1),obj,tile_size=16));assert result.statistics.means==[13.0]
    else:
        assert 'retained object plane' in report.file_fingerprint_error
        assert state(project,image)==before and saved.read_bytes()==baseline and doc.server.raw_metadata.z_planes==3
