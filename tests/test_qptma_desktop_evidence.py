"""Desktop qualification cannot omit or skip the real QPTMA workflows."""
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET
import pytest
spec=importlib.util.spec_from_file_location('qpx_desktop_evidence_controls',Path(__file__).resolve().parents[1]/'tools/ci/qptma_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)

def cases():return [ET.Element('testcase',name=name,classname='tests.test_gui_qptma') for name in evidence.DESKTOP_CASES]

def test_complete_desktop_cohort_accepted():assert evidence.desktop(cases())['passed']==8

@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','renamed'])
def test_incomplete_or_nonpassing_desktop_cohort_rejected(change):
    values=cases()
    if change=='missing':values.pop()
    elif change=='duplicate':values.append(values[0])
    elif change=='renamed':values[0].set('name','other')
    else:ET.SubElement(values[0],change)
    with pytest.raises(ValueError):evidence.desktop(values)
