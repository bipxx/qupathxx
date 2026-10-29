"""Pinned ImageJ stages localize the clipped-ROI numerical difference."""
import importlib.util
import json
from pathlib import Path
import shutil

import numpy as np
import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gaussian_reference',ROOT/'tools/qupath_gaussian.py')
reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
NAMES=[case['name'] for case in reference.read(reference.FIXTURE/'cases.json')]


@pytest.fixture(scope='module')
def report():return reference.verify()


@pytest.mark.parametrize('name',NAMES)
def test_captured_gaussian_blur_reconstructs_exact_log(name,report):
    case=next(c for c in report['cases'] if c['name']==name)
    assert case['laplacian_from_blur_exact']
    assert report['original_roi_log_exact']
    assert not report['native_recipe_changed']
    assert not report['segmentation_parity_accepted'] and not report['biological_validation']


@pytest.mark.parametrize('values,expected',[
    ([[.75]],[[0.]]),
    ([[1.,2.,3.]],[[-1.,0.,1.]]),
    ([[1.,2.],[3.,4.]],[[-3.,-1.],[1.,3.]]),
])
def test_laplacian_has_independent_hand_computed_edges(values,expected):
    np.testing.assert_array_equal(reference.laplacian(np.asarray(values,dtype='<f4')),np.asarray(expected,dtype='<f4'))


@pytest.mark.parametrize('name',['cases.json','probe.groovy','runtime.json','driver.log',
    'roi-sigma1-input.f32le','roi-sigma1-blur.f32le','roi-sigma1-log.f32le','roi-sigma1-kernel.json'])
def test_gaussian_capture_rejects_modified_payload(tmp_path,name):
    target=tmp_path/'capture';shutil.copytree(reference.FIXTURE,target)
    path=target/name;path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises(ValueError,match='capture bytes changed'):reference.verify(target)


@pytest.mark.parametrize('change',['generator','upstream','inventory','extra-file'])
def test_gaussian_capture_rejects_changed_identity(tmp_path,change):
    target=tmp_path/'capture';shutil.copytree(reference.FIXTURE,target)
    path=target/'manifest.json';manifest=reference.read(path)
    if change=='generator':manifest['generator_sha256']='0'*64
    elif change=='upstream':manifest['upstream']['revision']='unknown'
    elif change=='inventory':manifest['files'].pop('driver.log')
    else:(target/'unexpected.f32le').write_bytes(b'')
    path.write_text(json.dumps(manifest),encoding='utf-8')
    with pytest.raises(ValueError,match='capture identity differs'):reference.verify(target)


@pytest.mark.parametrize('change',['recipe','script','runtime','input','nonfinite-blur','size','kernel','log'])
def test_gaussian_capture_rejects_resealed_semantic_changes(tmp_path,change):
    target=tmp_path/'capture';shutil.copytree(reference.FIXTURE,target)
    if change=='recipe':
        name='cases.json';data=reference.read(target/name);data[0]['sigma']=2.;(target/name).write_text(json.dumps(data))
    elif change=='script':name='probe.groovy';(target/name).write_bytes(b'// changed script\n')
    elif change=='runtime':name='runtime.json';(target/name).write_text('{}')
    elif change=='kernel':name='roi-sigma1-kernel.json';(target/name).write_text('[[1.0]]')
    else:
        kind='input' if change=='input' else 'blur' if change in ('nonfinite-blur','size') else 'log'
        name='roi-sigma1-'+kind+'.f32le';values=np.frombuffer((target/name).read_bytes(),dtype='<f4').copy()
        if change=='size':values=values[:-1]
        elif change=='nonfinite-blur':values[0]=np.nan
        else:values[0]+=1.
        (target/name).write_bytes(values.tobytes())
    path=target/'manifest.json';manifest=reference.read(path)
    manifest['files'][name]=dict(bytes=(target/name).stat().st_size,sha256=reference.sha(target/name))
    path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError):reference.verify(target)
