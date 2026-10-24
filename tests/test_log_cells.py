"""Installed native LoG workflow; frozen references and immutable saved recipes."""
import csv
import gzip
import hashlib
import struct
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
from test_qupath_cells_reference import comparison, CONTRACT, CAPTURE
from test_gui_replay import event
from test_tissue_levels import pyramid_levels

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('log_phantoms',ROOT/'tools/qupath_cells_fixture.py')
phantoms=importlib.util.module_from_spec(spec);spec.loader.exec_module(phantoms)


def parameters(tile=64):
    p=q.CellParameters()
    for name,value in CONTRACT['native'].items():setattr(p,name,value)
    p.algorithm=q.CellAlgorithm.LOG_WATERSHED;p.tile_size=tile
    return p


def scene(case=None):
    case=case or next(c for c in CONTRACT['cases'] if c['name']=='isolated_three')
    rgb=phantoms.pixels(CONTRACT,case);doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(*case['pixel_size_um'])
    b=case.get('roi',[0,0,CONTRACT['width'],CONTRACT['height']])
    if 'hole' in case:
        def ring(rect):
            x,y,w,h=rect;return [q.Point(x,y),q.Point(x+w,y),q.Point(x+w,y+h),q.Point(x,y+h)]
        obj=doc.add_roi(q.Roi.polygon(ring(b),[ring(case['hole'])]))
    else:obj=doc.add_rectangle(q.Rect(*b))
    return doc,obj,q.StainTransform(CONTRACT['vectors'],CONTRACT['white'])


def parse(text):
    reader=csv.DictReader(io.StringIO(text,newline=''));rows=list(reader)
    assert all(None not in row for row in rows)
    return rows


@pytest.mark.skipif(not q.has_geos,reason='polygon source ROI')
@pytest.mark.parametrize('case',CONTRACT['cases'],ids=lambda c:c['name'])
@pytest.mark.parametrize('tile',CONTRACT['tile_sizes'])
def test_log_installed_nucleus_reference(case,tile):
    comparison.verify_capture(CAPTURE)
    doc,obj,t=scene(case);request=q.CellRequest(doc,obj,t,parameters(tile));result=q.detect_cells(request)
    assert result.algorithm=='qpx.log-watershed/1'
    assert result.nuclei.input_format=='qpx.log-rgba-tiles/1'
    assert q.input_sha256(request)==result.nuclei.input_sha256
    labels=np.zeros((CONTRACT['height'],CONTRACT['width']),dtype=np.uint32)
    g=result.nuclei.level_bounds;x,y=int(g.x),int(g.y)
    labels[y:y+result.nuclei.height,x:x+result.nuclei.width]=result.nuclei.labels_numpy()
    ref=np.frombuffer(gzip.decompress((CAPTURE/case['name']/'nucleus.u32le.gz').read_bytes()),dtype='<u4').reshape(labels.shape)
    metric=comparison.metrics(ref,labels,CONTRACT['comparison'])
    assert result.cell_count==metric['reference_count']==metric['native_count']
    assert not metric['unmatched_native'] and not metric['unmatched_reference']
    if case['name'] not in ['roi_cut_hole','resampled_reference']:
        assert metric['foreground_iou']==1 and all(m['iou']==1 and m['centroid_error_px']==0 for m in metric['matches'])
    if case['name']=='isolated_three':
        assert result.cell_count==case['baseline_count']
        assert all(m['iou']>=CONTRACT['comparison']['isolated_nucleus_iou_min'] and m['centroid_error_px']<=CONTRACT['comparison']['isolated_centroid_error_max_px'] for m in metric['matches'])
    assert request.total_tiles==math.ceil(g.width/tile)*math.ceil(g.height/tile)
    assert result.nuclei.foreground_pixels<=result.nuclei.threshold_pixels<=result.nuclei.samples
    assert all(f.cell_pixels>=f.nucleus_pixels>=parameters().min_nucleus_pixels for f in result.features)
    assert result.retained_bytes<=request.estimated_work_bytes<=request.parameters.work_bytes
    rgb=phantoms.pixels(CONTRACT,case)[y:y+result.nuclei.height,x:x+result.nuclei.width]
    vectors=np.asarray(CONTRACT['vectors'],dtype=float)
    vectors/=np.linalg.norm(vectors,axis=1)[:,None]
    od=np.maximum(0,np.log10(np.asarray(CONTRACT['white'])/np.maximum(rgb.astype(float),1)))
    if len(vectors)==2:
        residual=np.cross(*vectors);vectors=np.vstack([vectors,residual/np.linalg.norm(residual)])
    channels=(od if request.parameters.feature==q.TissueFeature.OPTICAL_DENSITY_SUM else np.linalg.solve(vectors.T,od.reshape(-1,3).T).T.reshape(rgb.shape)).astype(np.float32).astype(float)
    nuclei,bodies=result.nuclei.labels_numpy(),result.cells.labels_numpy()
    for f in result.features:
        masks=[nuclei==f.label,bodies==f.label,(bodies==f.label)&(nuclei!=f.label)]
        assert f.nucleus_pixels==int(masks[0].sum()) and f.cell_pixels==int(masks[1].sum())
        for compartment,mask in zip(['nucleus','cell','cytoplasm'],masks):
            if mask.any():
                np.testing.assert_allclose(getattr(f,compartment+'_rgb'),rgb[mask].mean(axis=0),atol=1e-12)
                np.testing.assert_allclose(getattr(f,compartment+'_channels'),channels[mask].mean(axis=0),atol=1e-6)
            else:
                assert getattr(f,compartment+'_rgb') is None and getattr(f,compartment+'_channels') is None



