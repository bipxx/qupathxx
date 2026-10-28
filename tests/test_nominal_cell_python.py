"""Installed original-document nominal cells: thin SDK, history and consumers."""
import csv
import gc
import io
from pathlib import Path

import numpy as np
import pytest
import qupathxx as q

FIXTURE=Path(__file__).parent/'fixtures/nominal-cells-v1'


def scene():
    rgb=np.frombuffer((FIXTURE/'source.rgb').read_bytes(),dtype=np.uint8).reshape(192,256,3)
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(.5,.5)
    obj=doc.add_rectangle(q.Rect(0,0,256,192))
    transform=q.StainTransform([[.65,.7,.29],[.07,.99,.11]])
    p=q.GridCellParameters();p.threshold=.4;p.sigma=1;p.cell_expansion=5
    p.min_nucleus_area=16;p.max_nucleus_area=512
    contours=p.contours;contours.work_bytes=1024*1024;p.contours=contours
    return doc,obj,transform,p


def request(doc,obj,transform,p):
    return q.CellRequest.for_nominal_roi(doc,obj,transform,1.5,0,0,p)


@pytest.mark.parametrize('field',['source_request','context_x','context_y','core_width','core_height','width','height'])
def test_nominal_sampling_is_immutable(field):
    n=q.NominalCellSampling(q.RgbNominalRequest(5,7,17,13,1.5),3,4)
    with pytest.raises(AttributeError):setattr(n,field,0)


@pytest.mark.parametrize('method,index',[('edge_x',-1),('edge_x',12),('center_x',-1),('center_x',11),
    ('edge_y',-1),('edge_y',10),('center_y',-1),('center_y',9)])
def test_nominal_sampling_rejects_indices_outside_owned_lattice(method,index):
    n=q.NominalCellSampling(q.RgbNominalRequest(0,0,17,13,1.5),0,0)
    with pytest.raises(IndexError):getattr(n,method)(index)


def test_nominal_descriptor_retains_nominal_geometry_and_owned_request():
    n=q.NominalCellSampling(q.RgbNominalRequest(5,7,17,13,1.5),3,4)
    assert (n.core_width,n.core_height,n.width,n.height)==(11,9,17,17)
    assert n.edge_x(3)==5 and n.center_x(3)==5.75
    assert n.edge_x(14)==21.5 and n.edge_y(13)==20.5
    held=n.source_request;del n;gc.collect()
    assert held.x_px==5 and held.downsample==1.5 and held.algorithm=='qpx.rgb-nominal/1'
    assert q.NominalCellSampling(held).context_x==32


@pytest.mark.parametrize('factor',[0,-1,float('inf'),float('nan'),1e-300])
def test_nominal_roi_factory_rejects_invalid_factors(factor):
    doc,obj,transform,p=scene()
    with pytest.raises(ValueError):q.CellRequest.for_nominal_roi(doc,obj,transform,factor,0,0,p)


@pytest.mark.parametrize('cx,cy',[(4097,0),(0,4097),(-1,0),(0,-1)])
def test_nominal_sampling_rejects_invalid_context(cx,cy):
    with pytest.raises((ValueError,TypeError,OverflowError)):
        q.NominalCellSampling(q.RgbNominalRequest(0,0,17,13,1.5),cx,cy)


def test_nominal_roi_factory_encloses_fractional_bounds_without_changing_roi():
    doc,obj,transform,p=scene();doc.update_roi(obj,q.Roi.rectangle(q.Rect(20.25,30.5,80.5,60.25)))
    original=doc.annotation(obj).geometry
    r=q.CellRequest.for_nominal_roi(doc,obj,transform,2.3,3,4,p);n=r.nominal_sampling;s=n.source_request
    assert (s.x_px,s.y_px,s.width_px,s.height_px)==(20,30,81,61)
    assert s.downsample==2.3 and (n.context_x,n.context_y)==(3,4)
    assert r.roi.equals(original) and doc.annotation(obj).geometry.equals(original)
    assert r.scientific_grid is None


def test_nominal_original_reader_matches_captured_masks_features_and_input_hash():
    doc,obj,transform,p=scene();r=request(doc,obj,transform,p)
    p.min_nucleus_area=100000;r.grid_parameters.min_nucleus_area=999999
    result=q.detect_cells(r)
    assert result.algorithm=='qpx.nominal-rgb-log-watershed-bodies/1' and result.cell_count==3
    assert result.grid_parameters.min_nucleus_area==16 and result.scientific_grid is None
    expected={name:np.frombuffer((FIXTURE/(name+'.u32le')).read_bytes(),dtype='<u4').reshape(128,171) for name in ('nucleus','body')}
    assert np.array_equal(result.nuclei.labels_numpy(),expected['nucleus'])
    assert np.array_equal(result.cells.labels_numpy(),expected['body'])
    assert q.input_sha256(r)==result.nuclei.input_sha256==result.cells.input_sha256
    assert result.nominal_sampling.source_request.downsample==1.5
    assert result.nuclei.nominal_sampling.core_width==171 and result.cells.nominal_sampling.core_height==128
    assert result.nuclei.sample_edge_x(171)==256.5 and result.nuclei.downsample_x==1.5
    rgb=np.frombuffer((FIXTURE/'processed.rgb').read_bytes(),dtype=np.uint8).reshape(128,171,3)
    for f in result.features:
        for prefix,name in [('nucleus','nucleus'),('cell','body')]:
            selected=expected[name]==f.label;count=int(selected.sum());values=rgb[selected].astype(float)
            assert getattr(f,prefix+'_pixels')==count
            assert np.allclose(getattr(f,prefix+'_rgb'),values.mean(axis=0),atol=1e-6,rtol=1e-6)
            od=np.maximum(0,np.log10(255/np.maximum(values,1)))
            assert np.allclose(getattr(f,prefix+'_channels'),od.mean(axis=0),atol=1e-6,rtol=1e-6)
            assert getattr(result.calibrated_feature(f.label),prefix+'_sample_area_um2')==count*.75**2
    descriptor=result.nominal_sampling;mask=result.cells.labels_numpy();doc.server.close()
    del doc,r,result;gc.collect()
    assert descriptor.source_request.width_px==256 and np.array_equal(mask,expected['body'])


