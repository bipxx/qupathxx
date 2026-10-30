"""Immutable real-tissue QuPath reference; no runtime or large slide for basic checks."""
import gzip
import importlib.util
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import numpy as np
import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_ndpi_reference',ROOT/'tools/compare_qupath_ndpi_cells.py')
comparison=importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
CAPTURE=ROOT/'tests/fixtures/qupath-ndpi-cells-0.7.0/capture'


def test_unit_nominal_reference_crop_preserves_original_coordinates_and_context():
    values=np.arange(35,dtype=np.uint32).reshape(5,7)
    request=SimpleNamespace(x_px=10002,y_px=20001,downsample=1.)
    nominal=SimpleNamespace(source_request=request,context_x=2,context_y=1)
    mask=SimpleNamespace(nominal_sampling=nominal,width=7,height=5,labels_numpy=lambda:values)
    actual=comparison.local_labels(mask,[10002,20001,3,2])
    np.testing.assert_array_equal(actual,values[1:3,2:5])
    actual=comparison.local_labels(mask,[9999,19999,3,3])
    expected=np.zeros((3,3),dtype=np.uint32);expected[1:,1:]=values[:2,:2]
    np.testing.assert_array_equal(actual,expected)
    request.downsample=1.5
    with pytest.raises(ValueError,match='unit nominal'):
        comparison.local_labels(mask,[10002,20001,3,2])


def test_body_boundary_vectors_decode_public_capture_without_expansion(tmp_path):
    spec=importlib.util.spec_from_file_location('ndpi_body_vectors',ROOT/'tools/reference_qupath_body_boundary.py')
    tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
    generated=tmp_path/'vectors';manifest=tool.generate(generated)
    assert manifest['labels']==506 and manifest['expansion_performed'] is False
    assert {p.name:p.read_bytes() for p in generated.iterdir()}=={p.name:p.read_bytes() for p in tool.FIXTURE.iterdir()}


@pytest.mark.skipif(not os.environ.get('QUPATHXX_HAMAMATSU_FIXTURE'),reason='opt-in pinned Hamamatsu NDPI fixture')
def test_real_slide_nominal_revision_preserves_v1_and_corrects_v2_boundary_masks(tmp_path):
    image=Path(os.environ['QUPATHXX_HAMAMATSU_FIXTURE'])
    for revision in (1,2):
        report=comparison.compare(CAPTURE,image,context_padding=0,include_nominal=True,nominal_revision=revision)
        (tmp_path/('nominal-v'+str(revision)+'.json')).write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
        assert report['source_pixels_equal'] and report['source_pixel_disagreements']==0
        run,=[r for r in report['runs'] if r['mode']=='nominal-qupath070']
        assert run['tile_invariant'] is None and run['independent_rgb_features_passed'] and len(run['results'])==1
        result=run['results'][0]
        assert result['cells']==506 and result['algorithm']=='qpx.nominal-affine-rgb-qupath070-cell-bodies/'+str(revision)
        assert result['requested_grid_recipe']['processing']=='qupath070/'+str(revision)
        assert result['segmentation']['nucleus']['foreground_iou']==1
        if revision==1:
            assert result['segmentation']['body']['foreground_iou']==0.9998423893770441
        else:
            for metric in result['segmentation'].values():
                assert metric['foreground_iou']==metric['foreground_dice']==1
                assert metric['unmatched_reference']==metric['unmatched_native']==0
                assert not metric['split_reference_labels'] and not metric['merged_native_labels']
                assert all(m['iou']==1 and m['centroid_error_px']==0 for m in metric['matches'])
        assert not report['biological_validation'] and not report['performance_accepted']
