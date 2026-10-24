"""Installed native LoG workflow; frozen references and immutable saved recipes."""
import csv
import gzip
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
