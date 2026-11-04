"""Shared clipboard authority: graph/state, foreign text, atomic history and UI."""
import json
import math
import os

import pytest
import qupathxx as q
from test_interchange import feature, rectangle

pytestmark=pytest.mark.skipif(not q.has_interchange,reason="GeoJSON clipboard")


def document(width=80,height=60):
    return q.Document(q.Image(width,height,[20,40,60]*(width*height)))


def scene():
    doc=document()
    parent=doc.add_rectangle(q.Rect(1,1,40,40),'parent')
    child=doc.add_cell(q.Roi.rectangle(q.Rect(4,5,10,12)),q.Roi.rectangle(q.Rect(6,7,2,3)),parent_id=parent,class_name='Tumor')
    doc.set_class('Tumor',0xff2030)
    doc.set_object_properties(child,q.ObjectProperties({'note':'clipboard'},{'Area':3.5,'missing':math.nan},True,0x123456,.875))
    other=doc.add_rectangle(q.Rect(45,2,8,9),'unselected')
    return doc,parent,child,other


def test_clipboard_snapshot_graph_properties_ids_and_compound_history(tmp_path):
    source,parent,child,other=scene()
    copied=q.copy_objects(source,[child,parent,child])
    assert copied.features==2 and copied.issues==[]
    assert copied.text==q.copy_objects(source,[parent,child]).text
    source.set_object_properties(child,None)
    source.remove_annotation(parent)
    del source
    target=document();existing=target.add_rectangle(q.Rect(55,45,2,2))
    target.set_calibration(.25,.5);before=target.revision
    pasted=q.paste_objects(target,copied.text)
    a,b=pasted.imported_ids
    assert pasted.source_ids==[str(parent),str(child)] and a!=existing and len({a,b,existing})==3
    value=target.annotation(b)
    assert target.annotation(a).name=='parent' and value.parent_id==a
    assert value.kind==q.ObjectKind.CELL and value.geometry.area==120 and value.nucleus.area==6
    assert value.properties.metadata=={'note':'clipboard'} and value.properties.measurements['Area']==3.5
    assert math.isnan(value.properties.measurements['missing']) and value.locked
    assert value.color_rgb==0x123456 and value.class_probability==.875
    assert target.object_class('Tumor').color==0xff2030
    assert target.pixel_width_um==.25 and target.pixel_height_um==.5
    assert target.revision==before+1 and target.undo_label=='Paste objects'
    assert target.undo() and target.object_count==1 and not target.classes
    assert target.redo() and target.annotation(b).parent_id==a
    if q.has_project:
        project=q.Project();image=project.add_document(target);path=tmp_path/'clipboard.qpxproj';project.save(str(path))
        restored=q.Project.open(str(path)).document(image)
        assert restored.annotation(b).locked and restored.annotation(b).parent_id==a
        assert restored.annotation(b).nucleus.area==6 and restored.annotation(b).properties.metadata=={'note':'clipboard'}


def test_selection_scope_is_explicit_and_descendants_are_optional():
    doc,parent,child,other=scene()
    only=q.copy_objects(doc,[parent]);assert only.features==1
    subtree=q.copy_objects(doc,[parent,child],include_descendants=True);assert subtree.features==2
    detached=q.copy_objects(doc,[child]);assert detached.features==1
    assert [(i.code,i.count) for i in detached.issues]==[('parent_not_exported',1)]
    target=document();result=q.paste_objects(target,detached.text)
    assert target.annotation(result.imported_ids[0]).parent_id==0
    with pytest.raises(ValueError,match='allow_losses'):q.export_geojson(doc,[child])
    with pytest.raises(ValueError,match='select'):q.copy_objects(doc,[])
    with pytest.raises(IndexError):q.copy_objects(doc,[other+100])


