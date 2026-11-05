"""Image-level historical facts stay distinct from ROI and computed data."""
import gc
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import subprocess
import sys

import pytest
import qupathxx as q
from project_schema_contract import CURRENT_SCHEMA_VERSION

FIXTURE=Path(__file__).parent/'fixtures/qupath-root-facts-0.7.0'


def document():return q.Document(q.Image(24,20,[20,40,60]*(24*20)))


def facts(label='original'):
    return q.ObjectProperties({'note':label},{'Area':12.5,'missing':math.nan,'positive':math.inf,'negative':-math.inf,'zero':-0.0})


def assert_special(props,label='original'):
    assert props.metadata=={'note':label}
    assert props.measurements['Area']==12.5 and math.isnan(props.measurements['missing'])
    assert props.measurements['positive']==math.inf and props.measurements['negative']==-math.inf
    assert struct.pack('<d',props.measurements['zero'])==struct.pack('<d',-0.0)


def workspace(tmp_path):
    doc=document();obj=doc.add_rectangle(q.Rect(1,2,4,5));doc.set_image_properties(facts())
    project=q.Project();image=project.add_document(doc);path=tmp_path/'image-facts.qpxproj';project.save(str(path))
    return project,image,doc,obj,path


def foreign(tmp_path):
    directory=tmp_path/'foreign';shutil.copytree(FIXTURE,directory)
    path=directory/'project/project.qpproj';data=json.loads(path.read_bytes())
    data['images'][0]['serverBuilder']['uri']=(directory/'source.ome.tif').as_uri()
    path.write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8');return path


def test_pinned_root_capture_has_independent_facts_and_probe_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in manifest['probes'].items():assert hashlib.sha256((Path(__file__).parents[1]/name).read_bytes()).hexdigest()==digest
    assert manifest['upstream']['revision']=='04ccfa4fb7d43e9b566393e08e83690b72248d44'
    reference=json.loads((FIXTURE/'reference.json').read_bytes())
    assert reference['metadata']['specimen']=='synthetic α 🚀' and reference['measurements']['Root area']==12.5
    assert reference['child']['measurement']==3.5 and reference['child']['area']==120


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='saved-data import')
def test_captured_root_facts_import_without_loss_or_fake_roi_then_reopen(tmp_path):
    path=foreign(tmp_path);before={p:p.read_bytes() for p in path.parent.rglob('*') if p.is_file()}
    preview=q.preview_qupath_project(str(path))
    assert not any(issue.code=='root_properties_dropped' for issue in preview.issues)
    assert preview.issues==[] and preview.images[0].objects==1
    output=tmp_path/'root-import.qpxproj';preview.import_new(str(output))
    doc=q.Project.open(str(output)).document(preview.images[0].native_id)
    reference=json.loads((FIXTURE/'reference.json').read_bytes());props=doc.image_properties
    assert props.metadata==reference['metadata'] and props.measurements['Root area']==12.5
    assert math.isnan(props.measurements['missing']) and props.measurements['positive infinity']==math.inf
    assert props.measurements['negative infinity']==-math.inf
    assert struct.pack('<d',props.measurements['negative zero'])==struct.pack('<d',-0.0)
    assert doc.object_count==1 and doc.annotations[0].properties.metadata==reference['child']['metadata']
    assert doc.annotations[0].properties.measurements=={'Root area':3.5} and doc.annotations[0].geometry.area==120
    assert (doc.pixel_width_um,doc.pixel_height_um)==(.25,.5)
    assert all(p.read_bytes()==data for p,data in before.items())


def test_image_facts_are_immutable_owning_undoable_and_do_not_invalidate_rois():
    doc=document();obj=doc.add_rectangle(q.Rect(1,2,4,5));doc.set_calibration(.25,.5)
    doc.set_object_properties(obj,q.ObjectProperties({'note':'object'},{'Area':3.5}))
    selection=q.ObjectSelection(doc);selection.replace([obj]);initial=(doc.revision,doc.annotation(obj).revision,selection.revision)
    measured=doc.mean_intensity(obj);value=facts();doc.set_image_properties(value)
    assert doc.image_properties is value and doc.revision==initial[0]+1 and doc.undo_label=='Set image properties'
    assert (doc.annotation(obj).revision,selection.revision)==initial[1:]
    assert doc.object_count==1 and selection.ids==[obj] and doc.mean_intensity(obj)==measured
    assert doc.annotation(obj).properties.measurements=={'Area':3.5} and doc.pixel_width_um==.25
    revision=doc.revision;doc.set_image_properties(facts());assert doc.revision==revision
    assert doc.undo() and doc.image_properties is None and selection.ids==[obj]
    assert doc.redo() and doc.image_properties is value
    doc.set_image_properties();assert doc.image_properties is None and doc.undo() and doc.image_properties is value
    copied=doc.image_properties.metadata;copied['note']='not native';assert_special(doc.image_properties)
    held=doc.image_properties;del selection,doc,value;gc.collect();assert_special(held)


