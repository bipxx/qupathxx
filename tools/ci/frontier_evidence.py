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


def unchanged_real_masks(candidate, baseline, padding):
    def masks(report):
        runs=report['runs']
        require(len(runs)==3 and {r['mode'] for r in runs}=={'log','log-bodies','log-sampled'},
                'real-slide recipe set differs')
        output={}
        for run in runs:
            require(run['context_padding']==padding and
                    sorted(r['tile_size'] for r in run['results'])==[16,64,256],
                    'real-slide context/tile set differs')
            for result in run['results']:
                require(set(result['labels_sha256'])=={'nucleus','body'},'real-slide compartments differ')
                output[run['mode'],result['tile_size']]=result['labels_sha256']
        return output
    require(masks(candidate)==masks(baseline),'expansion optimization changed a frozen real-slide label map')
    return dict(context_padding=padding,recipes=3,tile_sizes=3,unchanged_compartment_maps=18)
