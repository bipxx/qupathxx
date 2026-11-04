"""Object merge: pinned QuPath behavior plus native atomic history and consumers."""
import hashlib
import json
import os
from pathlib import Path
import re

import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-annotation-merge-0.7.0'
CASES=json.loads((FIXTURE/'input.json').read_text())
REFERENCE={r['name']:r for r in json.loads((FIXTURE/'reference.json').read_text())}
GUI=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in annotation merge desktop')


def setup(case):
    doc=q.Document(q.Image(100,100,[50]*100*100*3));ids={}
    for n in case['nodes']:
        roi=q.Roi.rectangle(q.Rect(*n['rect'])) if n['rect'] else q.Roi.points([q.Point(*p) for p in n['points']])
        ids[n['token']]=doc.execute([q.ObjectEdit.create_geometry(roi,n['token'],ids.get(n['parent'],0),n['classification'])])[0]
    return doc,ids


def state(doc):
    return [(a.id,a.name,a.parent_id,a.class_name,a.geometry.wkt,a.kind) for a in doc.annotations]


def test_pinned_merge_capture_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_text())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    assert hashlib.sha256((ROOT/manifest['probe']).read_bytes()).hexdigest()==manifest['probe_sha256']
    assert len(CASES)==len(REFERENCE)==10
    assert all(r['changed'] and r['new_identity'] and r['measurements']==0 for r in REFERENCE.values())


@pytest.mark.parametrize('case',CASES,ids=lambda c:c['name'])
def test_merge_matches_pinned_geometry_objects_and_atomic_history(case):
    if not q.has_geos and sum(n['rect'] is not None for n in case['nodes'] if n['token'] in case['selected'])>1:
        pytest.skip('area union requires GEOS')
    doc,ids=setup(case);before=state(doc);revision=doc.revision
    selected=[ids[t] for t in case['selected']];ref=REFERENCE[case['name']]
    presentation=q.ObjectSelection(doc);presentation.replace(selected)
    merged=doc.merge_annotations(selected,discard_points=case['name']=='mixed-dimension')
    assert merged not in ids.values() and doc.revision==revision+1 and doc.undo_label=='Merge annotations'
    obj=doc.annotation(merged);assert obj.name=='Annotation' and obj.parent_id==0
    assert obj.class_name==(ref['classification'] or '') and obj.geometry.area==ref['area']
    tokens={value:key for key,value in ids.items()};tokens[merged]='merged'
    observed=sorted([dict(token=tokens[a.id],parent=tokens.get(a.parent_id),classification=a.class_name or None) for a in doc.annotations],key=lambda a:a['token'])
    assert observed==ref['objects']
    if obj.geometry.kind==q.RoiKind.POINTS:
        coordinates=[float(n) for n in re.findall(r'-?\d+(?:\.\d+)?',ref['geometry'])]
        assert {(p.x,p.y) for p in obj.geometry.paths[0]}==set(zip(coordinates[::2],coordinates[1::2]))
    else:
        rectangles=[n['rect'] for n in case['nodes'] if n['token'] in case['selected'] and n['rect']]
        for y in range(0,40):
            for x in range(0,70):
                expected=any(a<=x+.5<a+w and b<=y+.5<b+h for a,b,w,h in rectangles)
                assert obj.geometry.contains(q.Point(x+.5,y+.5))==expected
    # Selection is presentation state. The UI/managed controller selects the
    # returned identity; undo pruning never silently selects restored inputs.
    assert presentation.ids==[];presentation.replace([merged]);assert presentation.primary==merged
    after=state(doc);assert doc.undo() and state(doc)==before and presentation.ids==[]
    assert doc.redo() and state(doc)==after and presentation.ids==[]


@pytest.mark.parametrize('failure',['empty','duplicate','missing','line','detection','mixed','budget'])
def test_merge_rejection_preserves_document_redo_and_next_identity(failure):
    doc,ids=setup(CASES[0]);a,b=ids.values();chosen=[a,b]
    if failure=='empty':chosen=[]
    elif failure=='duplicate':chosen=[a,a]
    elif failure=='missing':chosen=[a,999]
    elif failure=='line':chosen=[a,doc.add_roi(q.Roi.polyline([q.Point(1,1),q.Point(2,2)]))]
    elif failure=='detection':chosen=[a,doc.add_detection(q.Roi.rectangle(q.Rect(1,1,2,2)))]
    elif failure=='mixed':chosen=[a,doc.add_roi(q.Roi.points([q.Point(80,80)]))]
    maximum=max(o.id for o in doc.annotations)
    doc.rename_annotation(a,'temporary');assert doc.undo()
    if failure=='budget':doc.set_history_limits(128,1)
    before=state(doc);revision=doc.revision;history=doc.history_bytes;redo=doc.can_redo
    with pytest.raises((ValueError,IndexError)):doc.merge_annotations(chosen)
    assert state(doc)==before and (doc.revision,doc.history_bytes,doc.can_redo)==(revision,history,redo)
    if failure=='budget':doc.set_history_limits(128,64*1024*1024)
    assert doc.add_rectangle(q.Rect(80,80,2,2))==maximum+1


