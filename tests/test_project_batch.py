"""Batch transaction boundaries, independent CSV parsing, restart and native CLI."""
import csv
import gc
import io
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time

import pytest
import qupathxx as q
from qupathxx import _native
from test_tissue_levels import pyramid_levels, decoded_reference

pytestmark = pytest.mark.skipif(not q.has_project, reason='requires project storage')


def scene(count=2):
    project=q.Project(); docs=[]
    for i in range(count):
        doc=q.Document(q.Image(32,24,[16+i]*32*24*3));obj=doc.add_rectangle(q.Rect(1,2,20,16));doc.set_calibration(.5,.25)
        image=project.add_document(doc,f'Image {i}, "quoted"\n标注');docs.append((image,doc,obj))
    return project,docs,q.StainTransform([[1,0,0],[0,1,0]])


def rows(path):return list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(path),newline='')))


def run(request,path):
    r=q.run_project_batch(request,str(path));assert r.state=='succeeded',q.project_batch_report_json(r);return r


@pytest.mark.parametrize('kind',[q.ProjectBatchKind.ROI,q.ProjectBatchKind.TISSUE,q.ProjectBatchKind.CELLS])
def test_workflows_reopen_csv_and_resume(tmp_path,kind):
    project,docs,t=scene();o=q.ProjectBatchOptions();o.kind=kind
    # Existing records must be preserved but excluded from this batch's CSV.
    for _,doc,obj in docs:
        a=q.AnalysisRequest(doc,obj,t);doc.keep_analysis(a,q.analyze(a))
    request=q.ProjectBatchRequest(project,t,options=o);r=run(request,tmp_path)
    assert r.saved==2 and r.reused==r.failed==0 and request.object_count==request.image_count==2
    assert json.loads(request.parameters_json)['environment']['source_tree_sha256']==q.build_environment().source_tree_sha256
    assert len(request.parameters_sha256)==64 and request.snapshot_bytes>0
    for result,(image,original,obj) in zip(r.images,docs):
        assert result.state=='saved' and result.image_id==image and result.records==result.objects==1
        restored=q.Project.open(result.path);doc=restored.document(image)
        assert len(restored.images)==1 and doc.annotation(obj).geometry.wkt==original.annotation(obj).geometry.wkt
        assert len(original.analysis_records)==1 and not original.tissue_records and not original.cell_records
        table=rows(result.path);assert table and all(None not in row for row in table)
        for row in table:
            assert row['batch_project_identity']==project.identity and int(row['batch_image_id'])==image
            assert row['batch_image_name']==restored.images[0].name
            assert row['status_at_capture']=='current'
            if kind!=q.ProjectBatchKind.CELLS:assert row['source_tree_sha256']==q.build_environment().source_tree_sha256
        if kind==q.ProjectBatchKind.ROI:
            direct=q.analyze(q.AnalysisRequest(original,obj,t,kind=q.AnalysisKind.OPTICAL_DENSITY_MEANS))
            assert len(doc.analysis_records)==2 and len(table)==1
            assert int(table[0]['record_id'])==doc.analysis_records[-1].id and int(table[0]['samples'])==320
            assert [float(table[0][f'mean_channel_{n}']) for n in range(1,4)]==direct.means
            assert table[0]['input_sha256']==direct.input_sha256
        elif kind==q.ProjectBatchKind.TISSUE:assert len(doc.tissue_records)==1
        else:assert len(doc.cell_records)==1
        dest=tmp_path/f'{image}.csv';q.save_batch_checkpoint_csv(result.path,str(dest));assert dest.read_bytes()==q.batch_checkpoint_csv(result.path).encode()
        with pytest.raises((ValueError,RuntimeError)):q.save_batch_checkpoint_csv(result.path,result.path)
    again=run(request,tmp_path);assert again.reused==2 and again.saved==0
    assert [x.path for x in again.images]==[x.path for x in r.images]
    assert json.loads(Path(r.report_path).read_text(encoding='utf8'))==json.loads(q.project_batch_report_json(r))
    assert not list(tmp_path.glob('.qupathxx-batch-work-*'))


