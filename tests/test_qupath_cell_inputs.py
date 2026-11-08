"""Actual public OD/stain captures and reusable native input arithmetic."""
import gzip
import importlib.util
import json
from pathlib import Path
import shutil

import numpy as np
import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('cell_input_reference',ROOT/'tools/qupath_cell_inputs.py')
reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
NAMES=[c['name'] for c in reference.read(reference.FIXTURE/'cases.json')]


@pytest.fixture(scope='module')
def report():return reference.compare_native()


@pytest.mark.parametrize('name',NAMES)
def test_existing_native_input_channels_match_capture_bits(name,report):
    case=next(c for c in report['native_comparisons'] if c['name']==name)
    assert case['exact_arrays']==7 and case['normalized_stain_arrays']==3
    assert report['original_plugin_inputs_exact'] and not report['native_math_changed']
    assert not report['biological_validation'] and not report['segmentation_parity_accepted']


@pytest.mark.parametrize('name',NAMES)
def test_od_sum_records_float32_rounding_and_preserves_signed_stains(name,report):
    case=next(c for c in report['cases'] if c['name']==name)
    assert case['float32_sum_exact'] and case['od_arrays']==3
    if name=='white-2':assert case['sum_once_in_float64_changed_pixels']==0
    else:assert case['sum_once_in_float64_changed_pixels']>0
    if name=='signed-residual':assert case['signed_stain_samples']>0


@pytest.mark.parametrize('name',['cases.json','probe.groovy','runtime.json','driver.log','pairwise.rgb',
    'white-0-model.json','white-0-od0.f32le.gz','white-0-sum.f32le.gz','white-0-stain0.f32le.gz'])
def test_cell_input_capture_rejects_changed_payload(tmp_path,name):
    folder=tmp_path/'capture';shutil.copytree(reference.FIXTURE,folder)
    path=folder/name;path.write_bytes(path.read_bytes()+b'!')
    with pytest.raises(ValueError,match='capture bytes changed'):reference.verify(folder)


@pytest.mark.parametrize('change',['upstream','generator','inventory','extra'])
def test_cell_input_capture_rejects_changed_identity(tmp_path,change):
    folder=tmp_path/'capture';shutil.copytree(reference.FIXTURE,folder)
    path=folder/'manifest.json';manifest=reference.read(path)
    if change=='upstream':manifest['upstream']['revision']='unknown'
    elif change=='generator':manifest['generator_sha256']='0'*64
    elif change=='inventory':manifest['files'].pop('driver.log')
    else:(folder/'extra').write_bytes(b'')
    path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError,match='capture identity differs'):reference.verify(folder)


@pytest.mark.parametrize('change',['recipe','script','runtime','rgb','matrix','model-authority','nonfinite','size','sum','stain'])
def test_cell_input_capture_rejects_resealed_semantic_changes(tmp_path,change):
    folder=tmp_path/'capture';shutil.copytree(reference.FIXTURE,folder)
    if change=='recipe':
        name='cases.json';cases=reference.read(folder/name);cases[0]['white'][0]=240;(folder/name).write_text(json.dumps(cases))
    elif change=='script':name='probe.groovy';(folder/name).write_bytes(b'// changed\n')
    elif change=='runtime':name='runtime.json';(folder/name).write_text('{}')
    elif change=='rgb':name='pairwise.rgb';(folder/name).write_bytes(b'changed')
    elif change in ('matrix','model-authority'):
        name='white-0-model.json';model=reference.read(folder/name)
        if change=='matrix':model['inverse']=[[0]*3]*3
        else:model['normalized']=model['inverse']=[[1,0,0],[0,1,0],[0,0,1]]
        (folder/name).write_text(json.dumps(model))
    else:
        name='white-0-'+('sum' if change=='sum' else 'stain0' if change=='stain' else 'od0')+'.f32le.gz'
        values=np.frombuffer(gzip.decompress((folder/name).read_bytes()),dtype='<f4').copy()
        if change=='nonfinite':values[0]=np.nan
        elif change=='size':values=values[:-1]
        else:values[0]+=1.
        (folder/name).write_bytes(gzip.compress(values.tobytes(),mtime=0))
    path=folder/'manifest.json';manifest=reference.read(path)
    manifest['files'][name]=dict(bytes=(folder/name).stat().st_size,sha256=reference.sha(folder/name));path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError):reference.compare_native(folder)
