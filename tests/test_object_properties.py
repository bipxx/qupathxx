"""Imported facts are owned native state, independent from computed features."""
import csv
import io
import json
import math
import os
from pathlib import Path
import sqlite3
import struct

import pytest
import qupathxx as q
from test_interchange import feature, rectangle


def document():
    doc=q.Document(q.Image.demo());obj=doc.add_rectangle(q.Rect(1,2,10,12))
    return doc,obj


def test_owned_properties_survive_edits_and_explicit_clear_is_undoable():
    doc,obj=document();metadata={'note':'α 🚀','quoted':'a,"b"\nline'};measurements={'Area':3.5,'zero':-0.0,'missing':math.nan,'positive':math.inf,'negative':-math.inf}
    props=q.ObjectProperties(metadata,measurements);metadata['note']='mutated';measurements['Area']=99
    doc.set_object_properties(obj,props);snapshot=doc.annotation(obj)
    assert snapshot.properties.metadata['note']=='α 🚀' and snapshot.properties.measurements['Area']==3.5
    detached=props.metadata;detached['note']='another';assert props.metadata['note']=='α 🚀'
    assert math.copysign(1,props.measurements['zero'])==-1 and math.isnan(props.measurements['missing'])
    revision=doc.revision;doc.set_object_properties(obj,q.ObjectProperties(props.metadata,props.measurements));assert doc.revision==revision
    doc.update_rectangle(obj,q.Rect(2,3,8,9));doc.rename_annotation(obj,'Edited');doc.set_calibration(.25,.5)
    assert doc.annotation(obj).properties.measurements['Area']==3.5 and doc.annotation(obj).geometry.area==72
    doc.set_object_properties(obj,None);assert doc.annotation(obj).properties is None
    assert snapshot.properties.measurements['Area']==3.5 and doc.undo() and doc.annotation(obj).properties.metadata==props.metadata
    assert doc.redo() and doc.annotation(obj).properties is None


@pytest.mark.parametrize('metadata,measurements', [({'':'value'},{}),({'a':'bad\0value'},{}),({'a': 'x'*4097},{}),({}, {'':1}),({str(i):'x' for i in range(4097)},{}),({str(i):'x'*4096 for i in range(256)}, {})])
def test_property_limits(metadata,measurements):
    with pytest.raises((ValueError,RuntimeError,IndexError)):q.ObjectProperties(metadata,measurements)


def test_compound_property_edit_is_atomic_and_merge_requires_explicit_clearing():
    doc,obj=document();props=q.ObjectProperties({'note':'retain'},{'Area':3.5});revision=doc.revision
    with pytest.raises(IndexError):doc.execute([q.ObjectEdit.properties(obj,props),q.ObjectEdit.properties(999,props)])
    assert doc.revision==revision and doc.annotation(obj).properties is None
    doc.set_object_properties(obj,props);other=doc.add_rectangle(q.Rect(5,5,12,12))
    if q.has_geos:
        with pytest.raises(ValueError,match='historical object properties'):doc.merge_annotations([obj,other])
        assert doc.object_count==2 and doc.annotation(obj).properties.metadata=={'note':'retain'}


@pytest.mark.skipif(not q.has_interchange,reason='GeoJSON')
def test_geojson_and_csv_preserve_metadata_and_measurement_semantics():
    doc=q.Document(q.Image.demo())
    data=feature(rectangle(),metadata={'note':'α, "quoted"\nline'},measurements={'Area':3.5,'negative zero':-0.0,'missing':'NaN','plus':'Infinity','minus':'-Infinity'})
    report=q.import_geojson(doc,json.dumps(data),assume_pixels=True);assert report.issues==[]
    obj=report.imported_ids[0];props=doc.annotation(obj).properties
    assert props.metadata==data['properties']['metadata'] and math.isnan(props.measurements['missing'])
    exported=q.export_geojson(doc);values=json.loads(exported.text)['features'][0]['properties']
    assert values==dict(name='Annotation',objectType='annotation',qupathxx={'parentId':'0'},**data['properties'])
    assert struct.pack('<d',values['measurements']['negative zero'])==struct.pack('<d',-0.0)
    restored=q.Document(q.Image.demo());q.import_geojson(restored,exported.text)
    assert q.export_geojson(restored).text==exported.text
    rows=list(csv.DictReader(io.StringIO(q.measurements_csv(doc))))
    assert rows[0]['metadata:note']==props.metadata['note'] and rows[0]['imported:Area']=='3.5' and rows[0]['imported:missing']=='NaN'
    assert doc.undo() and doc.object_count==0 and doc.redo() and doc.annotation(obj).properties.measurements['Area']==3.5


