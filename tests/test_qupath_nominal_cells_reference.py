"""Original-source nominal masks and honest sampling-domain comparisons."""
import importlib.util
from pathlib import Path
import shutil
from types import SimpleNamespace

import numpy as np
import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('nominal_reference',ROOT/'tools/compare_qupath_nominal_cells.py')
comparison=importlib.util.module_from_spec(spec);spec.loader.exec_module(comparison)
CONTRACT=comparison.shared().read(comparison.FIXTURES/'contract.json')
NAMES=[c['name'] for c in CONTRACT['cases']]
EXACT=[name for name in NAMES if name!='roi_cut_hole']
pytestmark=pytest.mark.skipif(not q.has_geos,reason='GEOS nominal cell reference')


@pytest.fixture(scope='module')
def reports():
    return {context:comparison.compare(context=context) for context in (0,32)}


@pytest.mark.parametrize('name',EXACT)
def test_original_source_nominal_exact_level_zero_masks_and_object_relations(reports,name):
    for report in reports.values():
        case=next(c for c in report['cases'] if c['case_name']==name)
        assert case['original_source_request'] and case['independent_processing_rgb_exact']
        assert case['algorithm']=='qpx.nominal-rgb-log-watershed-bodies/1'
        assert all(f['processing_arithmetic_accepted'] for f in case['feature_checks'])
        for metric in case['level_zero_segmentation'].values():
            assert metric['reference_count']==metric['native_count']
            assert metric['foreground_iou']==metric['foreground_dice']==1
            assert metric['unmatched_reference']==metric['unmatched_native']==0
            assert not metric['split_reference_labels'] and not metric['merged_native_labels']
            assert all(m['iou']==1 and m['centroid_error_px']==0 for m in metric['matches'])


@pytest.mark.parametrize('name',EXACT)
def test_full_image_context_preserves_core_outputs_and_keeps_distinct_input_identity(reports,name):
    a,b=[next(c for c in reports[k]['cases'] if c['case_name']==name) for k in (0,32)]
    assert a['processing_label_sha256']==b['processing_label_sha256']
    assert a['level_zero_label_sha256']==b['level_zero_label_sha256']
    assert a['feature_checks']==b['feature_checks']
    assert a['input_sha256']!=b['input_sha256']


def test_clipped_roi_differences_are_retained_with_exact_boundary_diagnostics(reports):
    cases=[next(c for c in reports[k]['cases'] if c['case_name']=='roi_cut_hole') for k in (0,32)]
    a,b=cases
    assert a['source_bounds']==b['source_bounds']==[32,32,192,128]
    assert a['core_shape']==b['core_shape']==[128,192]
    assert a['foreground_pixel_differences']['nucleus']['reference_only']==dict(count=4,first_xy=[[32,68],[33,68],[32,81],[33,81]],truncated=False)
    assert a['foreground_pixel_differences']['nucleus']['native_only']['count']==0
    assert b['foreground_pixel_differences']['nucleus']['reference_only']['count']==0
    assert b['foreground_pixel_differences']['nucleus']['native_only']==dict(count=24,first_xy=[[x,y] for y in range(69,81) for x in (32,33)],truncated=False)
    for c in cases:
        assert c['level_zero_segmentation']['nucleus']['foreground_iou']<1
        assert c['level_zero_segmentation']['body']['foreground_iou']==1
        assert all(f['processing_arithmetic_accepted'] for f in c['feature_checks'])
    full=comparison.compare(grid_scope='full-image',context=0)
    c=next(c for c in full['cases'] if c['case_name']=='roi_cut_hole')
    assert c['source_bounds']==[0,0,256,192] and c['core_shape']==[192,256]
    assert c['level_zero_label_sha256']==b['level_zero_label_sha256']


def test_processing_features_and_level_zero_features_are_separate_domains(reports):
    for report in reports.values():
        assert report['frozen_comparison_tolerances']==CONTRACT['comparison']
        assert [c['case_name'] for c in report['cases']]==NAMES
        assert not report['omitted_anisotropic_physical_cases']
        assert report['state']=='characterized-partial'
        assert not report['segmentation_parity_accepted'] and not report['biological_validation'] and not report['general_speed_claim']
        for c in report['cases']:
            if c['case_name']!='resampled_reference':
                assert all(f['same_sampling_domain'] and f['within_frozen_tolerance'] for f in c['matched_reference_feature_differences'])
                continue
            assert c['nominal_downsample']==1.5 and c['core_shape']==[128,171]
            assert len(c['feature_checks'])==6 and all(f['processing_arithmetic_accepted'] for f in c['feature_checks'])
            assert all(m['foreground_iou']==m['foreground_dice']==1 for m in c['processing_segmentation'].values())
            differences=c['matched_reference_feature_differences']
            assert len(differences)==6 and all(not f['same_sampling_domain'] and not f['within_frozen_tolerance'] for f in differences)


