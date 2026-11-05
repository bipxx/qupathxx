"""Plane identity must constrain pixels, masks, edits and saved user workflows."""
from project_schema_contract import CURRENT_SCHEMA_VERSION
import csv
import io
import json
import os
import sqlite3

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_interchange import feature, rectangle
from test_qupath_saved_data import corpus
from test_raw_analysis import scene
from test_tiff_reader import request

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project or not q.has_interchange,reason='TIFF/project/interchange')


def plane_tuple(plane):return plane.c,plane.z,plane.t


def display(doc,z,t,channels=None):
    view=doc.server.tiff_view;view.z=z;view.t=t
    if channels is not None:view.channels=channels
    doc.set_tiff_view(view)


def imported(doc,c=-1,z=1,t=1):
    geometry=rectangle(1,2,5,6);geometry['plane']={'c':c,'z':z,'t':t}
    return q.import_geojson(doc,json.dumps(feature(geometry)),assume_pixels=True).imported_ids[0]


def test_import_view_mask_analysis_restart_and_export_use_exact_plane(tmp_path):
    doc,values,_=scene(tmp_path);obj=imported(doc)
    selection=q.ObjectSelection(doc)
    assert not doc.object_visible(obj) and doc.pick(q.Point(2,3))==[]
    with pytest.raises((ValueError,IndexError)):selection.replace([obj])
    display(doc,1,1);selection.select_all();assert selection.ids==[obj]
    assert doc.pick(q.Point(2,3))==[obj] and plane_tuple(doc.annotation(obj).plane)==(-1,1,1)
    capture=q.RawAnalysisRequest(doc,request(1,1,c=2,z=1,t=1),obj,tile_size=16)
    result=q.analyze_raw(capture);record=doc.keep_raw_analysis(capture,result)
    np.testing.assert_allclose(result.statistics.means,values[1,1,:,2:8,1:6].mean(axis=(1,2)),rtol=1e-12)
    assert result.statistics.valid_pixels==30 and doc.area_um2(obj)==pytest.approx(3.75)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'planes.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION
        assert db.execute('SELECT c,z,t FROM object_planes').fetchall()==[(-1,1,1)]
    restored=q.Project.open(str(path)).document(image)
    assert plane_tuple(restored.annotation(obj).plane)==(-1,1,1) and restored.object_visible(obj)
    replay=q.RawAnalysisRequest.replay(restored,record);again=q.analyze_raw(replay)
    assert again.input_sha256==result.input_sha256
    exported=q.export_geojson(restored).text;geometry=json.loads(exported)['features'][0]['geometry']
    assert geometry['plane']=={'c':-1,'z':1,'t':1}
    rows=list(csv.DictReader(io.StringIO(q.measurements_csv(restored))))
    assert (rows[0]['plane_c'],rows[0]['plane_z'],rows[0]['plane_t'])==('-1','1','1')
    copy=q.Document(q.open_tiff(str(tmp_path/'channels.ome.tif')));other=q.import_geojson(copy,exported).imported_ids[0]
    assert plane_tuple(copy.annotation(other).plane)==(-1,1,1) and not copy.object_visible(other)
    display(doc,0,0);assert selection.ids==[] and capture.matches(doc)
    doc.set_object_plane(obj,q.ObjectPlane(-1,2,1));assert not capture.matches(doc)
    with pytest.raises((ValueError,RuntimeError)):doc.keep_raw_analysis(capture,result)


def test_new_objects_follow_active_view_and_geometry_edits_preserve_plane(tmp_path):
    doc,_,_=scene(tmp_path);legacy=doc.add_rectangle(q.Rect(1,2,5,6))
    display(doc,2,1);obj=doc.add_rectangle(q.Rect(2,3,4,5))
    assert plane_tuple(doc.annotation(legacy).plane)==(-1,0,0)
    assert plane_tuple(doc.annotation(obj).plane)==(-1,2,1)
    doc.update_rectangle(obj,q.Rect(3,4,4,5));doc.rename_annotation(obj,'Moved')
    assert plane_tuple(doc.annotation(obj).plane)==(-1,2,1)
    doc.set_object_plane(obj,q.ObjectPlane(1,1,0));assert not doc.object_visible(obj)
    assert doc.undo() and plane_tuple(doc.annotation(obj).plane)==(-1,2,1)
    assert doc.redo() and plane_tuple(doc.annotation(obj).plane)==(1,1,0)


