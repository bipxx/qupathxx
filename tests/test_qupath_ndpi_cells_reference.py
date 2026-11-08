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


def test_pinned_real_slide_capture_and_independent_label_ledger():
    contract,reference=comparison.verify_capture(CAPTURE)
    labels=comparison.reference_labels(CAPTURE,contract,reference)
    assert reference['count']==506
    assert reference['source_width']*reference['source_height']>10**9
    for value in labels.values():
        assert value.shape==(256,256)
        assert set(np.unique(value))==set(range(507))


@pytest.mark.parametrize('filename',comparison.FILES)
def test_reference_rejects_changed_capture_bytes(tmp_path,filename):
    shutil.copytree(CAPTURE,tmp_path/'capture')
    target=tmp_path/'capture'/filename
    target.write_bytes(target.read_bytes()+b'!')
    with pytest.raises(ValueError,match='capture changed'):
        comparison.verify_capture(tmp_path/'capture')


@pytest.mark.parametrize('field',['generator_sha256','contract_sha256','fixture','upstream'])
def test_reference_rejects_changed_manifest_identity(tmp_path,field):
    shutil.copytree(CAPTURE,tmp_path/'capture')
    path=tmp_path/'capture/capture-manifest.json'
    value=json.loads(path.read_text())
    value[field]='unrecognized'
    path.write_text(json.dumps(value))
    with pytest.raises(ValueError,match='identity differs'):
        comparison.verify_capture(tmp_path/'capture')


@pytest.mark.parametrize('change',['short_canvas','long_canvas','wrong_count','wrong_ledger','collision'])
def test_reference_rejects_inconsistent_labels(tmp_path,change):
    shutil.copytree(CAPTURE,tmp_path/'capture')
    contract,reference=comparison.verify_capture(tmp_path/'capture')
    path=tmp_path/'capture/nucleus.u32le.gz'
    raw=gzip.decompress(path.read_bytes())
    if change=='short_canvas':path.write_bytes(gzip.compress(raw[:-4]))
    if change=='long_canvas':path.write_bytes(gzip.compress(raw+b'\0'*4))
    if change=='wrong_count':reference['count']+=1
    if change=='wrong_ledger':reference['sampled_pixels']['nucleus'][0]+=1
    if change=='collision':reference['collisions']['nucleus']=1
    with pytest.raises(ValueError,match='NDPI reference'):
        comparison.reference_labels(tmp_path/'capture',contract,reference)


def test_reference_crop_allocates_only_roi_with_context_offset():
    values=np.arange(35,dtype=np.uint32).reshape(5,7)
    mask=SimpleNamespace(level=0,downsample=1,level_bounds=SimpleNamespace(x=10000,y=20000),
                         width=7,height=5,labels_numpy=lambda:values)
    actual=comparison.local_labels(mask,[10002,20001,3,2])
    np.testing.assert_array_equal(actual,values[1:3,2:5])
    padded=comparison.local_labels(mask,[9999,19999,3,3])
    expected=np.zeros((3,3),dtype=np.uint32);expected[1:,1:]=values[:2,:2]
    np.testing.assert_array_equal(padded,expected)
    mask.level=1
    with pytest.raises(ValueError,match='natural level-zero'):
        comparison.local_labels(mask,[10002,20001,3,2])


@pytest.mark.skipif(not os.environ.get('QUPATHXX_HAMAMATSU_FIXTURE'),reason='opt-in pinned Hamamatsu NDPI fixture')
def test_real_slide_characterization_preserves_pixels_tiles_and_independent_features():
    report=comparison.compare(CAPTURE,Path(os.environ['QUPATHXX_HAMAMATSU_FIXTURE']))
    assert report['source_pixels_equal'] and report['source_pixel_disagreements']==0
    assert report['reference_count']==506
    assert not report['segmentation_parity_accepted'] and not report['biological_validation']
    assert not report['performance_accepted']
    assert {run['mode'] for run in report['runs']}=={'log','log-bodies'}
    for run in report['runs']:
        assert run['tile_invariant'] and run['independent_rgb_features_passed']
        assert len(run['results'])==3
        for result in run['results']:
            for metrics in result['segmentation'].values():
                assert metrics['reference_count']==506 and metrics['matches']
