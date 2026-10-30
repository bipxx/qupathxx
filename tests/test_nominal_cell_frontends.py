"""Nominal cell desktop/managed consumers of the original-source native recipe."""
import csv
import gc
import io
import os

import numpy as np
import pytest
import qupathxx as q
from test_nominal_cell_python import scene,FIXTURE
from test_nominal_cell_batches import cohort

GUI=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in nominal cell desktop')


def sampling(x=0,y=0):
    value=q.NominalCellRoiSampling();value.context_x=x;value.context_y=y
    return value


def configure_policy(processing,p,n):
    if processing=='imagej154p':p.processing=q.CellProcessingPolicy.IMAGEJ_154P
    elif processing=='qupath070-affine':
        p.processing=q.CellProcessingPolicy.QUPATH_070;n.sampling=q.RgbNominalSampling.AFFINE_INVERSE
    elif processing=='qupath070-v2-affine':
        p.processing=q.CellProcessingPolicy.QUPATH_070_V2;n.sampling=q.RgbNominalSampling.AFFINE_INVERSE


def pinned(result):
    assert result.cell_count==3
    assert result.nuclei.foreground_pixels==379 and result.cells.foreground_pixels==825
    n=result.nominal_sampling
    assert n.source_request.downsample==1.5 and (n.core_width,n.core_height)==(171,128)
    for mask,name in [(result.nuclei,'nucleus.u32le'),(result.cells,'body.u32le')]:
        expected=np.frombuffer((FIXTURE/name).read_bytes(),dtype='<u4').reshape(128,171)
        actual=mask.labels_numpy()[n.context_y:n.context_y+128,n.context_x:n.context_x+171]
        assert np.array_equal(actual,expected)


def test_nominal_gui_options_own_sampling_and_detector_parameters():
    _,_,_,p=scene();n=sampling();options=q.GuiOptions();options.cell_nominal=n;options.grid_cell_parameters=p
    n.downsample=3;p.min_nucleus_area=10000
    options.cell_nominal.downsample=7;options.grid_cell_parameters.min_nucleus_area=20000
    assert options.cell_nominal.downsample==1.5 and options.grid_cell_parameters.min_nucleus_area==16
    options.cell_nominal=None;assert options.cell_nominal is None


@pytest.mark.parametrize('factor',[0,-1,float('nan'),float('inf')])
def test_nominal_gui_options_reject_invalid_sampling(factor):
    n=sampling();n.downsample=factor
    with pytest.raises(ValueError):q.GuiOptions().cell_nominal=n


@pytest.mark.parametrize('bad',['factor','context','detector'])
def test_nominal_managed_admission_precedes_enqueue(tmp_path,bad):
    _,_,transform,p=scene();n=sampling();session=q.ViewerSession(str(tmp_path/'absent.qpxproj'))
    if bad=='factor':n.downsample=0
    elif bad=='context':n.context_y=4097
    else:p.min_nucleus_area=-1
    with pytest.raises(ValueError):session.nominal_cells(1,1,transform,n,p,True)
    assert session.stats.tickets==session.stats.queued_bytes==0


def test_nominal_managed_queued_cancellation_releases_copied_command(tmp_path):
    _,_,transform,p=scene();session=q.ViewerSession(str(tmp_path/'absent.qpxproj'))
    ticket=session.nominal_cells(1,1,transform,sampling(),p,True)
    assert session.poll(ticket).action=='cells'
    assert session.cancel(ticket) and session.take(ticket).state=='cancelled'
    assert session.stats.tickets==session.stats.queued_bytes==session.stats.retained_bytes==0