def test_snapshot_survives_edits_gc_and_async_job(tmp_path):
    p,docs,t=scene();request=q.ProjectBatchRequest(p,t)
    for image,doc,obj in docs:doc.remove_annotation(obj);p.remove_image(image)
    del p,docs,doc;gc.collect()
    job=q.ProjectBatchJob();assert job.result is None and not job.busy
    assert job.request(request,str(tmp_path));last=0
    while job.busy:
        s=job.poll();assert s.completed_objects>=last;last=s.completed_objects;time.sleep(.001)
    assert s.state=='succeeded' and s.completed_images==s.total_images==2 and s.completed_objects==s.total_objects==2
    assert job.result.saved==2
    for image in job.result.images:assert len(q.Project.open(image.path).document(image.image_id).annotations)==1
    assert job.request(request,str(tmp_path));assert job.wait().state=='succeeded' and job.result.reused==2


def test_selected_images_ids_empty_header_and_nonreplacement(tmp_path):
    p,docs,t=scene();selection=q.ProjectBatchSelection();selection.image_id=docs[1][0];selection.object_ids=[docs[1][2]]*2
    request=q.ProjectBatchRequest(p,t,[selection]);assert request.object_count==request.image_count==1
    first=run(request,tmp_path);o=q.ProjectBatchOptions();o.resume=False
    second=run(q.ProjectBatchRequest(p,t,[selection],o),tmp_path)
    assert first.images[0].path!=second.images[0].path and Path(first.images[0].path).exists()
    p,docs,t=scene(1);docs[0][1].remove_annotation(docs[0][2]);r=run(q.ProjectBatchRequest(p,t),tmp_path/'empty')
    assert r.images[0].records==0 and rows(r.images[0].path)==[]
    assert q.batch_checkpoint_csv(r.images[0].path).startswith('batch_project_identity,')


@pytest.mark.parametrize('damage',['metadata','csv','canonical','ordinary_save'])
def test_resume_rejects_damaged_or_edited_checkpoints(tmp_path,damage):
    p,_,t=scene(1);request=q.ProjectBatchRequest(p,t);first=run(request,tmp_path);path=first.images[0].path
    if damage=='ordinary_save':q.Project.open(path).save(path)
    else:
        with sqlite3.connect(path) as db:
            if damage=='metadata':db.execute("UPDATE qpx_batch_checkpoint SET image_name='bad'")
            elif damage=='csv':db.execute("UPDATE qpx_batch_checkpoint SET csv=x'626164'")
            else:db.execute("UPDATE objects SET name='bad'")
    with pytest.raises((ValueError,RuntimeError)):q.batch_checkpoint_csv(path)
    before=Path(path).read_bytes();r=q.run_project_batch(request,str(tmp_path))
    assert r.state=='failed' and r.failed==1 and r.reused==r.saved==0 and Path(path).read_bytes()==before


def test_linked_pixels_changed_restart_and_source_paths(tmp_path):
    image=tmp_path/'标注.ppm';image.write_bytes(b'P6\n32 24\n255\n'+bytes([16])*32*24*3)
    p=q.Project();id=p.add_image(str(image));p.document(id).add_rectangle(q.Rect(0,0,32,24));t=q.StainTransform([[1,0,0],[0,1,0]])
    request=q.ProjectBatchRequest(p,t);first=run(request,tmp_path/'results');assert run(request,tmp_path/'results').reused==1
    image.write_bytes(b'P6\n32 24\n255\n'+bytes([32])*32*24*3)
    second=run(request,tmp_path/'results');assert second.saved==1 and second.images[0].input_sha256==first.images[0].input_sha256
    assert second.images[0].pixel_sha256!=first.images[0].pixel_sha256 and second.images[0].path!=first.images[0].path
    reopened=q.Project.open(second.images[0].path);assert Path(reopened.images[0].source_path).samefile(image)
    assert reopened.document(id).server.read_region(q.RegionRequest(0,0,0,1,1)).data[:3]==bytes([32]*3)
    with pytest.raises((ValueError,RuntimeError)):q.save_batch_checkpoint_csv(second.images[0].path,str(image))


