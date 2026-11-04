"""Actual pinned project data, explicit migration losses and atomic publication."""
import hashlib
import json
import os
from pathlib import Path
import shutil

import numpy as np
import pytest
import qupathxx as q
import tifffile

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-project-0.7.0'
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='project/interchange/TIFF')
GUI=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in QuPath project desktop')


def write(path,data):path.write_bytes((json.dumps(data,ensure_ascii=False,indent=2)+'\n').encode())


def update_project(path,data):
    write(path,data);index=path.parent/'qupathxx-exchange/index.json'
    if index.exists():
        value=json.loads(index.read_bytes());value['project_sha256']=hashlib.sha256(path.read_bytes()).hexdigest();write(index,value)


def corpus(tmp_path):
    directory=tmp_path/'foreign';shutil.copytree(FIXTURE,directory)
    path=directory/'project/project.qpproj';data=json.loads(path.read_bytes())
    # Synthetic relocation variant of the exact raw capture, with a matching
    # exchange binding. Original binary data and annotation bytes stay intact.
    data['uri']='file:///qpx-missing-original/project/project.qpproj'
    for item,name in zip(data['images'],('slide%20%CE%B1.ome.tif','gray.ome.tif'),strict=True):
        item['serverBuilder']['uri']='file:///qpx-missing-original/sources/'+name
    update_project(path,data);return path


