"""Pinned native TMA grids adopt the existing grid/ID/history/store authority."""
import csv
import gc
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct

import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-saved-tma-0.7.0'
REFERENCE={r['name']:r for r in json.loads((FIXTURE/'reference.json').read_bytes())}
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='native TMA import')


def foreign(tmp_path,label='grid'):
    root=tmp_path/'foreign';shutil.copytree(FIXTURE,root);path=root/'project/project.qpproj';data=json.loads(path.read_bytes())
    data['images']=[r for r in data['images'] if r['imageName']==label];assert len(data['images'])==1
    data['images'][0]['serverBuilder']['uri']=(root/'source.ome.tif').as_uri();path.write_text(json.dumps(data),encoding='utf-8');return path


def data_file(path):
    entry=json.loads(path.read_bytes())['images'][0]['entryID'];return path.parent/'data'/str(entry)/'data.qpdata'


def imported(tmp_path,label='grid'):
    source=foreign(tmp_path,label);preview=q.preview_qupath_project(str(source));assert preview.issues==[]
    path=tmp_path/'native.qpxproj';project=preview.import_new(str(path));info=preview.images[0]
    return project,info.native_id,project.document(info.native_id),dict(zip(info.source_object_ids,info.imported_object_ids,strict=True)),path


def assert_document(doc,mapping,label):
    ref=REFERENCE[label];grid=doc.tma_grid();assert grid is not None and doc.tma_core_count==6 and doc.object_count==11
    assert (grid.info.rows,grid.info.columns)==(ref['rows'],ref['columns'])
    assert grid.info.row_labels==['A','B'] and grid.info.column_labels==['1','2','3']
    assert doc.tma_core_ids(True)==[mapping[x] for x in ref['core_ids']]
    assert len(set(mapping.values()))==len(mapping)==11
    by_id={r['id']:r for r in ref['objects']}
    for identity,row in by_id.items():
        obj=doc.annotation(mapping[identity]);assert obj.name==row['name'] and obj.parent_id==(mapping[row['parent']] if row['parent'] else 0)
        b=obj.geometry.bounds;assert [b.x,b.y,b.width,b.height]==row['bounds']
        assert obj.geometry.area==pytest.approx(row['area'],rel=1e-12) and obj.plane==q.ObjectPlane(*row['plane'])
        assert obj.class_name==(row['classification'] or '') and obj.locked==row['locked'] and obj.color_rgb==row['color']
        if row['metadata'] or row['measurements']:
            assert obj.properties.metadata==row['metadata'] and obj.properties.measurements==row['measurements']
        if row['kind']=='tma':
            i=ref['core_ids'].index(identity);assert obj.kind==q.ObjectKind.ANNOTATION and obj.geometry.kind==q.RoiKind.ELLIPSE
            assert (obj.tma_core.row,obj.tma_core.column)==divmod(i,ref['columns'])
            assert obj.tma_core.case_id==row['case_id'] and obj.tma_core.missing==row['missing']
            assert grid.core(*divmod(i,ref['columns'])).id==obj.id
        else:
            assert obj.tma_core is None
            assert obj.kind=={'annotation':q.ObjectKind.ANNOTATION,'detection':q.ObjectKind.DETECTION,'cell':q.ObjectKind.CELL}[row['kind']]
        if row['nucleus_area'] is not None:assert obj.nucleus.area==pytest.approx(row['nucleus_area'],rel=1e-12)
    expected_present=[mapping[x] for x in ref['core_ids'] if not by_id[x]['missing']]
    assert doc.tma_core_ids()==expected_present


