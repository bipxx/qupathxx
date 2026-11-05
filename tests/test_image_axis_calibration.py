"""Saved physical axes are owning document state, with independent wire witnesses."""
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import struct

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_axis_calibration_capture import FIXTURE, EXPECTED
from project_schema_contract import CURRENT_SCHEMA_VERSION

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project or not q.has_interchange,reason='TIFF/project/interchange')


def foreign(tmp_path,label,assisted=False):
    root=tmp_path/'foreign';shutil.copytree(FIXTURE,root);path=root/'project/project.qpproj'
    data=json.loads(path.read_bytes());data['images']=[i for i in data['images'] if i['imageName']==label]
    assert len(data['images'])==1;entry=str(data['images'][0]['entryID'])
    data['images'][0]['serverBuilder']['uri']=(root/'source.ome.tif').as_uri()
    path.write_text(json.dumps(data),encoding='utf-8')
    if assisted:
        # Synthetic consumer contract variant; the qpdata is the exact capture.
        bundle=path.parent/'qupathxx-exchange';bundle.mkdir()
        index={'schema':'qpx.qupath-project-exchange/2','qupath_version':'0.7.0','project_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'files':{'classifiers/classes.json':None,'data/metadata.json':None},'images':[{'entry_id':entry,'representation':'native-saved-data','qpdata_sha256':hashlib.sha256((path.parent/'data'/entry/'data.qpdata').read_bytes()).hexdigest()}]}
        (bundle/'index.json').write_text(json.dumps(index),encoding='utf-8')
    return path


def assert_axes(doc,z,times):
    value=doc.image_axis_calibration;assert value is not None
    assert value.z_spacing_um==z and value.timepoints_seconds==times
    assert doc.z_spacing_um==z and doc.z_position_um(2)==(None if z is None else z*2)
    assert [doc.timepoint_seconds(t) for t in range(4)]==times+[None]*(4-len(times))
    assert doc.server.raw_metadata.z_spacing_um==1.75 and doc.server.raw_metadata.time_interval_seconds==.25


@pytest.mark.parametrize('label',list(EXPECTED))
@pytest.mark.parametrize('assisted',[False,True],ids=['direct','v2-native'])
def test_saved_calibration_coordinates_and_reopen(tmp_path,label,assisted):
    path=foreign(tmp_path,label,assisted);preview=q.preview_qupath_project(str(path));assert preview.issues==[]
    target=tmp_path/'saved.qpxproj';project=preview.import_new(str(target));info=preview.images[0];doc=project.document(info.native_id)
    z,times=EXPECTED[label];assert_axes(doc,z,times);assert not doc.can_undo
    assert (doc.annotations[0].plane.z,doc.annotations[0].plane.t)==(2,3)
    assert_axes(q.Project.open(str(target)).document(info.native_id),z,times)
    with sqlite3.connect(target) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION


def workspace(tmp_path):
    source=tmp_path/'source.ome.tif';shutil.copyfile(FIXTURE/'source.ome.tif',source)
    doc=q.Document(q.open_tiff(str(source)));project=q.Project();image=project.add_document(doc,'Axes');return project,image,doc,source


def test_owning_copies_history_unknown_source_fallback_and_budget(tmp_path):
    project,image,doc,source=workspace(tmp_path)
    assert doc.image_axis_calibration is None and doc.z_position_um(2)==3.5 and doc.timepoint_seconds(3)==.75
    value=q.ImageAxisCalibration(4.5,[-30,0,45,120]);doc.set_image_axis_calibration(value);held=doc.image_axis_calibration
    copied=held.timepoints_seconds;copied.clear();assert_axes(doc,4.5,[-30,0,45,120])
    revision=doc.revision;doc.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[-30,0,45,120]));assert doc.revision==revision
    doc.set_image_axis_calibration(q.ImageAxisCalibration());assert_axes(doc,None,[])
    assert doc.undo();assert_axes(doc,4.5,[-30,0,45,120]);assert doc.redo();assert_axes(doc,None,[])
    doc.set_image_axis_calibration();assert doc.image_axis_calibration is None and doc.timepoint_seconds(3)==.75
    assert held.timepoints_seconds==[-30,0,45,120]
    doc.set_history_limits(128,1);before=doc.revision
    with pytest.raises((ValueError,RuntimeError),match='budget'):doc.set_image_axis_calibration(value)
    assert doc.revision==before and doc.image_axis_calibration is None
    with pytest.raises(ValueError,match='time domain'):doc.set_image_axis_calibration(q.ImageAxisCalibration(None,[0]*5))
    for z,t in [(3,0),(0,4)]:
        with pytest.raises(IndexError):doc.z_position_um(z) if z else doc.timepoint_seconds(t)


@pytest.mark.parametrize('z,times',[(0,[]),(-1,[]),(math.nan,[]),(math.inf,[]),(1,[math.nan]),(1,[math.inf]),(1,[-math.inf]),(None,[0]*65537)])
def test_invalid_native_values_are_rejected(z,times):
    with pytest.raises(ValueError):q.ImageAxisCalibration(z,times)