@pytest.mark.parametrize('c,z,t',[(2,0,0),(-1,3,0),(-1,0,2)])
def test_import_and_commands_refuse_absent_source_planes_atomically(tmp_path,c,z,t):
    doc,_,_=scene(tmp_path);obj=doc.add_rectangle(q.Rect(1,2,5,6));before=doc.revision
    with pytest.raises((ValueError,IndexError),match='plane is unavailable'):imported(doc,c,z,t)
    with pytest.raises((ValueError,IndexError),match='plane is unavailable'):doc.set_object_plane(obj,q.ObjectPlane(c,z,t))
    assert doc.revision==before and doc.object_count==1 and plane_tuple(doc.annotation(obj).plane)==(-1,0,0)


@pytest.mark.parametrize('plane',[{'z':-1},{'c':-2},{'t':65536},{'z':2**64-1},{'c':True},{'series':0}])
def test_foreign_plane_components_are_bounded_before_narrowing(tmp_path,plane):
    doc,_,_=scene(tmp_path);geometry=rectangle();geometry['plane']=plane
    with pytest.raises((ValueError,IndexError)):q.import_geojson(doc,json.dumps(feature(geometry)),assume_pixels=True)
    assert doc.object_count==0


def test_hierarchy_and_boolean_operations_cannot_mix_planes(tmp_path):
    doc,_,_=scene(tmp_path);parent=doc.add_rectangle(q.Rect(0,0,20,15));child=doc.add_rectangle(q.Rect(1,2,5,6));doc.reparent_annotation(child,parent)
    before=doc.revision
    with pytest.raises(ValueError,match='parent plane'):doc.set_object_plane(parent,q.ObjectPlane(-1,1,0))
    assert doc.revision==before
    doc.execute([q.ObjectEdit.plane(parent,q.ObjectPlane(-1,1,0)),q.ObjectEdit.plane(child,q.ObjectPlane(0,1,0))])
    assert plane_tuple(doc.annotation(child).plane)==(0,1,0)
    with pytest.raises(ValueError,match='share an image plane'):doc.merge_annotations([parent,child])
    with pytest.raises(ValueError,match='share an image plane'):doc.apply_boolean(parent,[child],q.BooleanOp.UNION)
    assert doc.undo() and plane_tuple(doc.annotation(parent).plane)==(-1,0,0) and plane_tuple(doc.annotation(child).plane)==(-1,0,0)


def test_channel_specific_masks_refuse_composite_and_wrong_raw_selection(tmp_path):
    doc,values,_=scene(tmp_path);obj=imported(doc,c=1)
    display(doc,1,1,[0,0,0]);assert not doc.object_visible(obj)
    display(doc,1,1,[0,1,-1]);assert doc.object_visible(obj)
    with pytest.raises(ValueError,match='single-channel'):doc.mean_intensity(obj)
    for first,channels,z,t in [(0,1,1,1),(0,2,1,1),(1,1,0,1),(1,1,1,0)]:
        region=request(1,1,c=channels,z=z,t=t);region.first_channel=first
        with pytest.raises(ValueError,match='object plane'):q.RawAnalysisRequest(doc,region,obj,tile_size=16)
    region=request(1,1,c=1,z=1,t=1);region.first_channel=1
    result=q.analyze_raw(q.RawAnalysisRequest(doc,region,obj,tile_size=16))
    np.testing.assert_allclose(result.statistics.means,[values[1,1,1,2:8,1:6].mean()],rtol=1e-12)
    display(doc,1,1,[1,1,1]);assert np.isfinite(doc.mean_intensity(obj))


def test_saved_data_plane_uses_a_valid_replacement_source_not_the_invalid_original(tmp_path):
    path=corpus(tmp_path,'plane');source=path.parent.parent/'source.ome.tif'
    # The original pinned fixture has only Z0 and must remain a refusal. This
    # explicitly derived source has two real Z planes with independent values.
    values=np.stack([np.full((48,64,3),11,dtype='uint8'),np.full((48,64,3),79,dtype='uint8')])
    tifffile.imwrite(source,values,ome=True,photometric='rgb',metadata={'axes':'ZYXS','PhysicalSizeX':.25,'PhysicalSizeY':.5})
    manifest=json.loads(path.read_text(encoding='utf-8'));manifest['images'][0]['serverBuilder']['metadata']['sizeZ']=2;path.write_text(json.dumps(manifest),encoding='utf-8')
    data=path.parent/'data/3/data.qpdata';raw=data.read_bytes();assert raw.count(b'"sizeZ":1')==1
    # Same-length metadata scalar patch declares the derived source truthfully;
    # all original ROI bytes remain unchanged. This is not a new upstream capture.
    data.write_bytes(raw.replace(b'"sizeZ":1',b'"sizeZ":2'))
    preview=q.preview_qupath_project(str(path));assert {i.code for i in preview.issues}=={'image_settings_dropped'}
    destination=tmp_path/'native.qpxproj';preview.import_new(str(destination),allow_losses=True)
    doc=q.Project.open(str(destination)).document(preview.images[0].native_id);obj=doc.annotations[0]
    assert plane_tuple(obj.plane)==(-1,1,0) and not doc.object_visible(obj.id)
    display(doc,1,0)
    result=q.analyze_raw(q.RawAnalysisRequest(doc,request(1,1,z=1),obj.id,tile_size=16))
    assert result.statistics.valid_pixels==100 and result.statistics.means==[79.0]


