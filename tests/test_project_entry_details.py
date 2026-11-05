"""Nullable descriptions and ordered unique tags use the project entry authority."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3

import pytest
import qupathxx as q
from project_schema_contract import CURRENT_SCHEMA_VERSION
from test_project_image_metadata import workspace,RICH

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-entry-details-0.7.0'
MANIFEST=json.loads((FIXTURE/'manifest.json').read_bytes())
REFERENCE={r['name']:r for r in json.loads((FIXTURE/'reference.json').read_bytes())}
DESCRIPTION='Description α 🚀\nSecond line\t"quoted"'
TAGS=['z-last','a-first','','µ','α']
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_tiff or not q.has_interchange,reason='project/TIFF/interchange')


def foreign(tmp_path):
    root=tmp_path/'foreign';shutil.copytree(FIXTURE,root)
    parent=json.loads((ROOT/'tests/fixtures'/MANIFEST['parent']['fixture']/'manifest.json').read_bytes())
    source=ROOT/'tests/fixtures'/parent['parent']['fixture']/parent['parent']['source'];shutil.copyfile(source,root/'source.ome.tif')
    path=root/'project/project.qpproj';data=json.loads(path.read_bytes())
    for image in data['images']:image['serverBuilder']['uri']=(root/'source.ome.tif').as_uri()
    path.write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8');return path


def assert_details(project,image,description,tags):
    assert project.image_description(image)==description and project.image_tags(image)==tags
    info=next(i for i in project.images if i.id==image);assert info.description==description and info.tags==tags


def test_pinned_parent_probe_and_independent_nullable_ordered_values():
    for name,sha in MANIFEST['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==sha
    for name,sha in MANIFEST['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha
    parent=ROOT/'tests/fixtures'/MANIFEST['parent']['fixture'];assert hashlib.sha256((parent/'manifest.json').read_bytes()).hexdigest()==MANIFEST['parent']['manifest_sha256']
    assert REFERENCE['null-description']['description'] is None and REFERENCE['null-description']['tags']==[]
    assert REFERENCE['empty-description']['description']=='' and REFERENCE['empty-description']['tags']==TAGS
    assert REFERENCE['unicode-description']['description']==DESCRIPTION and REFERENCE['unicode-description']['tags']==['Tumor','tumor','e\u0301','é']


def test_captured_entries_import_without_loss_and_preserve_null_empty_order(tmp_path):
    path=foreign(tmp_path);preview=q.preview_qupath_project(str(path));assert preview.issues==[]
    target=tmp_path/'entries.qpxproj';project=preview.import_new(str(target));restored=q.Project.open(str(target))
    for info in preview.images:
        row=REFERENCE[info.name];assert_details(project,info.native_id,row['description'],row['tags']);assert_details(restored,info.native_id,row['description'],row['tags'])
        doc=restored.document(info.native_id);assert doc.image_properties is None and doc.image_data_properties is None
    with sqlite3.connect(target) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION
        assert db.execute('SELECT count(*) FROM image_details').fetchone()[0]==2 and db.execute('SELECT count(*) FROM image_tags').fetchone()[0]==9


def test_atomic_ordered_set_updates_owning_getters_and_project_only_dirty(tmp_path):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,RICH);path=tmp_path/'saved.qpxproj';project.save(str(path));revision=doc.revision
    supplied=['z-last','a-first','z-last','','µ','α'];project.set_image_details(image,DESCRIPTION,supplied);supplied.clear();held=project.images[0]
    assert_details(project,image,DESCRIPTION,TAGS);assert project.dirty and doc.revision==revision and project.image_metadata(image)==RICH
    copy=project.image_tags(image);copy.clear();copy=held.tags;copy.clear();assert held.tags==TAGS
    project.save(str(path));project.set_image_details(image,DESCRIPTION,['z-last','a-first','z-last','','µ','α']);assert not project.dirty
    project.set_image_details(image,'',list(reversed(TAGS)));assert project.dirty and held.description==DESCRIPTION and held.tags==TAGS
    project.set_image_details(image);assert_details(project,image,None,[]);assert project.image_metadata(image)==RICH
    for call in [lambda:project.image_description(999),lambda:project.image_tags(999),lambda:project.set_image_details(999)]:
        with pytest.raises(IndexError):call()


@pytest.mark.parametrize('description,tags',[('bad\0text',[]),('x'*1048577,[]),('changed',['bad\0tag']),('changed',['x'*4097]),('changed',['x']*4097),('changed',[str(i)+'x'*4090 for i in range(260)]),(7,[]),('changed',[7])],ids=['description-nul', 'description-limit', 'tag-nul', 'tag-limit', 'tag-count', 'tag-payload', 'description-type', 'tag-type'])
def test_invalid_inputs_refuse_both_fields_atomically(tmp_path,description,tags):
    project,image,doc,source=workspace(tmp_path);project.set_image_details(image,DESCRIPTION,TAGS);path=tmp_path/'clean.qpxproj';project.save(str(path));before=path.read_bytes()
    with pytest.raises((ValueError,TypeError,RuntimeError)):project.set_image_details(image,description,tags)
    assert_details(project,image,DESCRIPTION,TAGS);assert not project.dirty and path.read_bytes()==before


@pytest.mark.parametrize('description,tags',[('x'*1048576,[]),(None,['µ'*2048]),('',[str(i) for i in range(4096)])],ids=['description-max', 'tag-text-max', 'tag-count-max'])
def test_declared_bounds_and_null_empty_roundtrip(tmp_path,description,tags):
    project,image,doc,source=workspace(tmp_path);project.set_image_details(image,description,tags);path=tmp_path/'bounds.qpxproj';project.save(str(path));assert_details(q.Project.open(str(path)),image,description,tags)


def test_snapshot_ownership_and_each_attribute_counts_toward_budget(tmp_path):
    project,image,doc,source=workspace(tmp_path);path=tmp_path/'snapshot.qpxproj';small=q.ProjectSaveJob(100000);assert small.request(project,str(path)) and small.wait().applied
    project.set_image_details(image,DESCRIPTION,TAGS);job=q.ProjectSaveJob();assert job.request(project,str(path));project.set_image_details(image,'later',['later']);assert job.wait().applied
    assert_details(q.Project.open(str(path)),image,DESCRIPTION,TAGS);assert project.dirty
    for description,tags in [('x'*600000,[]),(None,[str(i)+'x'*3000 for i in range(200)])]:
        project.set_image_details(image,description,tags);before=path.read_bytes()
        with pytest.raises((ValueError,RuntimeError),match='budget'):q.ProjectSaveJob(100000).request(project,str(path))
        assert path.read_bytes()==before and project.dirty


def test_legacy23_defaults_and_failed_upgrade_preserve_file(tmp_path):
    project,image,doc,source=workspace(tmp_path);project.set_image_details(image,DESCRIPTION,TAGS);path=tmp_path/'legacy.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:db.execute('DROP TABLE image_tags');db.execute('DROP TABLE image_details');db.execute('PRAGMA user_version=23')
    legacy=q.Project.open(str(path));assert_details(legacy,image,None,[]);legacy.set_image_details(image,DESCRIPTION,TAGS)
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_details BEFORE INSERT ON images BEGIN SELECT RAISE(ABORT,'details fault'); END")
    before=path.read_bytes()
    with pytest.raises(RuntimeError,match='details fault'):legacy.save(str(path))
    assert path.read_bytes()==before
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==23
        assert not db.execute("SELECT name FROM sqlite_master WHERE name IN ('image_details','image_tags')").fetchall();db.execute('DROP TRIGGER fail_details')
    legacy.save(str(path));assert_details(q.Project.open(str(path)),image,DESCRIPTION,TAGS)


@pytest.mark.parametrize('damage',['description-nul','description-size','description-type','description-utf8','description-duplicate','tag-nul','tag-size','tag-type','tag-duplicate','position','count','payload','orphan-details','orphan-tags'])
def test_corrupt_details_or_tag_rows_are_refused(tmp_path,damage):
    project,image,doc,source=workspace(tmp_path);project.set_image_details(image,'value',['first']);path=tmp_path/'bad.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        if damage=='description-nul':db.execute('UPDATE image_details SET description=?',('bad\0text',))
        elif damage=='description-size':db.execute('UPDATE image_details SET description=?',('x'*1048577,))
        elif damage=='description-utf8':db.execute("UPDATE image_details SET description=CAST(x'ff' AS TEXT)")
        elif damage in ['description-type','description-duplicate']:
            db.execute('DROP TABLE image_details');db.execute('CREATE TABLE image_details(image_id TEXT,description)');db.execute('INSERT INTO image_details VALUES(?,?)',(str(image),7 if damage=='description-type' else 'value'))
            if damage=='description-duplicate':db.execute('INSERT INTO image_details VALUES(?,?)',(str(image),'other'))
        elif damage=='tag-nul':db.execute('UPDATE image_tags SET tag=?',('bad\0tag',))
        elif damage=='tag-size':db.execute('UPDATE image_tags SET tag=?',('x'*4097,))
        elif damage=='orphan-details':db.execute('INSERT INTO image_details VALUES(?,?)',('999','orphan'))
        elif damage=='orphan-tags':db.execute('DELETE FROM image_details')
        else:
            db.execute('DROP TABLE image_tags');db.execute('CREATE TABLE image_tags(image_id TEXT,position,tag)')
            if damage=='tag-type':rows=[(str(image),0,7)]
            elif damage=='tag-duplicate':rows=[(str(image),0,'duplicate'),(str(image),1,'duplicate')]
            elif damage=='position':rows=[(str(image),1,'gap')]
            elif damage=='count':rows=[(str(image),i,str(i)) for i in range(4097)]
            else:rows=[(str(image),i,str(i)+'x'*4090) for i in range(260)]
            db.executemany('INSERT INTO image_tags VALUES(?,?,?)',rows)
    with pytest.raises((ValueError,RuntimeError)):q.Project.open(str(path))


@pytest.mark.parametrize('bad',[{'description':7},{'description':'bad\0text'},{'tags':'text'},{'tags':[None]},{'tags':['bad\0tag']}])
def test_invalid_foreign_attributes_refuse_before_publication(tmp_path,bad):
    path=foreign(tmp_path);data=json.loads(path.read_bytes());data['images'][0].update(bad);path.write_text(json.dumps(data),encoding='utf-8')
    with pytest.raises(ValueError):q.preview_qupath_project(str(path))
    assert not list(tmp_path.glob('*.qpxproj'))


@pytest.mark.parametrize('route',['v1-geojson','v2-geojson','v2-native'])
def test_rich_attributes_with_actual_saved_geometry_routes(tmp_path,route):
    from test_qupath_project import corpus,update_project
    path=corpus(tmp_path);data=json.loads(path.read_bytes());data['images'][0].update(description=DESCRIPTION,tags=['z','a','z',''],metadata={'description':'metadata-scope'});update_project(path,data)
    index=path.parent/'qupathxx-exchange/index.json';bundle=json.loads(index.read_bytes())
    if route!='v1-geojson':
        bundle['schema']='qpx.qupath-project-exchange/2'
        if route=='v2-native':bundle['images']=[{k:item[k] for k in ['entry_id','qpdata_sha256']}|{'representation':'native-saved-data'} for item in bundle['images']]
        else:
            for item in bundle['images']:item['representation']='geojson'
        index.write_text(json.dumps(bundle),encoding='utf-8')
    preview=q.preview_qupath_project(str(path));assert preview.issues==[] and [i.objects for i in preview.images]==[2,2]
    target=tmp_path/'geometry.qpxproj';project=preview.import_new(str(target));restored=q.Project.open(str(target));first,second=preview.images
    assert_details(restored,first.native_id,DESCRIPTION,['z','a','']);assert_details(restored,second.native_id,None,[])
    assert restored.image_metadata(first.native_id)=={'description':'metadata-scope'} and restored.document(first.native_id).object_count==2


def test_stale_attributes_preserve_preview_and_missing_source_keeps_inspection(tmp_path):
    path=foreign(tmp_path);preview=q.preview_qupath_project(str(path));data=json.loads(path.read_bytes());data['images'][0]['description']='changed';path.write_text(json.dumps(data),encoding='utf-8')
    with pytest.raises(RuntimeError,match='changed'):preview.import_new(str(tmp_path/'changed.qpxproj'))
    assert not preview.consumed and not (tmp_path/'changed.qpxproj').exists()
    project,image,doc,source=workspace(tmp_path);project.set_image_details(image,DESCRIPTION,TAGS);target=tmp_path/'missing.qpxproj';project.save(str(target));doc.server.close();source.unlink();assert_details(q.Project.open(str(target)),image,DESCRIPTION,TAGS)


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in entry details desktop')
def test_desktop_inspects_changed_attributes_and_saves(tmp_path):
    from test_gui_replay import event
    project,image,doc,source=workspace(tmp_path);target=tmp_path/'desktop.qpxproj';project.save(str(target));project.set_image_metadata(image,RICH);project.set_image_details(image,'Desktop saved α\nsecond line',['desktop-tag','α',''])
    options=q.GuiOptions();options.width=1500;options.height=1400;options.project_image_path=str(source);options.inputs=[event(20,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=45,capture_path=str(tmp_path/'entry-details.ppm'),options=options);assert report.replay_complete
    restored=q.Project.open(str(target));assert_details(restored,image,'Desktop saved α\nsecond line',['desktop-tag','α','']);assert restored.image_metadata(image)==RICH


@pytest.mark.parametrize('fields,description,tags',[({},None,[]),({'description':None,'tags':None},None,[]),({'description':'','tags':[]},'',[]),({'tags':['b','a','b','']},None,['b','a',''])],ids=['absent','null','empty','duplicate-order'])
def test_optional_fields_and_duplicate_tag_membership(tmp_path,fields,description,tags):
    path=foreign(tmp_path);data=json.loads(path.read_bytes());entry=data['images'][0];entry.pop('description',None);entry.pop('tags',None);entry.update(fields);path.write_text(json.dumps(data),encoding='utf-8')
    preview=q.preview_qupath_project(str(path));assert preview.issues==[];target=tmp_path/'optional.qpxproj';project=preview.import_new(str(target));image=preview.images[0].native_id
    assert_details(project,image,description,tags);assert_details(q.Project.open(str(target)),image,description,tags)
