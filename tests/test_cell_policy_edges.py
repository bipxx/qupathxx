"""Held-out characterization preserves exact successes and named parity gaps."""
import importlib.util
from pathlib import Path

import numpy as np
import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('policy_edges',ROOT/'tools/qupath_cell_policy_edges.py')
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
NAMES=[case['name'] for case in tool.contract()['cases']]
EXACT=['slide-edge-1.0','slide-edge-1.25','slide-edge-1.5','odd-crop-1.0','odd-crop-1.25']
GAPS=[name for name in NAMES if name not in EXACT]


@pytest.fixture(scope='module')
def capture():return tool.verify()


@pytest.fixture(scope='module')
def report():return tool.compare()


@pytest.fixture(scope='module')
def original_source_report():
    return tool.compare(processing_policy='qupath070',sampling='affine-inverse')


@pytest.mark.skipif(not q.has_geos,reason='requires held-out ROI geometry')
@pytest.mark.parametrize('name',NAMES)
def test_original_slide_affine_qupath_recipe_matches_captured_masks_and_features(original_source_report,name):
    case=next(case for case in original_source_report['cases'] if case['name']==name)
    assert case['input_rgb_exact'] and case['input_component_differences']==0
    for metric in case['metrics'].values():
        assert metric['foreground_iou']==metric['foreground_dice']==1
        assert metric['unmatched_reference']==metric['unmatched_native']==0
        assert not metric['split_reference_labels'] and not metric['merged_native_labels']
        assert all(match['iou']==1 and match['centroid_error_px']==0 for match in metric['matches'])
    assert all(feature['processing_arithmetic_accepted'] for feature in case['features'])
    assert not original_source_report['biological_validation']


@pytest.mark.parametrize('name',NAMES)
def test_captured_stage_inputs_and_geometry_are_independent_and_exact(capture,name):
    case=next(case for case in capture['cases'] if case['name']==name)
    assert case['input_exact'] and case['exported_debug_nuclei_exact']
    assert min(case['shape'])>0


@pytest.mark.skipif(not q.has_geos,reason='requires held-out ROI geometry')
@pytest.mark.parametrize('name',EXACT)
def test_held_out_exact_processing_masks_have_no_splits_or_feature_errors(report,name):
    case=next(case for case in report['cases'] if case['name']==name)
    assert case['input_rgb_exact'] and case['input_component_differences']==0
    assert all(feature['processing_arithmetic_accepted'] for feature in case['features'])
    for metric in case['metrics'].values():
        assert metric['foreground_iou']==metric['foreground_dice']==1
        assert metric['unmatched_reference']==metric['unmatched_native']==0
        assert not metric['split_reference_labels'] and not metric['merged_native_labels']
        assert all(match['iou']==1 and match['centroid_error_px']==0 for match in metric['matches'])


@pytest.mark.skipif(not q.has_geos,reason='requires held-out ROI geometry')
@pytest.mark.parametrize('name',GAPS)
def test_unresolved_detector_gaps_remain_explicit_characterization(report,name):
    case=next(case for case in report['cases'] if case['name']==name)
    assert report['state']=='characterized-partial'
    assert not report['segmentation_parity_accepted'] and not report['biological_validation']
    assert case['metrics']['nucleus']['foreground_iou']<1
    if name.startswith('odd-') and name.endswith('-1.5'):
        assert not case['input_rgb_exact'] and case['input_component_differences']==82
    else:
        assert case['input_rgb_exact'] and case['input_component_differences']==0
    if name.startswith('sigma-') and name!='sigma-0.0':assert case['upstream_refined_boundaries']
    if name=='sigma-0.0':
        assert case['metrics']['nucleus']['reference_count']==0
        assert case['metrics']['nucleus']['native_count']==2
    # Matching native feature arithmetic is separate from segmentation acceptance.
    assert all(feature['processing_arithmetic_accepted'] for feature in case['features'])


