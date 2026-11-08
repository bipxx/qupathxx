"""Exact independent kernel expectations for the new grid cell recipe."""
import importlib.util
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('grid_cell_reference',ROOT/'tools/reference_grid_cells.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)


def test_frozen_exact_reference_and_native_consumer_are_reproducible():
    manifest=json.loads((ROOT/'tests/fixtures/grid-cells-v1/manifest.json').read_text(encoding='utf-8'))
    for key in ('generator','contract','native_header'):
        assert hashlib.sha256((ROOT/manifest[key]).read_bytes()).hexdigest()==manifest[key+'_sha256']
    raw=(ROOT/'tests/fixtures/grid-cells-v1/reference.json').read_bytes()
    assert manifest['files']['reference.json']==dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest())
    data=json.loads((ROOT/'tests/fixtures/grid-cells-v1/reference.json').read_text(encoding='utf-8'))
    assert data==oracle.reference()
    assert len(data['curvature'])==32 and len(data['expansion'])==128
    assert (ROOT/'cpp/tests/grid_cell_reference.hpp').read_bytes()==oracle.native_header(data).encode('utf-8')


def test_expansion_keeps_all_seeds_and_obeys_domain():
    for c in oracle.reference()['expansion']:
        assert all(not seed or label==seed for seed,label in zip(c['seeds'],c['labels']))
        assert all(not label or domain for label,domain in zip(c['labels'],c['domain']))


def test_exact_anisotropic_curvature_changes_sign():
    c=dict(width=3,height=3,score=[4,7,4,4,5,4,4,7,4],valid=[1]*9,step=['1','2'])
    assert oracle.curvature(c)[4]==1
    c['step']=['1','1'];assert oracle.curvature(c)[4]==0


def test_reference_capture_never_overwrites_either_existing_output(tmp_path):
    a=tmp_path/'a.json';b=tmp_path/'b.hpp';a.write_text('keep')
    result=subprocess.run([sys.executable,str(ROOT/'tools/reference_grid_cells.py'),'--output',str(a),'--native-header',str(b)],capture_output=True)
    assert result.returncode!=0 and a.read_text()=='keep' and not b.exists()
    a.unlink();b.write_text('keep')
    result=subprocess.run([sys.executable,str(ROOT/'tools/reference_grid_cells.py'),'--output',str(a),'--native-header',str(b)],capture_output=True)
    assert result.returncode!=0 and b.read_text()=='keep' and not a.exists()
