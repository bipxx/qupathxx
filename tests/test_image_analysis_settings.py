"""Saved defaults are owned state; explicit recipes remain authoritative."""
import gc
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3

import numpy as np
import pytest
import qupathxx as q
from project_schema_contract import CURRENT_SCHEMA_VERSION

FIXTURE=Path(__file__).parent/'fixtures/qupath-image-settings-0.7.0'
REFERENCE=json.loads((FIXTURE/'reference.json').read_bytes())


def foreign(tmp_path,name):
    root=tmp_path/'foreign';shutil.copytree(FIXTURE,root)
    path=root/'project/project.qpproj';data=json.loads(path.read_bytes());data['images']=[row for row in data['images'] if row['imageName']==name]
    assert len(data['images'])==1
    data['images'][0]['serverBuilder']['uri']=(root/'source.ome.tif').as_uri();path.write_text(json.dumps(data),encoding='utf-8');return path


def settings():return q.ImageAnalysisSettings('BRIGHTFIELD_OTHER',[[1,0,0],[0,1,0],[0,0,1]],[240,245,250],'Three',['R','G','B'])


def workspace(tmp_path):
    doc=q.Document(q.Image(16,12,[20,40,60]*16*12));obj=doc.add_rectangle(q.Rect(1,2,8,9));doc.set_image_analysis_settings(settings())
    project=q.Project();image=project.add_document(doc);path=tmp_path/'settings.qpxproj';project.save(str(path));return project,image,doc,obj,path


def signature(s):return None if s is None else (s.image_type,s.has_stains,s.profile_name,s.stain_names,s.basis,s.white,s.residual_stain,s.model_sha256)


def test_pinned_settings_capture_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in manifest['probes'].items():assert hashlib.sha256((Path(__file__).parents[1]/name).read_bytes()).hexdigest()==digest
    assert len(REFERENCE)==8 and manifest['upstream']['revision']=='04ccfa4fb7d43e9b566393e08e83690b72248d44'


@pytest.mark.skipif(not q.has_project or not q.has_tiff or not q.has_interchange,reason='native saved defaults')
@pytest.mark.parametrize('row',REFERENCE,ids=[r['name'] for r in REFERENCE])
def test_pinned_types_stains_names_white_and_residual_are_retained(tmp_path,row):
    source=foreign(tmp_path,row['name']);before={p:p.read_bytes() for p in source.parent.rglob('*') if p.is_file()}
    preview=q.preview_qupath_project(str(source));assert not any(i.code=='image_settings_dropped' for i in preview.issues)
    # QuPath records setting actions as workflow history; this remains explicit.
    assert {i.code for i in preview.issues}<={'workflow_dropped'}
    path=tmp_path/'native.qpxproj';project=preview.import_new(str(path),allow_losses=True);image=project.images[0].id
    value=project.document(image).image_analysis_settings;restored=q.Project.open(str(path)).document(image).image_analysis_settings
    assert signature(value)==signature(restored)
    if row['image_type']=='UNSET':assert value is None
    else:
        assert value.image_type==row['image_type'] and value.has_stains is (row['stains'] is not None)
        if row['stains'] is not None:
            expected=row['stains'];assert value.profile_name==expected['name'] and value.stain_names==expected['names']
            assert value.residual_stain is expected['residual'];np.testing.assert_allclose(value.basis,expected['basis'],rtol=0,atol=1e-14)
            assert list(value.white)==expected['white']
            if row['name']=='wide-white':
                assert not value.rgb8_supported and value.model_sha256==''
                with pytest.raises(ValueError,match='RGB8'):value.transform()
            else:assert value.rgb8_supported and value.model_sha256==q.stain_model_sha256(value.transform())
    assert project.document(image).object_count==1 and all(p.read_bytes()==data for p,data in before.items())


