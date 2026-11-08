"""Compare fast label metrics with the independent small-map oracle."""
import importlib.util
from pathlib import Path

import numpy as np
import pytest

ROOT=Path(__file__).resolve().parents[1]


def load(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/file)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


fast=load('qpx_fast_labels','label_comparison.py')
oracle=load('qpx_oracle_labels','compare_qupath_cells.py')
TOLERANCE=dict(match_min_iou=.1,relation_min_pixels=2,relation_min_fraction=.1)


@pytest.mark.parametrize('seed',range(12))
def test_contingency_metrics_match_direct_pixel_pair_oracle(seed):
    rng=np.random.default_rng(seed)
    reference=rng.integers(0,7,(17,23),dtype=np.uint32)
    native=rng.integers(0,6,(17,23),dtype=np.uint32)
    if seed==0:reference[:]=0
    if seed==1:native[:]=0
    if seed==2:reference[:]=0;native[:]=0
    if seed==3:native=reference.copy()
    if seed==4:native=np.where(reference>0,reference*1001,0)
    assert fast.metrics(reference,native,TOLERANCE)==oracle.metrics(reference,native,TOLERANCE)


def test_matching_rejects_unbounded_dense_pair_scratch():
    labels=np.arange(100,dtype=np.uint32).reshape(10,10)
    with pytest.raises(ValueError,match='budget'):
        fast.metrics(labels,labels,TOLERANCE,overlap_budget=1)


@pytest.mark.parametrize('bad',[np.zeros((3,4),dtype=float),np.full((3,4),-1,dtype=np.int32),np.zeros((4,3),dtype=np.uint32)])
def test_matching_rejects_invalid_label_canvases(bad):
    with pytest.raises(ValueError,match='label canvases'):
        fast.metrics(np.zeros((3,4),dtype=np.uint32),bad,TOLERANCE)