def test_isotropic_physical_requests_match_pixel_requests_and_keep_anisotropic_boundary(reports):
    physical=comparison.compare(context=32,units='micrometres')
    assert physical['omitted_anisotropic_physical_cases']==['anisotropic_level0']
    assert len(physical['cases'])==8
    for c in physical['cases']:
        pixel=next(x for x in reports[32]['cases'] if x['case_name']==c['case_name'])
        assert c['processing_label_sha256']==pixel['processing_label_sha256']
        assert c['level_zero_segmentation']==pixel['level_zero_segmentation']
        assert c['feature_checks']==pixel['feature_checks']


@pytest.mark.parametrize('failure',['none','origin','extent','step','counts','context','shape','dtype','escaped','bounds','image','uncovered'])
def test_projection_rejects_changed_descriptors_and_hidden_labels(failure):
    width,height=4,3;bounds=[0,0,width,height];downsample=1
    n=q.NominalCellSampling(q.RgbNominalRequest(*bounds,downsample),1,1)
    labels=np.zeros((5,6),dtype=np.uint32)
    if failure=='none':n=None
    elif failure=='origin':n=q.NominalCellSampling(q.RgbNominalRequest(1,0,4,3),1,1)
    elif failure=='extent':n=q.NominalCellSampling(q.RgbNominalRequest(0,0,3,3),1,1)
    elif failure=='step':n=q.NominalCellSampling(q.RgbNominalRequest(0,0,4,3,1.5),1,1)
    elif failure in ('counts','context'):
        n=SimpleNamespace(source_request=n.source_request,core_width=4,core_height=3,context_x=1,context_y=1)
        if failure=='counts':n.core_width=3
        else:n.context_x=-1
    elif failure=='shape':labels=labels[:-1]
    elif failure=='dtype':labels=labels.astype(float)
    elif failure=='escaped':labels[0,0]=1
    elif failure=='bounds':bounds[0]=-1
    elif failure=='image':width=3
    elif failure=='uncovered':
        width=5;bounds=[0,0,5,3];downsample=1.5
        n=q.NominalCellSampling(q.RgbNominalRequest(*bounds,downsample),1,1);labels=np.zeros((4,5),dtype=np.uint32)
    raster=SimpleNamespace(nominal_sampling=n,labels_numpy=lambda:labels)
    with pytest.raises(ValueError,match='bounds|nominal|descriptor|raster'):
        comparison.nominal_canvas(raster,width,height,bounds,downsample)


def test_projection_uses_nominal_step_and_source_origin_without_realized_ratio():
    core=np.arange(1,21,dtype=np.uint32).reshape(4,5)
    n=q.NominalCellSampling(q.RgbNominalRequest(0,0,7,6,1.5),1,1)
    labels=np.pad(core,1);raster=SimpleNamespace(nominal_sampling=n,labels_numpy=lambda:labels)
    expected=core[np.array([0,1,1,2,3,3])[:,None],np.array([0,1,1,2,3,3,4])[None,:]]
    np.testing.assert_array_equal(comparison.nominal_canvas(raster,7,6,[0,0,7,6],1.5),expected)
    n=q.NominalCellSampling(q.RgbNominalRequest(1,1,2,2,1),1,1)
    raster=SimpleNamespace(nominal_sampling=n,labels_numpy=lambda:np.pad(np.array([[1,2],[3,4]],dtype=np.uint32),1))
    expected=np.zeros((4,4),dtype=np.uint32);expected[1:3,1:3]=[[1,2],[3,4]]
    np.testing.assert_array_equal(comparison.nominal_canvas(raster,4,4,[1,1,2,2],1),expected)


@pytest.mark.parametrize('units',['micrometres','unknown'])
def test_incompatible_physical_or_unknown_recipe_is_not_silently_coerced(units):
    case=next(c for c in CONTRACT['cases'] if c['name']=='anisotropic_level0')
    with pytest.raises(ValueError,match='isotropic calibration|unknown comparison units'):
        comparison.compare_case(comparison.FIXTURES/'capture',CONTRACT,case,units=units)


def test_changed_reference_payload_rejects_before_native_detection(tmp_path,monkeypatch):
    copied=tmp_path/'capture';shutil.copytree(comparison.FIXTURES/'capture',copied)
    p=copied/'isolated_three/nucleus.u32le.gz';p.write_bytes(p.read_bytes()+b'x')
    def forbidden(*args,**kwargs):raise AssertionError('native detector reached before fixture verification')
    monkeypatch.setattr(q,'detect_cells',forbidden)
    with pytest.raises(ValueError,match='cell capture changed'):
        comparison.compare(copied)
