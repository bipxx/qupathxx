"""The upstream comparator must reject plausible incomplete migrations."""
import copy
import importlib.util
import json
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qpx_hierarchy_compare', ROOT / 'tools/compare_qupath_hierarchy.py')
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
EXPECTED = json.loads((ROOT / 'tests/fixtures/qupath-hierarchy-0.7.0/reference.json').read_text(encoding='utf-8'))['created']


def test_complete_hierarchy_and_points_pass():
    assert comparison.compare(EXPECTED, EXPECTED, 1e-6)['parent_links'] == 4


@pytest.mark.parametrize('mutation', ['missing', 'duplicate', 'flat', 'wrong_parent', 'nucleus', 'class', 'kind', 'points', 'nonfinite'])
def test_incomplete_upstream_results_reject(mutation):
    actual = copy.deepcopy(EXPECTED)
    if mutation == 'missing':
        actual.pop()
    elif mutation == 'duplicate':
        actual[-1] = actual[0]
    elif mutation == 'flat':
        for obj in actual:
            obj.pop('parent', None)
    elif mutation == 'wrong_parent':
        actual[2]['parent'] = actual[0]['id']
    elif mutation == 'nucleus':
        actual[2]['nucleus_bounds'][0] += 1
    elif mutation == 'class':
        actual[2]['class_color'] = 0
    elif mutation == 'kind':
        actual[2]['kind'] = 'annotation'
    elif mutation == 'points':
        actual[-1]['point_coordinates'].insert(1, [217, 28])
    else:
        actual[0]['area'] = float('nan')
    with pytest.raises(ValueError):
        comparison.compare(EXPECTED, actual, 1e-6)