def test_failure_policies_budgets_and_validation(tmp_path):
    p,docs,t=scene();docs[0][1].add_rectangle(q.Rect(1,1,.1,.1))
    stop=q.run_project_batch(q.ProjectBatchRequest(p,t),str(tmp_path/'stop'));assert stop.state=='failed' and len(stop.images)==1
    o=q.ProjectBatchOptions();o.failure=q.ProjectBatchFailure.CONTINUE
    r=q.run_project_batch(q.ProjectBatchRequest(p,t,options=o),str(tmp_path/'continue'));assert r.state=='partial' and r.saved==r.failed==1
    assert len(list((tmp_path/'continue').rglob('*.qpxproj')))==1
    p,_,t=scene(1)
    for field,value in [('materialize_budget',1),('result_budget',1),('csv_limit',1),('checkpoint_limit',4096)]:
        o=q.ProjectBatchOptions();setattr(o,field,value)
        r=q.run_project_batch(q.ProjectBatchRequest(p,t,options=o),str(tmp_path/field));assert r.state=='failed' and not list((tmp_path/field).rglob('*.qpxproj'))
    request=q.ProjectBatchRequest(p,t);o=q.ProjectBatchOptions();o.snapshot_budget=request.snapshot_bytes-1
    with pytest.raises((ValueError,RuntimeError)):q.ProjectBatchRequest(p,t,options=o)
    s=q.ProjectBatchSelection();s.image_id=p.images[0].id
    with pytest.raises(ValueError):q.ProjectBatchRequest(p,t,[s,s])
    s.object_ids=[999]
    with pytest.raises((IndexError,ValueError,RuntimeError)):q.ProjectBatchRequest(p,t,[s])


def test_cli_jsonl_unicode_export_and_errors(tmp_path):
    cli=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    assert cli.is_file();p,_,_=scene();source=tmp_path/'项目.qpxproj';p.save(str(source));dest=tmp_path/'输出'
    r=subprocess.run([str(cli),str(source),str(dest)],capture_output=True,text=True,encoding='utf8',timeout=30)
    assert r.returncode==0,r.stderr+r.stdout;lines=[json.loads(x) for x in r.stdout.splitlines()];assert len(lines)==3 and lines[-1]['saved']==2
    r=subprocess.run([str(cli),str(source),str(dest)],capture_output=True,text=True,encoding='utf8',timeout=30);assert r.returncode==0 and json.loads(r.stdout.splitlines()[-1])['reused']==2
    r=subprocess.run([str(cli),str(source),str(dest),'--no-resume'],capture_output=True,text=True,encoding='utf8',timeout=30);assert r.returncode==0,r.stderr+r.stdout
    repeated=json.loads(r.stdout.splitlines()[0]);assert repeated['state']=='saved' and repeated['path']!=lines[0]['path']
    checkpoint=repeated['path'];csvpath=tmp_path/'表.csv'
    r=subprocess.run([str(cli),'--export-csv',checkpoint,str(csvpath)],capture_output=True,timeout=30);assert r.returncode==0 and csvpath.read_bytes()==q.batch_checkpoint_csv(checkpoint).encode()
    for args in [[],[str(source),str(dest),'--bad'],[str(source),str(dest),'--kind'],[str(source),str(dest),'--white','nan,2,3'],[str(source),str(dest),'--object','1']]:
        r=subprocess.run([str(cli),*args],capture_output=True,timeout=30);assert r.returncode==2