def test_cross_image_pixels_do_not_rescale_or_relax_file_import():
    source=document(120,100);obj=source.add_rectangle(q.Rect(5,7,10,12))
    text=q.copy_objects(source,[obj]).text
    target=document(40,30);target.set_calibration(2,3)
    with pytest.raises(ValueError,match='dimensions'):q.import_geojson(target,text)
    result=q.paste_objects(target,text);value=target.annotation(result.imported_ids[0])
    assert (value.roi.x,value.roi.y,value.roi.width,value.roi.height)==(5,7,10,12)
    assert target.area_um2(value.id)==720 and target.pixel_width_um==2 and target.pixel_height_um==3


@pytest.mark.parametrize('payload',[
    '', '{}', '{"type":"FeatureCollection","features":[]}',
    '{"type":"Feature","type":"FeatureCollection","features":[]}',
    '{"type":"FeatureCollection","features":[]}\0',
    json.dumps(feature(rectangle(75,55,10,10))),
    json.dumps(dict(feature(rectangle()),crs={'type':'name','properties':{'name':'EPSG:4326'}})),
    json.dumps({'type':'FeatureCollection','features':[feature(rectangle()),feature(rectangle())]}),
])
def test_untrusted_text_refuses_before_ids_classes_or_history_change(payload):
    doc=document();obj=doc.add_rectangle(q.Rect(1,2,3,4));before=doc.revision;history=doc.undo_label
    with pytest.raises((ValueError,IndexError,OverflowError)):q.paste_objects(doc,payload)
    assert doc.revision==before and doc.object_count==1 and not doc.classes and doc.undo_label==history
    assert doc.add_rectangle(q.Rect(10,10,2,2))==obj+1


def test_class_conflict_and_late_bad_feature_refuse_the_entire_paste():
    source,parent,child,_=scene();text=q.copy_objects(source,[parent,child]).text
    target=document();target.set_class('Tumor',0x987654);before=target.revision
    with pytest.raises(ValueError,match='color'):q.paste_objects(target,text)
    assert target.revision==before and not target.annotations and target.object_class('Tumor').color==0x987654
    data=json.loads(text);data['features'].append(feature(rectangle(100,100)))
    clean=document()
    with pytest.raises(ValueError):q.paste_objects(clean,json.dumps(data))
    assert clean.object_count==0 and not clean.classes and not clean.can_undo


@pytest.mark.parametrize('limit',[0,-1,256*1024*1024+1])
def test_clipboard_limit_options_refuse(limit):
    doc=document();obj=doc.add_rectangle(q.Rect(1,2,3,4))
    with pytest.raises((ValueError,TypeError,OverflowError)):q.copy_objects(doc,[obj],max_bytes=limit)
    with pytest.raises((ValueError,TypeError,OverflowError)):q.paste_objects(doc,'{}',max_bytes=limit)


def test_byte_and_object_limits_are_applied_before_mutation():
    doc=document();obj=doc.add_rectangle(q.Rect(1,2,3,4));text=q.copy_objects(doc,[obj]).text
    with pytest.raises((ValueError,IndexError),match='limit'):q.copy_objects(doc,[obj],max_bytes=16)
    target=document()
    with pytest.raises((ValueError,IndexError),match='limit'):q.paste_objects(target,text,max_bytes=len(text.encode())-1)
    assert not target.can_undo and target.object_count==0
    assert q.paste_objects(target,text,max_bytes=len(text.encode())).features==1
    with pytest.raises((ValueError,IndexError),match='100000'):q.copy_objects(doc,[obj]*100001)


def test_losses_require_permission_and_report_the_omitted_state():
    doc=document();obj=doc.add_roi(q.Roi.ellipse(q.Rect(1,2,10,12)))
    with pytest.raises(ValueError,match='allow_losses'):q.copy_objects(doc,[obj])
    result=q.copy_objects(doc,[obj],allow_losses=True)
    assert any(i.code=='ellipse_polygonized' for i in result.issues)
    target=document();pasted=q.paste_objects(target,result.text)
    assert target.annotation(pasted.imported_ids[0]).geometry.kind==q.RoiKind.ELLIPSE