@pytest.mark.parametrize('alpha',[0,127,254])
@pytest.mark.parametrize('processing',['imagej154p','qupath070'])
@pytest.mark.skipif(not q.has_tiff,reason='requires native TIFF alpha source')
def test_imagej_policy_refuses_nonopaque_input_without_publishing(tmp_path,alpha,processing):
    import tifffile
    pixels=np.full((23,31,4),255,dtype=np.uint8);pixels[11,15,3]=alpha
    path=tmp_path/'alpha.tif';tifffile.imwrite(path,pixels,photometric='rgb',extrasamples='unassalpha',metadata=None)
    doc=q.Document(q.open_tiff(str(path)));obj=doc.add_rectangle(q.Rect(0,0,31,23))
    p=q.GridCellParameters();p.processing=q.CellProcessingPolicy.QUPATH_070 if processing=='qupath070' else q.CellProcessingPolicy.IMAGEJ_154P
    transform=q.StainTransform([[.65,.7,.29],[.07,.99,.11]])
    request=q.CellRequest.for_nominal_roi(doc,obj,transform,1,0,0,p)
    with pytest.raises(ValueError,match='opaque'):q.detect_cells(request)
    assert doc.cell_record_count==0


@pytest.mark.parametrize('processing',['imagej154p','qupath070'])
def test_imagej_policy_budget_and_missing_calibration_refuse_before_publication(processing):
    from test_nominal_cell_python import scene
    doc,obj,transform,p=scene();p.processing=q.CellProcessingPolicy.QUPATH_070 if processing=='qupath070' else q.CellProcessingPolicy.IMAGEJ_154P;p.work_bytes=1
    with pytest.raises((ValueError,MemoryError)):q.CellRequest.for_nominal_roi(doc,obj,transform,1.5,0,0,p)
    p.work_bytes=256*1024*1024;p.units=q.GridCellUnits.MICROMETRES;doc.clear_calibration()
    with pytest.raises(ValueError,match='calibration'):q.CellRequest.for_nominal_roi(doc,obj,transform,1.5,0,0,p)
    assert doc.cell_record_count==0


@pytest.mark.parametrize('step,x,y,left,top',[(1,-3,-4,3,4),(1.5,-3,-6,2,4)])
@pytest.mark.parametrize('processing',['imagej154p','qupath070-affine'])
def test_outside_source_centers_do_not_become_black_gaussian_input(step,x,y,left,top,processing):
    from test_nominal_cell_python import scene
    doc,obj,transform,p=scene();p.processing=q.CellProcessingPolicy.IMAGEJ_154P
    coordinates=q.RgbNominalSampling.INTEGER_ORIGIN
    if processing=='qupath070-affine':p.processing=q.CellProcessingPolicy.QUPATH_070;coordinates=q.RgbNominalSampling.AFFINE_INVERSE
    ordinary=q.detect_cells(q.CellRequest.for_nominal_roi(doc,obj,transform,step,0,0,p,coordinates))
    sampling=q.NominalCellSampling(q.RgbNominalRequest(x,y,262,204,step,coordinates),0,0)
    padded=q.detect_cells(q.CellRequest(doc,obj,transform,sampling,p))
    height,width=ordinary.nuclei.labels_numpy().shape
    for name in ('nuclei','cells'):
        actual=getattr(padded,name).labels_numpy();expected=getattr(ordinary,name).labels_numpy()
        np.testing.assert_array_equal(actual[top:top+height,left:left+width],expected)
        outside=actual.copy();outside[top:top+height,left:left+width]=0
        assert not np.any(outside)
    assert padded.nuclei.input_sha256!=ordinary.nuclei.input_sha256
    assert [(f.nucleus_rgb,f.cell_rgb,f.nucleus_channels,f.cell_channels) for f in padded.features]==[
        (f.nucleus_rgb,f.cell_rgb,f.nucleus_channels,f.cell_channels) for f in ordinary.features]
