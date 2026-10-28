"""Original-image nominal batch capture, per-ROI recipes and installed native CLI."""
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess

import pytest
import qupathxx as q
from qupathxx import _native
from test_nominal_cell_python import scene, FIXTURE

pytestmark=pytest.mark.skipif(not q.has_project,reason='requires project storage')


def cohort():
    project=q.Project();docs=[]
    for index in range(2):
        doc,obj,transform,p=scene()
        if index:
            doc.update_roi(obj,q.Roi.rectangle(q.Rect(32.25,48.5,180.5,100.25)))
            doc.set_calibration(.8,.4)
        image=project.add_document(doc,f'Nominal {index}, 原始𠀋');docs.append((image,doc,obj))
    options=q.ProjectBatchOptions();options.kind=q.ProjectBatchKind.CELLS;options.grid_cells=p
    sampling=q.NominalCellRoiSampling();sampling.context_x=sampling.context_y=0;options.cell_nominal=sampling
    return project,docs,transform,options,sampling


def execute(request,path):
    result=q.run_project_batch(request,str(path))
    assert result.state=='succeeded',q.project_batch_report_json(result)
    return result


def cli():return Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')


def invoke(source,destination,flags):
    return subprocess.run([str(cli()),str(source),str(destination),*flags],capture_output=True,text=True,encoding='utf-8',timeout=90)


def flags():
    return ['--kind','cells','--cell-nominal-downsample','1.5','--cell-nominal-context','0,0',
        '--threshold','.4','--grid-sigma','1','--grid-expansion','5','--grid-min-area','16','--grid-max-area','512','--grid-contour-bytes',str(1024*1024)]


@pytest.mark.parametrize('factor',[0,-1,float('nan'),float('inf')])
def test_nominal_roi_sampling_rejects_invalid_factors(factor):
    sampling=q.NominalCellRoiSampling();sampling.downsample=factor
    with pytest.raises(ValueError):sampling.validate()


@pytest.mark.parametrize('field',['context_x','context_y'])
def test_nominal_roi_sampling_rejects_excess_context(field):
    sampling=q.NominalCellRoiSampling();setattr(sampling,field,4097)
    with pytest.raises(ValueError):sampling.validate()


def test_nominal_options_conflicts_fail_before_batch_capture():
    _,_,_,options,_=cohort();options.cell_grid=q.GridCellSampling()
    with pytest.raises(ValueError,match='mutually exclusive'):options.validate()
    options.cell_grid=None;options.kind=q.ProjectBatchKind.ROI
    with pytest.raises(ValueError,match='cells batch'):options.validate()
    options.kind=q.ProjectBatchKind.CELLS;options.cell_min_nucleus_area_um2=1
    with pytest.raises(ValueError):options.validate()
    options.cell_min_nucleus_area_um2=None
    legacy=options.cells;legacy.level=0;options.cells=legacy
    with pytest.raises(ValueError):options.validate()


def test_nominal_batch_freezes_options_resolves_each_roi_and_reuses_checkpoints(tmp_path):
    project,docs,transform,options,sampling=cohort();captured=q.ProjectBatchRequest(project,transform,options=options)
    recipe=json.loads(captured.parameters_json)['nominal_cells']
    assert recipe['sampling_algorithm']=='qpx.rgb-nominal/1' and recipe['detector_algorithm']=='qpx.nominal-rgb-log-watershed-bodies/1'
    assert recipe['downsample']==1.5 and recipe['context_x']==recipe['context_y']==0
    assert 'grid_cells' not in json.loads(captured.parameters_json)
    sampling.downsample=3;options.cell_nominal=sampling;changed=options.grid_cells;changed.min_nucleus_area=10000;options.grid_cells=changed
    saved=execute(captured,tmp_path);assert saved.saved==2 and saved.reused==0
    snapshots=[]
    for entry,(image,original,obj) in zip(saved.images,docs):
        assert entry.image_id==image and entry.records==1 and original.cell_record_count==0
        doc=q.Project.open(entry.path).document(image);record=doc.cell_records[0];n=record.result.nominal_sampling;r=n.source_request
        assert r.downsample==1.5 and n.context_x==n.context_y==0 and doc.annotation(obj).geometry.equals(original.annotation(obj).geometry)
        assert record.result.grid_parameters.min_nucleus_area==16 and doc.cell_status(record.id)=='unverified'
        if image==docs[0][0]:
            assert (r.x_px,r.y_px,r.width_px,r.height_px,n.core_width,n.core_height)==(0,0,256,192,171,128)
            assert record.result.cell_count==3 and record.result.nuclei.foreground_pixels==379 and record.result.cells.foreground_pixels==825
        else:assert (r.x_px,r.y_px,r.width_px,r.height_px,n.core_width,n.core_height)==(32,48,181,101,121,67)
        rows=list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(entry.path))))
        assert rows and all(row['sampling_grid']=='nominal_rgb' and row['table_schema']=='qpx.cell-table/7' for row in rows)
        assert all(float(row['sample_step_x_px'])==1.5 for row in rows)
        snapshots.append((entry.path,Path(entry.path).stat().st_mtime_ns,hashlib.sha256(Path(entry.path).read_bytes()).hexdigest()))
    resumed=execute(captured,tmp_path);assert resumed.reused==2 and resumed.saved==0
    assert [(entry.path,Path(entry.path).stat().st_mtime_ns,hashlib.sha256(Path(entry.path).read_bytes()).hexdigest()) for entry in resumed.images]==snapshots


