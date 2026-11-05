"""Project-entry string maps are owned project state, not image measurements."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3

import pytest
import qupathxx as q
from project_schema_contract import CURRENT_SCHEMA_VERSION

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-entry-metadata-0.7.0'
MANIFEST=json.loads((FIXTURE/'manifest.json').read_bytes())
REFERENCE={r['name']:r for r in json.loads((FIXTURE/'reference.json').read_bytes())}
RICH={'patient':'Synthetic α 🚀','batch':'A','empty':'','':'empty key','multiline':'line1\nline2\t"quoted"','integer':'9223372036854775807','null-word':'null','é':'composed','e\u0301':'decomposed'}
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_tiff or not q.has_interchange,reason='project/TIFF/interchange')


def foreign(tmp_path,label=None,exchange=None):
    root=tmp_path/'foreign';shutil.copytree(FIXTURE,root)
    shutil.copyfile(ROOT/'tests/fixtures'/MANIFEST['parent']['fixture']/MANIFEST['parent']['source'],root/'source.ome.tif')
    path=root/'project/project.qpproj';data=json.loads(path.read_bytes())
    if label is not None:data['images']=[i for i in data['images'] if i['imageName']==label]
    for image in data['images']:image['serverBuilder']['uri']=(root/'source.ome.tif').as_uri()
    path.write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
    if exchange:
        # Metadata is manifest authority for either bundle; there is no qpdata.
        bundle=path.parent/'qupathxx-exchange';bundle.mkdir()
        files={name:(hashlib.sha256((path.parent/name).read_bytes()).hexdigest() if (path.parent/name).exists() else None) for name in ['classifiers/classes.json','data/metadata.json']}
        index={'schema':'qpx.qupath-project-exchange/'+exchange,'qupath_version':'0.7.0','project_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'files':files,'images':[]}
        (bundle/'index.json').write_text(json.dumps(index),encoding='utf-8')
    return path


def workspace(tmp_path):
    source=tmp_path/'source.ome.tif';shutil.copyfile(ROOT/'tests/fixtures'/MANIFEST['parent']['fixture']/MANIFEST['parent']['source'],source)
    doc=q.Document(q.open_tiff(str(source)));project=q.Project();image=project.add_document(doc,'Entry');return project,image,doc,source


def test_capture_parent_probe_and_independent_metadata_values():
    for name,sha in MANIFEST['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==sha
    for name,sha in MANIFEST['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha
    parent=ROOT/'tests/fixtures'/MANIFEST['parent']['fixture'];assert hashlib.sha256((parent/'manifest.json').read_bytes()).hexdigest()==MANIFEST['parent']['manifest_sha256']
    assert REFERENCE['metadata α']['metadata']==RICH and REFERENCE['empty']['metadata']=={}
    assert REFERENCE['with-losses']['metadata']==RICH and REFERENCE['with-losses']['tags']==['research-tag']


@pytest.mark.parametrize('exchange',[None,'1','2'],ids=['manifest','v1','v2'])
def test_manifest_metadata_namespaces_and_save_reopen(tmp_path,exchange):
    path=foreign(tmp_path,exchange=exchange);preview=q.preview_qupath_project(str(path))
    assert len(preview.issues)==2 and all(i.code=='image_metadata_dropped' and any(k in i.message for k in ['tags','description']) for i in preview.issues)
    target=tmp_path/'import.qpxproj'
    with pytest.raises(ValueError,match='loss'):preview.import_new(str(target))
    assert not target.exists() and not preview.consumed
    project=preview.import_new(str(target),allow_losses=True);reopened=q.Project.open(str(target))
    for info in preview.images:
        expected=REFERENCE[info.name]['metadata'];assert project.image_metadata(info.native_id)==expected
        assert next(i for i in project.images if i.id==info.native_id).metadata==expected
        assert reopened.image_metadata(info.native_id)==expected
        doc=reopened.document(info.native_id);assert doc.object_count==0 and doc.image_properties is None and doc.image_data_properties is None
    with sqlite3.connect(target) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION
        assert db.execute('SELECT count(*) FROM image_metadata').fetchone()[0]==18


def test_getters_listings_noop_and_replacement_are_owning_and_project_only(tmp_path):
    project,image,doc,source=workspace(tmp_path);path=tmp_path/'saved.qpxproj';project.save(str(path));revision=doc.revision
    supplied=RICH.copy();project.set_image_metadata(image,supplied);supplied.clear();assert project.image_metadata(image)==RICH and project.dirty and doc.revision==revision
    held=project.images[0];copied=held.metadata;copied.clear();returned=project.image_metadata(image);returned.clear();assert project.image_metadata(image)==RICH
    project.save(str(path));project.set_image_metadata(image,dict(reversed(list(RICH.items()))));assert not project.dirty
    project.set_image_metadata(image,{'next':'value'});assert held.metadata==RICH and project.image_metadata(image)=={'next':'value'}
    project.set_image_metadata(image);assert project.image_metadata(image)=={} and held.metadata==RICH
    for call in [lambda:project.image_metadata(999),lambda:project.set_image_metadata(999,{})]:
        with pytest.raises(IndexError):call()


@pytest.mark.parametrize('values',[{'bad\0key':'value'},{'key':'bad\0value'},{'x'*4097:'value'},{'key':'x'*4097},{str(i):'x' for i in range(4097)},{str(i):'x'*4096 for i in range(260)},{'key':1},{1:'value'},{'key':None}])
def test_invalid_or_over_budget_metadata_is_atomic(tmp_path,values):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,RICH);path=tmp_path/'clean.qpxproj';project.save(str(path));before=path.read_bytes()
    with pytest.raises((ValueError,TypeError,RuntimeError)):project.set_image_metadata(image,values)
    assert project.image_metadata(image)==RICH and not project.dirty and path.read_bytes()==before


def test_snapshot_owns_map_and_metadata_counts_toward_budget(tmp_path):
    project,image,doc,source=workspace(tmp_path);path=tmp_path/'saved.qpxproj'
    # A clean linked snapshot fits; adding only metadata must cross this budget.
    small=q.ProjectSaveJob(100000);assert small.request(project,str(path));assert small.wait().applied
    project.set_image_metadata(image,RICH);job=q.ProjectSaveJob();assert job.request(project,str(path));project.set_image_metadata(image,{'later':'edit'});assert job.wait().applied
    assert q.Project.open(str(path)).image_metadata(image)==RICH and project.dirty
    before=path.read_bytes();project.set_image_metadata(image,{str(i):'x'*3000 for i in range(200)})
    with pytest.raises((ValueError,RuntimeError),match='budget'):q.ProjectSaveJob(100000).request(project,str(path))
    assert path.read_bytes()==before and project.dirty


def test_schema22_default_and_failed_upgrade_preserve_original(tmp_path):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,RICH);path=tmp_path/'legacy.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:db.execute('DROP TABLE image_metadata');db.execute('PRAGMA user_version=22')
    legacy=q.Project.open(str(path));assert legacy.image_metadata(image)=={}
    legacy.set_image_metadata(image,RICH)
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_metadata BEFORE INSERT ON images BEGIN SELECT RAISE(ABORT,'metadata fault'); END")
    before=path.read_bytes()
    with pytest.raises(RuntimeError,match='metadata fault'):legacy.save(str(path))
    assert path.read_bytes()==before and legacy.dirty
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==22
        assert not db.execute("SELECT name FROM sqlite_master WHERE name='image_metadata'").fetchall();db.execute('DROP TRIGGER fail_metadata')
    legacy.save(str(path));assert q.Project.open(str(path)).image_metadata(image)==RICH


@pytest.mark.parametrize('damage',['key-nul','value-nul','key-size','value-size','invalid-utf8','wrong-type','duplicate','orphan','count','payload'])
def test_invalid_persisted_rows_are_refused(tmp_path,damage):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,{'key':'value'});path=tmp_path/'bad.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        if damage=='key-nul':db.execute('UPDATE image_metadata SET metadata_key=?',('bad\0key',))
        elif damage=='value-nul':db.execute('UPDATE image_metadata SET metadata_value=?',('bad\0value',))
        elif damage=='key-size':db.execute('UPDATE image_metadata SET metadata_key=?',('x'*4097,))
        elif damage=='value-size':db.execute('UPDATE image_metadata SET metadata_value=?',('x'*4097,))
        elif damage=='invalid-utf8':db.execute("UPDATE image_metadata SET metadata_value=CAST(x'ff' AS TEXT)")
        elif damage=='orphan':db.execute('INSERT INTO image_metadata VALUES(?,?,?)',('999','key','value'))
        elif damage in ['wrong-type','duplicate']:
            db.execute('DROP TABLE image_metadata');db.execute('CREATE TABLE image_metadata(image_id TEXT,metadata_key,metadata_value)');db.execute('INSERT INTO image_metadata VALUES(?,?,?)',(str(image),'key',7 if damage=='wrong-type' else 'value'))
            if damage=='duplicate':db.execute('INSERT INTO image_metadata VALUES(?,?,?)',(str(image),'key','second'))
        else:
            db.execute('DELETE FROM image_metadata');rows=4097 if damage=='count' else 260;value='x' if damage=='count' else 'x'*4096
            db.executemany('INSERT INTO image_metadata VALUES(?,?,?)',[(str(image),str(i),value) for i in range(rows)])
    with pytest.raises((ValueError,RuntimeError)):q.Project.open(str(path))


@pytest.mark.parametrize('bad',[[],{'key':1},{'key':None},{'bad\0key':'value'}])
def test_malformed_foreign_map_refuses_before_publication(tmp_path,bad):
    path=foreign(tmp_path,'metadata α');data=json.loads(path.read_bytes());data['images'][0]['metadata']=bad;path.write_text(json.dumps(data),encoding='utf-8')
    with pytest.raises(ValueError):q.preview_qupath_project(str(path))
    assert not list(tmp_path.glob('*.qpxproj'))


def test_reviewed_manifest_change_and_existing_destination_preserve_state(tmp_path):
    path=foreign(tmp_path,'metadata α');preview=q.preview_qupath_project(str(path));assert not preview.issues
    target=tmp_path/'existing.qpxproj';target.write_bytes(b'keep existing destination')
    with pytest.raises((ValueError,RuntimeError)):preview.import_new(str(target))
    assert target.read_bytes()==b'keep existing destination' and not preview.consumed
    data=json.loads(path.read_bytes());data['images'][0]['metadata']['patient']='changed';path.write_text(json.dumps(data),encoding='utf-8')
    with pytest.raises(RuntimeError,match='changed'):preview.import_new(str(tmp_path/'changed.qpxproj'))
    assert not preview.consumed and not (tmp_path/'changed.qpxproj').exists()


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in project-entry metadata desktop')
def test_desktop_inspects_entry_metadata_and_saves(tmp_path):
    from test_gui_replay import event
    path=foreign(tmp_path,'metadata α');preview=q.preview_qupath_project(str(path));target=tmp_path/'desktop.qpxproj';project=preview.import_new(str(target));image=preview.images[0].native_id
    changed=RICH|{'batch':'desktop-saved'};project.set_image_metadata(image,changed)
    options=q.GuiOptions();options.width=1500;options.height=1400;options.project_image_path=str(path.parent.parent/'source.ome.tif');options.inputs=[event(20,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=45,capture_path=str(tmp_path/'entry-metadata.ppm'),options=options)
    assert report.replay_complete and project.image_metadata(image)==changed
    assert q.Project.open(str(target)).image_metadata(image)==changed


@pytest.mark.parametrize('values',[{'µ'*2048:'ok'},{'key':'µ'*2048},{str(i):'x' for i in range(4096)}])
def test_documented_text_and_entry_bounds_are_admitted(tmp_path,values):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,values);path=tmp_path/'bounds.qpxproj';project.save(str(path))
    assert q.Project.open(str(path)).image_metadata(image)==values


def test_metadata_survives_missing_source_without_opening_reader(tmp_path):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,RICH);path=tmp_path/'missing.qpxproj';project.save(str(path));doc.server.close();source.unlink()
    reopened=q.Project.open(str(path));assert reopened.image_metadata(image)==RICH and reopened.images[0].metadata==RICH
