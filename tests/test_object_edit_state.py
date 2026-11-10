"""Persisted editing hints and reported confidence use shared native authority."""
from project_schema_contract import CURRENT_SCHEMA_VERSION, drop_newer_project_tables
import csv
import hashlib
import io
import json
import math
import os
import sqlite3
import struct

import pytest
import qupathxx as q
from test_interchange import feature, rectangle
from test_qupath_saved_data import corpus


def scene():
    doc=q.Document(q.Image.demo());obj=doc.add_roi(q.Roi.polygon([q.Point(100,100),q.Point(300,100),q.Point(300,300),q.Point(100,300)]))
    doc.set_object_properties(obj,q.ObjectProperties({'note':'retained'},{'Synthetic':3.5},locked=True,color_rgb=0x123456,class_probability=.875))
    return doc,obj


def test_owned_state_and_scripted_geometry_hint_semantics():
    doc,obj=scene();old=doc.annotation(obj);assert old.locked and old.color_rgb==0x123456 and old.class_probability==.875
    props=old.properties.with_edit_state(locked=False,color_rgb=0,class_probability=-0.0)
    assert props.metadata==old.properties.metadata and props.measurements=={'Synthetic':3.5} and not props.locked and props.color_rgb==0
    assert math.copysign(1,props.class_probability)==-1
    doc.set_object_properties(obj,props);doc.update_roi(obj,q.Roi.rectangle(q.Rect(10,10,20,20)))
    doc.set_object_edit_state(obj,True,0x654321,.625);assert doc.annotation(obj).locked
    doc.update_roi(obj,q.Roi.rectangle(q.Rect(5,5,10,10)))
    assert doc.annotation(obj).geometry.area==100 and doc.annotation(obj).class_probability==.625 and old.class_probability==.875
    assert doc.undo() and doc.annotation(obj).geometry.area==400
    doc.set_object_properties(obj,None);assert not doc.annotation(obj).locked and doc.annotation(obj).color_rgb is None
    assert doc.undo() and doc.annotation(obj).locked
    plain=q.Document(q.Image.demo());other=plain.add_rectangle(q.Rect(1,2,3,4));revision=plain.revision
    plain.set_object_edit_state(other,False);assert plain.revision==revision and plain.annotation(other).properties is None


@pytest.mark.parametrize('operation',['single','compound','batch','optimized','optimized_same_class'])
def test_reclassification_resets_confidence_and_override_atomically_with_exact_undo(operation):
    doc,obj=scene();doc.classify_annotation(obj,'Before');doc.set_object_edit_state(obj,True,0x123456,.875)
    before=doc.annotation(obj);after_class='Before' if operation=='optimized_same_class' else 'After'
    if operation=='single':doc.classify_annotation(obj,after_class)
    elif operation=='compound':doc.execute([q.ObjectEdit.replace_geometry(obj,before.geometry,'renamed',0,after_class)])
    elif operation=='batch':doc.classify_objects([obj],after_class)
    else:doc.apply_classifications([(obj,after_class)])
    value=doc.annotation(obj);assert value.class_name==after_class and value.locked and value.color_rgb is None and value.class_probability is None
    assert value.properties.metadata=={'note':'retained'} and value.properties.measurements=={'Synthetic':3.5}
    assert doc.undo();restored=doc.annotation(obj);assert restored.class_name=='Before' and restored.color_rgb==0x123456 and restored.class_probability==.875
    assert doc.redo() and doc.annotation(obj).class_probability is None


def test_taxonomy_rename_preserves_reported_state_and_failed_assignments_do_not_clear_it():
    doc,obj=scene();doc.classify_annotation(obj,'Before');doc.set_object_edit_state(obj,True,0x123456,.875)
    doc.rename_class('Before','Renamed');assert doc.annotation(obj).class_probability==.875 and doc.annotation(obj).color_rgb==0x123456
    revision=doc.revision
    with pytest.raises(IndexError):doc.apply_classifications([(obj,'New'),(999,'New')])
    assert doc.revision==revision and doc.annotation(obj).class_probability==.875
    assert doc.undo() and doc.annotation(obj).class_name=='Before' and doc.annotation(obj).class_probability==.875


