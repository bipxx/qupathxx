"""Shared correctness cohort checks for the small-radius expansion candidate."""
import hashlib
import json
from pathlib import Path
import subprocess


def require(value, message):
    if not value:
        raise ValueError(message)


def native(cases):
    selected=[t for t in cases if t.get('name')=='cell_expansion']
    marker='Small frontier 64 exact priority keys and 1344 full heap-equivalence cases passed'
    require(len(selected)==1 and marker in selected[0].findtext('system-out',''),
            'missing small-frontier native equivalence cohort')
    require(all(selected[0].find(k) is None for k in ['failure','error','skipped']),
            'small-frontier native case failed or skipped')
    return dict(priority_keys=64,heap_equivalence_cases=1344)


def python(cases, root, commit):
    relative='tests/fixtures/imagej-shape-1.54p/expansion-frontier-workflows.json'
    path=Path(root)/relative
    git=subprocess.run(['git','-C',str(root),'show',commit+':'+relative],capture_output=True,check=False)
    require(git.returncode==0 and git.stdout==path.read_bytes(),
            'frontier workflow contract differs from dispatched source')
    expected=json.loads(path.read_text())
    require(expected['schema']=='qpx.expansion-frontier-cohort/1' and
            len(expected['cases'])==len(set(expected['cases']))==8,'invalid frontier workflow contract')
    selected=[t for t in cases if t.get('classname','').rsplit('.',1)[-1]==expected['classname']]
    require(sorted(t.get('name') for t in selected)==expected['cases'],'frontier workflow cases differ')
    require(all(t.find(k) is None for t in selected for k in ['failure','error','skipped']),
            'frontier workflow case failed or skipped')
    return dict(passed=8,cohort_contract_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