def test_value_lifetime_history_noop_and_explicit_request_precedence(tmp_path):
    doc=q.Document(q.Image(16,12,[20,40,60]*16*12));obj=doc.add_rectangle(q.Rect(1,2,8,9))
    request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]],[230,231,232]));expected=q.analyze(request)
    selection=q.ObjectSelection(doc);selection.replace([obj]);revision=doc.revision;object_revision=doc.annotation(obj).revision
    value=settings();doc.set_image_analysis_settings(value);assert doc.revision==revision+1 and doc.undo_label=='Set image analysis defaults'
    assert doc.annotation(obj).revision==object_revision and selection.ids==[obj] and request.matches(doc,obj)
    result=q.analyze(request);assert result.input_sha256==expected.input_sha256 and list(result.white)==[230,231,232]
    np.testing.assert_array_equal(result.means,expected.means)
    doc.set_image_analysis_settings(settings());assert doc.revision==revision+1
    assert doc.undo() and doc.image_analysis_settings is None and doc.redo()
    held=doc.image_analysis_settings;assert signature(held)==signature(value)
    doc.set_image_analysis_settings();assert doc.image_analysis_settings is None
    assert doc.undo() and signature(doc.image_analysis_settings)==signature(value)
    del doc,selection,value,request;gc.collect();assert held.profile_name=='Three' and held.transform().white==[240,245,250]


@pytest.mark.skipif(not q.has_project or not q.has_tiff or not q.has_interchange,reason='native saved defaults')
@pytest.mark.parametrize('old,new',[
    (b'"Stain 3"',b'"Stain 9"'),
    (b'"Values 2"',b'"Values 1"'),
    (b'"1 0 0"',b'"0 0 0"'),
    (b'" 240 245 250"',b'"   0 245 250"'),
    (b'xpw\x04\x00\x00\x00\x01t',b'xpw\x04\x00\x00\x00\x02t'),
],ids=['unknown-field','duplicate-field','singular-basis','zero-white','unknown-version'])
def test_malformed_saved_defaults_refuse_without_publication(tmp_path,old,new):
    source=foreign(tmp_path,'custom-three');saved=source.parent/'data/7/data.qpdata'
    data=saved.read_bytes();assert old in data and len(old)==len(new)
    saved.write_bytes(data.replace(old,new,1));before=saved.read_bytes()
    with pytest.raises(ValueError):q.preview_qupath_project(str(source))
    assert saved.read_bytes()==before and not list(tmp_path.glob('*.qpxproj'))


@pytest.mark.parametrize('kwargs',[
    {'image_type':'unknown'}, {'image_type':'FLUORESCENCE','stains':[[1,0,0],[0,1,0]]},
    {'image_type':'BRIGHTFIELD_H_E','stains':[[1,0,0]]}, {'image_type':'BRIGHTFIELD_H_E','stains':[[1,0,0],[1,0,0]]},
    {'image_type':'BRIGHTFIELD_H_E','stains':[[1,0,0],[0,1,0]],'white':[0,255,255]},
    {'image_type':'BRIGHTFIELD_H_E','stains':[[1,0,0],[0,1,0]],'white':[float('nan'),255,255]},
    {'image_type':'OTHER','profile_name':'missing vectors'},
    {'image_type':'BRIGHTFIELD_H_E','stains':[[1,0,0],[0,1,0]],'stain_names':['only one']},
    {'image_type':'BRIGHTFIELD_H_E','stains':[[1,0,0],[0,1,0]],'profile_name':'x'*4097},
])
def test_invalid_settings_refuse_at_the_native_boundary(kwargs):
    with pytest.raises((ValueError,TypeError)):q.ImageAnalysisSettings(**kwargs)


def test_unset_empty_and_history_admission():
    doc=q.Document(q.Image.demo());doc.set_image_analysis_settings(q.ImageAnalysisSettings());assert doc.image_analysis_settings is None and not doc.can_undo
    doc.set_history_limits(128,1);before=doc.revision
    with pytest.raises(ValueError,match='history'):doc.set_image_analysis_settings(settings())
    assert doc.revision==before and doc.image_analysis_settings is None


