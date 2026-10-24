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