def test_detector_parameters_are_captured_and_inactive_options_reject():
    assert q.CellParameters().algorithm==q.CellAlgorithm.DISTANCE_WATERSHED
    doc,obj,t=scene();p=parameters();request=q.CellRequest(doc,obj,t,p);p.algorithm=q.CellAlgorithm.DISTANCE_WATERSHED
    assert request.parameters.algorithm==q.CellAlgorithm.LOG_WATERSHED
    p.context_padding=33
    with pytest.raises(ValueError,match='LoG'):p.validate()
    p=parameters();p.log_prominence=0
    with pytest.raises(ValueError):p.validate()
    p=parameters();p.context_padding=4097
    with pytest.raises(ValueError):p.validate()


@pytest.mark.skipif(not q.has_project,reason='native project storage')
@pytest.mark.parametrize('calibrated',[False,True])
def test_log_saved_recipe_replay_tables_and_classifier_sources(tmp_path,calibrated):
    doc,obj,t=scene();p=parameters();p.log_prominence=.75;p.context_padding=48
    request=q.CellRequest(doc,obj,t,p);result=q.detect_cells(request);record=doc.keep_cells(request,result)
    options=q.CellTableOptions();options.include_capture_calibration=calibrated
    rows=parse(q.cell_measurements_csv(q.CellTableRequest(doc,[record],options)))
    assert len(rows)==3
    assert {r['table_schema'] for r in rows}=={'qpx.cell-table/'+('4' if calibrated else '3')}
    assert all(r['detector_mode']=='log' and float(r['log_peak_prominence'])==.75 and int(r['context_padding_sample_px'])==48 for r in rows)
    if q.has_geos:
        ids=doc.publish_cell_record(record)
        from test_object_classifier_archives import predicted
        feature_options=q.ObjectFeatureOptions();feature_options.cell_rgb=True
        _,job=predicted(doc,ids,feature_options);job.apply(doc)
        source=doc.classifier_records[0].archive.table.cell_origin(0).source
        assert source.parameters.algorithm==p.algorithm and source.parameters.log_prominence==.75
    project=q.Project();image=project.add_document(doc);path=tmp_path/'log.qpxproj';project.save(str(path))
    loaded=q.Project.open(str(path)).document(image);historical=loaded.cell_record(record).result
    assert historical.parameters.algorithm==p.algorithm and historical.parameters.log_prominence==.75 and historical.parameters.context_padding==48
    assert historical.nuclei.labels==result.nuclei.labels and historical.cells.labels==result.cells.labels
    replay=q.CellRequest.replay(loaded,record);assert loaded.verify_cells(record,replay,q.detect_cells(replay))
    if q.has_geos:
        source=loaded.classifier_records[0].archive.table.cell_origin(0).source
        assert source.algorithm==result.algorithm and source.parameters.context_padding==48
        assert source.parameters.algorithm==p.algorithm and source.parameters.log_prominence==.75