def test_mixed_points_require_explicit_loss_admission():
    doc,ids=setup(next(c for c in CASES if c['name']=='mixed-dimension'))
    with pytest.raises(ValueError,match='discard_points'):doc.merge_annotations(list(ids.values()))
    merged=doc.merge_annotations(list(ids.values()),discard_points=True)
    assert doc.annotation(merged).geometry.area==400 and doc.undo()
    assert len(doc.annotations)==2


@pytest.mark.skipif(not q.has_project or not q.has_geos,reason='project/GEOS')
def test_merge_save_reopen_and_existing_boolean_contract(tmp_path):
    case=next(c for c in CASES if c['name']=='children');doc,ids=setup(case)
    a,b=ids['a'],ids['b'];doc.apply_boolean(a,[b],q.BooleanOp.UNION)
    assert doc.annotation(b).name=='b' and doc.annotation(a).geometry.area==600
    assert doc.undo()
    merged=doc.merge_annotations([a,b]);expected=state(doc)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'merged.qpxproj';project.save(str(path))
    restored=q.Project.open(str(path)).document(image)
    assert state(restored)==expected and restored.annotation(merged).class_name=='Tumor'
    assert restored.annotation(ids['child']).parent_id==0


def test_managed_merge_submission_limits_and_cancel(tmp_path):
    limits=q.ViewerLimits();limits.max_edits=2;session=q.ViewerSession(str(tmp_path/'not-opened'),limits)
    with pytest.raises((ValueError,IndexError)):session.merge_annotations(0,[1])
    with pytest.raises((ValueError,IndexError)):session.merge_annotations(1,[])
    with pytest.raises(ValueError):session.merge_annotations(1,[1,2,3])
    ticket=session.merge_annotations(1,[1,2]);event=session.poll(ticket)
    assert event.action=='merge_annotations' and event.state=='queued'
    assert session.cancel(ticket);assert session.take(ticket).state=='cancelled'


@GUI
@pytest.mark.skipif(not q.has_geos,reason='GEOS')
def test_imgui_merge_keyboard_undo_redo_and_selection(tmp_path):
    from test_gui_replay import event
    doc,ids=setup(CASES[0]);options=q.GuiOptions();options.settle_tiles=True
    options.inputs=[event(10,q.GuiInputKind.SELECT_ALL_KEY),event(20,q.GuiInputKind.MERGE_KEY),
                    event(30,q.GuiInputKind.UNDO_KEY),event(40,q.GuiInputKind.REDO_KEY)]
    capture=tmp_path/'merge.ppm'
    report=q.show(doc,frame_limit=50,capture_path=str(capture),options=options)
    assert report.replay_complete and doc.object_count==1 and doc.undo_label=='Merge annotations'
    assert doc.annotations[0].geometry.area==600 and doc.annotations[0].id not in ids.values()
    assert capture.exists() and report.selected_ids==[]


@GUI
@pytest.mark.skipif(not q.has_geos or not q.has_project,reason='GEOS/project')
def test_managed_merge_staleness_identity_and_saved_result(tmp_path):
    from test_viewer_session import completed,run
    doc,ids=setup(CASES[0]);project=q.Project();image=project.add_document(doc)
    source=tmp_path/'source.qpxproj';project.save(str(source));session=q.ViewerSession(str(source));saved=tmp_path/'merged.qpxproj'
    def producer():
        before=completed(session,session.inspect(image))
        ticket=session.merge_annotations(image,list(ids.values()),expected_revision=before.document_revision-1)
        assert session.wait(ticket,10).state=='stale';assert not session.take(ticket).committed
        event=completed(session,session.merge_annotations(image,list(ids.values()),expected_revision=before.document_revision))
        assert event.committed and event.object_ids==[event.object_id] and event.object_id not in ids.values()
        current=completed(session,session.inspect(image,event.object_id));assert current.object.geometry.area==600
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    run(session,producer)
    restored=q.Project.open(str(saved)).document(image)
    assert len(restored.annotations)==1 and restored.annotations[0].class_name=='Tumor'
