"""Reject incomplete admission evidence, premature decode and poisoned recovery."""
import copy
import importlib.util
import json
from pathlib import Path

import pytest

spec=importlib.util.spec_from_file_location('large_tma_admission',Path(__file__).resolve().parents[1]/'tools/check_tma_large_pyramid_admission.py')
admission=importlib.util.module_from_spec(spec);spec.loader.exec_module(admission)
C=json.loads(admission.probe.CONTRACT.read_text(encoding='utf-8'))


def valid():
    messages=['source pixel limit','output pixel limit','working payload budget','256 MiB output limit']
    cases=[]
    for kind,message in zip(admission.KINDS,messages,strict=True):
        cases.append(dict(kind=kind,exception_type='ValueError',exception_message=message,
            after_refusal=dict(decoded_blocks=0,decoded_bytes=0,cache_bytes=0,peak_cache_bytes=0),
            closed_after_refusal=False,admitted_work_bytes=2787328,
            request=dict(source_pixel_limit=168960-(kind=='source_pixel'),
                         output_pixel_limit=168960-(kind=='output_pixel'),work_bytes=2787328-(kind=='work_byte')),
            recovery=dict(estimated_work_bytes=2787328,decoded_blocks=4,decoded_bytes=786432,
                          input_sha256='retained',geometry_and_flags_match=True)))
    return dict(cases=cases,baseline_input_sha256='retained',memory=dict(peak_working_set_bytes=29048832))


def test_complete_admission_protocol():
    assert admission.assess(valid(),C,'retained')==[]


@pytest.mark.parametrize('index',range(4))
@pytest.mark.parametrize('path,value',[
    (('exception_type',),''),(('exception_type',),'MemoryError'),
    (('exception_message',),'unrelated error'),
    (('after_refusal','decoded_blocks'),1),(('after_refusal','decoded_bytes'),1),
    (('after_refusal','cache_bytes'),1),(('after_refusal','peak_cache_bytes'),1),
    (('closed_after_refusal',),True),(('admitted_work_bytes',),3145729),
    (('request','source_pixel_limit'),1),(('request','output_pixel_limit'),1),
    (('request','work_bytes'),1),
    (('recovery','estimated_work_bytes'),2787327),(('recovery','decoded_blocks'),5),
    (('recovery','decoded_bytes'),786433),(('recovery','input_sha256'),'changed'),
    (('recovery','geometry_and_flags_match'),False),
])
def test_each_refusal_and_recovery_rejects_wrong_evidence(index,path,value):
    data=copy.deepcopy(valid());target=data['cases'][index]
    for key in path[:-1]: target=target[key]
    target[path[-1]]=value
    assert admission.assess(data,C,'retained')


@pytest.mark.parametrize('mode',['missing','duplicate','reorder','baseline','memory','zero_memory'])
def test_cohort_identity_and_process_budget(mode):
    data=valid()
    if mode=='missing': data['cases'].pop()
    elif mode=='duplicate': data['cases'].append(data['cases'][0])
    elif mode=='reorder': data['cases'].reverse()
    elif mode=='baseline': data['baseline_input_sha256']='changed'
    elif mode=='memory': data['memory']['peak_working_set_bytes']=536870913
    else: data['memory']['peak_working_set_bytes']=0
    assert admission.assess(data,C,'retained')
