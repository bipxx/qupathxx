"""Independent ROI/component CSV values, captured authority, bounded atomic export."""
import csv
import gc
import io
import os
from pathlib import Path
import sqlite3
import subprocess
import sys

import pytest
import qupathxx as q
from test_tissue_levels import pyramid_levels

KINDS=[q.MeasurementTableKind.ROI,q.MeasurementTableKind.TISSUE]

def parsed(text):return list(csv.DictReader(io.StringIO(text,newline="")))

def setup(empty=False):
    doc=q.Document(q.Image(32,24,[255 if empty else 16]*32*24*3))
    obj=doc.add_rectangle(q.Rect(1,2,20,16));doc.set_calibration(.5,.25)
    transform=q.StainTransform([[1,0,0],[0,1,0]])
    a=q.AnalysisRequest(doc,obj,transform,kind=q.AnalysisKind.OPTICAL_DENSITY_MEANS,tile_size=16);ar=q.analyze(a);aid=doc.keep_analysis(a,ar)
    t=q.TissueRequest(doc,obj,transform);tr=q.detect_tissue(t);tid=doc.keep_tissue(t,tr)
    return doc,obj,(aid,tid),(ar,tr)

@pytest.mark.parametrize("kind",KINDS)
def test_values_and_historical_calibration(kind):
    doc,obj,ids,results=setup();doc.set_calibration(3,4)
    request=q.MeasurementTableRequest(doc,kind);rows=parsed(q.measurements_csv(request));row=rows[0]
    assert len(rows)==request.row_count==1 and request.record_count==1 and request.kind==kind
    assert request.image_count==1 and not request.project_scope and request.document_revision==doc.revision
    assert None not in row and len(row)==(43 if kind==KINDS[0] else 70)
    assert int(row['source_object_id'])==obj and row['status_at_capture']=='current'
    assert float(row['roi_area_px2'])==320 and float(row['roi_length_px'])==72
    assert float(row['captured_pixel_width_um'])==.5 and float(row['captured_pixel_height_um'])==.25
    assert float(row['roi_area_um2'])==40 and float(row['roi_length_um'])==28
    result=results[KINDS.index(kind)]
    assert row['input_sha256']==result.input_sha256 and row['source_tree_sha256']==q.build_environment().source_tree_sha256
    if kind==KINDS[0]:
        assert int(row['record_id'])==ids[0] and int(row['samples'])==result.samples
        assert int(row['tiles'])==result.tiles and row['channel_space']=='optical_density_rgb'
        assert [float(row[f'mean_channel_{i}']) for i in range(1,4)]==result.means
    else:
        component=result.components[0]
        assert int(row['component_label'])==component.label and int(row['component_samples'])==component.pixels
        assert float(row['component_area_px2'])==320 and float(row['component_area_um2'])==40
        for field in ['threshold_samples','cleaned_samples','filled_samples','foreground_samples']:
            assert int(row[field])==getattr(result,field.replace('samples','pixels'))
        assert float(row['component_centroid_x_px'])==component.centroid.x
        assert float(row['component_min_y_px'])==component.bounds.y


def test_empty_tissue_has_summary_but_no_invented_component():
    doc,_,_,_=setup(True);request=q.MeasurementTableRequest(doc,KINDS[1]);row=parsed(q.measurements_csv(request))[0]
    assert row['kept_components']=='0' and request.row_count==1
    assert row['component_label']==row['component_samples']==row['component_area_px2']==row['component_area_um2']==''
    assert row['foreground_samples']=='0' and None not in row

@pytest.mark.parametrize('kind',KINDS)
def test_capture_is_immutable_after_remove_edit_and_gc(kind):
    doc,obj,ids,_=setup();request=q.MeasurementTableRequest(doc,kind);text=q.measurements_csv(request)
    doc.remove_analysis(ids[0]);doc.remove_tissue(ids[1]);doc.rename_annotation(obj,'changed')
    del doc;gc.collect();assert q.measurements_csv(request)==text