@pytest.mark.skipif(not q.has_project,reason='native batch storage')
def test_log_batch_resume_and_cli_recipe(tmp_path):
    doc,obj,t=scene();project=q.Project();image=project.add_document(doc)
    o=q.ProjectBatchOptions();o.kind=q.ProjectBatchKind.CELLS;o.cells=parameters()
    request=q.ProjectBatchRequest(project,t,options=o);run=q.run_project_batch(request,str(tmp_path/'batch'))
    assert run.state=='succeeded' and run.saved==1
    rows=parse(q.batch_checkpoint_csv(run.images[0].path));assert len(rows)==3 and all(r['detector_mode']=='log' for r in rows)
    again=q.run_project_batch(request,str(tmp_path/'batch'));assert again.reused==1
    altered=parameters();altered.context_padding=48;o.cells=altered
    changed=q.run_project_batch(q.ProjectBatchRequest(project,t,options=o),str(tmp_path/'batch'));assert changed.reused==0 and changed.saved==1
    path=tmp_path/'input.qpxproj';project.save(str(path))
    from qupathxx import _native
    exe=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    done=subprocess.run([str(exe),str(path),str(tmp_path/'cli'),'--kind','cells','--cell-algorithm','log','--threshold','.4','--cell-context-padding','48'],capture_output=True,text=True)
    assert done.returncode==0,done.stdout+done.stderr
    reports=[json.loads(line) for line in done.stdout.splitlines()]
    assert len(reports)==2 and reports[0]["schema"]=="qpx.batch-image/1"
    report=reports[-1]
    cli_rows=parse(q.batch_checkpoint_csv(report['images'][0]['path']))
    assert cli_rows and all(r['detector_mode']=='log' and r['context_padding_sample_px']=='48' for r in cli_rows)
    o.kind=q.ProjectBatchKind.ROI
    with pytest.raises(ValueError,match='LoG'):o.validate()