@pytest.mark.parametrize('color',[0x1000000,0xffffffff])
def test_color_bounds_reject_before_mutation(color):
    doc,obj=scene();revision=doc.revision
    with pytest.raises(ValueError):doc.set_object_edit_state(obj,False,color,.5)
    assert doc.revision==revision and doc.annotation(obj).color_rgb==0x123456


@pytest.mark.parametrize('probability',[None,math.nan,-1.0,0.0,-0.0,.875,math.inf,-math.inf])
def test_reported_probability_preserves_source_values_without_claiming_calibration(probability):
    p=q.ObjectProperties(class_probability=probability)
    if probability is None or math.isnan(probability):assert p.class_probability is None
    else:assert struct.pack('<d',p.class_probability)==struct.pack('<d',probability)


@pytest.mark.skipif(not q.has_interchange,reason='GeoJSON')
def test_geojson_csv_native_extension_and_flat_nested_roundtrip():
    doc,obj=scene()
    for nested in (False,True):
        result=q.export_geojson(doc,nested_hierarchy=nested);assert [i.code for i in result.issues]==['class_probability_extension']
        props=json.loads(result.text)['features'][0]['properties'];assert props['isLocked'] and props['color']==[18,52,86] and props['qupathxx']['classProbability']==.875
        target=q.Document(q.Image.demo());q.import_geojson(target,result.text);other=target.annotations[0]
        assert other.locked and other.color_rgb==0x123456 and other.class_probability==.875 and other.properties.metadata=={'note':'retained'}
    row=list(csv.DictReader(io.StringIO(q.measurements_csv(doc))))[0]
    assert row['roi_locked']=='true' and row['object_color_rgb']==str(0x123456) and row['class_probability']=='0.875'
    for field,value in [('isLocked',1),('color',[0,0,256]),('color',1.5),('qupathxx',{'classProbability':True})]:
        candidate=feature(rectangle(),**{field:value});revision=doc.revision
        with pytest.raises(ValueError):q.import_geojson(doc,json.dumps(candidate),assume_pixels=True,allow_losses=True)
        assert doc.revision==revision


@pytest.mark.skipif(not q.has_interchange,reason='GeoJSON')
def test_distinct_export_columns_are_bounded_before_materializing_rows():
    doc,obj=scene();doc.set_object_properties(obj,q.ObjectProperties({str(i):'a' for i in range(2049)},{}))
    second=doc.add_rectangle(q.Rect(20,20,2,2));doc.set_object_properties(second,q.ObjectProperties({str(i):'b' for i in range(2049,4097)},{}))
    with pytest.raises((ValueError,RuntimeError,IndexError),match='CSV property columns'):q.measurements_csv(doc)


@pytest.mark.skipif(not q.has_project,reason='project')
def test_version1_property_payload_and_schema16_upgrade_rollback(tmp_path):
    doc,obj=scene();doc.set_object_properties(obj,q.ObjectProperties({'note':'old'},{'Synthetic':3.5}))
    project=q.Project();image=project.add_document(doc);path=tmp_path/'legacy.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        payload=db.execute('SELECT payload FROM object_properties').fetchone()[0];assert payload[:4]==b'QOP\x01'
        drop_newer_project_tables(db, 26); db.execute('DROP TABLE IF EXISTS class_semantics');db.execute('DROP TABLE IF EXISTS project_class_defaults');db.execute('DROP TABLE IF EXISTS project_classes');db.execute('DROP TABLE IF EXISTS project_metadata');db.execute("DROP TABLE IF EXISTS image_tags");db.execute("DROP TABLE IF EXISTS image_details");db.execute("DROP TABLE IF EXISTS image_metadata");db.execute("DROP TABLE IF EXISTS image_axis_calibration");db.execute("DROP TABLE IF EXISTS image_data_properties");db.execute("DROP TABLE IF EXISTS image_analysis_settings");db.execute("DROP TABLE IF EXISTS image_properties");db.execute('DROP TABLE object_planes');db.execute('PRAGMA user_version=16');db.execute("CREATE TRIGGER fail_upgrade BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'editing upgrade failed'); END")
    restored=q.Project.open(str(path));value=restored.document(image);value.set_object_edit_state(obj,True,0x123456,.875)
    with pytest.raises(RuntimeError,match='editing upgrade failed'):restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==16 and db.execute('SELECT payload FROM object_properties').fetchone()[0]==payload
        db.execute('DROP TRIGGER fail_upgrade')
    restored.save(str(path));result=q.Project.open(str(path)).document(image).annotation(obj)
    assert result.locked and result.class_probability==.875 and result.properties.metadata=={'note':'old'}
    with sqlite3.connect(path) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION and db.execute('SELECT payload FROM object_properties').fetchone()[0][:4]==b'QOP\x02'


