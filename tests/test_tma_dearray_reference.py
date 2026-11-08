"""Independent geometry is fixed before observing upstream detector behavior."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import pytest

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/tma-dearray-v1'
spec=importlib.util.spec_from_file_location('tma_dearray_reference_assessment',ROOT/'tools/assess_tma_dearray_reference.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)


def data():
    return (json.loads((FIXTURE/'contract.json').read_bytes()),json.loads((FIXTURE/'qupath-reference.json').read_bytes()))


def test_captured_geometry_does_not_relax_preregistered_tolerance():
    report=audit.load_and_assess(FIXTURE);cases={c['name']:c for c in report['cases']}
    assert report['center_tolerance_pixels']==2 and report['within_preregistered_contract']==4 and report['full_grid_cases']==5
    assert cases['rotated_missing']['max_center_error_pixels']==2
    assert cases['skewed_missing']['max_center_error_pixels']>2
    assert not cases['skewed_missing']['eligible_for_automatic_acceptance']
    assert (cases['unobserved_row']['rows'],cases['unobserved_row']['columns'])==(2,4)
    assert (cases['blank']['rows'],cases['blank']['columns'])==(1,1)
    assert all(cases[n]['requires_review_or_refusal'] for n in ['skewed_missing','unobserved_row','blank'])
    assert report['native_detector_implemented'] is report['biological_accepted'] is report['release_ready'] is False


@pytest.mark.parametrize('change',['missing','duplicate','renamed','version','revision','mask','count','nan_center','nan_density','boolean_dimensions'])
def test_malformed_or_mixed_reference_evidence_rejected(change):
    contract,reference=data();case=reference['cases'][0]
    if change=='missing':reference['cases'].pop()
    elif change=='duplicate':reference['cases'].append(copy.deepcopy(case))
    elif change=='renamed':case['name']='unknown'
    elif change=='version':reference['version']='other'
    elif change=='revision':reference['upstream_revision']='f'*40
    elif change=='mask':case['mask_sha256']='f'*64
    elif change=='count':case['centers'].pop()
    elif change=='nan_center':case['centers'][0][0]=float('nan')
    elif change=='nan_density':case['densities'][0]=float('nan')
    else:case['rows']=True
    with pytest.raises(ValueError):audit.assess(contract,reference)


@pytest.mark.parametrize('change',['center','density'])
def test_valid_but_wrong_geometry_or_missing_flags_cannot_be_accepted(change):
    contract,reference=data();case=reference['cases'][1]
    if change=='center':case['centers'][0][0]+=10
    else:case['densities'][5]=1
    result=next(c for c in audit.assess(contract,reference)['cases'] if c['name']==case['name'])
    assert not result['eligible_for_automatic_acceptance'] and result['requires_review_or_refusal']


@pytest.mark.parametrize('change',['contract','mask','capture_failure','boolean_exit','runtime'])
def test_changed_preregistered_inputs_or_capture_identity_rejected(tmp_path,change):
    root=tmp_path/'fixture';shutil.copytree(FIXTURE,root)
    if change=='contract':
        path=root/'contract.json';value=json.loads(path.read_bytes());value['acceptance']['full_grid']['max_center_error_pixels']=3;path.write_text(json.dumps(value),encoding='utf-8')
    elif change=='mask':
        path=root/'aligned.pgm';raw=bytearray(path.read_bytes());raw[-1]^=1;path.write_bytes(raw)
    else:
        path=root/'capture-command.json';value=json.loads(path.read_bytes())
        if change=='capture_failure':value['exit']=1
        elif change=='boolean_exit':value['exit']=False
        else:value['runtime_contract']['revision']='f'*40
        path.write_text(json.dumps(value),encoding='utf-8')
    with pytest.raises(ValueError):audit.load_and_assess(root)
