"""Portable typed ImageData values are independent of facts and analysis state."""
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

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-image-data-properties-0.7.0'
REFERENCE={row['name']:row['properties'] for row in json.loads((FIXTURE/'reference.json').read_bytes())}


def signature(v):
    if isinstance(v,float):return ('real',struct.pack('>d',v).hex())
    if isinstance(v,dict):return ('map',tuple((k,signature(x)) for k,x in sorted(v.items())))
    if isinstance(v,list):return ('list',tuple(map(signature,v)))
    return (type(v).__name__,v)


def expected(v):
    kind=v['kind']
    if kind=='null':return None
    if kind=='integer':return int(v['value'])
    if kind=='real':return float(v['value']) if v['value'] in ('NaN','Infinity','-Infinity') else float.fromhex(v['value'])
    if kind in ('text','bool'):return v['value']
    if kind=='map':return {k:expected(x) for k,x in v['value'].items()}
    if kind=='list':return [expected(x) for x in v['value']]
    raise AssertionError(v)


def foreign(tmp_path,label):
    root=tmp_path/'foreign';shutil.copytree(FIXTURE,root);path=root/'project/project.qpproj';data=json.loads(path.read_bytes())
    data['images']=[row for row in data['images'] if row['imageName']==label];assert len(data['images'])==1
    data['images'][0]['serverBuilder']['uri']=(root/'source.ome.tif').as_uri();path.write_text(json.dumps(data),encoding='utf-8');return path


def properties():return q.ImageDataProperties({'text':'Ω α','typed':[None,True,1,9007199254740993,-0.0,float('nan'),float('inf')],'nested':{'score':3.5}})


def workspace(tmp_path):
    doc=q.Document(q.Image(16,12,[20,40,60]*16*12));obj=doc.add_rectangle(q.Rect(1,2,8,9));doc.set_image_data_properties(properties())
    project=q.Project();image=project.add_document(doc);path=tmp_path/'properties.qpxproj';project.save(str(path));return project,image,doc,obj,path


def test_pinned_property_capture_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in manifest['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest
    assert manifest['upstream']['revision']=='04ccfa4fb7d43e9b566393e08e83690b72248d44'


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='native import')
def test_captured_typed_arrays_maps_and_shared_values_roundtrip_exactly(tmp_path):
    source=foreign(tmp_path,'portable-values');before={p:p.read_bytes() for p in source.parent.rglob('*') if p.is_file()}
    preview=q.preview_qupath_project(str(source));assert preview.issues==[]
    path=tmp_path/'native.qpxproj';project=preview.import_new(str(path));doc=project.document(project.images[0].id)
    wanted={k:expected(v) for k,v in REFERENCE['portable-values'].items()};assert signature(doc.image_data_properties.values)==signature(wanted)
    values=doc.image_data_properties.values;assert type(values['bool']) is bool and type(values['beyond-double']) is int
    assert values['min-long']==-(2**63) and values['max-long']==2**63-1 and values['beyond-double']==2**53+1
    assert values['array-list']==values['shared-list'] and values['array-list'] is not values['shared-list']
    assert doc.image_properties is None and doc.image_analysis_settings is None
    reopened=q.Project.open(str(path)).document(project.images[0].id);assert signature(reopened.image_data_properties.values)==signature(wanted)
    assert doc.object_count==1 and all(p.read_bytes()==b for p,b in before.items())


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='loss review')
def test_unknown_or_cyclic_values_require_review_and_drop_whole_properties(tmp_path):
    source=foreign(tmp_path,'unsupported-values');preview=q.preview_qupath_project(str(source))
    assert {i.code for i in preview.issues}=={'image_property_dropped'} and sum(i.count for i in preview.issues)==4
    assert all(any("'"+key+"'" in i.message for i in preview.issues) for key in ['uuid','cycle','non-string-map','unknown-in-list'])
    output=tmp_path/'reviewed.qpxproj'
    with pytest.raises(ValueError,match='loss'):preview.import_new(str(output))
    assert not output.exists() and not preview.consumed
    project=preview.import_new(str(output),allow_losses=True);assert project.document(project.images[0].id).image_data_properties.values=={'retained':'safe'}


@pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='bounded import')
@pytest.mark.parametrize('label',['depth-limit','text-limit','count-limit'])
def test_excessive_saved_values_refuse_without_publication(tmp_path,label):
    source=foreign(tmp_path,label)
    with pytest.raises(ValueError):q.preview_qupath_project(str(source))
    assert not list(tmp_path.glob('*.qpxproj'))


def test_immutable_deep_copies_typed_noops_history_and_fact_separation():
    doc=q.Document(q.Image.demo());obj=doc.add_rectangle(q.Rect(1,2,8,9));doc.clear_history();selection=q.ObjectSelection(doc);selection.replace([obj])
    doc.set_image_properties(q.ObjectProperties({'text':'root fact'}));doc.set_image_analysis_settings(q.ImageAnalysisSettings('FLUORESCENCE'))
    request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]],[230,231,232]));analysis=q.analyze(request)
    before=doc.revision;roi_revision=doc.annotation(obj).revision;value=properties();doc.set_image_data_properties(value)
    assert doc.revision==before+1 and doc.undo_label=='Set ImageData properties' and doc.annotation(obj).revision==roi_revision and selection.ids==[obj]
    assert request.matches(doc,obj)
    after=q.analyze(request);assert after.input_sha256==analysis.input_sha256 and after.means==analysis.means and after.white==analysis.white
    doc.set_image_data_properties(properties());assert doc.revision==before+1
    changed=doc.image_data_properties.values;changed['typed'][3]=0;changed['nested']['score']=0
    assert doc.image_data_properties.values['typed'][3]==9007199254740993 and doc.image_data_properties.values['nested']['score']==3.5
    held=doc.image_data_properties;assert held is value
    doc.set_image_data_properties();assert doc.image_data_properties is None and doc.undo() and doc.image_data_properties is value
    assert doc.undo() and doc.image_data_properties is None and doc.redo()
    assert doc.image_properties.metadata=={'text':'root fact'} and doc.image_analysis_settings.image_type=='FLUORESCENCE'
    doc.set_image_data_properties(q.ImageDataProperties({'typed':True}));rev=doc.revision
    doc.set_image_data_properties(q.ImageDataProperties({'typed':1}));assert doc.revision==rev+1
    doc.set_image_data_properties(q.ImageDataProperties({'zero':-0.0}));rev=doc.revision
    doc.set_image_data_properties(q.ImageDataProperties({'zero':0.0}));assert doc.revision==rev+1 and doc.undo()
    assert math.copysign(1,doc.image_data_properties.values['zero'])==-1
    del doc,selection,value;gc.collect();assert held.values['nested']['score']==3.5


@pytest.mark.parametrize('values',[
    {1:'nonstring'}, {'object':object()}, {'tuple':(1,2)}, {'set':{1}}, {'bytes':b'x'},
    {'int':2**63}, {'int':-(2**63)-1}, {'bad\0key':1}, {'bad':'nul\0text'},
    {'large':'x'*4097}, {'x'*4097:1}, {'many':[None]*4097},
    {'node-limit':[[None]*4096 for _ in range(4)]},
    {str(i):'x'*4000 for i in range(256)},
])
def test_invalid_python_values_refuse_at_the_boundary(values):
    error=OverflowError if 'int' in values else (ValueError,TypeError)
    with pytest.raises(error):q.ImageDataProperties(values)


def test_python_cycles_depth_empty_null_and_history_budget():
    cycle=[];cycle.append(cycle)
    with pytest.raises(ValueError,match='cyclic'):q.ImageDataProperties({'cycle':cycle})
    deep=0
    for _ in range(15):deep=[deep]
    assert signature(q.ImageDataProperties({'deep':deep}).values)==signature({'deep':deep})
    with pytest.raises(ValueError,match='depth'):q.ImageDataProperties({'deep':[deep]})
    assert len(q.ImageDataProperties({'many':[None]*4096}).values['many'])==4096
    doc=q.Document(q.Image.demo());doc.set_image_data_properties(q.ImageDataProperties());assert doc.image_data_properties is None and not doc.can_undo
    doc.set_image_data_properties(q.ImageDataProperties({'null':None}));assert doc.image_data_properties.values=={'null':None}
    doc.clear_history();doc.set_history_limits(128,1);before=doc.revision
    with pytest.raises(ValueError,match='history'):doc.set_image_data_properties(properties())
    assert doc.revision==before and doc.image_data_properties.values=={'null':None}