@pytest.mark.parametrize('kind',KINDS)
def test_status_filter_selection_and_snapshot_limits(kind):
    doc,obj,ids,_=setup();key=ids[KINDS.index(kind)]
    request=q.MeasurementTableRequest(doc,kind,[key,key]);assert request.record_count==1
    with pytest.raises(IndexError):q.MeasurementTableRequest(doc,kind,[key,99999])
    options=q.MeasurementTableOptions();options.snapshot_budget=request.snapshot_bytes
    assert q.MeasurementTableRequest(doc,kind,[key],options).snapshot_bytes==request.snapshot_bytes
    options.snapshot_budget-=1
    with pytest.raises(ValueError,match='budget'):q.MeasurementTableRequest(doc,kind,[key],options)
    doc.rename_annotation(obj,'changed');assert parsed(q.measurements_csv(doc,kind))[0]['status_at_capture']=='object_changed'
    options=q.MeasurementTableOptions();options.require_current=True
    with pytest.raises(ValueError,match='current'):q.MeasurementTableRequest(doc,kind,options=options)

@pytest.mark.parametrize('field,value',[('byte_limit',0),('byte_limit',256*1024*1024+1),('snapshot_budget',0),('snapshot_budget',512*1024*1024+1)])
def test_invalid_options(field,value):
    options=q.MeasurementTableOptions();setattr(options,field,value)
    with pytest.raises(ValueError):options.validate()

@pytest.mark.parametrize('kind',KINDS)
def test_byte_limit_atomic_failure_and_job_reuse(tmp_path,kind):
    doc,_,_,_=setup();request=q.MeasurementTableRequest(doc,kind);text=q.measurements_csv(request)
    options=q.MeasurementTableOptions();options.byte_limit=len(text.encode());path=tmp_path/'\u6d4b\u91cf.csv'
    q.save_measurements_csv(doc,kind,str(path),options=options);assert path.read_bytes()==text.encode()
    options.byte_limit-=1;short=q.MeasurementTableRequest(doc,kind,options=options)
    with pytest.raises(ValueError,match='byte limit'):q.save_measurements_csv(short,str(path))
    assert path.read_bytes()==text.encode()
    job=q.MeasurementTableExportJob();assert job.request(short,str(path));assert job.wait().state=='failed'
    assert path.read_bytes()==text.encode() and job.poll().completed==0
    assert job.request(request,str(path));status=job.wait()
    assert status.state=='succeeded' and status.rows==status.total_rows==request.row_count and status.completed==1
    assert status.bytes==len(text.encode()) and status.snapshot_bytes==request.snapshot_bytes
    job.cancel();assert job.poll().state=='succeeded' and not list(tmp_path.glob('*.qpx-tmp-*'))

@pytest.mark.skipif(not q.has_project,reason='native project')
@pytest.mark.parametrize('kind',KINDS)
def test_project_context_order_escaping_and_isolation(tmp_path,kind):
    project=q.Project();images=[];docs=[]
    for name in ['First,"image"\r\nrow','\u96ea']:
        doc,_,_,_=setup();images.append(project.add_document(doc,name=name));docs.append(doc)
    empty=q.Document(q.Image.demo());images.append(project.add_document(empty,name='no runs'))
    request=q.MeasurementTableRequest(project,kind,[images[2],images[1],images[0],images[0]])
    text=q.measurements_csv(request);actual=parsed(text);assert request.record_count==2 and request.image_count==3
    names={image.id:image.name for image in project.images};expected=[]
    for image,doc in zip(images,docs):
        for row in parsed(q.measurements_csv(doc,kind)):
            row.update(project_identity=project.identity,project_generation=str(project.generation),project_revision=str(request.project_revision),image_id=str(image),image_name=names[image],table_schema='qpx.project-'+('roi' if kind==KINDS[0] else 'tissue')+'-table/1');expected.append(row)
    assert actual==expected and list(actual[0])[:5]==['project_identity','project_generation','project_revision','image_id','image_name']
    assert text==q.measurements_csv(project,kind)
    path=tmp_path/'project.csv';q.save_measurements_csv(project,kind,str(path));assert path.read_bytes()==text.encode()
    for image in images:project.remove_image(image)
    del project,docs,doc,empty;gc.collect();assert q.measurements_csv(request)==text

