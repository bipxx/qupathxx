"""Project-wide string maps are separate, owning state, including zero images."""
import hashlib
import json
import os
import shutil
import sqlite3
from pathlib import Path

import pytest
import qupathxx as q
from project_schema_contract import CURRENT_SCHEMA_VERSION
from test_project_image_metadata import workspace

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-project-metadata-0.7.0'
MANIFEST=json.loads((FIXTURE/'manifest.json').read_bytes())
REFERENCE={row['name']:row for row in json.loads((FIXTURE/'reference.json').read_bytes())}
RICH=REFERENCE['rich']['metadata']
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='project/TIFF/interchange')


def foreign(tmp_path,label='rich',exchange=None):
    directory=tmp_path/'foreign';shutil.copytree(FIXTURE/label,directory)
    path=directory/'project.qpproj';data=json.loads(path.read_bytes())
    if data['images']:
        parent=ROOT/'tests/fixtures'/MANIFEST['parent']['fixture'];origin=json.loads((parent/'manifest.json').read_bytes())['parent']
        source=directory/'source.ome.tif';shutil.copyfile(ROOT/'tests/fixtures'/origin['fixture']/origin['source'],source)
        for item in data['images']:item['serverBuilder']['uri']=source.as_uri()
    path.write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
    if exchange:
        bundle=directory/'qupathxx-exchange';bundle.mkdir()
        files={name:hashlib.sha256((directory/name).read_bytes()).hexdigest() if (directory/name).exists() else None for name in ['classifiers/classes.json','data/metadata.json']}
        index={'schema':'qpx.qupath-project-exchange/'+exchange,'qupath_version':'0.7.0','project_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'files':files,'images':[]}
        (bundle/'index.json').write_text(json.dumps(index),encoding='utf-8')
    return path


def test_project_capture_reference_and_parent_identity():
    for name,digest in MANIFEST['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in MANIFEST['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest
    parent=ROOT/'tests/fixtures'/MANIFEST['parent']['fixture'];assert hashlib.sha256((parent/'manifest.json').read_bytes()).hexdigest()==MANIFEST['parent']['manifest_sha256']
    assert RICH=={'project-key':'project-scope','':'empty key','empty':'','multiline':'line1\nline2\t"quoted"','unicode':'Synthetic α 🚀','é':'composed','e\u0301':'decomposed','integer':'9223372036854775807','null-word':'null'}
    assert REFERENCE['no-images']['metadata']==RICH and REFERENCE['no-images']['images']==[] and REFERENCE['empty']['metadata']=={}
    assert REFERENCE['rich']['images'][0]['metadata']=={'project-key':'entry-scope'}


@pytest.mark.parametrize('label',['rich','empty','no-images'])
@pytest.mark.parametrize('exchange',[None,'1','2'],ids=['manifest','v1','v2'])
def test_captured_global_map_and_empty_project_import(tmp_path,label,exchange):
    path=foreign(tmp_path,label,exchange);preview=q.preview_qupath_project(str(path));assert preview.issues==[]
    target=tmp_path/'saved.qpxproj';project=preview.import_new(str(target));restored=q.Project.open(str(target))
    assert project.metadata==restored.metadata==REFERENCE[label]['metadata'] and len(project.images)==len(REFERENCE[label]['images'])
    for image in restored.images:assert image.metadata=={'project-key':'entry-scope'} and image.description is None and image.tags==[]
    with sqlite3.connect(target) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION
        assert db.execute('SELECT count(*) FROM project_metadata').fetchone()[0]==len(REFERENCE[label]['metadata'])


def test_owning_atomic_noop_clear_and_namespace_separation(tmp_path):
    project,image,doc,source=workspace(tmp_path);project.set_image_metadata(image,{'project-key':'entry-scope'});project.set_image_details(image,'details',['tag']);path=tmp_path/'owned.qpxproj';project.save(str(path));revision=doc.revision
    supplied=RICH.copy();project.set_metadata(supplied);supplied.clear();returned=project.metadata;returned.clear()
    assert project.metadata==RICH and project.dirty and doc.revision==revision
    assert project.image_metadata(image)=={'project-key':'entry-scope'} and project.image_description(image)=='details' and project.image_tags(image)==['tag']
    project.save(str(path));project.set_metadata(dict(reversed(list(RICH.items()))));assert not project.dirty
    with pytest.raises(AttributeError):project.metadata={}
    project.set_metadata();assert project.metadata=={} and project.dirty;project.save(str(path));assert q.Project.open(str(path)).metadata=={}


@pytest.mark.parametrize('values',[{'x':0},{0:'x'},{'bad\0key':'ok'},{'x':'bad\0value'},{'x':'µ'*2049},{'x'*4097:'ok'},{str(i):'' for i in range(4097)},{str(i):'x'*4096 for i in range(253)}],ids=['value-type','key-type','key-nul','value-nul','utf8-bytes','key-size','count','payload'])
def test_invalid_map_never_partially_changes_project(tmp_path,values):
    project=q.Project();project.set_metadata(RICH);path=tmp_path/'atomic.qpxproj';project.save(str(path))
    with pytest.raises((TypeError,ValueError)):project.set_metadata(values)
    assert project.metadata==RICH and not project.dirty and q.Project.open(str(path)).metadata==RICH


@pytest.mark.parametrize('values',[{'µ'*2048:'ok'},{'x':'µ'*2048},{str(i):'' for i in range(4096)}],ids=['key-bytes','value-bytes','count'])
def test_exact_global_map_boundaries(tmp_path,values):
    project=q.Project();project.set_metadata(values);path=tmp_path/'limit.qpxproj';project.save(str(path));assert q.Project.open(str(path)).metadata==values


def test_snapshot_budget_ownership_and_recovery_restore(tmp_path):
    project=q.Project();path=tmp_path/'snapshot.qpxproj';job=q.ProjectSaveJob(100000);assert job.request(project,str(path)) and job.wait().applied
    project.set_metadata(RICH);job=q.ProjectSaveJob();assert job.request(project,str(path));project.set_metadata({'later':'live'});assert job.wait().applied
    assert q.Project.open(str(path)).metadata==RICH and project.dirty
    project.set_metadata({str(i):'x'*3000 for i in range(200)});before=path.read_bytes()
    with pytest.raises((ValueError,RuntimeError),match='budget'):q.ProjectSaveJob(100000).request(project,str(path))
    assert path.read_bytes()==before
    project.set_metadata(RICH);recovery=tmp_path/'recovery.qpxrec';writer=q.ProjectAutosave();assert writer.request(project,str(recovery));assert writer.wait().state=='ready'
    assert q.Project.recover(str(recovery)).metadata==RICH
    project.set_metadata({'later':'discard'});project.restore_recovery(str(recovery));assert project.metadata==RICH


def test_schema24_default_atomic_failed_upgrade_and_reopen(tmp_path):
    project=q.Project();path=tmp_path/'legacy.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:db.execute('DROP TABLE project_metadata');db.execute('PRAGMA user_version=24')
    legacy=q.Project.open(str(path));assert legacy.metadata=={};legacy.set_metadata(RICH)
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_global BEFORE INSERT ON project BEGIN SELECT RAISE(ABORT,'global fault'); END")
    before=path.read_bytes()
    with pytest.raises(RuntimeError,match='global fault'):legacy.save(str(path))
    assert path.read_bytes()==before
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==24 and not db.execute("SELECT name FROM sqlite_master WHERE name='project_metadata'").fetchall();db.execute('DROP TRIGGER fail_global')
    legacy.save(str(path));assert q.Project.open(str(path)).metadata==RICH


@pytest.mark.parametrize('damage',['key-nul','value-nul','key-size','value-size','key-type','value-type','utf8','duplicate','count','payload'])
def test_malformed_global_store_refuses(tmp_path,damage):
    project=q.Project();project.set_metadata({'key':'value'});path=tmp_path/'bad.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute('DROP TABLE project_metadata');db.execute('CREATE TABLE project_metadata(metadata_key,metadata_value)')
        rows=[('key','value')]
        if damage=='key-nul':rows=[('bad\0key','ok')]
        elif damage=='value-nul':rows=[('key','bad\0value')]
        elif damage=='key-size':rows=[('x'*4097,'ok')]
        elif damage=='value-size':rows=[('key','x'*4097)]
        elif damage=='key-type':rows=[(7,'ok')]
        elif damage=='value-type':rows=[('key',7)]
        elif damage=='duplicate':rows=[('key','one'),('key','two')]
        elif damage=='count':rows=[(str(i),'') for i in range(4097)]
        elif damage=='payload':rows=[(str(i),'x'*4096) for i in range(253)]
        if damage=='utf8':db.execute("INSERT INTO project_metadata VALUES('key',CAST(X'ff' AS TEXT))")
        else:db.executemany('INSERT INTO project_metadata VALUES(?,?)',rows)
    with pytest.raises((ValueError,RuntimeError)):q.Project.open(str(path))


@pytest.mark.parametrize('value',[None,[],{'typed':7},{'null':None},{'nul':'bad\0text'},{'long':'x'*4097}],ids=['null-root','array','number','null-value','nul','size'])
def test_malformed_foreign_global_metadata_refuses_before_publication(tmp_path,value):
    path=foreign(tmp_path);(path.parent/'data/metadata.json').write_text(json.dumps(value),encoding='utf-8')
    with pytest.raises((ValueError,RuntimeError)):q.preview_qupath_project(str(path))
    assert not (tmp_path/'saved.qpxproj').exists()


def test_missing_global_file_defaults_empty_and_later_appearance_is_stale(tmp_path):
    path=foreign(tmp_path);metadata=path.parent/'data/metadata.json';metadata.unlink();preview=q.preview_qupath_project(str(path));assert preview.issues==[]
    metadata.write_text('{}',encoding='utf-8')
    with pytest.raises(RuntimeError,match='appeared'):preview.import_new(str(tmp_path/'stale.qpxproj'))
    assert not preview.consumed and not (tmp_path/'stale.qpxproj').exists()
    fresh=q.preview_qupath_project(str(path));assert fresh.import_new(str(tmp_path/'empty.qpxproj')).metadata=={}


def test_global_file_stale_guard_and_all_unavailable_refusal(tmp_path):
    path=foreign(tmp_path);preview=q.preview_qupath_project(str(path));(path.parent/'data/metadata.json').write_text('{}',encoding='utf-8')
    with pytest.raises(RuntimeError,match='changed'):preview.import_new(str(tmp_path/'changed.qpxproj'))
    assert not preview.consumed
    (path.parent/'source.ome.tif').unlink();preview=q.preview_qupath_project(str(path));assert any(i.code=='unavailable_image' for i in preview.issues)
    with pytest.raises(ValueError,match='no available supported images'):preview.import_new(str(tmp_path/'unavailable.qpxproj'),allow_losses=True)


@pytest.mark.parametrize('route',['v1-geojson','v2-geojson','v2-native'])
def test_global_metadata_with_actual_saved_geometry(tmp_path,route):
    from test_qupath_project import corpus,update_project
    path=corpus(tmp_path);data=json.loads(path.read_bytes());data['images'][0]['metadata']={'project-key':'entry-scope'};update_project(path,data)
    metadata=path.parent/'data/metadata.json';metadata.write_text(json.dumps(RICH,ensure_ascii=False),encoding='utf-8')
    index=path.parent/'qupathxx-exchange/index.json';bundle=json.loads(index.read_bytes());bundle['files']['data/metadata.json']=hashlib.sha256(metadata.read_bytes()).hexdigest()
    if route!='v1-geojson':
        bundle['schema']='qpx.qupath-project-exchange/2'
        if route=='v2-native':bundle['images']=[{k:item[k] for k in ['entry_id','qpdata_sha256']}|{'representation':'native-saved-data'} for item in bundle['images']]
        else:
            for item in bundle['images']:item['representation']='geojson'
    index.write_text(json.dumps(bundle),encoding='utf-8');preview=q.preview_qupath_project(str(path));assert preview.issues==[] and [i.objects for i in preview.images]==[2,2]
    target=tmp_path/'geometry.qpxproj';project=preview.import_new(str(target));restored=q.Project.open(str(target));assert project.metadata==restored.metadata==RICH
    first=preview.images[0];assert restored.image_metadata(first.native_id)=={'project-key':'entry-scope'} and restored.document(first.native_id).object_count==2


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in project metadata desktop')
@pytest.mark.parametrize('with_image',[False,True],ids=['zero-images','with-image'])
def test_desktop_saves_changed_project_metadata(tmp_path,with_image):
    from test_gui_replay import event
    project=q.Project()
    if with_image:project.add_document(q.Document(q.Image.demo()),'Entry')
    target=tmp_path/'desktop.qpxproj';project.save(str(target));project.set_metadata(RICH)
    options=q.GuiOptions();options.width=1500;options.height=1400;options.inputs=[event(20,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=45,capture_path=str(tmp_path/'project-metadata.ppm'),options=options)
    assert report.replay_complete and q.Project.open(str(target)).metadata==RICH


def test_empty_project_class_registry_loss_is_explicit(tmp_path):
    path=foreign(tmp_path,'no-images');classes=path.parent/'classifiers/classes.json'
    classes.write_text(json.dumps({'pathClasses':[{'name':'Tumor','color':0xff0000}]}),encoding='utf-8')
    preview=q.preview_qupath_project(str(path));assert [(i.code,i.count) for i in preview.issues]==[('project_classes_dropped',1)]
    with pytest.raises(ValueError,match='reported losses'):preview.import_new(str(tmp_path/'not-approved.qpxproj'))
    project=preview.import_new(str(tmp_path/'approved.qpxproj'),allow_losses=True);assert project.metadata==RICH and project.images==[]