@pytest.mark.skipif(not q.has_project,reason='project')
@pytest.mark.parametrize('kind',['flags','color','nan','schema16'])
def test_checksummed_invalid_editing_state_is_refused(tmp_path,kind):
    doc,obj=scene();project=q.Project();image=project.add_document(doc);path=tmp_path/'bad.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        data=bytearray(db.execute('SELECT payload FROM object_properties').fetchone()[0]);assert data[:4]==b'QOP\x02'
        # The existing maps precede flags/color/probability; these are the final3 words.
        at=len(data)-64-24
        if kind=='flags':struct.pack_into('<Q',data,at,8)
        elif kind=='color':struct.pack_into('<Q',data,at+8,0x1000000)
        elif kind=='nan':struct.pack_into('<d',data,at+16,math.nan)
        else:db.execute('PRAGMA user_version=16')
        if kind!='schema16':data[-64:]=hashlib.sha256(data[:-64]).hexdigest().encode();db.execute('UPDATE object_properties SET payload=?',(data,))
    with pytest.raises(ValueError):q.Project.open(str(path))


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='QuPath import')
def test_pinned_saved_color_lock_and_scalar_probability_remain_native(tmp_path):
    path=corpus(tmp_path,'losses');data=path.parent/'data/2/data.qpdata';raw=data.read_bytes();needle=b'\x77\x08\x7f\xf8\0\0\0\0\0\0';assert raw.count(needle)==1
    # Replace only the explicit serialized class-probability word in the pinned fixture.
    data.write_bytes(raw.replace(needle,b'\x77\x08'+struct.pack('>d',.875)))
    preview=q.preview_qupath_project(str(path));codes={i.code for i in preview.issues}
    assert not {'object_color_dropped','object_lock_dropped','class_probability_dropped'} & codes
    destination=tmp_path/'imported.qpxproj';preview.import_new(str(destination),allow_losses=True)
    obj=q.Project.open(str(destination)).document(preview.images[0].native_id).annotations[0]
    assert obj.locked and obj.color_rgb==0x123456 and obj.class_probability==.875 and obj.properties.measurements=={'Synthetic':3.5}


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop')
@pytest.mark.parametrize('unlock',[False,True])
def test_desktop_roi_lock_blocks_drag_until_explicit_unlock_then_save(tmp_path,unlock):
    from test_gui_tiles import replay
    from test_gui_replay import event
    doc,obj=scene();project=q.Project();image=project.add_document(doc);path=tmp_path/'locked.qpxproj';project.save(str(path))
    options=q.GuiOptions();options.docked_workspace=True;options.height=1400;options.width=1600;options.start_view=q.Viewport();options.autosave_interval_seconds=0;options.settle_tiles=True
    options.layout_path=str(tmp_path/'annotations.ini')
    (tmp_path/'annotations.ini').write_text('; qupathxx workspace layout v1\n; qupathxx sidebar=annotations\n',encoding='utf8')
    events=[event(4,q.GuiInputKind.SELECT_ALL_KEY)]
    if unlock:events += [event(6,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.TOGGLE_OBJECT_LOCK,q.GuiInputWait.TARGET_READY),event(7,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.TOGGLE_OBJECT_LOCK)]
    events += [event(10,q.GuiInputKind.VERTEX_KEY),replay(12,q.GuiInputKind.LEFT_DOWN,100,100,target=q.GuiInputTarget.IMAGE),replay(14,q.GuiInputKind.MOVE,80,80,target=q.GuiInputTarget.IMAGE),replay(16,q.GuiInputKind.LEFT_UP,80,80,target=q.GuiInputTarget.IMAGE),event(22,q.GuiInputKind.SAVE_KEY)]
    options.inputs=events;capture=tmp_path/'lock-inspector.ppm';report=q.show(project,frame_limit=40,capture_path=str(capture),options=options)
    assert report.replay_complete and report.selected_ids==[obj] and not project.dirty
    restored=q.Project.open(str(path)).document(image).annotation(obj)
    assert restored.locked is not unlock and restored.geometry.area==(44000 if unlock else 40000)
    assert restored.color_rgb==0x123456 and restored.class_probability==.875 and restored.properties.measurements=={'Synthetic':3.5}


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop')
def test_desktop_explicit_clear_then_undo_redo_keeps_the_exact_state(tmp_path):
    from test_gui_replay import event
    doc,obj=scene();project=q.Project();image=project.add_document(doc);path=tmp_path/'clear.qpxproj';project.save(str(path))
    options=q.GuiOptions();options.docked_workspace=True;options.height=1400;options.width=1600;options.autosave_interval_seconds=0;options.settle_tiles=True
    options.layout_path=str(tmp_path/'annotations.ini')
    (tmp_path/'annotations.ini').write_text('; qupathxx workspace layout v1\n; qupathxx sidebar=annotations\n',encoding='utf8')
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(6,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.APPLY_OBJECT_EDIT_STATE,q.GuiInputWait.TARGET_READY),event(7,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.APPLY_OBJECT_EDIT_STATE),event(10,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.CLEAR_OBJECT_PROPERTIES,q.GuiInputWait.TARGET_READY),event(11,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.CLEAR_OBJECT_PROPERTIES),event(14,q.GuiInputKind.UNDO_KEY),event(18,q.GuiInputKind.REDO_KEY),event(22,q.GuiInputKind.UNDO_KEY),event(26,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=40,capture_path=str(tmp_path/'restored-state.ppm'),options=options)
    assert report.replay_complete and not project.dirty
    restored=q.Project.open(str(path)).document(image).annotation(obj)
    assert restored.locked and restored.color_rgb==0x123456 and restored.class_probability==.875 and restored.properties.metadata=={'note':'retained'}


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop')
def test_desktop_object_color_overrides_class_and_invalidates_cached_overlay(tmp_path):
    from PIL import Image
    doc,obj=scene();doc.set_class('Tumor',0xff2030);doc.classify_annotation(obj,'Tumor');doc.set_object_edit_state(obj,True,0x123456,.875)
    options=q.GuiOptions();options.docked_workspace=True;options.start_view=q.Viewport();options.settle_tiles=True
    capture=tmp_path/'object-color.ppm';report=q.show(doc,frame_limit=20,capture_path=str(capture),options=options)
    bounds=report.canvas_bounds
    with Image.open(capture) as image:
        canvas=image.crop((int(bounds.x),int(bounds.y),int(bounds.x+bounds.width),int(bounds.y+bounds.height)))
        assert sum(pixel==(18,52,86) for pixel in canvas.getdata())>100
    assert doc.annotation(obj).color_rgb==0x123456 and report.overlay_cache_hits>0