def test_nominal_direct_constructor_and_roi_factory_share_the_native_recipe():
    doc,obj,transform,p=scene();descriptor=q.NominalCellSampling(q.RgbNominalRequest(0,0,256,192,1.5),0,0)
    direct=q.detect_cells(q.CellRequest(doc,obj,transform,descriptor,p));automatic=q.detect_cells(request(doc,obj,transform,p))
    assert direct.nuclei.input_sha256==automatic.nuclei.input_sha256
    assert direct.cells.labels==automatic.cells.labels
    assert q.detect_tissue(q.TissueRequest(doc,obj,transform)).nominal_sampling is None


@pytest.mark.skipif(not q.has_project,reason='requires project storage')
def test_nominal_job_project_reopen_verification_and_csv_keep_original_authority(tmp_path):
    doc,obj,transform,p=scene();r=request(doc,obj,transform,p);job=q.CellJob()
    assert job.request(r) and job.wait(doc,obj).state=='ready'
    run=job.keep_record(doc,obj);assert doc.cell_status(run)=='current'
    options=q.CellTableOptions();options.include_capture_calibration=True
    held=q.CellTableRequest(doc,[run],options)
    rows=list(csv.DictReader(io.StringIO(q.cell_measurements_csv(held))))
    assert len(rows)==3
    for row in rows:
        assert row['table_schema']=='qpx.cell-table/8' and row['sampling_grid']=='nominal_rgb'
        assert row['detector_mode']=='nominal-log-bodies' and float(row['sample_step_x_px'])==1.5
        assert row['grid_extent_width_px']=='256' and row['grid_samples_x']=='171'
        assert row['grid_context_x']=='0' and row['grid_input_format']=='qpx.nominal-cell-rgb/1'
    project=q.Project();image=project.add_document(doc);path=tmp_path/'nominal.qpxproj';project.save(str(path))
    reopened=q.Project.open(str(path)).document(image);record=reopened.cell_record(run)
    assert reopened.cell_status(run)=='unverified' and record.result.nominal_sampling.source_request.downsample==1.5
    replay=q.CellRequest.replay(reopened,run);fresh=q.detect_cells(replay)
    assert reopened.verify_cells(run,replay,fresh) and reopened.cell_status(run)=='current'
    reopened.set_calibration(.5,.25)
    assert reopened.cell_status(run)=='calibration_changed'
    with pytest.raises(ValueError):q.CellRequest.replay(reopened,run)
    doc.server.close();assert len(list(csv.DictReader(io.StringIO(q.cell_measurements_csv(held)))))==3


@pytest.mark.skipif(not q.has_geos or not q.has_project,reason='requires GEOS and project nominal cell publication')
def test_nominal_classifier_origin_and_project_reopen_preserve_descriptor(tmp_path):
    from test_object_classifier_archives import model_for
    doc,obj,transform,p=scene();r=request(doc,obj,transform,p);result=q.detect_cells(r)
    run=doc.keep_cells(r,result);ids=doc.publish_cell_record(run)
    options=q.ObjectFeatureOptions();options.cell_rgb=True
    features=q.ObjectFeatureRequest(doc,ids,options);table=q.extract_object_features(features)
    job=q.ObjectClassifierJob();assert job.predict(features,model_for(table.features))
    assert job.wait(doc).state=='ready';origin=job.archive.table.cell_origin(0).source
    assert origin.nominal_sampling.source_request.downsample==1.5 and origin.scientific_grid is None
    assert origin.resolved_grid_parameters.step_x==1.5 and origin.grid_parameters.min_nucleus_area==16
    for name in ('parameters','downsample'):
        with pytest.raises(RuntimeError):getattr(origin,name)
    job.apply(doc);record=doc.classifier_records[0]
    project=q.Project();image=project.add_document(doc);path=tmp_path/'classifier.qpxproj';project.save(str(path))
    restored=q.Project.open(str(path)).document(image).classifier_record(record.id).archive.table.cell_origin(0).source
    assert restored.nominal_sampling.source_request.width_px==256 and restored.nominal_sampling.core_width==171
    descriptor=restored.nominal_sampling;del restored,origin,table,features,job,doc;gc.collect()
    assert descriptor.source_request.downsample==1.5


def test_nominal_physical_parameters_resolve_per_axis_and_unknown_calibration_rejects():
    doc,obj,transform,p=scene();doc.set_calibration(.5,.25);p.units=q.GridCellUnits.MICROMETRES
    r=request(doc,obj,transform,p)
    assert (r.resolved_grid_parameters.step_x,r.resolved_grid_parameters.step_y)==(.75,.375)
    doc.clear_calibration()
    with pytest.raises(ValueError):request(doc,obj,transform,p)
