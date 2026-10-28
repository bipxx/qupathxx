"""Native golden vectors derive from captured bytes, not the new C++ kernel."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_rgb_nominal_reference', ROOT / 'tools/reference_rgb_nominal.py')
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)


def test_golden_vectors_are_byte_reproducible_without_interpolation(tmp_path):
    generated = tmp_path / 'vectors'
    manifest = reference.generate(generated)
    assert manifest['cases'] == 99 and not manifest['interpolation_performed']
    assert len(manifest['files']) == 12
    expected = {p.relative_to(reference.FIXTURE).as_posix(): p.read_bytes()
                for p in reference.FIXTURE.rglob('*') if p.is_file()}
    actual = {p.relative_to(generated).as_posix(): p.read_bytes() for p in generated.rglob('*') if p.is_file()}
    assert actual == expected


def test_holdout_has_all_63_unique_requests_and_separate_runtime_identity():
    value = reference.verify_holdout()
    assert len(value['cases']) == 63
    assert len({(v['case_name'], v['request_name']) for v in value['cases']}) == 63
    assert value['qupath_version'] == '0.7.0' and value['java_version'] == '25.0.2'


@pytest.mark.parametrize('name', ['contract.json', 'reference.json', 'gradient/input.png',
                                'checker/crop_1_1.png', 'one/full_0_6.png'])
def test_changed_holdout_payload_is_rejected(tmp_path, name):
    folder = tmp_path / 'capture'
    shutil.copytree(reference.HOLDOUT, folder)
    with (folder / name).open('ab') as stream:
        stream.write(b'!')
    with pytest.raises(ValueError, match='holdout bytes changed'):
        reference.verify_holdout(folder)


@pytest.mark.parametrize('change', ['schema', 'runtime', 'generator', 'inventory'])
def test_mixed_holdout_identity_is_rejected(tmp_path, change):
    folder = tmp_path / 'capture'
    shutil.copytree(reference.HOLDOUT, folder)
    path = folder / 'manifest.json'
    value = json.loads(path.read_text())
    if change == 'schema': value['schema'] = 'other'
    elif change == 'runtime': value['upstream']['revision'] = '0' * 40
    elif change == 'generator': value['generator_sha256'] = '0' * 64
    else: value['files'].pop('reference.json')
    path.write_text(json.dumps(value))
    with pytest.raises(ValueError, match='holdout identity differs'):
        reference.verify_holdout(folder)


def test_frozen_native_input_and_expected_output_hashes_are_bound():
    manifest = json.loads((reference.FIXTURE / 'manifest.json').read_text())
    for name, identity in manifest['files'].items():
        path = reference.FIXTURE / name
        assert path.stat().st_size == identity['bytes']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == identity['sha256']
    lines = (reference.FIXTURE / 'index.txt').read_text().splitlines()
    assert lines[0] == 'qpx.rgb-nominal-golden/1' and len(lines) == 100
    assert len({line.split()[0] for line in lines[1:]}) == 99
