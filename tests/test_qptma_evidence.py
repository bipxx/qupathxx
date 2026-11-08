"""Artifact cohort gates reject omitted, duplicate and nonpassing evidence."""
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET
import pytest

spec=importlib.util.spec_from_file_location('qpx_qptma_test_audit',Path(__file__).resolve().parents[1]/'tools/ci/qptma_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_case():
    case=ET.Element('testcase',name='qptma')
    ET.SubElement(case,'system-out').text=evidence.MARKER+'\n'
    return case


def python_cases():
    return [ET.Element('testcase',classname='tests.'+classname,name=name)
            for classname,names in evidence.CASES.items() for name in names]


def test_complete_native_and_installed_qptma_cohorts():
    assert evidence.native([native_case()])['bounded_grid_cores']==10000
    assert evidence.python(python_cases())['passed']==sum(map(len,evidence.CASES.values()))


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
    monkeypatch.setattr(evidence.subprocess, 'run', lambda *a, **kw: SimpleNamespace(returncode=0, stdout=b'1111111111111111111111111111111111111111 blob 4\na\nb\n\n'))
    if accepted:
        result = evidence.source_contract(tmp_path, 'a'*40)['source.txt']
        assert len(result['git_blob_sha256'])==64 and len(result['checked_out_sha256'])==64
    else:
        with pytest.raises(ValueError, match='source differs'): evidence.source_contract(tmp_path, 'a'*40)


def test_captured_fixture_bytes_are_never_line_normalized(tmp_path, monkeypatch):
    from types import SimpleNamespace
    relative='tests/fixtures/captured.qptma';path=tmp_path/relative;path.parent.mkdir(parents=True)
    path.write_bytes(b'a\r\nb\r\n')
    monkeypatch.setattr(evidence,'SOURCES',[relative])
    monkeypatch.setattr(evidence.subprocess,'run',lambda *a,**kw:SimpleNamespace(returncode=0,stdout=b'1111111111111111111111111111111111111111 blob 4\na\nb\n\n'))
    with pytest.raises(ValueError,match='source differs'):evidence.source_contract(tmp_path,'a'*40)


@pytest.mark.parametrize('enabled',[True,False])
def test_project_native_capability_is_explicit(enabled):
    case=ET.Element('testcase',name='qptma_project')
    ET.SubElement(case,'system-out').text=evidence.PROJECT_MARKER if enabled else evidence.PROJECT_DISABLED
    assert evidence.project_native([case],enabled)['project_storage_enabled'] is enabled


@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','disabled_branch','duplicate_marker'])
def test_project_native_coverage_cannot_be_replaced_by_disabled_or_failed_test(failure):
    case=ET.Element('testcase',name='qptma_project');out=ET.SubElement(case,'system-out');out.text=evidence.PROJECT_MARKER
    cases=[case]
    if failure=='missing':cases=[]
    elif failure=='duplicate':cases.append(case)
    elif failure in ['failure','error','skipped']:ET.SubElement(case,failure)
    elif failure=='disabled_branch':out.text=evidence.PROJECT_DISABLED
    else:out.text=(evidence.PROJECT_MARKER+'\n')*2
    with pytest.raises(ValueError):evidence.project_native(cases)


@pytest.mark.parametrize('output', [b'', b'not-found missing\n', b'abc tree 4\na\nb\n\n',
    b'abc blob 4\na\n', b'abc blob 4\na\nb\nx', b'abc blob 4\na\nb\n\ntrailing'])
def test_source_batch_rejects_missing_wrong_kind_truncation_and_trailing(tmp_path, monkeypatch, output):
    from types import SimpleNamespace
    (tmp_path/'source.txt').write_bytes(b'a\nb\n')
    monkeypatch.setattr(evidence.subprocess,'run',lambda *a,**kw:SimpleNamespace(returncode=0,stdout=output))
    with pytest.raises(ValueError):evidence.source_contract(tmp_path,'a'*40,['source.txt'])


def test_source_batch_reads_multiple_blobs_in_one_process(tmp_path, monkeypatch):
    from types import SimpleNamespace
    (tmp_path/'a.txt').write_bytes(b'a\r\n');(tmp_path/'b.txt').write_bytes(b'')
    calls=[]
    def run(command,**kwargs):
        calls.append((command,kwargs))
        return SimpleNamespace(returncode=0,stdout=b'abc blob 2\na\n\ndef blob 0\n\n')
    monkeypatch.setattr(evidence.subprocess,'run',run)
    result=evidence.source_contract(tmp_path,'a'*40,['a.txt','b.txt'])
    assert list(result)==['a.txt','b.txt'] and len(calls)==1
    assert calls[0][0][-2:]==['cat-file','--batch']
    assert calls[0][1]['input']==(('a'*40+':a.txt\n')+('a'*40+':b.txt\n')).encode()


def test_named_cohort_manifest_matches_independent_pytest_collection():
    # Fixtures built from CASES cannot expose a stale CASES list. Collect the
    # actual source independently, including parameter IDs, before qualification.
    import os
    import subprocess
    import sys
    root = Path(__file__).resolve().parents[1]
    contract = evidence.CASES
    env = os.environ.copy()
    env.pop('PYTEST_ADDOPTS', None)
    env['PYTEST_DISABLE_PLUGIN_AUTOLOAD'] = '1'
    env['PYTHONUTF8'] = '1'
    env['PYTHONIOENCODING'] = 'utf-8'
    command = [sys.executable, '-I', '-m', 'pytest',
               *[str(root/'tests'/(module+'.py')) for module in contract],
               '--collect-only', '-q', '--noconftest', '--color=no', '-o', 'addopts=']
    result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                            text=True, encoding='utf-8', timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    actual = {module: [] for module in contract}
    for line in result.stdout.splitlines():
        if '::' not in line:
            continue
        filename, name = line.split('::', 1)
        module = Path(filename).stem
        assert module in actual, line
        actual[module].append(name)
    for module, names in actual.items():
        assert names and len(names) == len(set(names)), module
        assert sorted(names) == contract[module], module
