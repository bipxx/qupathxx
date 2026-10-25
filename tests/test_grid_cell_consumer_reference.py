import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qca5_oracle',ROOT/'tools/reference_grid_classifier_archive.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)


def test_independent_qca5_and_native_header_are_byte_reproducible():
    folder=ROOT/'tests/fixtures/grid-cell-consumers-v1';packet,metadata=oracle.reference()
    assert packet==(folder/'synthetic-classifier.qcr').read_bytes()
    assert metadata==json.loads((folder/'synthetic-classifier.json').read_text(encoding='utf-8'))
    assert metadata['features']==11 and metadata['source_steps']==[.5,.25]
    assert (ROOT/'cpp/tests/grid_classifier_archive_reference.hpp').read_bytes()==oracle.native_header(packet).encode()
    manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
    for key in ('generator','source_generator','contract','native_header'):
        assert hashlib.sha256((ROOT/manifest[key]).read_bytes()).hexdigest()==manifest[key+'_sha256']
    for name,proof in manifest['files'].items():
        raw=(folder/name).read_bytes();assert proof==dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest())


def test_synthetic_classifier_reference_does_not_claim_training_acceptance():
    _,metadata=oracle.reference()
    assert 'hand-authored forest' in metadata['scope'] and 'not research/model acceptance' in metadata['scope']


def test_capture_rejects_each_existing_output_before_creating_others(tmp_path):
    files=[tmp_path/name for name in ('a.qcr','b.json','c.hpp')]
    for existing in files:
        existing.write_bytes(b'keep')
        result=subprocess.run([sys.executable,str(ROOT/'tools/reference_grid_classifier_archive.py'),'--output',str(files[0]),'--metadata',str(files[1]),'--native-header',str(files[2])],capture_output=True)
        assert result.returncode!=0 and existing.read_bytes()==b'keep'
        assert all(not path.exists() for path in files if path!=existing);existing.unlink()