def test_save_snapshot_owns_values_and_schema21_upgrade_is_atomic(tmp_path):
    project,image,doc,source=workspace(tmp_path);path=tmp_path/'saved.qpxproj'
    doc.add_rectangle(q.Rect(1,2,3,4));doc.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[1,2,3,4]))
    job=q.ProjectSaveJob();assert job.request(project,str(path));doc.set_image_axis_calibration(q.ImageAxisCalibration());assert job.wait().applied
    assert_axes(q.Project.open(str(path)).document(image),4.5,[1,2,3,4])
    with sqlite3.connect(path) as db:
        db.execute('DROP TABLE IF EXISTS project_metadata');db.execute('DROP TABLE IF EXISTS image_tags');db.execute('DROP TABLE IF EXISTS image_details');db.execute('DROP TABLE image_metadata');db.execute('DROP TABLE image_axis_calibration');db.execute('PRAGMA user_version=21');db.execute("CREATE TRIGGER fail_axis BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'axis fault'); END")
    legacy=q.Project.open(str(path));restored=legacy.document(image);assert restored.image_axis_calibration is None
    restored.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[1,2,3,4]))
    with pytest.raises(RuntimeError,match='axis fault'):legacy.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==21
        assert not db.execute("SELECT name FROM sqlite_master WHERE name='image_axis_calibration'").fetchall();db.execute('DROP TRIGGER fail_axis')
    legacy.save(str(path));assert_axes(q.Project.open(str(path)).document(image),4.5,[1,2,3,4])


def payload(z,times,flags=None,count=None,extra=b''):
    body=b'QAC\x01'+bytes([int(z is not None) if flags is None else flags])
    if z is not None:body+=struct.pack('<d',z)
    body+=struct.pack('<Q',len(times) if count is None else count)+b''.join(struct.pack('<d',t) for t in times)+extra
    return body+hashlib.sha256(body).hexdigest().encode()


@pytest.mark.parametrize('damage',['checksum','truncated','flags','nan','negative-z','infinite-time','negative-zero','count','trailing','orphan','wrong-type','duplicate'])
def test_corrupt_or_noncanonical_store_is_refused(tmp_path,damage):
    project,image,doc,source=workspace(tmp_path);doc.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[0,1,2,3]));path=tmp_path/'saved.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        value=payload(4.5,[0,1,2,3])
        if damage=='checksum':value=value[:-1]+bytes([value[-1]^1])
        elif damage=='truncated':value=b'QAC\x01'
        elif damage=='flags':value=payload(None,[],flags=2)
        elif damage=='nan':value=payload(math.nan,[])
        elif damage=='negative-z':value=payload(-1,[])
        elif damage=='infinite-time':value=payload(None,[math.inf])
        elif damage=='negative-zero':value=payload(None,[-0.0])
        elif damage=='count':value=payload(None,[],count=65537)
        elif damage=='trailing':value=payload(None,[],extra=b'!')
        elif damage=='orphan':db.execute('INSERT INTO image_axis_calibration VALUES(?,?)',('999',value))
        elif damage in ['wrong-type','duplicate']:
            db.execute('DROP TABLE image_axis_calibration');db.execute('CREATE TABLE image_axis_calibration(image_id TEXT,payload)');db.execute('INSERT INTO image_axis_calibration VALUES(?,?)',(str(image),3 if damage=='wrong-type' else value))
            if damage=='duplicate':db.execute('INSERT INTO image_axis_calibration VALUES(?,?)',(str(image),value))
        if damage not in ['orphan','wrong-type','duplicate']:db.execute('UPDATE image_axis_calibration SET payload=?',(value,))
    with pytest.raises(ValueError):q.Project.open(str(path))


