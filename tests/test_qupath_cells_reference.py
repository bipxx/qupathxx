"""Pinned upstream cell characterization, including explicit unresolved gates."""
import importlib.util
from pathlib import Path
import shutil

import numpy as np
import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_cell_comparison', ROOT/'tools/compare_qupath_cells.py')
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
CAPTURE = comparison.FIXTURES/'capture'
CONTRACT = comparison.read(comparison.FIXTURES/'contract.json')
GEOMETRY = pytest.mark.skipif(not q.has_geos or not q.has_interchange, reason='GEOS/GeoJSON cell reference')


def test_cell_capture_identity():
    comparison.verify_capture(CAPTURE)
    manifest = comparison.read(CAPTURE/'capture-manifest.json')
    assert len(manifest['files']) == 64
    assert manifest['upstream']['runtime_files'] == 496
    assert manifest['upstream']['revision'] == '04ccfa4fb7d43e9b566393e08e83690b72248d44'
    for case in CONTRACT['cases']:
        reference = comparison.read(CAPTURE/case['name']/'reference.json')
        assert reference['qupath_version'] == '0.7.0'
        assert reference['java_version'] == '25.0.2'
        assert reference['collisions'] == reference['decoded_collisions'] == dict(nucleus=0,body=0)


@pytest.mark.parametrize('name', ['blank/input.png','isolated_three/nucleus.u32le.gz',
                                 'resampled_reference/reference.json','contract.json'])
def test_cell_capture_mutation_rejected(tmp_path, name):
    target = tmp_path/'capture'
    shutil.copytree(CAPTURE,target)
    with (target/name).open('ab') as stream:
        stream.write(b' ')
    with pytest.raises(ValueError,match='capture changed'):
        comparison.verify_capture(target)


@pytest.mark.parametrize('mode', ['empty','permuted','disjoint','split','merge'])
def test_overlap_assignment_and_many_to_many_relations(mode):
    a = np.array([[1,1,0,2,2],[1,1,0,2,2]],dtype=np.uint32)
    if mode == 'empty':
        a[:] = 0; b = a.copy()
    elif mode == 'permuted':
        b = np.where(a==1,8,np.where(a==2,3,0))
    elif mode == 'disjoint':
        a = np.array([[1,0]],dtype=np.uint32); b = np.array([[0,2]],dtype=np.uint32)
    elif mode == 'split':
        a = np.ones((2,4),dtype=np.uint32); b = np.array([[4,4,9,9],[4,4,9,9]])
    else:
        a = np.array([[4,4,9,9],[4,4,9,9]]); b = np.ones((2,4),dtype=np.uint32)
    result = comparison.metrics(a,b,CONTRACT['comparison'])
    if mode in ['empty','permuted']:
        assert result['foreground_iou'] == result['foreground_dice'] == 1
        assert result['unmatched_reference'] == result['unmatched_native'] == 0
        assert all(m['iou']==1 and m['centroid_error_px']==0 for m in result['matches'])
    elif mode == 'disjoint':
        assert result['foreground_iou'] == 0 and not result['matches']
        assert result['unmatched_reference'] == result['unmatched_native'] == 1
    elif mode == 'split':
        assert result['split_reference_labels'] == [1] and not result['merged_native_labels']
        assert result['unmatched_native'] == 1
    else:
        assert result['merged_native_labels'] == [1] and not result['split_reference_labels']
        assert result['unmatched_reference'] == 1


@pytest.fixture(scope='module')
def characterized():
    if not q.has_geos or not q.has_interchange:
        pytest.skip('GEOS/GeoJSON cell reference')
    comparison.verify_capture(CAPTURE)
    return {case['name']:comparison.compare_case(CAPTURE,CONTRACT,case) for case in CONTRACT['cases']}


@GEOMETRY
@pytest.mark.parametrize('name', [case['name'] for case in CONTRACT['cases']])
def test_cell_characterization_preserves_tile_and_measurement_evidence(characterized, name):
    result = characterized[name]
    assert result['tile_invariant']
    assert [t['tile_size'] for t in result['timings']] == [16,64,256]
    assert result['serialization_changed_label_pixels'] == dict(nucleus=0,body=0)
    assert result['reference_collisions'] == dict(nucleus=0,body=0)
    if name != 'resampled_reference':
        assert all(region['passed'] for region in result['shared_roi_regions'])
    else:
        regions = result['shared_roi_regions']
        assert not result['grid_equivalent']
        assert [r['mask_mismatches'] for r in regions] == [12,10,14,16,16,30]
        assert all(not r['passed'] and r['mismatches_only_on_polygon_boundary'] for r in regions)
        assert all(r['native_only_centers']==r['mask_mismatches'] and r['reference_only_centers']==0 for r in regions)


@GEOMETRY
@pytest.mark.xfail(strict=True,reason='Frozen isolated-cell QuPath acceptance gates remain unresolved')
@pytest.mark.parametrize('gate',['baseline_count','isolated_nucleus_iou','isolated_centroid'])
def test_isolated_cell_upstream_acceptance_gate(characterized, gate):
    assert characterized['isolated_three']['characterization_gates'][gate]


@GEOMETRY
@pytest.mark.xfail(strict=True,reason='Resampled polygon boundary memberships differ from QuPath')
def test_resampled_cell_shared_roi_acceptance_gate(characterized):
    assert all(r['passed'] for r in characterized['resampled_reference']['shared_roi_regions'])