def test_empty_facts_normalize_and_history_budget_refusal_is_atomic():
    doc=document();doc.set_image_properties(q.ObjectProperties());assert doc.image_properties is None and not doc.can_undo
    doc.set_history_limits(128,1);before=doc.revision
    with pytest.raises(ValueError,match='history'):doc.set_image_properties(facts())
    assert doc.revision==before and doc.image_properties is None and not doc.can_undo
    doc.set_history_limits(0,0);doc.set_image_properties(facts());assert_special(doc.image_properties)
    assert not doc.can_undo


@pytest.mark.parametrize('state',[{'locked':True},{'color_rgb':0x123456},{'class_probability':.75}])
def test_roi_edit_state_is_not_accepted_as_image_facts(state):
    doc=document();doc.set_image_properties(facts());before=doc.revision
    with pytest.raises(ValueError,match='ROI'):doc.set_image_properties(q.ObjectProperties(**state))
    assert doc.revision==before;assert_special(doc.image_properties)


@pytest.mark.skipif(not q.has_project,reason='project snapshots')
def test_saved_snapshot_owns_the_captured_image_facts_and_charges_payload(tmp_path):
    project,image,doc,obj,path=workspace(tmp_path)
    job=q.ProjectSaveJob();assert job.request(project,str(path))
    doc.set_image_properties(facts('later'));assert job.wait().applied
    assert_special(q.Project.open(str(path)).document(image).image_properties)
    assert_special(doc.image_properties,'later');assert project.dirty
    project.save(str(path));assert_special(q.Project.open(str(path)).document(image).image_properties,'later')
    # Choose a budget between the same workspace's empty and populated captures.
    doc.set_image_properties();empty=q.ProjectSaveJob();empty.request(project,str(tmp_path/'empty.qpxproj'));empty_stats=empty.wait()
    doc.set_image_properties(q.ObjectProperties({'large':'x'*4000}))
    constrained=q.ProjectSaveJob(snapshot_budget=empty_stats.snapshot_bytes+100)
    with pytest.raises(ValueError,match='budget'):constrained.request(project,str(tmp_path/'refused.qpxproj'))
    assert not (tmp_path/'refused.qpxproj').exists()


@pytest.mark.skipif(not q.has_project,reason='migration')
@pytest.mark.parametrize('version',[17,18])
def test_legacy_default_and_failed_migration_preserve_the_original_file(tmp_path,version):
    project,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE IF EXISTS image_tags");db.execute("DROP TABLE IF EXISTS image_details");db.execute("DROP TABLE IF EXISTS image_metadata");db.execute("DROP TABLE IF EXISTS image_axis_calibration");db.execute("DROP TABLE IF EXISTS image_data_properties");db.execute("DROP TABLE IF EXISTS image_analysis_settings");db.execute('DROP TABLE image_properties')
        if version<18:db.execute('DROP TABLE object_planes')
        db.execute('PRAGMA user_version='+str(version))
        db.execute("CREATE TRIGGER fail_insert BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'root migration fault'); END")
    legacy=q.Project.open(str(path));target=legacy.document(image)
    assert target.image_properties is None and target.object_count==1
    target.set_image_properties(facts())
    with pytest.raises(RuntimeError,match='root migration fault'):legacy.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==version
        assert not db.execute("SELECT name FROM sqlite_master WHERE name='image_properties'").fetchall()
        db.execute('DROP TRIGGER fail_insert')
    legacy.save(str(path));assert_special(q.Project.open(str(path)).document(image).image_properties)
    with sqlite3.connect(path) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION


@pytest.mark.skipif(not q.has_project,reason='project integrity')
@pytest.mark.parametrize('damage',['checksum','orphan','roi-state'])
def test_corrupt_or_inapplicable_image_properties_refuse_instead_of_disappearing(tmp_path,damage):
    project,image,doc,obj,path=workspace(tmp_path)
    if damage=='roi-state':
        doc.set_object_properties(obj,q.ObjectProperties(locked=True));project.save(str(path))
    with sqlite3.connect(path) as db:
        payload=db.execute('SELECT payload FROM image_properties').fetchone()[0]
        if damage=='checksum':
            value=bytearray(payload);value[-1]^=1;db.execute('UPDATE image_properties SET payload=?',(value,))
        elif damage=='orphan':db.execute('INSERT INTO image_properties VALUES(?,?)',('999',payload))
        else:db.execute('UPDATE image_properties SET payload=(SELECT payload FROM object_properties LIMIT 1)')
    with pytest.raises(ValueError):q.Project.open(str(path))


@pytest.mark.skipif(not q.has_gui or not q.has_project or not q.has_tiff or not q.has_interchange or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in historical image inspector')
def test_desktop_imported_root_and_child_facts_are_separate_and_saved(tmp_path):
    from test_gui_replay import event
    preview=q.preview_qupath_project(str(foreign(tmp_path)));path=tmp_path/'inspector.qpxproj'
    project=preview.import_new(str(path));image=project.images[0].id
    options=q.GuiOptions();options.width=1600;options.height=1400;options.docked_workspace=True;options.settle_tiles=True;options.autosave_interval_seconds=0
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(12,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=30,capture_path=str(tmp_path/'image-facts.ppm'),options=options)
    assert report.replay_complete and report.selected_ids==[project.document(image).annotations[0].id]
    assert not project.dirty
    restored=q.Project.open(str(path)).document(image)
    assert restored.image_properties.measurements['Root area']==12.5 and restored.annotations[0].properties.measurements=={'Root area':3.5}


@pytest.mark.skipif(not q.has_project,reason='native worker ownership')
@pytest.mark.parametrize('scope',['image','object'])
def test_async_image_facts_final_release_needs_no_python_on_save_worker(tmp_path,scope):
    # A blocked writer forces worker-side snapshot cleanup after a replacement.
    # Keep this bounded in a child: a GIL-dependent deleter must not hang pytest.
    script = r"""
import sys,sqlite3
sys.path.insert(0,sys.argv[1])
import qupathxx as q
assert q.build_environment().git_revision==sys.argv[3]
doc=q.Document(q.Image(8,8,[20,40,60]*64));doc.set_history_limits(0,0)
obj=doc.add_rectangle(q.Rect(1,1,2,2))
set_facts=lambda value: doc.set_image_properties(value) if sys.argv[4]=='image' else doc.set_object_properties(obj,value)
read_facts=lambda value: value.image_properties if sys.argv[4]=='image' else value.annotation(obj).properties
set_facts(q.ObjectProperties({'note':'captured'}))
project=q.Project();image=project.add_document(doc);path=sys.argv[2];project.save(path)
lock=sqlite3.connect(path);lock.execute('BEGIN IMMEDIATE')
job=q.ProjectSaveJob();assert job.request(project,path)
set_facts(q.ObjectProperties({'note':'later'}))
print('waiting for blocked writer cleanup after final old-facts release',flush=True)
result=job.wait()
assert result.state=='error' and 'locked' in result.error,result.error
lock.rollback();lock.close()
assert read_facts(q.Project.open(path).document(image)).metadata=={'note':'captured'}
assert job.request(project,path) and job.wait().applied
assert read_facts(q.Project.open(path).document(image)).metadata=={'note':'later'}
print('native-only image-facts cleanup and retry passed',flush=True)
"""
    argv=[sys.executable,'-I','-X','utf8','-c',script,str(Path(q.__file__).parent.parent),str(tmp_path/'worker.qpxproj'),q.build_environment().git_revision,scope]
    try:result=subprocess.run(argv,cwd=tmp_path,capture_output=True,text=True,timeout=15)
    except subprocess.TimeoutExpired as failure:
        output=failure.stdout or b''
        pytest.fail('image facts worker cleanup timed out: '+(output.decode('utf-8',errors='replace') if isinstance(output,bytes) else output))
    assert result.returncode==0,result.stdout+'\n'+result.stderr
    assert 'native-only image-facts cleanup and retry passed' in result.stdout
