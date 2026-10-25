"""Catch incomplete qualification requests before spending time on CI builders."""
import importlib.util
from pathlib import Path
import pytest

spec=importlib.util.spec_from_file_location('qpx_campaign_inputs',Path(__file__).resolve().parents[1]/'tools/ci/campaign_inputs.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)


@pytest.mark.parametrize('role',['engine','macos','portable'])
def test_full_profile_serializes_bool_values_for_gh(role):
    semantic=module.full_inputs(role)
    wire=module.gh_inputs(role,{k:v for k,v in semantic.items() if v})
    assert all(type(v) is str and v in ('true','false') for v in wire.values())
    assert {k:v=='true' for k,v in wire.items()}==semantic
    semantic['macos_native']=not semantic['macos_native']
    assert module.full_inputs(role)['macos_native']!=semantic['macos_native']


@pytest.mark.parametrize('role,field',[('engine','extended_readers'),('engine','live_reference'),('portable','extended_readers'),('portable','portable_linux'),('macos','macos_native')])
def test_missing_required_path_rejects(role,field):
    values=module.full_inputs(role);values.pop(field)
    with pytest.raises(ValueError,match='incomplete qualification'):module.gh_inputs(role,values)


@pytest.mark.parametrize('value',['true',1,None])
def test_wire_values_cannot_masquerade_as_semantic_bools(value):
    values=module.full_inputs('portable');values['extended_readers']=value
    with pytest.raises(ValueError,match='bool values'):module.gh_inputs('portable',values)


@pytest.mark.parametrize('role,field',[('engine','portable_linux'),('portable','macos_native'),('macos','portable_linux')])
def test_conflicting_gate_mode_rejects(role,field):
    values=module.full_inputs(role);values[field]=True
    with pytest.raises(ValueError,match='incomplete qualification'):module.gh_inputs(role,values)


def test_unknown_inputs_and_role_reject():
    values=module.full_inputs('engine');values['extended_reader']=True
    with pytest.raises(ValueError):module.gh_inputs('engine',values)
    with pytest.raises(ValueError):module.full_inputs('typo')
