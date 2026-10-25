import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qcl5_oracle',ROOT/'tools/reference_grid_cell_archive.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)


def test_independent_packet_metadata_and_native_consumer_are_reproducible():
    packet,metadata=oracle.reference();folder=ROOT/'tests/fixtures/grid-cell-archive-v1'
    manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
    for key in ('generator','contract','native_header'):
        assert hashlib.sha256((ROOT/manifest[key]).read_bytes()).hexdigest()==manifest[key+'_sha256']
    for name,proof in manifest['files'].items():
        raw=(folder/name).read_bytes();assert proof==dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest())
    assert packet==(folder/'synthetic-single-cell.qcl').read_bytes()
    assert metadata==json.loads((folder/'synthetic-single-cell.json').read_text(encoding='utf-8'))
    assert packet[:4]==b'QCL\5' and metadata['nucleus_area_um2']==.125
    assert (ROOT/'cpp/tests/grid_cell_archive_reference.hpp').read_bytes()==oracle.native_header(packet).encode()


def test_reference_is_synthetic_and_does_not_invent_real_input_capture():
    _,metadata=oracle.reference()
    assert metadata['input_sha256']=='a'*64 and metadata['units']=='micrometres'
    assert 'Synthetic' in metadata['scope'] and 'segmentation acceptance' in metadata['scope']


def test_generation_refuses_each_existing_output_before_writing_others(tmp_path):
    files=[tmp_path/name for name in ('a.qcl','b.json','c.hpp')]
    for existing in files:
        existing.write_bytes(b'keep')
        result=subprocess.run([sys.executable,str(ROOT/'tools/reference_grid_cell_archive.py'),'--output',str(files[0]),'--metadata',str(files[1]),'--native-header',str(files[2])],capture_output=True)
        assert result.returncode!=0 and existing.read_bytes()==b'keep'
        assert all(not path.exists() for path in files if path!=existing)
        existing.unlink()