@pytest.mark.skipif(not q.has_project,reason='native project')
@pytest.mark.parametrize('kind',KINDS)
def test_legacy_provenance_unknown_and_reloaded_unverified(tmp_path,kind):
    doc,_,_,_=setup();project=q.Project();image=project.add_document(doc);path=tmp_path/'legacy.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as database:database.execute('DELETE FROM analysis_provenance')
    loaded=q.Project.open(str(path));row=parsed(q.measurements_csv(loaded,kind))[0]
    assert row['status_at_capture']=='unverified'
    for field in ['captured_unix_ms','source_tree_sha256','captured_pixel_width_um','captured_pixel_height_um','roi_area_um2','roi_length_um']:assert row[field]==''
    if kind==KINDS[1]:assert row['component_area_um2']==''

@pytest.mark.skipif(not q.has_openslide,reason='native pyramids')
def test_tissue_sample_area_and_level_zero_bounds(pyramid_levels):
    server,_=pyramid_levels;doc=q.Document(server);obj=doc.add_rectangle(q.Rect(12,16,80,80));doc.set_calibration(.5,.25)
    params=q.TissueParameters();params.level=2
    source=q.TissueRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),params);result=q.detect_tissue(source);doc.keep_tissue(source,result)
    for row,component in zip(parsed(q.measurements_csv(doc,KINDS[1])),result.components):
        assert row['level']=='2' and float(row['downsample'])==4
        assert float(row['component_area_px2'])==component.pixels*16
        assert float(row['component_area_um2'])==component.pixels*2
        assert float(row['component_min_x_px'])==component.bounds.x


def test_worker_native_lifetime_after_python_owners_are_released(tmp_path):
    script=r'''
import gc,pathlib,sys
import qupathxx as q
for kind in [q.MeasurementTableKind.ROI,q.MeasurementTableKind.TISSUE]:
    doc=q.Document(q.Image(32,32,[0]*32*32*3));obj=doc.add_rectangle(q.Rect(0,0,32,32));transform=q.StainTransform([[1,0,0],[0,1,0]])
    source=q.AnalysisRequest(doc,obj,transform);result=q.analyze(source);doc.keep_analysis(source,result)
    tissue=q.TissueRequest(doc,obj,transform);mask=q.detect_tissue(tissue);doc.keep_tissue(tissue,mask)
    request=q.MeasurementTableRequest(doc,kind);job=q.MeasurementTableExportJob();path=pathlib.Path(sys.argv[1])/'worker.csv'
    assert job.request(request,str(path));del doc,source,result,tissue,mask,request;gc.collect()
    assert job.wait().state=='succeeded';del job;gc.collect()
'''
    child=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path)],capture_output=True,text=True,timeout=20)
    assert child.returncode==0,child.stdout+child.stderr

@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='desktop replay')
@pytest.mark.parametrize('kind',[0,1])
@pytest.mark.parametrize('project_scope',[False,True])
def test_gui_exports_snapshot_without_selection(tmp_path,kind,project_scope):
    doc,_,_,_=setup();project=q.Project();project.add_document(doc)
    output=tmp_path/'gui.csv';config=q.GuiOptions();config.measurement_table_path=str(output);config.measurement_table_kind=kind
    start=q.GuiInput();start.frame,start.kind,start.shift=4,q.GuiInputKind.EXPORT_MEASUREMENT_TABLE_KEY,project_scope
    ready=q.GuiInput();ready.frame,ready.kind,ready.wait_for=6,q.GuiInputKind.MOVE,q.GuiInputWait.MEASUREMENT_TABLE_SAVED
    config.inputs=[start,ready];authority=project if project_scope else doc
    report=q.show(authority,frame_limit=20,options=config)
    assert report.measurement_table_state=='succeeded' and not report.measurement_table_error and report.selected_ids==[]
    assert report.measurement_table_rows==1 and output.read_bytes()==q.measurements_csv(authority,KINDS[kind]).encode()