@pytest.mark.skipif(not q.has_interchange,reason='GeoJSON')
@pytest.mark.parametrize('properties',[{'metadata':{'note':3}},{'metadata':[]},{'measurements':{'value':True}},{'measurements':{'value':None}},{'measurements':{'value':'garbage'}},{'measurements':[{'name':'x','value':1},{'name':'x','value':2}]}])
def test_invalid_properties_refuse_even_when_losses_allowed(properties):
    doc,obj=document();before=q.export_geojson(doc).text
    with pytest.raises(ValueError):q.import_geojson(doc,json.dumps(feature(rectangle(),**properties)),assume_pixels=True,allow_losses=True)
    assert q.export_geojson(doc).text==before


@pytest.mark.skipif(not q.has_interchange,reason='GeoJSON')
def test_legacy_measurement_array_remains_supported_and_unknown_fields_report_loss():
    doc=q.Document(q.Image.demo());data=feature(rectangle(),measurements=[{'name':'Area','value':3.5}],custom={'anything':1})
    with pytest.raises(ValueError,match='allow_losses'):q.import_geojson(doc,json.dumps(data),assume_pixels=True)
    result=q.import_geojson(doc,json.dumps(data),assume_pixels=True,allow_losses=True)
    assert [i.code for i in result.issues]==['properties_dropped'] and doc.annotations[0].properties.measurements=={'Area':3.5}


def workspace(tmp_path):
    doc,obj=document();doc.set_object_properties(obj,q.ObjectProperties({'note':'original'},{'Area':3.5,'zero':-0.0,'missing':math.nan}))
    project=q.Project();image=project.add_document(doc);path=tmp_path/'properties.qpxproj';project.save(str(path))
    return project,image,doc,obj,path


@pytest.mark.skipif(not q.has_project,reason='project')
def test_project_snapshot_isolation_and_exact_reopen(tmp_path):
    p,image,doc,obj,path=workspace(tmp_path);job=q.ProjectSaveJob();assert job.request(p,str(path))
    doc.set_object_properties(obj,q.ObjectProperties({'note':'new'},{'Area':8.5}));assert job.wait().state=='saved'
    props=q.Project.open(str(path)).document(image).annotation(obj).properties
    assert props.metadata=={'note':'original'} and props.measurements['Area']==3.5 and math.isnan(props.measurements['missing'])
    assert struct.pack('<d',props.measurements['zero'])==struct.pack('<d',-0.0) and p.dirty
    p.save(str(path));assert q.Project.open(str(path)).document(image).annotation(obj).properties.metadata=={'note':'new'}


@pytest.mark.skipif(not q.has_project,reason='project')
def test_schema15_migration_rollback_and_property_insert_fault(tmp_path):
    p,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:db.execute('DROP TABLE object_planes');db.execute('DROP TABLE object_properties');db.execute('PRAGMA user_version=15');db.execute("CREATE TRIGGER fail_insert BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'property migration failed'); END")
    restored=q.Project.open(str(path));target=restored.document(image);assert target.annotation(obj).properties is None
    target.set_object_properties(obj,q.ObjectProperties({'note':'retained'},{'Area':3.5}))
    with pytest.raises(RuntimeError,match='property migration failed'):restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==15 and db.execute("SELECT count(*) FROM sqlite_schema WHERE name='object_properties'").fetchone()[0]==0
        db.execute('DROP TRIGGER fail_insert')
    restored.save(str(path));before=q.Project.open(str(path)).document(image).annotation(obj).properties.metadata
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_properties BEFORE INSERT ON object_properties BEGIN SELECT RAISE(ABORT,'property write failed'); END")
    target.set_object_properties(obj,q.ObjectProperties({'note':'unsaved'},{}))
    with pytest.raises(RuntimeError,match='property write failed'):restored.save(str(path))
    assert restored.dirty and q.Project.open(str(path)).document(image).annotation(obj).properties.metadata==before