@pytest.mark.skipif(not q.has_project,reason='snapshot/migration')
def test_snapshot_owned_values_and_legacy19_atomic_upgrade(tmp_path):
    project,image,doc,obj,path=workspace(tmp_path);before=signature(doc.image_analysis_settings)
    job=q.ProjectSaveJob();assert job.request(project,str(path));doc.set_image_analysis_settings(q.ImageAnalysisSettings('FLUORESCENCE'));doc.clear_history();assert job.wait().applied
    assert signature(q.Project.open(str(path)).document(image).image_analysis_settings)==before and project.dirty
    with sqlite3.connect(path) as db:
        db.execute('DROP TABLE IF EXISTS image_data_properties');db.execute('DROP TABLE image_analysis_settings');db.execute('PRAGMA user_version=19');db.execute("CREATE TRIGGER fail_settings BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'settings fault'); END")
    legacy=q.Project.open(str(path));assert legacy.document(image).image_analysis_settings is None
    legacy.document(image).set_image_analysis_settings(settings())
    with pytest.raises(RuntimeError,match='settings fault'):legacy.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==19
        assert not db.execute("SELECT name FROM sqlite_master WHERE name='image_analysis_settings'").fetchall();db.execute('DROP TRIGGER fail_settings')
    legacy.save(str(path));assert signature(q.Project.open(str(path)).document(image).image_analysis_settings)==before
    with sqlite3.connect(path) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION


@pytest.mark.skipif(not q.has_project,reason='integrity')
@pytest.mark.parametrize('damage',['checksum','orphan','wrong-kind'])
def test_bad_settings_payload_is_rejected(tmp_path,damage):
    project,image,doc,obj,path=workspace(tmp_path)
    with sqlite3.connect(path) as db:
        payload=db.execute('SELECT payload FROM image_analysis_settings').fetchone()[0]
        if damage=='checksum':
            changed=bytearray(payload);changed[-1]^=1;db.execute('UPDATE image_analysis_settings SET payload=?',(changed,))
        elif damage=='orphan':db.execute('INSERT INTO image_analysis_settings VALUES(?,?)',('999',payload))
        else:
            doc.set_image_properties(q.ObjectProperties({'note':'not settings'}));project.save(str(tmp_path/'facts.qpxproj'))
            with sqlite3.connect(tmp_path/'facts.qpxproj') as other:wrong=other.execute('SELECT payload FROM image_properties').fetchone()[0]
            db.execute('UPDATE image_analysis_settings SET payload=?',(wrong,))
    with pytest.raises(ValueError):q.Project.open(str(path))


@pytest.mark.skipif(not q.has_gui or not q.has_project or not q.has_tiff or not q.has_interchange or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in saved defaults UI')
@pytest.mark.parametrize('apply',[False,True])
def test_desktop_defaults_are_explicit_and_use_the_full_saved_basis(tmp_path,apply):
    from test_gui_replay import event
    source=foreign(tmp_path,'custom-three');preview=q.preview_qupath_project(str(source));path=tmp_path/'ui.qpxproj';project=preview.import_new(str(path),allow_losses=True)
    doc=project.document(project.images[0].id);saved=doc.image_analysis_settings
    options=q.GuiOptions();options.width=1600;options.height=1400;options.docked_workspace=True;options.settle_tiles=True;options.autosave_interval_seconds=0;options.analysis_white=[230,231,232]
    events=[event(4,q.GuiInputKind.SELECT_ALL_KEY)]
    if apply:events += [event(7,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.USE_IMAGE_STAINS,q.GuiInputWait.TARGET_READY),event(8,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.USE_IMAGE_STAINS)]
    events += [event(12,q.GuiInputKind.ANALYZE_KEY),event(30,q.GuiInputKind.KEEP_ANALYSIS_KEY),event(48,q.GuiInputKind.SAVE_KEY)];options.inputs=events
    report=q.show(project,frame_limit=75,capture_path=str(tmp_path/('settings-'+str(apply)+'.ppm')),options=options)
    assert report.replay_complete and report.analysis_state=='ready' and report.analysis_records==1
    result=doc.latest_analysis(doc.annotations[0].id).result
    expected_white=saved.white if apply else [230,231,232]
    assert list(result.white)==list(expected_white)
    assert result.kind==(q.AnalysisKind.STAIN_MEANS if apply else q.AnalysisKind.OPTICAL_DENSITY_MEANS)
    np.testing.assert_array_equal(report.analysis_means,result.means)
    if apply:np.testing.assert_array_equal(result.stains,saved.basis)
    reopened=q.Project.open(str(path)).document(project.images[0].id)
    assert signature(reopened.image_analysis_settings)==signature(saved)
    retained=reopened.latest_analysis(reopened.annotations[0].id).result
    assert list(retained.white)==list(result.white) and retained.kind==result.kind
    np.testing.assert_array_equal(retained.stains,result.stains)