def test_pinned_saved_tma_capture_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in manifest['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest
    assert manifest['upstream']['revision']=='04ccfa4fb7d43e9b566393e08e83690b72248d44'


@pytest.mark.parametrize('label',['grid','legacy-case'])
def test_native_grid_order_identity_case_state_children_and_reopen(tmp_path,label):
    source=foreign(tmp_path,label);before={p:p.read_bytes() for p in source.parent.rglob('*') if p.is_file()}
    preview=q.preview_qupath_project(str(source));assert preview.issues==[]
    path=tmp_path/'native.qpxproj';project=preview.import_new(str(path));info=preview.images[0];mapping=dict(zip(info.source_object_ids,info.imported_object_ids,strict=True))
    doc=project.document(info.native_id);assert_document(doc,mapping,label);assert not doc.can_undo
    assert_document(q.Project.open(str(path)).document(info.native_id),mapping,label)
    assert all(p.read_bytes()==b for p,b in before.items())


@pytest.mark.parametrize('label',['ragged-grid','duplicate-member','orphan-core','absent-member','empty-grid','unlisted-core'])
def test_inconsistent_or_unrepresented_grids_refuse_without_downgrade(tmp_path,label):
    source=foreign(tmp_path,label)
    with pytest.raises(ValueError):q.preview_qupath_project(str(source))
    assert not list(tmp_path.glob('*.qpxproj'))


def change_class(data,old,new):
    old=old.encode();new=new.encode();needle=struct.pack('>H',len(old))+old
    assert data.count(needle)==1;return data.replace(needle,struct.pack('>H',len(new))+new,1)


@pytest.mark.parametrize('damage',['zero-columns','negative-columns','oversize-columns','count-mismatch','duplicate-uuid','bad-missing','non-ellipse','unknown-grid','truncated'])
def test_malformed_binary_grid_refuses_before_publication(tmp_path,damage):
    source=foreign(tmp_path);file=data_file(source);data=file.read_bytes()
    if damage in ['zero-columns','negative-columns','oversize-columns','count-mismatch']:
        old=struct.pack('>ii',2,3);assert data.count(old)==1
        rows,columns={'zero-columns':(2,0),'negative-columns':(2,-1),'oversize-columns':(2,10001),'count-mismatch':(3,3)}[damage]
        data=data.replace(old,struct.pack('>ii',rows,columns),1)
    elif damage=='duplicate-uuid':
        a,b=REFERENCE['grid']['core_ids'][:2];assert data.count(b.encode())==1;data=data.replace(b.encode(),a.encode(),1)
    elif damage=='bad-missing':
        old=b'\x77\x09'+struct.pack('>d',float('nan'))+b'\x00\x78';assert data.count(old)==4
        data=data.replace(old,old[:-2]+b'\x02\x78',1)
    elif damage=='non-ellipse':data=change_class(data,'qupath.lib.roi.EllipseROI$SerializationProxy','qupath.lib.roi.RectangleROI$SerializationProxy')
    elif damage=='unknown-grid':data=change_class(data,'qupath.lib.objects.hierarchy.DefaultTMAGrid','qupath.lib.objects.hierarchy.OtherTMAGrid')
    else:data=data[:-1]
    file.write_bytes(data)
    with pytest.raises(ValueError):q.preview_qupath_project(str(source))
    assert file.read_bytes()==data and not list(tmp_path.glob('*.qpxproj'))


def test_native_tma_edits_history_snapshot_case_pooling_and_children(tmp_path):
    project,image,doc,mapping,path=imported(tmp_path);ids=doc.tma_core_ids(True);held=doc.tma_grid()
    records=list(csv.DictReader(io.StringIO(q.tma_case_summary_csv(held))))
    case=next(r for r in records if r['case_id']=='case-one');assert [case[k] for k in ['cores','present_cores','missing_cores']]==['2','1','1']
    assert len([r for r in records if not r['case_id']])==2
    doc.set_tma_missing([ids[0]],True);doc.set_tma_case_ids({ids[0]:'edited',ids[2]:'edited'})
    point=next(r for r in REFERENCE['grid']['objects'] if r['name']=='Grandchild');point_id=mapping[point['id']];before=doc.annotation(point_id).geometry.bounds
    outside=next(r for r in REFERENCE['grid']['objects'] if r['name']=='Outside');outside_id=mapping[outside['id']];outside_before=doc.annotation(outside_id).geometry.bounds
    doc.translate_tma_grid(1,2,True);after=doc.annotation(point_id).geometry.bounds;assert (after.x,after.y)==(before.x+1,before.y+2)
    b=doc.annotation(outside_id).geometry.bounds;assert (b.x,b.y)==(outside_before.x,outside_before.y)
    assert not held.core(0,0).tma_core.missing and held.core(0,0).tma_core.case_id=='case-one'
    assert doc.undo() and doc.undo() and doc.undo();assert_document(doc,mapping,'grid')
    job=q.ProjectSaveJob();assert job.request(project,str(path));doc.set_tma_missing([ids[0]],True);doc.clear_history();assert job.wait().applied
    assert_document(q.Project.open(str(path)).document(image),mapping,'grid');assert project.dirty
    del project,doc;gc.collect();assert held.core(0,0).id==ids[0]


def test_saved_grid_source_guard_preserves_destination_and_preview(tmp_path):
    source=foreign(tmp_path);preview=q.preview_qupath_project(str(source));file=data_file(source);file.write_bytes(file.read_bytes()+b'\x70')
    output=tmp_path/'changed.qpxproj'
    with pytest.raises(RuntimeError,match='changed'):preview.import_new(str(output))
    assert not preview.consumed and not output.exists()


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in saved-grid desktop')
def test_desktop_uses_imported_grid_exports_metadata_and_saves(tmp_path):
    from test_gui_tma import click
    from test_gui_replay import event
    project,image,doc,mapping,path=imported(tmp_path);output=tmp_path/'metadata.csv'
    options=q.GuiOptions();options.width=1600;options.height=1400;options.docked_workspace=True;options.settle_tiles=True;options.autosave_interval_seconds=0
    options.tma_open=True;options.tma_tab=2;options.tma_metadata_path=str(output)
    options.inputs=[*click(5,q.GuiInputTarget.TMA_EXPORT),event(30,q.GuiInputKind.SAVE_KEY)]
    report=q.show(project,frame_limit=55,capture_path=str(tmp_path/'saved-tma.ppm'),options=options)
    assert report.replay_complete and report.tma_cores==6 and report.tma_missing_cores==2 and not report.tma_error and report.tma_export_state=='saved'
    rows=list(csv.DictReader(io.StringIO(output.read_text(encoding='utf-8'))));assert len(rows)==6 and [r['slot_state'] for r in rows]==['present','missing','present','present','missing','present']
    assert [r['object_id'] for r in rows]==list(map(str,doc.tma_core_ids(True)))
    assert_document(q.Project.open(str(path)).document(image),mapping,'grid')
