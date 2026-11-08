"""Public float32 response oracle, independent native vectors and resealed faults."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil

import numpy as np
import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('response_reference',ROOT/'tools/reference_qupath_response.py')
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
NAMES=[case['name'] for case in tool.cases()]


@pytest.fixture(scope='module')
def reference():return tool.verify()


@pytest.mark.parametrize('name',NAMES)
def test_public_response_inputs_labels_and_boundary_semantics(reference,name):
    assert reference['cases']==33 and reference['public_boundary_semantics_verified']
    case=next(case for case in tool.cases() if case['name']==name)
    labels=np.frombuffer((tool.FIXTURE/(name+'-labels.u32le')).read_bytes(),dtype='<u4')
    assert labels.size==case['width']*case['height']
    if name in ('single','row','column','flat','border-plateau','below-minimum'):assert not np.any(labels)


def test_native_response_vectors_come_only_from_verified_public_capture():
    assert tool.HEADER.read_bytes()==tool.header()


@pytest.mark.parametrize('fault',['input','label_id','seed_changed','runtime'])
def test_resealed_response_semantic_faults_are_rejected(tmp_path,fault):
    folder=tmp_path/'capture';shutil.copytree(tool.FIXTURE,folder)
    name='interior-plateau'
    if fault=='input':
        path=folder/(name+'-input.f32le');a=np.frombuffer(path.read_bytes(),dtype='<f4').copy();a[0]=np.float32(.123);path.write_bytes(a.tobytes())
    elif fault in ('label_id','seed_changed'):
        path=folder/(name+'-labels.u32le');a=np.frombuffer(path.read_bytes(),dtype='<u4').copy()
        if fault=='label_id':a[0]=1000000
        else:a[np.flatnonzero(a)[0]]=0
        path.write_bytes(a.tobytes())
    else:
        path=folder/'runtime.json';value=tool.read(path);value['cases'][0]['outside_is_nan']=False
        path.write_text(json.dumps(value)+'\n',encoding='utf-8')
    manifest=tool.read(folder/'manifest.json');manifest['files'][path.name]=dict(bytes=path.stat().st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (folder/'manifest.json').write_text(json.dumps(manifest)+'\n',encoding='utf-8')
    with pytest.raises(ValueError):tool.verify(folder)