@pytest.mark.skipif(not q.has_project,reason='project')
def test_optimized_reclassification_snapshots_publish_the_correct_property_owner(tmp_path):
    doc,obj=scene();doc.classify_annotation(obj,'Before');doc.set_object_edit_state(obj,True,0x123456,.875)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'before.qpxproj';job=q.ProjectSaveJob();assert job.request(project,str(path))
    doc.apply_classifications([(obj,'After')]);assert job.wait().applied
    captured=q.Project.open(str(path)).document(image).annotation(obj)
    assert captured.class_name=='Before' and captured.locked and captured.color_rgb==0x123456 and captured.class_probability==.875
    after=tmp_path/'after.qpxproj';project.save(str(after));current=q.Project.open(str(after)).document(image).annotation(obj)
    assert current.class_name=='After' and current.locked and current.color_rgb is None and current.class_probability is None
    assert doc.undo();project.save(str(after));restored=q.Project.open(str(after)).document(image).annotation(obj)
    assert restored.class_name=='Before' and restored.color_rgb==0x123456 and restored.class_probability==.875


def test_plain_classification_keeps_the_existing_compact_history_budget():
    doc=q.Document(q.Image.demo());doc.set_history_limits(0,0)
    ids=doc.execute([q.ObjectEdit.create(q.Rect(1,1,2,2)) for i in range(1000)])
    doc.set_history_limits(128,64*1024)
    # Existing class-only transactions fit this declared payload budget.
    # Objects without editing state must not pay a property-owner cost per row.
    doc.apply_classifications([(id,'Tumor') for id in ids])
    assert doc.history_bytes<=64*1024 and doc.undo() and all(a.class_name=='' for a in doc.annotations)
