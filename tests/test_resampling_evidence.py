"""Artifact cohort gates reject omitted, duplicate and nonpassing evidence."""
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET
import pytest

spec=importlib.util.spec_from_file_location('qpx_resampling_test_audit',Path(__file__).resolve().parents[1]/'tools/ci/resampling_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_case():
    case=ET.Element('testcase',name='resampling')
    ET.SubElement(case,'system-out').text=evidence.MARKER+'\n'
    return case


def python_cases():
    return [ET.Element('testcase',classname='tests.test_resampling',name=name) for name in evidence.PYTHON_NAMES]


def test_complete_native_and_installed_resampling_cohorts():
    assert evidence.native([native_case()])['independent_oracle_shapes']==2205
    assert evidence.python(python_cases())['passed']==13


@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','marker','duplicate_marker'])
def test_native_incomplete_or_false_evidence_rejected(failure):
    case=native_case();cases=[case]
    if failure=='missing':cases=[]
    elif failure=='duplicate':cases.append(native_case())
    elif failure in ['failure','error','skipped']:ET.SubElement(case,failure)
    elif failure=='marker':case.find('system-out').text='other'
    else:case.find('system-out').text=(evidence.MARKER+'\n')*2
    with pytest.raises(ValueError):evidence.native(cases)


@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','name','class'])
def test_installed_incomplete_or_false_evidence_rejected(failure):
    cases=python_cases()
    if failure=='missing':cases.pop()
    elif failure=='duplicate':cases.append(cases[0])
    elif failure in ['failure','error','skipped']:ET.SubElement(cases[0],failure)
    elif failure=='name':cases[0].set('name','other')
    else:cases[0].set('classname','other')
    with pytest.raises(ValueError):evidence.python(cases)


@pytest.mark.parametrize('data,accepted', [(b'a\nb\n', True), (b'a\r\nb\r\n', True), (b'a\nchanged\n', False)])
def test_source_contract_only_normalizes_checkout_line_endings(tmp_path, monkeypatch, data, accepted):
    from types import SimpleNamespace
    (tmp_path/'source.txt').write_bytes(data)
    monkeypatch.setattr(evidence, 'SOURCES', ['source.txt'])
    monkeypatch.setattr(evidence.common.subprocess, 'run', lambda *a, **kw: SimpleNamespace(returncode=0, stdout=b'1111111111111111111111111111111111111111 blob 4\na\nb\n\n'))
    if accepted:
        result = evidence.source_contract(tmp_path, 'a'*40)['source.txt']
        assert result['crlf_normalized'] == (b'\r' in data)
    else:
        with pytest.raises(ValueError, match='source differs'): evidence.source_contract(tmp_path, 'a'*40)