@pytest.mark.skipif(not q.has_gui or not q.has_geos or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop')
def test_log_gui_and_copied_recipe(tmp_path):
    doc,obj,t=scene();p=parameters();expected=q.detect_cells(q.CellRequest(doc,obj,t,p))
    options=q.GuiOptions();options.cell_parameters=p;p.algorithm=q.CellAlgorithm.DISTANCE_WATERSHED
    assert options.cell_parameters.algorithm==q.CellAlgorithm.LOG_WATERSHED
    options.inputs=[event(2,q.GuiInputKind.SELECT_ALL_KEY),event(4,q.GuiInputKind.DETECT_CELLS_KEY),event(10,q.GuiInputKind.KEEP_CELLS_KEY,wait=q.GuiInputWait.CELLS_READY)]
    report=q.show(doc,frame_limit=50,capture_path=str(tmp_path/'log.ppm'),options=options)
    assert report.cell_state=='ready' and not report.cell_error
    assert report.cell_result.algorithm==expected.algorithm
    assert report.cell_result.nuclei.labels==expected.nuclei.labels and report.cells_published
    assert report.cell_annotations_added==3 and report.ui_overflow_items==0



def test_context_pixels_change_only_the_contextual_digest():
    a=np.full((192,192,3),255,dtype=np.uint8);b=a.copy();b[64,96]=0
    def digest(rgb,p):
        doc=q.Document(q.Image.from_numpy(rgb));obj=doc.add_rectangle(q.Rect(80,80,32,32))
        return q.input_sha256(q.CellRequest(doc,obj,q.StainTransform(CONTRACT['vectors']),p))
    assert digest(a,parameters())!=digest(b,parameters())
    assert digest(a,q.CellParameters())==digest(b,q.CellParameters())


def test_context_budget_is_admitted_before_launch():
    doc=q.Document(q.Image(2048,2048,[255]*(2048*2048*3)));obj=doc.add_rectangle(q.Rect(1000,1000,16,16))
    p=parameters();p.work_bytes=128*1024*1024
    assert q.CellRequest(doc,obj,q.StainTransform(CONTRACT['vectors']),p).estimated_work_bytes<=p.work_bytes
    p.context_padding=512
    with pytest.raises((ValueError,RuntimeError),match='budget'):
        q.CellRequest(doc,obj,q.StainTransform(CONTRACT['vectors']),p)


def test_log_physical_area_limits_and_stale_calibration():
    doc,obj,t=scene();p=parameters()
    request=q.CellRequest.with_nucleus_area_limits(doc,obj,t,40.,80.,p)
    result=q.detect_cells(request)
    assert result.parameters.algorithm==p.algorithm and result.parameters.min_nucleus_pixels==160 and result.parameters.max_nucleus_pixels==320
    assert result.cell_count==1 and result.calibrated_feature(1).nucleus_sample_area_um2==64
    doc.set_calibration(.75,.5);assert not request.matches(doc,obj)


@pytest.mark.skipif(not q.has_tiff,reason='native RGBA TIFF')
def test_log_transparent_pixels_never_become_labels(tmp_path):
    import tifffile
    rgb=phantoms.pixels(CONTRACT,next(c for c in CONTRACT['cases'] if c['name']=='isolated_three'))
    rgba=np.concatenate([rgb,np.full((*rgb.shape[:2],1),255,dtype=np.uint8)],axis=2)
    rgba[55:65,43:53,3]=0;rgba[95:100,110:115,3]=128
    path=tmp_path/'alpha.tif';tifffile.imwrite(path,rgba,photometric='rgb',extrasamples='unassalpha',metadata=None)
    doc=q.Document(q.open_tiff(str(path)));obj=doc.add_rectangle(q.Rect(0,0,256,192));r=q.detect_cells(q.CellRequest(doc,obj,q.StainTransform(CONTRACT['vectors']),parameters()))
    opaque=rgba[...,3]==255
    assert r.nuclei.samples==int(opaque.sum())
    assert not r.nuclei.labels_numpy()[~opaque].any() and not r.cells.labels_numpy()[~opaque].any()


@pytest.mark.parametrize('tile',[16,64])
def test_log_natural_pyramid_context_hash(pyramid_levels,tile):
    server,_=pyramid_levels;doc=q.Document(server);obj=doc.add_rectangle(q.Rect(90,90,120,90));p=parameters(tile);p.level=1
    request=q.CellRequest(doc,obj,q.StainTransform(CONTRACT['vectors']),p);r=q.detect_cells(request);g=request.level_bounds
    assert r.nuclei.downsample==1.5 and r.nuclei.samples==80*60
    assert r.nuclei.input_format=='qpx.log-rgba-level-tiles/1'
    digest=hashlib.sha256(b'qpx.log-rgba-level-tiles/1')
    def integers(*values):digest.update(struct.pack('<'+'Q'*len(values),*values))
    level=server.metadata.levels[1];integers(server.metadata.width,server.metadata.height,1,level.width,level.height)
    digest.update(struct.pack('<d',1.5));x0,y0,x1,y1=int(g.x),int(g.y),int(g.x+g.width),int(g.y+g.height);integers(x0,y0,x1,y1,tile)
    for y in range(y0,y1,tile):
        for x in range(x0,x1,tile):
            w,h=min(tile,x1-x),min(tile,y1-y);rx,ry=math.floor(x*1.5+.5),math.floor(y*1.5+.5)
            pixels=server.read_region(q.RegionRequest(rx,ry,1,w,h));integers(x,y,rx,ry,w,h);digest.update(pixels.data)
    assert digest.hexdigest()==r.nuclei.input_sha256==q.input_sha256(request)


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='managed desktop')
def test_log_managed_viewer_captures_detector_recipe(tmp_path):
    from test_viewer_session import completed,run
    doc,obj,t=scene();project=q.Project();image=project.add_document(doc);source=tmp_path/'source.qpxproj';project.save(str(source))
    session=q.ViewerSession(str(source));destination=tmp_path/'saved.qpxproj'
    def producer():
        p=parameters();ticket=session.cells(image,obj,t,p,retain_result=True);p.context_padding=48;p.log_prominence=2
        result=completed(session,ticket);assert result.committed and result.cells.cell_count==3
        assert result.cells.parameters.algorithm==q.CellAlgorithm.LOG_WATERSHED and result.cells.parameters.context_padding==32 and result.cells.parameters.log_prominence==.5
        completed(session,session.save(str(destination)));completed(session,session.close(True))
    run(session,producer)
    r=q.Project.open(str(destination)).document(image).latest_cells(obj).result
    assert r.algorithm=='qpx.log-watershed/1' and r.parameters.context_padding==32