def test_nondefault_plane_requires_supported_reader():
    doc=q.Document(q.Image.demo());obj=doc.add_rectangle(q.Rect(1,2,5,6))
    with pytest.raises(ValueError,match='quantitative TIFF'):doc.set_object_plane(obj,q.ObjectPlane(-1,1,0))
    assert plane_tuple(doc.annotation(obj).plane)==(-1,0,0)


def test_plane_snapshot_ownership_and_missing_source_reopen(tmp_path):
    doc,_,source=scene(tmp_path);obj=imported(doc);project=q.Project();image=project.add_document(doc)
    frozen=tmp_path/'frozen.qpxproj';job=q.ProjectSaveJob();assert job.request(project,str(frozen))
    doc.set_object_plane(obj,q.ObjectPlane(-1,2,1));assert job.wait().applied
    assert plane_tuple(q.Project.open(str(frozen)).document(image).annotation(obj).plane)==(-1,1,1)
    before=tmp_path/'before.qpxproj';project.save(str(before));doc.server.close();source.rename(source.with_suffix('.missing'))
    restored=q.Project.open(str(before)).document(image)
    assert plane_tuple(restored.annotation(obj).plane)==(-1,2,1)


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in real desktop')
def test_desktop_plane_switch_filters_selection_and_measures_matching_pixels(tmp_path):
    from test_gui_raw import options
    from test_gui_tma import click
    from test_gui_tiles import replay
    doc,values,_=scene(tmp_path);legacy=doc.add_rectangle(q.Rect(1,2,5,6))
    geometry=rectangle(1,2,5,6);geometry['plane']={'c':-1,'z':1,'t':1};exchange=tmp_path/'plane.geojson';exchange.write_text(json.dumps(feature(geometry)),encoding='utf-8')
    project=q.Project();image=project.add_document(doc);o=options();o.width=1600;o.height=1400;o.docked_workspace=True
    preset=q.TiffOpenOptions();preset.view.z=1;preset.view.t=1;preset.view.channels=[0,1,-1];preset.view.lower=[0,0,0];preset.view.upper=[5000,5000,5000];o.tiff_options=preset
    o.interchange_path=str(exchange);o.interchange_assume_pixels=True
    o.raw_request=request(1,1,c=2,z=1,t=1)
    def wait(frame,condition):
        value=replay(frame,q.GuiInputKind.MOVE);value.wait_for=condition;return value
    o.inputs=[replay(2,q.GuiInputKind.IMPORT_KEY)]+click(5,q.GuiInputTarget.TIFF_APPLY)+[wait(10,q.GuiInputWait.TIFF_VIEW_APPLIED),replay(12,q.GuiInputKind.SELECT_ALL_KEY)]
    o.inputs+=click(15,q.GuiInputTarget.RAW_DATA_TAB)+click(18,q.GuiInputTarget.RAW_MEASURE)+[wait(22,q.GuiInputWait.RAW_TASK_FINISHED)]
    o.inputs+=click(25,q.GuiInputTarget.RAW_RESULTS_TAB)+click(28,q.GuiInputTarget.RAW_KEEP)+[wait(32,q.GuiInputWait.RAW_ANALYSIS_KEPT)]
    report=q.show(project,frame_limit=42,capture_path=str(tmp_path/'plane-view.ppm'),options=o)
    obj=next(a.id for a in doc.annotations if a.plane.z==1 and a.plane.t==1)
    assert not report.raw_error,report.raw_error
    assert report.tiff_view_changes==1 and report.selected_ids==[obj] and legacy not in report.selected_ids
    assert report.raw_analysis_result.object_id==obj and report.raw_analysis_result.statistics.valid_pixels==30
    np.testing.assert_allclose(report.raw_analysis_result.statistics.means,values[1,1,:,2:8,1:6].mean(axis=(1,2)),rtol=1e-12)
    assert report.raw_records==1 and report.ui_overflow_items==0
    path=tmp_path/'desktop.qpxproj';project.save(str(path));restored=q.Project.open(str(path)).document(image)
    assert plane_tuple(restored.annotation(obj).plane)==(-1,1,1) and restored.raw_analysis_record_count==1


