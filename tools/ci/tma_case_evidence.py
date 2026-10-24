"""Require exact-source TMA case aggregation correctness cohorts."""
import hashlib
import json
from pathlib import Path
import subprocess


def require(value, message):
    if not value:
        raise ValueError(message)


def native(cases, file_export=False):
    selected = [t for t in cases if t.get('name') == 'tma']
    markers = [
        'TMA case aggregation, weighted arithmetic, missing/unassigned cores, stale revisions and cancellation passed',
        'TMA compensated signed sums and nonzero mean underflow checks passed',
    ]
    require(len(selected) == 1 and all(marker in selected[0].findtext('system-out', '') for marker in markers),
            'missing native TMA case aggregation or numerical regression cohort')
    require(all(selected[0].find(k) is None for k in ['failure', 'error', 'skipped']),
            'native TMA case aggregation failed or skipped')
    decimals = [t for t in cases if t.get('name') == 'decimal_quantities']
    require(len(decimals) == 1 and
            'signed decimal fields: grammar, representable subnormals and locale independence passed' in decimals[0].findtext('system-out', '') and
            all(decimals[0].find(k) is None for k in ['failure', 'error', 'skipped']),
            'missing portable signed-decimal correctness cohort')
    if file_export:
        require(all(marker in selected[0].findtext('system-out', '') for marker in [
            'TMA case file cancellation precedes output path access',
            'TMA case file exports preserve captured CSV bytes and existing files']),
            'missing native atomic case-file export cohort')
    return dict(case_aggregation_checked=True, compensated_signed_sums_checked=True,
                mean_underflow_checked=True, cancellation_checked=True, signed_decimal_locale_checked=True, atomic_file_export_checked=file_export)


def python(cases, root, commit, gui=False, file_export=False):
    relative = 'tests/fixtures/tma-case-summary-workflows.json'
    path = Path(root)/relative
    committed = subprocess.run(['git', '-C', str(root), 'show', commit+':'+relative], capture_output=True)
    require(committed.returncode == 0 and committed.stdout == path.read_bytes(),
            'TMA case workflow contract differs from dispatched source')
    contract = json.loads(path.read_text(encoding='utf8'))
    require(contract['schema'] == 'qpx.tma-case-cohort/1', 'invalid TMA case cohort schema')
    groups = [('test_tma_case_summary', 'cases', 13)]
    if file_export:
        groups.append(('test_tma_case_export', 'file_cases', 27))
    if gui:
        groups.append(('test_gui_tma', 'gui_cases', 4))
    result = dict(cohort_contract_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    for classname, field, count in groups:
        expected = contract[field]
        require(len(expected) == len(set(expected)) == count and expected == sorted(expected),
                'invalid TMA case workflow contract')
        selected = [t for t in cases if t.get('classname', '').rsplit('.', 1)[-1] == classname and
                    (field != 'gui_cases' or t.get('name', '').startswith('test_case_summary_export['))]
        require(sorted(t.get('name') for t in selected) == expected,
                'TMA case workflow cases differ')
        require(all(t.find(k) is None for t in selected for k in ['failure', 'error', 'skipped']),
                'TMA case workflow failed or skipped')
        result[field+'_passed'] = count
    return result