def test_source_reduction_checks_retained_undo_and_redo_states(tmp_path):
    project,image,doc,source=workspace(tmp_path);doc.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[0,1,2,3]));doc.set_image_axis_calibration()
    smaller=tmp_path/'smaller.ome.tif';tifffile.imwrite(smaller,np.zeros((2,3,16,20),dtype='uint16'),ome=True,photometric='minisblack',metadata={'axes':'TZYX'})
    project.save(str(tmp_path/'before-relink.qpxproj'));old=doc.server;revision=doc.revision
    with pytest.raises(ValueError,match='retained calibration'):project.relink(image,str(smaller))
    assert doc.server is old and doc.revision==revision and not project.dirty
    assert doc.undo() and doc.undo();assert doc.image_axis_calibration is None
    with pytest.raises(ValueError,match='retained calibration'):project.relink(image,str(smaller))
    doc.clear_history();project.relink(image,str(smaller));assert doc.server.raw_metadata.timepoints==2


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in saved axes desktop')
def test_desktop_inspects_saved_physical_axes_and_saves(tmp_path):
    from test_gui_raw import options
    from test_gui_replay import event
    path=foreign(tmp_path,'minutes-irregular');preview=q.preview_qupath_project(str(path));target=tmp_path/'saved.qpxproj';project=preview.import_new(str(target))
    doc=project.document(preview.images[0].native_id);view=doc.server.tiff_view;view.z=2;view.t=3;doc.set_tiff_view(view)
    o=options();o.inputs=[event(25,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=45,capture_path=str(tmp_path/'saved-axes.ppm'),options=o)
    assert report.replay_complete and not report.raw_error
    assert_axes(q.Project.open(str(target)).document(preview.images[0].native_id),4.5,[-30,0,45,120])


def test_axis_override_does_not_rewrite_retained_acquisition_records(tmp_path):
    project,image,doc,source=workspace(tmp_path)
    region=q.RawRegionRequest();region.region=q.RegionRequest(0,0,0,2,2);region.channels=1;region.z=2;region.t=3
    capture=q.RawAnalysisRequest(doc,region,tile_size=16);result=q.analyze_raw(capture)
    record=doc.keep_raw_analysis(capture,result);before=result.input_sha256
    doc.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[-30,0,45,120]))
    assert result.input_sha256==before and result.metadata.z_spacing_um==1.75 and result.metadata.time_interval_seconds==.25
    path=tmp_path/'records.qpxproj';project.save(str(path));restored=q.Project.open(str(path)).document(image)
    assert restored.raw_analysis_record_count==1
    assert_axes(restored,4.5,[-30,0,45,120])


def test_native_value_bound_and_independent_valid_wire_payload(tmp_path):
    values=[float(i) for i in range(65536)];v=q.ImageAxisCalibration(None,values)
    values.clear();assert len(v.timepoints_seconds)==65536 and v.timepoint_seconds(65535)==65535 and v.timepoint_seconds(65536) is None
    project,image,doc,source=workspace(tmp_path);doc.set_image_axis_calibration(q.ImageAxisCalibration());path=tmp_path/'wire.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:db.execute('UPDATE image_axis_calibration SET payload=?',(payload(4.5,[-30,0,45,120]),))
    assert_axes(q.Project.open(str(path)).document(image),4.5,[-30,0,45,120])


@pytest.mark.parametrize('case',['missing-time-unit','unknown-time-unit','time-unit-type','time-array-type','time-element-type','time-overflow','too-many-source-times','unknown-z-unit','zero-z'])
def test_malformed_saved_calibration_is_refused_without_publication(tmp_path,case):
    path=foreign(tmp_path,'nanoseconds')
    data=path.parent/'data/1/data.qpdata';raw=data.read_bytes();assert raw[27]==0x74
    size=struct.unpack('>H',raw[28:30])[0];header=json.loads(raw[30:30+size].decode());cal=header['server']['metadata']['pixelCalibration']
    if case=='missing-time-unit':del cal['timeUnit']
    elif case=='unknown-time-unit':cal['timeUnit']='FORTNIGHTS'
    elif case=='time-unit-type':cal['timeUnit']=1
    elif case=='time-array-type':cal['timepoints']={}
    elif case=='time-element-type':cal['timepoints']=['NaN']
    elif case=='time-overflow':cal['timeUnit']='DAYS';cal['timepoints']=[1e308]
    elif case=='too-many-source-times':cal['timepoints']=[0]*5
    elif case=='unknown-z-unit':cal['zSpacing']['unit']='mm'
    else:cal['zSpacing']['value']=0
    encoded=json.dumps(header).encode();assert len(encoded)<65536
    data.write_bytes(raw[:28]+struct.pack('>H',len(encoded))+encoded+raw[30+size:])
    with pytest.raises(ValueError):q.preview_qupath_project(str(path))
    assert not list(tmp_path.glob('*.qpxproj'))


def test_effective_coordinate_overflow_refuses_without_state_change(tmp_path):
    project,image,doc,source=workspace(tmp_path);doc.set_image_axis_calibration(q.ImageAxisCalibration(1e308))
    before=doc.revision
    with pytest.raises(OverflowError):doc.z_position_um(2)
    assert doc.revision==before and doc.z_spacing_um==1e308
    huge=tmp_path/'huge-time.ome.tif'
    tifffile.imwrite(huge,np.zeros((4,16,20),dtype='uint8'),ome=True,photometric='minisblack',metadata={'axes':'TYX','TimeIncrement':1e308,'TimeIncrementUnit':'s'})
    timed=q.Document(q.open_tiff(str(huge)));assert timed.server.raw_metadata.time_interval_seconds==1e308
    with pytest.raises(OverflowError):timed.timepoint_seconds(3)


def test_missing_source_keeps_saved_calibration_inspectable(tmp_path):
    project,image,doc,source=workspace(tmp_path);doc.set_image_axis_calibration(q.ImageAxisCalibration(4.5,[-30,0,45,120]))
    path=tmp_path/'missing.qpxproj';project.save(str(path));doc.server.close();source.unlink()
    restored=q.Project.open(str(path)).document(image);value=restored.image_axis_calibration
    assert value.z_spacing_um==4.5 and value.timepoints_seconds==[-30,0,45,120]
    assert restored.z_spacing_um==4.5