def test_explicit_default_creation_and_parent_inheritance_override_display_plane(tmp_path):
    doc,_,_=scene(tmp_path);parent=doc.add_rectangle(q.Rect(0,0,20,15));doc.set_object_plane(parent,q.ObjectPlane(1,1,1))
    display(doc,2,0)
    child=doc.add_detection(q.Roi.rectangle(q.Rect(1,2,5,6)),parent_id=parent)
    assert plane_tuple(doc.annotation(child).plane)==(1,1,1)
    edit=q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(1,2,5,6)),plane=q.ObjectPlane())
    explicit=doc.execute([edit])[0];assert plane_tuple(doc.annotation(explicit).plane)==(-1,0,0)


def test_unchanged_plane_metadata_edit_does_not_open_missing_link(tmp_path):
    doc,_,source=scene(tmp_path);obj=imported(doc);project=q.Project();image=project.add_document(doc)
    path=tmp_path/'missing.qpxproj';project.save(str(path));doc.server.close();source.rename(source.with_suffix('.missing'))
    restored=q.Project.open(str(path)).document(image);restored.rename_annotation(obj,'Still editable')
    assert restored.annotation(obj).name=='Still editable' and plane_tuple(restored.annotation(obj).plane)==(-1,1,1)


@pytest.mark.skipif(not q.has_classifier_training or not q.has_geos,reason='actual forest backend and GEOS')
@pytest.mark.parametrize('streamed',[False,True])
def test_raw_contour_publication_uses_recipe_plane_while_another_plane_is_displayed(tmp_path,streamed):
    from test_raw_pixel_python import trained
    doc,_,_=scene(tmp_path)
    ids=[doc.add_detection(q.Roi.rectangle(q.Rect(0,0,10,16)),class_name='A'),doc.add_detection(q.Roi.rectangle(q.Rect(14,0,10,16)),class_name='B')]
    for obj in ids:doc.set_object_plane(obj,q.ObjectPlane(-1,1,1))
    grid=q.ScientificGrid(24,16,q.ScientificGridAxis(0,24,24),q.ScientificGridAxis(0,16,16))
    selection=q.ScientificRawOptions();selection.z=1;selection.t=1;selection.channels=2
    features=q.RawPixelFeatureOptions();features.offsets=[0,0];features.scales=[1,1]
    options=q.RawPixelTrainingOptions();options.max_samples=32;options.max_samples_per_class=16;options.work_bytes=16*1024**2
    capture=q.RawPixelTrainingRequest(doc,ids,grid,selection,features,options);model=trained(q.sample_raw_pixel_training(capture))
    if streamed:
        stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=8;stream.work_bytes=16*1024**2
        request=q.RawPixelTiledPredictionRequest(doc,model,q.ScientificRawLattice(grid),stream)
        contour=q.RawPixelStreamContourOptions();contour.work_bytes=32*1024**2;contour.boundary_work_bytes=2*1024**2
        geometry=contour.contours;geometry.work_bytes=2*1024**2;contour.contours=geometry
        result=q.trace_raw_pixel_stream(request,contour);published=q.publish_raw_pixel_stream_contours(doc,request,result)
    else:
        request=q.RawPixelPredictionRequest(doc,model,grid);result=q.trace_raw_pixel_prediction(request,q.predict_raw_pixel_region(request))
        published=q.publish_raw_pixel_contours(doc,request,result)
    assert published and plane_tuple(doc.active_plane)==(-1,0,0)
    assert all(plane_tuple(doc.annotation(obj).plane)==(-1,1,1) and not doc.object_visible(obj) for obj in published)
    assert doc.undo() and not any(obj in [a.id for a in doc.annotations] for obj in published)
    assert doc.redo() and all(plane_tuple(doc.annotation(obj).plane)==(-1,1,1) for obj in published)


@pytest.mark.parametrize('fault',['orphan','index'])
def test_persisted_planes_reject_corrupt_rows(tmp_path,fault):
    doc,_,_=scene(tmp_path);obj=imported(doc);project=q.Project();project.add_document(doc);path=tmp_path/'corrupt.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        if fault=='orphan':db.execute("UPDATE object_planes SET object_id='999999'")
        else:db.execute('PRAGMA ignore_check_constraints=ON');db.execute('UPDATE object_planes SET z=65536')
    with pytest.raises((ValueError,RuntimeError,IndexError)):q.Project.open(str(path))