@pytest.mark.skipif(not q.has_tiff,reason='plane axes')
def test_preserved_or_explicit_plane_is_native_and_atomic(tmp_path):
    from test_raw_analysis import scene as raw_scene
    from test_object_planes import imported,plane_tuple,display
    doc,_,_=raw_scene(tmp_path);obj=imported(doc,c=1,z=1,t=1)
    text=q.copy_objects(doc,[obj]).text
    result=q.paste_objects(doc,text);assert plane_tuple(doc.annotation(result.imported_ids[0]).plane)==(1,1,1)
    display(doc,2,0)
    result=q.paste_objects(doc,text,plane=doc.active_plane())
    assert plane_tuple(doc.annotation(result.imported_ids[0]).plane)==(-1,2,0)
    before=doc.revision;count=doc.object_count
    with pytest.raises(ValueError):q.paste_objects(doc,text,plane=q.ObjectPlane(-1,3,0))
    assert doc.revision==before and doc.object_count==count
    assert doc.undo() and doc.object_count==count-1
    assert doc.redo() and plane_tuple(doc.annotation(result.imported_ids[0]).plane)==(-1,2,0)


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop')
def test_desktop_system_clipboard_paste_selection_history_and_saved_restart(tmp_path):
    from test_gui_replay import event
    doc,parent,child,_=scene();doc.remove_annotation(doc.annotations[-1].id)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'desktop.qpxproj';project.save(str(path))
    options=q.GuiOptions();options.docked_workspace=True;options.width=1600;options.height=1400
    options.autosave_interval_seconds=0;options.settle_tiles=True
    events=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(7,q.GuiInputKind.COPY_OBJECTS_KEY),
            event(10,q.GuiInputKind.PASTE_OBJECTS_KEY),event(14,q.GuiInputKind.UNDO_KEY),event(18,q.GuiInputKind.REDO_KEY),
            event(24,q.GuiInputKind.SAVE_KEY)]
    options.inputs=events
    report=q.show(project,frame_limit=45,capture_path=str(tmp_path/'clipboard.ppm'),options=options)
    assert report.replay_complete and report.clipboard_error==''
    assert report.clipboard_copies==1 and report.clipboard_pastes==1
    assert report.clipboard_copied_objects==report.clipboard_pasted_objects==2
    assert len(report.selected_ids)==2 and set(report.selected_ids).isdisjoint({parent,child})
    assert not project.dirty
    restored=q.Project.open(str(path)).document(image);assert restored.object_count==4
    a,b=report.selected_ids
    assert restored.annotation(b).parent_id==a and restored.annotation(b).locked


@pytest.mark.skipif(not q.has_gui or not q.has_tiff or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop planes')
def test_desktop_clipboard_survives_window_close_and_pastes_to_displayed_plane(tmp_path):
    from test_gui_replay import event
    from test_raw_analysis import scene as raw_scene
    from test_object_planes import imported,display,plane_tuple
    doc,_,_=raw_scene(tmp_path);obj=imported(doc,c=1,z=1,t=1);display(doc,1,1)
    options=q.GuiOptions();options.settle_tiles=True
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.COPY_OBJECTS_KEY)]
    first=q.show(doc,frame_limit=18,options=options)
    assert first.replay_complete and first.clipboard_copies==1 and first.clipboard_error==''
    display(doc,2,0)
    options.inputs=[event(4,q.GuiInputKind.PASTE_CURRENT_PLANE_KEY)]
    last=q.show(doc,frame_limit=20,capture_path=str(tmp_path/'clipboard-plane.ppm'),options=options)
    assert last.replay_complete and last.clipboard_pastes==1 and last.clipboard_error==''
    assert len(last.selected_ids)==1 and last.selected_ids[0]!=obj
    assert plane_tuple(doc.annotation(last.selected_ids[0]).plane)==(-1,2,0)
    assert plane_tuple(doc.annotation(obj).plane)==(1,1,1)