@pytest.mark.skipif(not q.has_project,reason='project')
@pytest.mark.parametrize('kind',['checksum','truncated','orphan_object','orphan_image','oversized','wrong_type'])
def test_bad_property_rows_reject_before_publication(tmp_path,kind):
    p,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:
        if kind=='checksum':
            value=bytearray(db.execute('SELECT payload FROM object_properties').fetchone()[0]);value[-1]^=1;db.execute('UPDATE object_properties SET payload=?',(value,))
        elif kind=='truncated':db.execute("UPDATE object_properties SET payload=x'514f5001'")
        elif kind=='orphan_object':db.execute("UPDATE object_properties SET object_id='999'")
        elif kind=='orphan_image':db.execute("UPDATE object_properties SET image_id='999'")
        elif kind=='oversized':db.execute('UPDATE object_properties SET payload=zeroblob(2097153)')
        else:db.execute('DROP TABLE object_planes');db.execute('DROP TABLE object_properties');db.execute('CREATE TABLE object_properties(image_id TEXT,object_id TEXT,payload)');db.execute("INSERT INTO object_properties VALUES('1','1',3)")
    with pytest.raises((ValueError,RuntimeError)):q.Project.open(str(path))


@pytest.mark.skipif(not q.has_gui or not q.has_project or not q.has_interchange or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop')
def test_desktop_inspects_imported_properties_and_saves(tmp_path):
    from test_gui_replay import event
    p,image,doc,obj,path=workspace(tmp_path)
    doc.set_object_properties(obj,q.ObjectProperties({'note':'desktop metadata'},{'Synthetic':3.5}))
    options=q.GuiOptions();options.settle_tiles=True;options.docked_workspace=True;options.autosave_interval_seconds=0
    options.inputs=[event(5,q.GuiInputKind.SELECT_ALL_KEY),event(20,q.GuiInputKind.SAVE_KEY)]
    capture=tmp_path/'object-properties.ppm'
    report=q.show(p,frame_limit=45,capture_path=str(capture),options=options)
    assert report.replay_complete and report.selected_ids==[obj] and capture.exists() and not p.dirty
    restored=q.Project.open(str(path)).document(image).annotation(obj).properties
    assert restored.metadata=={'note':'desktop metadata'} and restored.measurements=={'Synthetic':3.5}


@pytest.mark.skipif(not q.has_project,reason='project')
def test_properties_respect_history_and_snapshot_payload_budgets(tmp_path):
    doc,obj=document();props=q.ObjectProperties({str(i):'x'*4000 for i in range(24)},{})
    doc.set_history_limits(10,4096);revision=doc.revision
    with pytest.raises((ValueError,RuntimeError,IndexError),match='history payload budget'):doc.set_object_properties(obj,props)
    assert doc.revision==revision and doc.annotation(obj).properties is None
    doc.set_history_limits(0,0);doc.set_object_properties(obj,props)
    project=q.Project();image=project.add_document(doc)
    # Even allowing dense pixels, properties exceed the remaining capture budget.
    baseline=q.ProjectSaveJob();path=tmp_path/'budget.qpxproj';assert baseline.request(project,str(path));assert baseline.wait().applied
    count=baseline.stats.snapshot_bytes
    doc.set_object_properties(obj,None);small=q.ProjectSaveJob();assert small.request(project,str(path));assert small.wait().applied
    assert small.stats.snapshot_bytes+50000<count
    doc.set_object_properties(obj,props);limited=q.ProjectSaveJob(small.stats.snapshot_bytes+1000)
    with pytest.raises((ValueError,RuntimeError,IndexError),match='snapshot exceeds memory budget'):limited.request(project,str(path))