@pytest.mark.parametrize('change',['selection','rename','close','authority','cancel'])
def test_log_jobs_reject_stale_results_and_cancel(change):
    doc,obj,t=scene();job=q.CellJob();assert job.request(q.CellRequest(doc,obj,t,parameters()))
    selected=obj
    if change=='selection':selected=0
    elif change=='rename':doc.rename_annotation(obj,'changed');doc.undo()
    elif change=='close':doc.server.close()
    elif change=='authority':doc=q.Document(doc.server);assert doc.add_rectangle(q.Rect(0,0,256,192))==obj
    else:job.cancel()
    assert job.wait(doc,selected).state==('cancelled' if change=='cancel' else 'stale')
    assert job.result is None
    with pytest.raises(RuntimeError,match='no current'):job.keep_result(doc,selected)


def test_log_candidate_failure_is_atomic_and_worker_reusable():
    doc,obj,t=scene();p=parameters();p.max_cells=1;job=q.CellJob();revision=doc.revision
    assert job.request(q.CellRequest(doc,obj,t,p))
    report=job.wait(doc,obj)
    assert report.state=='failed' and 'limit' in report.error
    assert job.result is None and doc.revision==revision and doc.object_count==1
    assert job.request(q.CellRequest(doc,obj,t,parameters()))
    assert job.wait(doc,obj).state=='ready' and job.result.cell_count==3


@pytest.mark.parametrize('chosen',[0,1,2])
def test_log_auto_level_uses_expanded_context_budget(pyramid_levels,chosen):
    server,_=pyramid_levels;doc=q.Document(server);obj=doc.add_rectangle(q.Rect(90,90,120,90))
    t=q.StainTransform(CONTRACT['vectors']);p=parameters();p.context_padding=64
    estimates=[]
    for level in range(3):
        p.level=level;estimates.append(q.CellRequest(doc,obj,t,p).estimated_work_bytes)
    assert estimates[0]>estimates[1]>estimates[2]
    p.level=-1;p.work_bytes=estimates[chosen]
    request=q.CellRequest(doc,obj,t,p)
    assert request.level==chosen and request.parameters.level==-1 and request.estimated_work_bytes==estimates[chosen]
    result=q.detect_cells(request)
    assert result.nuclei.level==chosen and result.retained_bytes<=request.estimated_work_bytes
    if chosen:
        p.level=chosen-1
        with pytest.raises((ValueError,RuntimeError),match='budget'):q.CellRequest(doc,obj,t,p)
    p.level=-1;p.work_bytes=estimates[2]-1
    with pytest.raises((ValueError,RuntimeError),match='budget'):q.CellRequest(doc,obj,t,p)