def test_nominal_recipe_identity_ignores_inactive_legacy_thresholds_but_keeps_sampling(tmp_path):
    project,_,transform,options,sampling=cohort();baseline=q.ProjectBatchRequest(project,transform,options=options)
    legacy=options.cells;legacy.threshold=9;options.cells=legacy
    tissue=options.tissue;tissue.threshold=8;options.tissue=tissue
    assert q.ProjectBatchRequest(project,transform,options=options).parameters_sha256==baseline.parameters_sha256
    sampling.context_x=1;options.cell_nominal=sampling
    assert q.ProjectBatchRequest(project,transform,options=options).parameters_sha256!=baseline.parameters_sha256
    sampling.context_x=0;sampling.downsample=2;options.cell_nominal=sampling
    changed=q.ProjectBatchRequest(project,transform,options=options)
    assert changed.parameters_sha256!=baseline.parameters_sha256
    first=execute(baseline,tmp_path);second=execute(changed,tmp_path)
    assert first.saved==second.saved==2 and second.reused==0
    assert {entry.path for entry in first.images}.isdisjoint(entry.path for entry in second.images)


def test_nominal_physical_batch_resolves_anisotropic_calibration_per_image(tmp_path):
    project,docs,transform,options,_=cohort();p=options.grid_cells;p.units=q.GridCellUnits.MICROMETRES;options.grid_cells=p
    result=execute(q.ProjectBatchRequest(project,transform,options=options),tmp_path)
    for entry,(_,original,_) in zip(result.images,docs):
        run=q.Project.open(entry.path).document(entry.image_id).cell_records[0].result
        assert run.resolved_grid_parameters.step_x==pytest.approx(1.5*original.pixel_width_um)
        assert run.resolved_grid_parameters.step_y==pytest.approx(1.5*original.pixel_height_um)
        assert run.nominal_sampling.source_request.downsample==1.5


def test_nominal_linked_pixel_change_invalidates_resume(tmp_path):
    pixels=(FIXTURE/'source.rgb').read_bytes();source=tmp_path/'source-原始.ppm';source.write_bytes(b'P6\n256 192\n255\n'+pixels)
    project=q.Project();image=project.add_image(str(source));doc=project.document(image);doc.add_rectangle(q.Rect(0,0,256,192))
    _,_,transform,options,_=cohort();captured=q.ProjectBatchRequest(project,transform,options=options)
    first=execute(captured,tmp_path/'results');assert execute(captured,tmp_path/'results').reused==1
    modified=bytearray(pixels);modified[0]^=1;source.write_bytes(b'P6\n256 192\n255\n'+modified)
    second=execute(captured,tmp_path/'results')
    assert second.saved==1 and second.reused==0 and second.images[0].pixel_sha256!=first.images[0].pixel_sha256
    assert second.images[0].path!=first.images[0].path


@pytest.mark.parametrize('prefix_typed',[False,True])
def test_installed_nominal_cli_single_target_is_order_independent_and_resumable(tmp_path,prefix_typed):
    project,docs,_,_,_=cohort();source=tmp_path/'original-原始𠀋.qpxproj';project.save(str(source));before=source.read_bytes()
    values=flags()
    if prefix_typed:values=['--grid-min-area','16','--grid-sigma','1',*values]
    values+=['--image',str(docs[0][0]),'--object',str(docs[0][2])]
    first=invoke(source,tmp_path/'out',values);assert first.returncode==0,first.stderr
    report=json.loads(first.stdout.splitlines()[-1]);assert report['saved']==1 and report['reused']==0
    item=report['images'][0];opened=q.Project.open(item['path']).document(docs[0][0]);run=opened.cell_records[0].result
    assert run.nominal_sampling.source_request.downsample==1.5 and run.cell_count==3
    assert run.nuclei.foreground_pixels==379 and run.cells.foreground_pixels==825
    again=invoke(source,tmp_path/'out',values);assert again.returncode==0,again.stderr
    assert json.loads(again.stdout.splitlines()[-1])['reused']==1 and source.read_bytes()==before


@pytest.mark.parametrize('bad',['0','-1','nan','inf','0x1.8p0',' 1.5','1.5 ','1,5'])
def test_installed_nominal_cli_rejects_bad_factors_before_source_open(tmp_path,bad):
    result=invoke(tmp_path/'absent.qpxproj',tmp_path/'out',['--kind','cells','--cell-nominal-downsample',bad])
    assert result.returncode==2 and 'positive decimal' in json.loads(result.stderr)['error']
    assert not (tmp_path/'out').exists()


@pytest.mark.parametrize('extra',[
    ['--cell-grid-samples','10,10'],['--cell-algorithm','distance'],['--level','0'],['--tile-size','64'],
    ['--cell-min-area-um2','1'],['--cell-nominal-context','4097,0'],['--cell-nominal-context','0,-1']])
def test_installed_nominal_cli_rejects_conflicts_before_source_open(tmp_path,extra):
    result=invoke(tmp_path/'absent.qpxproj',tmp_path/'out',['--kind','cells','--cell-nominal-downsample','1.5',*extra])
    assert result.returncode==2 and 'error' in json.loads(result.stderr)
    assert 'cannot open' not in result.stderr.lower() and not (tmp_path/'out').exists()