@pytest.mark.skipif(not q.has_project,reason='snapshots')
def test_snapshot_ownership_payload_budget_and_python_worker_final_release(tmp_path):
    script='''import qupathxx as q, gc, sys
assert q.build_environment().git_revision==sys.argv[2]
doc=q.Document(q.Image(16,12,[20,40,60]*16*12))
doc.execute([q.ObjectEdit.create(q.Rect(0,0,1,1)) for _ in range(1000)])
doc.set_image_data_properties(q.ImageDataProperties({'values':[None,True,9007199254740993,{'text':'original'}]}))
project=q.Project();image=project.add_document(doc);job=q.ProjectSaveJob();assert job.request(project,sys.argv[1])
doc.set_image_data_properties();doc.clear_history();gc.collect();assert job.wait().applied
assert q.Project.open(sys.argv[1]).document(image).image_data_properties.values['values'][2]==9007199254740993
'''
    child=subprocess.run([sys.executable,'-I','-X','utf8','-c',script,str(tmp_path/'worker.qpxproj'),q.build_environment().git_revision],capture_output=True,text=True,encoding='utf-8',timeout=25)
    assert child.returncode==0,child.stdout+child.stderr
    project,image,doc,obj,path=workspace(tmp_path);doc.set_image_data_properties();empty=q.ProjectSaveJob();assert empty.request(project,str(tmp_path/'empty.qpxproj'));stats=empty.wait()
    doc.set_image_data_properties(q.ImageDataProperties({'large':'x'*4000}));job=q.ProjectSaveJob(snapshot_budget=stats.snapshot_bytes+100)
    with pytest.raises(ValueError,match='budget'):job.request(project,str(tmp_path/'refused.qpxproj'))
    assert not (tmp_path/'refused.qpxproj').exists()


@pytest.mark.skipif(not q.has_project,reason='migration')
def test_schema20_defaults_and_failed_upgrade_are_atomic(tmp_path):
    project,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:
        db.execute('DROP TABLE IF EXISTS image_tags');db.execute('DROP TABLE IF EXISTS image_details');db.execute('DROP TABLE image_metadata');db.execute('DROP TABLE image_axis_calibration');db.execute('DROP TABLE image_data_properties');db.execute('PRAGMA user_version=20');db.execute("CREATE TRIGGER fail_property BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'property fault'); END")
    legacy=q.Project.open(str(path));assert legacy.document(image).image_data_properties is None;legacy.document(image).set_image_data_properties(properties())
    with pytest.raises(RuntimeError,match='property fault'):legacy.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==20 and not db.execute("SELECT name FROM sqlite_master WHERE name='image_data_properties'").fetchall();db.execute('DROP TRIGGER fail_property')
    legacy.save(str(path));assert signature(q.Project.open(str(path)).document(image).image_data_properties.values)==signature(properties().values)
    with sqlite3.connect(path) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION


@pytest.mark.skipif(not q.has_project,reason='integrity')
@pytest.mark.parametrize('damage',['checksum','orphan','truncated','wrong-kind','duplicate','wrong-type','oversize'])
def test_corrupt_property_rows_refuse_before_publication(tmp_path,damage):
    project,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:
        payload=db.execute('SELECT payload FROM image_data_properties').fetchone()[0]
        if damage=='checksum':
            b=bytearray(payload);b[-1]^=1;db.execute('UPDATE image_data_properties SET payload=?',(b,))
        elif damage=='orphan':db.execute('INSERT INTO image_data_properties VALUES(?,?)',('999',payload))
        elif damage=='truncated':db.execute("UPDATE image_data_properties SET payload=x'51445001'")
        elif damage=='oversize':db.execute('UPDATE image_data_properties SET payload=zeroblob(2097153)')
        elif damage=='wrong-kind':
            doc.set_image_properties(q.ObjectProperties({'note':'not a data property'}));project.save(str(tmp_path/'facts.qpxproj'))
            with sqlite3.connect(tmp_path/'facts.qpxproj') as other:wrong=other.execute('SELECT payload FROM image_properties').fetchone()[0]
            db.execute('UPDATE image_data_properties SET payload=?',(wrong,))
        else:
            db.execute('DROP TABLE image_data_properties');db.execute('CREATE TABLE image_data_properties(image_id TEXT,payload)')
            db.execute('INSERT INTO image_data_properties VALUES(?,?)',(str(image),3 if damage=='wrong-type' else payload))
            if damage=='duplicate':db.execute('INSERT INTO image_data_properties VALUES(?,?)',(str(image),payload))
    with pytest.raises(ValueError):q.Project.open(str(path))


def property_payload(entries):
    # Independent format witness: ASCII header, little-endian counts/tags, ASCII SHA.
    body=b'QDP\x01'+struct.pack('<Q',len(entries))
    for key,value in entries:
        raw=key.encode('utf-8');body+=struct.pack('<Q',len(raw))+raw+value
    return body+hashlib.sha256(body).hexdigest().encode('ascii')


@pytest.mark.skipif(not q.has_project,reason='typed storage contract')
def test_independently_encoded_exact_integer_payload_is_readable(tmp_path):
    project,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:db.execute('UPDATE image_data_properties SET payload=?',(property_payload([('exact',b'\x02'+struct.pack('<q',2**53+1))]),))
    value=q.Project.open(str(path)).document(image).image_data_properties.values
    assert value=={'exact':9007199254740993} and type(value['exact']) is int


@pytest.mark.skipif(not q.has_project,reason='typed storage contract')
@pytest.mark.parametrize('damage',['tag','bool','duplicate-key','noncanonical-nan','count','trailing','key-order','depth'])
def test_rechecksummed_invalid_typed_payloads_are_rejected(tmp_path,damage):
    project,image,doc,obj,path=workspace(tmp_path);entries=[('key',b'\x00')]
    if damage=='tag':entries=[('key',b'\xff')]
    elif damage=='bool':entries=[('key',b'\x01\x02')]
    elif damage=='duplicate-key':entries*=2
    elif damage=='noncanonical-nan':entries=[('key',b'\x03'+struct.pack('<Q',0x7ff8000000000001))]
    elif damage=='count':entries=[('key',b'\x05'+struct.pack('<Q',2**64-1))]
    elif damage=='key-order':entries=[('z',b'\x00'),('a',b'\x00')]
    elif damage=='depth':
        value=b'\x00'
        for _ in range(17):value=b'\x05'+struct.pack('<Q',1)+value
        entries=[('key',value)]
    payload=property_payload(entries)
    if damage=='trailing':
        body=payload[:-64]+b'extra';payload=body+hashlib.sha256(body).hexdigest().encode('ascii')
    with sqlite3.connect(path) as db:db.execute('UPDATE image_data_properties SET payload=?',(payload,))
    with pytest.raises(ValueError):q.Project.open(str(path))


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in typed inspector')
def test_desktop_inspector_save_and_reopen_keep_typed_image_properties(tmp_path):
    from test_gui_replay import event
    project,image,doc,obj,path=workspace(tmp_path);doc.set_image_data_properties(q.ImageDataProperties({'Count':9007199254740993,'Flag':True,'Nested':{'Null':None,'Score':3.5},'Text':'Ω α'}))
    options=q.GuiOptions();options.width=1600;options.height=1400;options.docked_workspace=True;options.settle_tiles=True;options.autosave_interval_seconds=0
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(12,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=45,capture_path=str(tmp_path/'image-data-properties.ppm'),options=options)
    assert report.replay_complete and not project.dirty and doc.annotation(obj).geometry.area==72
    assert signature(q.Project.open(str(path)).document(image).image_data_properties.values)==signature(doc.image_data_properties.values)