@pytest.mark.skipif(not q.has_geos or not q.has_project,reason='mixed saved cell sources')
def test_mixed_detector_tables_and_classifier_sources_roundtrip(tmp_path):
    doc,obj,t=scene();records=[];ids=[];modes={}
    for mode in [q.CellAlgorithm.LOG_WATERSHED,q.CellAlgorithm.DISTANCE_WATERSHED]:
        p=parameters();p.algorithm=mode
        if mode==q.CellAlgorithm.LOG_WATERSHED:p.log_prominence=.75;p.context_padding=48
        request=q.CellRequest(doc,obj,t,p);result=q.detect_cells(request);record=doc.keep_cells(request,result)
        records.append(record);published=doc.publish_cell_record(record);ids+=published
        for identity in published:modes[identity]=mode
    rows=parse(q.cell_measurements_csv(q.CellTableRequest(doc,records)))
    assert rows and all(r['table_schema']=='qpx.cell-table/3' for r in rows)
    assert {r['detector_mode'] for r in rows}=={'distance','log'}
    assert all(r['log_peak_prominence']=='' and r['context_padding_sample_px']=='' for r in rows if r['detector_mode']=='distance')
    from test_object_classifier_archives import predicted
    options=q.ObjectFeatureOptions();options.cell_rgb=True
    _,job=predicted(doc,ids,options);job.apply(doc)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'mixed.qpxproj';project.save(str(path))
    loaded=q.Project.open(str(path)).document(image);table=loaded.classifier_records[0].archive.table
    for row,identity in enumerate(ids):
        origin=table.cell_origin(row);mode=modes[identity]
        assert origin.source.parameters.algorithm==mode
        assert origin.source.algorithm==('qpx.log-watershed/1' if mode==q.CellAlgorithm.LOG_WATERSHED else 'qpx.distance-watershed/1')
        assert origin.source.parameters.context_padding==(48 if mode==q.CellAlgorithm.LOG_WATERSHED else 32)
        assert origin.source.parameters.log_prominence==(.75 if mode==q.CellAlgorithm.LOG_WATERSHED else .5)


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='desktop batch LoG recipe')
def test_log_gui_batch_captures_recipe_and_checkpoint(tmp_path):
    doc,obj,t=scene();project=q.Project();image=project.add_document(doc)
    preset=q.ProjectBatchOptions();preset.kind=q.ProjectBatchKind.CELLS;preset.cells=parameters()
    # The inspector uses the identity basis for an OD-sum workflow.
    expected=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]],CONTRACT['white']),options=preset)
    options=q.GuiOptions();options.batch_options=preset;options.batch_output_directory=str(tmp_path/'batch')
    options.analysis_vectors=[value for row in CONTRACT['vectors'] for value in row];options.analysis_white=CONTRACT['white']
    options.inputs=[event(4,q.GuiInputKind.RUN_BATCH_KEY),event(8,q.GuiInputKind.MOVE,wait=q.GuiInputWait.BATCH_FINISHED)]
    report=q.show(project,frame_limit=35,capture_path=str(tmp_path/'log-batch.ppm'),options=options)
    assert report.batch_state=='succeeded' and not report.batch_error
    assert report.batch_result.parameters_sha256==expected.parameters_sha256
    saved=q.Project.open(report.batch_result.images[0].path).document(image).latest_cells(obj).result
    assert saved.algorithm=='qpx.log-watershed/1' and saved.parameters.context_padding==32 and saved.cell_count==3
    assert report.ui_overflow_items==0
