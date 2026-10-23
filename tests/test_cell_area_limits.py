"""Physical area bounds compile to frozen ordinary sampled-pixel recipes."""
import json
import math
import os
from pathlib import Path
import sqlite3
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_cell_calibration import scene
from test_tissue_levels import pyramid_levels


def request(doc,obj,parameters,minimum=None,maximum=None):
    return q.CellRequest.with_nucleus_area_limits(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),
        minimum_um2=minimum,maximum_um2=maximum,parameters=parameters)


def test_inclusive_area_bounds_and_adjacent_doubles_keep_the_correct_nuclei():
    doc,obj,base,_=scene();p=base.parameters
    assert sorted(f.nucleus_pixels for f in base.features)==[80,90]
    for minimum,maximum,counts in [(10.,10.,[80]),(math.nextafter(10.,math.inf),None,[90]),
                                  (None,math.nextafter(10.,0.),[]),(0.,0.,[]),(10.1,11.25,[90])]:
        r=request(doc,obj,p,minimum,maximum);actual=q.detect_cells(r)
        assert sorted(f.nucleus_pixels for f in actual.features)==counts
        manual=q.detect_cells(q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),r.parameters))
        np.testing.assert_array_equal(actual.nuclei.labels_numpy(),manual.nuclei.labels_numpy())
        np.testing.assert_array_equal(actual.cells.labels_numpy(),manual.cells.labels_numpy())
    assert p.min_nucleus_pixels==1 and p.max_nucleus_pixels==0
    resolved=request(doc,obj,p,10.1,11.25).parameters
    assert (resolved.min_nucleus_pixels,resolved.max_nucleus_pixels)==(81,90)


def test_physical_and_sampled_limits_intersect_and_empty_ranges_are_frozen():
    doc,obj,base,_=scene();p=base.parameters;p.max_nucleus_pixels=85
    r=request(doc,obj,p,10.1,None)
    assert q.detect_cells(r).cell_count==0
    # The valid count interval contains neither nucleus in this image.
    assert (r.parameters.min_nucleus_pixels,r.parameters.max_nucleus_pixels)==(81,85)
    empty=request(doc,obj,p,10.7,None)
    assert q.detect_cells(empty).cell_count==0
    assert empty.parameters.min_nucleus_pixels==doc.metadata.width*doc.metadata.height+1
    assert empty.parameters.max_nucleus_pixels==0
    assert (p.min_nucleus_pixels,p.max_nucleus_pixels)==(1,85)


def test_disabled_physical_limits_preserve_unknown_calibration_and_original_recipe():
    doc,obj,base,_=scene(None)
    r=request(doc,obj,base.parameters)
    assert q.detect_cells(r).nuclei.labels==base.nuclei.labels
    for minimum,maximum in [(0.,None),(None,0.),(1.,2.)]:
        with pytest.raises(ValueError,match='both pixel calibration axes'):
            request(doc,obj,base.parameters,minimum,maximum)


@pytest.mark.parametrize('minimum,maximum',[(math.nan,None),(None,math.inf),(-1.,None),(None,-1.),(2.,1.)])
def test_invalid_physical_ranges_are_rejected(minimum,maximum):
    doc,obj,base,_=scene()
    with pytest.raises(ValueError):request(doc,obj,base.parameters,minimum,maximum)