@GUI
@pytest.mark.skipif(not q.has_geos,reason='requires cell publication')
@pytest.mark.parametrize('context',[(0,0),(3,4)])
@pytest.mark.parametrize('processing',['quantitative','imagej154p','qupath070-affine','qupath070-v2-affine'])
def test_nominal_desktop_detect_archive_publish_export_matches_pinned_masks(tmp_path,context,processing):
    from test_gui_replay import event
    doc,obj,_,p=scene()
    n=sampling(*context);configure_policy(processing,p,n)
    options=q.GuiOptions();options.cell_nominal=n;options.grid_cell_parameters=p
    options.width=1100;options.height=800;options.docked_workspace=False;options.settle_tiles=True
    table=tmp_path/'nominal.csv';options.cell_table_path=str(table)
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.DETECT_CELLS_KEY),
        event(10,q.GuiInputKind.KEEP_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_READY),
        event(12,q.GuiInputKind.KEEP_CELLS_KEY,wait=q.GuiInputWait.CELLS_ARCHIVED),
        event(14,q.GuiInputKind.EXPORT_CELL_TABLE_KEY,wait=q.GuiInputWait.CELLS_PUBLISHED),
        event(16,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    capture=tmp_path/'nominal-desktop.ppm'
    report=q.show(doc,frame_limit=45,capture_path=str(capture),options=options)
    assert report.cell_state=='ready' and not report.cell_error and report.cells_published
    assert report.cell_annotations_added==3 and report.cell_preview_ready and report.cell_gpu_bytes>0
    assert report.selected_ids==[obj] and doc.object_count==4 and doc.cell_record_count==1
    pinned(report.cell_result);pinned(doc.cell_records[0].result)
    assert report.cell_result.grid_parameters.processing==p.processing
    assert report.cell_result.nominal_sampling.source_request.sampling==n.sampling
    rows=list(csv.DictReader(io.StringIO(table.read_text(encoding='utf-8'))))
    assert len(rows)==3 and all(row['sampling_grid']=='nominal_rgb' for row in rows)
    if processing=='imagej154p':assert all(row['processing_policy']=='imagej154p/1' for row in rows)
    if processing=='qupath070-affine':assert all(row['processing_policy']=='qupath070/1' and row['sampling_algorithm']=='qpx.rgb-nominal-affine/1' for row in rows)
    if processing=='qupath070-v2-affine':assert all(row['processing_policy']=='qupath070/2' and row['sampling_algorithm']=='qpx.rgb-nominal-affine/1' for row in rows)
    assert capture.is_file() and capture.stat().st_size>1100*800*3


@GUI
@pytest.mark.skipif(not q.has_project,reason='requires project storage')
@pytest.mark.parametrize('processing',['quantitative','imagej154p','qupath070-affine','qupath070-v2-affine'])
def test_nominal_desktop_batch_uses_existing_checkpoints_and_resumes(tmp_path,processing):
    from test_gui_batch import options as batch_options
    project,docs,transform,preset,_=cohort()
    p=preset.grid_cells;n=preset.cell_nominal;configure_policy(processing,p,n);preset.grid_cells=p;preset.cell_nominal=n
    # Existing OD-sum GUI capture uses an identity basis, independently of its visible stain vectors.
    expected=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=preset)
    ui=batch_options(tmp_path/'results',preset);ui.settle_tiles=True
    for attempt in range(2):
        report=q.show(project,frame_limit=40,options=ui)
        assert report.batch_state=='succeeded' and not report.batch_error
        result=report.batch_result;assert result.parameters_sha256==expected.parameters_sha256
        assert (result.saved,result.reused)==((2,0) if attempt==0 else (0,2))
        for entry,(image,original,_) in zip(result.images,docs):
            run=q.Project.open(entry.path).document(image).cell_records[0].result
            assert run.nominal_sampling.source_request.downsample==1.5 and original.cell_record_count==0
            assert run.grid_parameters.processing==preset.grid_cells.processing
            assert run.nominal_sampling.source_request.sampling==n.sampling
            assert run.nuclei.stains==[[1,0,0],[0,1,0],[0,0,1]]
            if image==docs[0][0]:pinned(run)


@GUI
@pytest.mark.skipif(not q.has_project,reason='requires project storage')
@pytest.mark.parametrize('retain',[False,True])
@pytest.mark.parametrize('processing',['quantitative','imagej154p','qupath070-affine','qupath070-v2-affine'])
def test_nominal_managed_command_copies_retains_exports_and_saves(tmp_path,retain,processing):
    from test_viewer_session import completed,run
    doc,obj,transform,p=scene()
    n=sampling();configure_policy(processing,p,n);coordinates=n.sampling
    policy=p.processing;project=q.Project();image=project.add_document(doc,'Managed nominal 原始𠀋')
    source=tmp_path/'source.qpxproj';saved=tmp_path/'saved.qpxproj';table=tmp_path/'cells.csv';project.save(str(source))
    original=source.read_bytes();session=q.ViewerSession(str(source));held={}
    def producer():
        completed(session,session.catalog())
        ticket=session.nominal_cells(image,obj,transform,n,p,retain_result=retain)
        n.downsample=3;n.sampling=q.RgbNominalSampling.INTEGER_ORIGIN;p.min_nucleus_area=10000;p.processing=q.CellProcessingPolicy.QUANTITATIVE
        result=completed(session,ticket);assert result.committed and result.record_id
        assert (result.cells is not None)==retain
        if retain:pinned(result.cells);held['result']=result.cells
        assert session.stats.retained_bytes==0
        assert completed(session,session.export_cells(image,str(table),True)).committed
        assert completed(session,session.save(str(saved))).committed
        completed(session,session.close(True))
    run(session,producer)
    assert session.stats.state=='closed' and session.stats.tickets==0 and source.read_bytes()==original
    restored=q.Project.open(str(saved)).document(image)
    assert restored.cell_record_count==1 and doc.cell_record_count==0
    pinned(restored.cell_records[0].result)
    assert restored.cell_records[0].result.grid_parameters.processing==policy
    assert restored.cell_records[0].result.nominal_sampling.source_request.sampling==coordinates
    rows=list(csv.DictReader(io.StringIO(table.read_text(encoding='utf-8'))))
    assert len(rows)==3 and all(row['sampling_grid']=='nominal_rgb' for row in rows)
    del restored,doc;gc.collect()
    if retain:pinned(held['result'])