def test_schema17_default_plane_upgrade_rolls_back_without_modifying_old_project(tmp_path):
    doc,_,_=scene(tmp_path);obj=doc.add_rectangle(q.Rect(1,2,5,6));project=q.Project();image=project.add_document(doc);path=tmp_path/'legacy17.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('SELECT count(*) FROM object_planes').fetchone()[0]==0
        db.execute("DROP TABLE IF EXISTS image_axis_calibration");db.execute("DROP TABLE IF EXISTS image_data_properties");db.execute("DROP TABLE IF EXISTS image_analysis_settings");db.execute("DROP TABLE IF EXISTS image_properties");db.execute('DROP TABLE object_planes');db.execute('PRAGMA user_version=17')
        db.execute("CREATE TRIGGER fail_plane_upgrade BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'plane upgrade failed'); END")
    baseline=path.read_bytes();loaded=q.Project.open(str(path));current=loaded.document(image)
    assert path.read_bytes()==baseline and plane_tuple(current.annotation(obj).plane)==(-1,0,0)
    current.set_object_plane(obj,q.ObjectPlane(-1,1,1))
    with pytest.raises(RuntimeError,match='plane upgrade failed'):loaded.save(str(path))
    assert path.read_bytes()==baseline
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==17
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='object_planes'").fetchone()[0]==0
        db.execute('DROP TRIGGER fail_plane_upgrade')
    loaded.save(str(path));assert plane_tuple(q.Project.open(str(path)).document(image).annotation(obj).plane)==(-1,1,1)


def test_2d_spatial_measurements_refuse_mixed_z_or_time(tmp_path):
    doc,_,_=scene(tmp_path);first=doc.add_rectangle(q.Rect(1,2,5,6));second=doc.add_rectangle(q.Rect(8,2,5,6))
    doc.set_object_plane(second,q.ObjectPlane(-1,1,0))
    with pytest.raises(ValueError,match='one Z/time plane'):q.SpatialDistanceRequest(doc,[first],[second])
    doc.set_object_plane(first,q.ObjectPlane(0,1,0));result=q.measure_spatial_distances(q.SpatialDistanceRequest(doc,[first],[second]))
    assert result.rows()[0].distance==pytest.approx(7.0)


def test_axisless_qptma_import_preserves_default_plane_and_export_refuses_plane_loss(tmp_path):
    doc,_,_=scene(tmp_path);options=q.TmaGridOptions();options.rows=options.columns=1;options.origin=q.Point(8,8);options.width=options.height=4
    doc.create_tma_grid(options);data=q.qptma_from_grid(doc.tma_grid()).data;doc.clear_tma_grid(True)
    display(doc,1,1);obj=q.import_qptma(doc,data)[0]
    assert plane_tuple(doc.annotation(obj).plane)==(-1,0,0) and not doc.object_visible(obj)
    doc.set_object_plane(obj,q.ObjectPlane(-1,1,1))
    with pytest.raises(ValueError,match='cannot represent a non-default object plane'):q.qptma_from_grid(doc.tma_grid())


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in managed desktop')
def test_managed_merge_keeps_hidden_result_plane_without_failing_automatic_selection(tmp_path):
    from test_viewer_session import completed,run
    doc,_,_=scene(tmp_path);ids=[doc.add_rectangle(q.Rect(1,2,5,6)),doc.add_rectangle(q.Rect(4,2,5,6))]
    for obj in ids:doc.set_object_plane(obj,q.ObjectPlane(-1,1,1))
    project=q.Project();image=project.add_document(doc);source=tmp_path/'source.qpxproj';project.save(str(source))
    session=q.ViewerSession(str(source));saved=tmp_path/'merged.qpxproj';held={}
    def producer():
        completed(session,session.inspect(image));event=completed(session,session.merge_annotations(image,ids))
        assert event.committed;held['id']=event.object_id
        current=completed(session,session.inspect(image,event.object_id));assert plane_tuple(current.object.plane)==(-1,1,1)
        completed(session,session.save(str(saved)));completed(session,session.close(True))
    report=run(session,producer);assert report.selected_ids==[]
    restored=q.Project.open(str(saved)).document(image);assert plane_tuple(restored.annotation(held['id']).plane)==(-1,1,1)