@pytest.mark.parametrize('level',[1,2])
def test_fractional_and_coarse_native_levels_match_independent_sample_counts(pyramid_levels,level):
    server,_=pyramid_levels;doc,obj,base,_=scene(server=server,level=level)
    labels=base.nuclei.labels_numpy();counts=np.bincount(labels.ravel())[1:]
    unit=base.nuclei.downsample**2*.5*.25
    boundary=float(sorted(counts)[len(counts)//2])*unit
    r=request(doc,obj,base.parameters,boundary,None);actual=q.detect_cells(r)
    expected=[label for label,count in enumerate(counts,1) if count*unit>=boundary]
    np.testing.assert_array_equal(actual.nuclei.labels_numpy()!=0,np.isin(labels,expected))
    assert sorted(f.nucleus_pixels for f in actual.features)==sorted(int(counts[label-1]) for label in expected)


def test_unrepresentable_intermediates_do_not_invent_a_zero_area_nucleus():
    doc,obj,base,_=scene((1e-300,1e-300))
    assert q.detect_cells(request(doc,obj,base.parameters,None,0.)).cell_count==0
    assert q.detect_cells(request(doc,obj,base.parameters,1e-300,None)).cell_count==0
    doc.set_calibration(1e300,1e300)
    assert q.detect_cells(request(doc,obj,base.parameters,None,1e300)).cell_count==0


def test_overflowing_intermediate_products_resolve_a_finite_area_interval():
    doc,obj,base,_=scene((1e308,1e-308))
    r=request(doc,obj,base.parameters,70.5,85.5)
    assert (r.parameters.min_nucleus_pixels,r.parameters.max_nucleus_pixels)==(71,85)
    assert [f.nucleus_pixels for f in q.detect_cells(r).features]==[80]


@pytest.mark.skipif(not q.has_project,reason='project storage')
def test_saved_pixel_recipe_replays_without_reinterpreting_later_calibration(tmp_path):
    doc,obj,base,_=scene();r=request(doc,obj,base.parameters,10.1,11.25)
    value=q.detect_cells(r);record=doc.keep_cells(r,value)
    doc.set_calibration(2,3)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'frozen.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        assert all(bytes(row[0]).startswith(b'QCL\x01') for row in db.execute('SELECT run FROM cell_records'))
    restored=q.Project.open(str(path)).document(image)
    saved=restored.cell_record(record).result
    assert (saved.parameters.min_nucleus_pixels,saved.parameters.max_nucleus_pixels)==(81,90)
    assert saved.calibrated_feature(1).nucleus_sample_area_um2==11.25
    replay=q.CellRequest.replay(restored,record)
    np.testing.assert_array_equal(q.detect_cells(replay).nuclei.labels_numpy(),saved.nuclei.labels_numpy())
    # A fresh physical request intentionally resolves against the later units.
    assert q.detect_cells(request(restored,obj,base.parameters,10.1,11.25)).cell_count==0


def batch_scene():
    project=q.Project();docs=[]
    for calibration in [(.5,.25),(.5,.5)]:
        doc,obj,base,_=scene(calibration);image=project.add_document(doc)
        docs.append((image,doc,obj,base.parameters))
    o=q.ProjectBatchOptions();o.kind=q.ProjectBatchKind.CELLS;o.cells=docs[0][3]
    o.cell_min_nucleus_area_um2=10.1;o.cell_max_nucleus_area_um2=22.5
    return project,docs,o


@pytest.mark.skipif(not q.has_project,reason='project batch')
def test_batch_resolves_each_image_and_binds_original_bounds_to_resume(tmp_path):
    project,docs,o=batch_scene();transform=q.StainTransform([[1,0,0],[0,1,0]])
    captured=q.ProjectBatchRequest(project,transform,options=o)
    assert json.loads(captured.parameters_json)['cell_area_limits']=={'minimum_um2':10.1,'maximum_um2':22.5}
    o.cell_min_nucleus_area_um2=1000
    first=q.run_project_batch(captured,str(tmp_path));assert first.state=='succeeded'
    assert first.saved==2 and first.failed==0
    for outcome,(image,_,obj,_),expected in zip(first.images,docs,[1,2],strict=True):
        restored=q.Project.open(outcome.path).document(image)
        assert restored.latest_cells(obj).result.cell_count==expected
    second=q.run_project_batch(captured,str(tmp_path))
    assert second.state=='succeeded' and second.reused==2 and second.saved==0
    o.cell_min_nucleus_area_um2=10.2
    changed=q.ProjectBatchRequest(project,transform,options=o)
    assert changed.parameters_sha256!=captured.parameters_sha256
    third=q.run_project_batch(changed,str(tmp_path))
    assert third.state=='succeeded' and third.saved==2 and third.reused==0


@pytest.mark.skipif(not q.has_project,reason='project batch')
def test_unknown_calibration_follows_per_image_failure_policy(tmp_path):
    project,docs,o=batch_scene();docs[0][1].clear_calibration()
    o.failure=q.ProjectBatchFailure.CONTINUE
    captured=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=o)
    result=q.run_project_batch(captured,str(tmp_path))
    assert result.state=='partial' and result.failed==result.saved==1
    assert 'both pixel calibration axes' in result.images[0].error


@pytest.mark.skipif(not q.has_project,reason='project batch')
def test_changed_calibration_cannot_reuse_an_old_physical_resolution(tmp_path):
    project,docs,o=batch_scene();t=q.StainTransform([[1,0,0],[0,1,0]])
    first=q.run_project_batch(q.ProjectBatchRequest(project,t,options=o),str(tmp_path))
    assert first.state=='succeeded' and first.saved==2
    docs[0][1].set_calibration(1,1);o.failure=q.ProjectBatchFailure.CONTINUE
    changed=q.run_project_batch(q.ProjectBatchRequest(project,t,options=o),str(tmp_path))
    assert changed.images[0].state!='reused'
    o.resume=False
    fresh=q.run_project_batch(q.ProjectBatchRequest(project,t,options=o),str(tmp_path))
    assert fresh.state=='succeeded' and fresh.saved==2
    image,_,obj,_=docs[0]
    assert q.Project.open(fresh.images[0].path).document(image).latest_cells(obj).result.cell_count==0


@pytest.mark.skipif(not q.has_project,reason='project CLI')
def test_cli_preserves_requested_physical_bounds_and_reuses_checkpoints(tmp_path):
    project,_,_=batch_scene();source=tmp_path/'source.qpxproj';project.save(str(source))
    cli=Path(_native.__file__).parent/('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    args=[str(cli),str(source),str(tmp_path/'output'),'--kind','cells','--threshold','.5',
        '--stains','1,0,0,0,1,0,0,0,1','--normalized','--cell-min-area-um2','10.1','--cell-max-area-um2','22.5']
    for saved,reused in [(2,0),(0,2)]:
        done=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert done.returncode==0,done.stderr+done.stdout
        report=json.loads(done.stdout.splitlines()[-1]);assert report['saved']==saved and report['reused']==reused
    for extra in [['--cell-min-area-um2','-1'],['--cell-max-area-um2','nan']]:
        assert subprocess.run([*args,*extra],capture_output=True,timeout=60).returncode!=0


@pytest.mark.parametrize('kind',[q.ProjectBatchKind.ROI,q.ProjectBatchKind.TISSUE])
def test_non_cell_batches_cannot_silently_ignore_physical_area_limits(kind):
    o=q.ProjectBatchOptions();o.kind=kind;o.cell_min_nucleus_area_um2=0
    with pytest.raises(ValueError,match='require a cell batch'):o.validate()