def test_pinned_project_and_exchange_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in manifest['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest
    reference=json.loads((FIXTURE/'reference.json').read_bytes())
    assert [e['objects'] for e in reference['images']]==[2,2]
    index=json.loads((FIXTURE/'project/qupathxx-exchange/index.json').read_bytes())
    assert index['project_sha256']==manifest['files']['project/project.qpproj']
    assert len(index['images'])==2 and all(not e['issues'] for e in index['images'])


def test_project_import_preserves_sources_objects_classes_calibration_and_pixels(tmp_path):
    path=corpus(tmp_path);preview=q.preview_qupath_project(str(path))
    assert not preview.consumed and preview.issues==[] and len(preview.images)==2
    assert [e.objects for e in preview.images]==[2,2]
    assert [e.name for e in preview.images]==['Slide α','Gray specimen']
    for image in preview.images:
        assert image.available and image.reader=='tiff' and len(image.source_object_ids)==2
        assert len(set(image.source_object_ids))==2 and len(image.imported_object_ids)==2
    snapshot=preview.images;snapshot.clear();assert len(preview.images)==2
    destination=tmp_path/'imported.qpxproj';project=preview.import_new(str(destination))
    assert preview.consumed and len(project.images)==2
    restored=q.Project.open(str(destination))
    for info,scale in zip(preview.images,((.25,.5),(.7,.8)),strict=True):
        doc=restored.document(info.native_id)
        assert (doc.pixel_width_um,doc.pixel_height_um)==scale
        objects=doc.annotations;assert [a.id for a in objects]==info.imported_object_ids
        assert [a.name for a in objects]==['Parent α','Child']
        assert objects[0].parent_id==0 and objects[1].parent_id==objects[0].id
        assert all(a.class_name=='Tumor' for a in objects) and doc.object_class('Tumor').color==0xff2030
        expected=tifffile.imread(info.source_path);r=q.RawRegionRequest();r.region=q.RegionRequest(0,0,0,expected.shape[1],expected.shape[0]);r.channels=3 if expected.ndim==3 else 1
        np.testing.assert_array_equal(doc.server.read_raw(r).numpy(),expected if expected.ndim==3 else expected[...,None])
    with pytest.raises(ValueError,match='consumed'):preview.import_new(str(tmp_path/'twice.qpxproj'))
    assert not (tmp_path/'twice.qpxproj').exists()


@pytest.mark.parametrize('loss',['missing_source','unsupported_builder'])
def test_preview_reports_losses_before_any_new_project_publication(tmp_path,loss):
    path=corpus(tmp_path);data=json.loads(path.read_bytes())
    if loss=='missing_source':(path.parent.parent/'sources/slide α.ome.tif').unlink()
    elif loss=='unsupported_builder':data['images'][0]['serverBuilder']['builderType']='rotated';update_project(path,data)
    preview=q.preview_qupath_project(str(path));assert preview.issues
    output=tmp_path/'loss.qpxproj'
    with pytest.raises(ValueError,match='loss'):preview.import_new(str(output))
    assert not output.exists() and not preview.consumed
    project=preview.import_new(str(output),allow_losses=True)
    assert len(project.images)==1 and project.images[0].name=='Gray specimen'


@pytest.mark.parametrize('member',['project.qpproj','data/1/data.qpdata','classifiers/classes.json','qupathxx-exchange/index.json','qupathxx-exchange/1.geojson','../sources/gray.ome.tif'])
def test_reviewed_file_changes_refuse_atomic_publication(tmp_path,member):
    path=corpus(tmp_path);preview=q.preview_qupath_project(str(path));source=path.parent/member
    with source.open('ab') as stream:stream.write(b' ')
    output=tmp_path/'changed.qpxproj'
    with pytest.raises(RuntimeError,match='changed'):preview.import_new(str(output))
    assert not output.exists() and not preview.consumed


@pytest.mark.parametrize('failure',['version','duplicate_id','stale_qpdata','missing_qpdata','unsafe_exchange','duplicate_geojson','stale_classes','unknown_override'])
def test_invalid_or_stale_foreign_data_is_refused(tmp_path,failure):
    path=corpus(tmp_path);data=json.loads(path.read_bytes());index_path=path.parent/'qupathxx-exchange/index.json'
    if failure=='version':data['version']='9.0';update_project(path,data)
    elif failure=='duplicate_id':data['images'][1]['entryID']=1;update_project(path,data)
    elif failure=='stale_qpdata':
        with (path.parent/'data/1/data.qpdata').open('ab') as stream:stream.write(b'!')
    elif failure=='missing_qpdata':(path.parent/'data/1/data.qpdata').unlink()
    elif failure=='unsafe_exchange':
        index=json.loads(index_path.read_bytes());index['images'][0]['file']='../escape.geojson';write(index_path,index)
    elif failure=='duplicate_geojson':
        member=path.parent/'qupathxx-exchange/1.geojson';raw=member.read_text();raw=raw.replace('"type": "FeatureCollection"','"type": "FeatureCollection", "type": "FeatureCollection"',1);member.write_text(raw)
        index=json.loads(index_path.read_bytes());index['images'][0]['sha256']=hashlib.sha256(member.read_bytes()).hexdigest();write(index_path,index)
    elif failure=='stale_classes':
        with (path.parent/'classifiers/classes.json').open('ab') as stream:stream.write(b' ')
    overrides={'999':str(path)} if failure=='unknown_override' else {}
    with pytest.raises((ValueError,RuntimeError)):q.preview_qupath_project(str(path),source_overrides=overrides)
    assert not list(tmp_path.glob('*.qpxproj'))


def test_optional_metadata_absence_and_unknown_calibration_survive_reopen(tmp_path):
    path=corpus(tmp_path);index_path=path.parent/'qupathxx-exchange/index.json'
    index=json.loads(index_path.read_bytes())
    for name in ('classifiers/classes.json','data/metadata.json'):
        (path.parent/name).unlink();index['files'][name]=None
    for image in index['images']:
        image['pixel_width_um']=None;image['pixel_height_um']=None
    write(index_path,index)
    preview=q.preview_qupath_project(str(path));assert preview.issues==[]
    output=tmp_path/'unknown-calibration.qpxproj';preview.import_new(str(output))
    restored=q.Project.open(str(output))
    for image in preview.images:
        doc=restored.document(image.native_id)
        assert doc.pixel_width_um is None and doc.pixel_height_um is None
        assert doc.object_count==2 and all(a.class_name=='Tumor' for a in doc.annotations)
        assert doc.annotations[1].parent_id==doc.annotations[0].id


def test_explicit_source_overrides_and_existing_destination_preservation(tmp_path):
    path=corpus(tmp_path);sources=path.parent.parent/'sources'
    preview=q.preview_qupath_project(str(path),source_overrides={'1':str(sources/'slide α.ome.tif'),'2':str(sources/'gray.ome.tif')})
    output=tmp_path/'existing.qpxproj';output.write_bytes(b'original')
    with pytest.raises((ValueError,RuntimeError)):preview.import_new(str(output))
    assert output.read_bytes()==b'original' and not preview.consumed
    project=preview.import_new(str(tmp_path/'fresh.qpxproj'));assert len(project.images)==2


@pytest.mark.parametrize('apply',[False,True])
@GUI
def test_imgui_review_and_import_preserve_workspace_until_apply(tmp_path,apply):
    from test_gui_replay import event
    path=corpus(tmp_path);project=q.Project();project.add_document(q.Document(q.Image.demo()),'Original')
    saved=tmp_path/'original.qpxproj';project.save(str(saved));before=saved.read_bytes()
    options=q.GuiOptions();options.project_open_path=str(path);options.settle_tiles=True
    inputs=[event(5,q.GuiInputKind.OPEN_PROJECT_KEY)]
    if apply:
        inputs += [event(60,q.GuiInputKind.OPEN_PROJECT_KEY),
                   event(80,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT,q.GuiInputWait.TARGET_READY),
                   event(81,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    options.inputs=inputs;capture=tmp_path/'project-import.ppm'
    report=q.show(project,frame_limit=120,capture_path=str(capture),options=options)
    assert report.replay_complete and saved.read_bytes()==before
    assert len(project.images)==(2 if apply else 1)
    assert report.project_replacements==(1 if apply else 0)
    assert path.with_suffix('.qpxproj').exists()==apply
    if apply:
        reopened=q.Project.open(str(path.with_suffix('.qpxproj')))
        assert [e.name for e in reopened.images]==['Slide α','Gray specimen']
        assert all(reopened.document(e.id).object_count==2 for e in reopened.images)
    assert capture.exists()


def test_native_saved_data_matches_independent_assisted_exchange(tmp_path):
    path=corpus(tmp_path);assisted=q.preview_qupath_project(str(path))
    assisted_project=assisted.import_new(str(tmp_path/'assisted.qpxproj'))
    shutil.rmtree(path.parent/'qupathxx-exchange')
    direct=q.preview_qupath_project(str(path));assert direct.issues==[]
    assert [i.source_object_ids for i in direct.images]==[i.source_object_ids for i in assisted.images]
    direct.import_new(str(tmp_path/'direct.qpxproj'))
    reopened=q.Project.open(str(tmp_path/'direct.qpxproj'))
    for actual,expected in zip(direct.images,assisted.images,strict=True):
        doc=reopened.document(actual.native_id);reference=assisted_project.document(expected.native_id)
        assert (doc.pixel_width_um,doc.pixel_height_um)==(reference.pixel_width_um,reference.pixel_height_um)
        # Direct decoding retains analytic rectangles. Assisted GeoJSON retains
        # their polygon outlines, whose vertex starts/hints differ in serialization.
        assert len(doc.annotations)==len(reference.annotations)
        for obj,expected in zip(doc.annotations,reference.annotations,strict=True):
            assert (obj.id,obj.parent_id,obj.name,obj.class_name,obj.kind)==(expected.id,expected.parent_id,expected.name,expected.class_name,expected.kind)
            assert obj.geometry.area==expected.geometry.area
            vertices=lambda roi:sorted((point.x,point.y) for point in roi.paths[0])
            assert vertices(obj.geometry)==vertices(expected.geometry)
            for y in range(48):
                for x in range(64):
                    point=q.Point(x+.5,y+.5)
                    assert obj.geometry.contains(point)==expected.geometry.contains(point)
        assert doc.object_count==2


@pytest.mark.parametrize('failure',['header','truncated','trailing','bad_handle','invalid_utf'])
def test_native_saved_data_corruption_never_publishes(tmp_path,failure):
    path=corpus(tmp_path);shutil.rmtree(path.parent/'qupathxx-exchange')
    data_path=path.parent/'data/1/data.qpdata';data=data_path.read_bytes()
    if failure=='header':data=b'BAD!'+data[4:]
    elif failure=='truncated':data=data[:-5]
    elif failure=='trailing':data+=b'\x70'
    elif failure=='bad_handle':data=data.replace(b'\x71\x00\x7e',b'\x71\xff\xff',1)
    else:data=data.replace('Parent α'.encode(),b'Parent \xff\xff',1)
    data_path.write_bytes(data)
    with pytest.raises(ValueError,match='saved data'):
        q.preview_qupath_project(str(path))
    assert not list(tmp_path.glob('*.qpxproj'))