@pytest.mark.parametrize('request_kind',['roi','tissue','cells'])
def test_digest_only_matches_kernel(request_kind):
    _,docs,t=scene(1);_,doc,obj=docs[0]
    if request_kind=='roi':r=q.AnalysisRequest(doc,obj,t);digest=q.analyze(r).input_sha256
    elif request_kind=='tissue':r=q.TissueRequest(doc,obj,t);digest=q.detect_tissue(r).input_sha256
    else:r=q.CellRequest(doc,obj,t);digest=q.detect_cells(r).nuclei.input_sha256
    assert q.input_sha256(r)==digest


@pytest.mark.parametrize('level',[1,2])
def test_digest_coarse_grid_independent(pyramid_levels,level):
    server,_=pyramid_levels;doc=q.Document(server);roi=q.Roi.rectangle(q.Rect(13,19,180,120));obj=doc.add_roi(roi);t=q.StainTransform([[1,0,0],[0,1,0]])
    _,_,expected,_=decoded_reference(server,roi,level,31)
    p=q.TissueParameters();p.level=level;p.tile_size=31;r=q.TissueRequest(doc,obj,t,p)
    assert q.input_sha256(r)==q.detect_tissue(r).input_sha256==expected
    c=q.CellParameters();c.level=level;c.tile_size=31;r=q.CellRequest(doc,obj,t,c)
    assert q.input_sha256(r)==q.detect_cells(r).nuclei.input_sha256==expected


@pytest.mark.skipif(not q.has_geos,reason='requires geometry')
def test_optional_cells_published_to_checkpoint_only(tmp_path):
    p,docs,t=scene(1);o=q.ProjectBatchOptions();o.kind=q.ProjectBatchKind.CELLS;o.publish_cells=True
    r=run(q.ProjectBatchRequest(p,t,options=o),tmp_path);image,original,_=docs[0];doc=q.Project.open(r.images[0].path).document(image)
    assert len(original.annotations)==1 and not original.cell_records
    record=doc.cell_records[0]
    assert sum(a.kind==q.ObjectKind.CELL for a in doc.annotations)==len(record.result.features)>0


@pytest.mark.skipif(os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in GUI')
def test_checkpoint_opens_in_existing_viewer(tmp_path):
    p,_,t=scene(1);r=run(q.ProjectBatchRequest(p,t),tmp_path);checkpoint=q.Project.open(r.images[0].path)
    report=q.show(checkpoint,frame_limit=8)
    assert report.frames==8 and report.active_image==r.images[0].image_id


def test_job_retains_snapshot_after_python_owners_collected(tmp_path):
    p,docs,t=scene(1);request=q.ProjectBatchRequest(p,t);job=q.ProjectBatchJob();job.request(request,str(tmp_path))
    del request,p,docs,t;gc.collect();assert job.wait().state=='succeeded' and job.result.saved==1
    p,docs,t=scene(1);job=q.ProjectBatchJob();job.request(q.ProjectBatchRequest(p,t),str(tmp_path/'destructor'))
    del job,p,docs,t;gc.collect()  # Native join must not require worker Python callbacks.


def test_missing_linked_source_continue_after_capture(tmp_path):
    image=tmp_path/'missing.ppm';image.write_bytes(b'P6\n8 8\n255\n'+bytes([16])*8*8*3)
    p,_,t=scene(1);id=p.add_image(str(image));p.document(id).add_rectangle(q.Rect(0,0,8,8));o=q.ProjectBatchOptions();o.failure=q.ProjectBatchFailure.CONTINUE
    request=q.ProjectBatchRequest(p,t,options=o);image.unlink();r=q.run_project_batch(request,str(tmp_path/'out'))
    assert r.state=='partial' and r.saved==r.failed==1 and len(r.images)==2


def test_output_lock_rejects_second_writer(tmp_path):
    p,_,t=scene(1);request=q.ProjectBatchRequest(p,t)
    with (tmp_path/'.qupathxx-batch.lock').open('a+b') as lock:
        if os.name!='nt':
            import fcntl
            fcntl.flock(lock.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        r=q.run_project_batch(request,str(tmp_path))
        assert r.state=='failed' and 'locked' in r.error and not r.images
    assert run(request,tmp_path).saved==1
