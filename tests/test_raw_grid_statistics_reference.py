"""Derived sample statistics are pinned independently of native reductions."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('raw_grid_stats_reference',ROOT/'tools/reference_raw_grid_statistics.py')
reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)

def test_fraction_statistics_and_native_table_regenerate():
    data=reference.reference()
    assert json.loads((ROOT/'tests/fixtures/raw-grid-statistics-v1/reference.json').read_text(encoding='utf-8'))==data
    assert (ROOT/'cpp/tests/raw_grid_statistics_reference.hpp').read_bytes()==reference.native_header(data).encode('utf-8')
    assert len(data['cases'])==188
    assert {c['source_case'] for c in data['cases']}==set(range(94))

def test_support_and_nonfinite_channels_are_distinct():
    data=reference.reference();inputs=reference.reference_raw_sampling.reference()['cases']
    cases={inputs[c['source_case']]['name']:c for c in data['cases'] if not c['roi']}
    assert cases['f64-nan-payload']['valid_pixels']==1
    assert [s['nonfinite'] for s in cases['f64-nan-payload']['channels']]==[0,1,0]
    invalid=cases['f64-invalid-before-nan'];assert invalid['pixels']==1 and invalid['valid_pixels']==0
    assert all(s['finite']==s['nonfinite']==0 and s['mean'] is None for s in invalid['channels'])
    assert cases['u64-integer-pair']['channels'][0]['mean']=='4340000000000001'

def test_generation_refuses_existing_outputs(tmp_path):
    output=tmp_path/'data.json';header=tmp_path/'reference.hpp'
    cmd=[sys.executable,'-I',str(ROOT/'tools/reference_raw_grid_statistics.py'),'--output',str(output),'--native-header',str(header)]
    result=subprocess.run(cmd,capture_output=True);assert result.returncode==0,result.stderr
    saved=output.read_bytes(),header.read_bytes()
    result=subprocess.run(cmd,capture_output=True);assert result.returncode==2
    assert (output.read_bytes(),header.read_bytes())==saved