def test_cancellation_and_no_second_queued_export(tmp_path):
    size=256;rgb=[]
    for y in range(size):
        for x in range(size):rgb.extend([0 if x%3==1 and y%3==1 else 255]*3)
    doc=q.Document(q.Image(size,size,rgb));obj=doc.add_rectangle(q.Rect(0,0,size,size))
    source=q.TissueRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));result=q.detect_tissue(source);doc.keep_tissue(source,result)
    request=q.MeasurementTableRequest(doc,KINDS[1]);assert request.row_count>7000
    path=tmp_path/'cancel.csv';path.write_bytes(b'sentinel');job=q.MeasurementTableExportJob();assert job.request(request,str(path))
    if job.busy:assert not job.request(request,str(tmp_path/'extra.csv'))
    job.cancel();status=job.wait();assert status.state in {'cancelled','succeeded'}
    if status.state=='cancelled':assert path.read_bytes()==b'sentinel'
    else:assert len(parsed(path.read_text()))==request.row_count
    assert not list(tmp_path.glob('*.qpx-tmp-*'))

@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='desktop replay')
def test_gui_export_failure_preserves_old_csv(tmp_path):
    doc,_,_,_=setup();output=tmp_path/'old.csv';output.write_bytes(b'sentinel')
    options=q.GuiOptions();options.measurement_table_path=str(output);options.measurement_table_byte_limit=1
    start=q.GuiInput();start.frame,start.kind=4,q.GuiInputKind.EXPORT_MEASUREMENT_TABLE_KEY;options.inputs=[start]
    report=q.show(doc,frame_limit=20,options=options)
    assert report.measurement_table_state=='failed' and 'byte limit' in report.measurement_table_error
    assert output.read_bytes()==b'sentinel'


def test_stain_means_and_nucleus_shape_use_original_geometry():
    doc=q.Document(q.Image(32,24,[16]*32*24*3));doc.set_calibration(.5,.25)
    body=q.Roi.rectangle(q.Rect(1,2,20,16));nucleus=q.Roi.ellipse(q.Rect(3,4,6,8));obj=doc.add_cell(body,nucleus)
    source=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),kind=q.AnalysisKind.STAIN_MEANS,nucleus=True)
    result=q.analyze(source);doc.keep_analysis(source,result);row=parsed(q.measurements_csv(doc,KINDS[0]))[0]
    assert row['nucleus']=='1' and row['roi_kind']=='1' and row['channel_space']=='stain_concentrations'
    assert float(row['roi_area_px2'])==pytest.approx(nucleus.area)
    assert float(row['roi_area_um2'])==pytest.approx(nucleus.area*.125)
    assert [float(row[f'mean_channel_{i}']) for i in range(1,4)]==result.means


def test_format_and_wait_release_python_gil(tmp_path):
    import threading
    doc,_,_,_=setup()
    source=q.AnalysisRequest(doc,1,q.StainTransform([[1,0,0],[0,1,0]]));result=q.analyze(source)
    for _ in range(2000):doc.keep_analysis(source,result)
    request=q.MeasurementTableRequest(doc,KINDS[0]);calls=[0];stop=threading.Event()
    def count():
        while not stop.is_set():calls[0]+=1
    thread=threading.Thread(target=count);thread.start()
    try:
        before=calls[0];q.measurements_csv(request);assert calls[0]>before
        job=q.MeasurementTableExportJob();assert job.request(request,str(tmp_path/'gil.csv'))
        before=calls[0];assert job.wait().state=='succeeded' and calls[0]>before
    finally:stop.set();thread.join()
