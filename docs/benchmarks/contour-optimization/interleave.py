"""Alternate old/new native workers; never run timed traces concurrently."""
import json
from pathlib import Path
import statistics
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
BUILD=ROOT.parents[1]
work=Path(sys.argv[1]).resolve()
stem=sys.argv[2] if len(sys.argv)>2 else 'interleaved'
mode=sys.argv[3] if len(sys.argv)>3 else 'trace'
fixture=BUILD/'native-detector-optimization/candidate-final/11/study.qpxproj'
bootstrap=ROOT/'docs/benchmarks/cell-kernel-optimization/run-native.py'
runtime=BUILD/'ci/gridfrontends167-package/Release'
worker=Path(__file__).with_name('worker.py')
children={};errors={};rows=[];identities={}
try:
    for name,binary in [('baseline',work/'baseline.pyd'),('candidate',runtime/'_native.cp312-win_amd64.pyd')]:
        errors[name]=(work/(name+'-'+stem+'.stderr.log')).open('w',encoding='utf-8')
        children[name]=subprocess.Popen([sys.executable,'-I',str(bootstrap),str(binary),str(runtime),str(worker),str(fixture),mode],
                                       stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=errors[name],text=True,encoding='utf-8')
        ready=children[name].stdout.readline()
        if not ready:raise RuntimeError(name+' exited before ready')
        identities[name]=json.loads(ready)
    for iteration in range(6 if mode=='detect' else 12):
        row=dict(iteration=iteration)
        order=['baseline','candidate'] if iteration%2==0 else ['candidate','baseline']
        row['order']=order
        for name in order:
            child=children[name];child.stdin.write('trace\n');child.stdin.flush()
            line=child.stdout.readline()
            if not line:raise RuntimeError(name+' exited during trace')
            row[name]=json.loads(line)
        for mask in ('nucleus','body'):
            for field in ('geometry','edges','vertices','work_bytes','retained_bytes'):
                assert row['baseline'][mask][field]==row['candidate'][mask][field],(iteration,mask,field)
        rows.append(row)
        (work/(stem+'-progress.json')).write_text(json.dumps(rows,indent=2),encoding='utf-8')
    summary={}
    for mask in (('detector',) if mode=='detect' else ('nucleus','body')):
        old=[row['baseline']['detector_ms'] if mode=='detect' else row['baseline'][mask]['wall_ms'] for row in rows]
        new=[row['candidate']['detector_ms'] if mode=='detect' else row['candidate'][mask]['wall_ms'] for row in rows]
        summary[mask]=dict(baseline_median_ms=statistics.median(old),candidate_median_ms=statistics.median(new),
                           median_paired_saving_ms=statistics.median(a-b for a,b in zip(old,new)),
                           reduction_fraction=1-statistics.median(new)/statistics.median(old),
                           baseline_samples=old,candidate_samples=new)
    report=dict(identities=identities,warmups_per_worker=4,measured_pairs=len(rows),mode=mode,concurrent_timing=False,
                exact_geometry_counts_work_and_retained=True,summary=summary,runs=rows)
    (work/(stem+'.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary,indent=2))
finally:
    for child in children.values():
        if child.poll() is None:
            try:child.stdin.write('stop\n');child.stdin.flush();child.communicate(timeout=10)
            except (BrokenPipeError,subprocess.TimeoutExpired):child.kill();child.communicate()
    for error in errors.values():error.close()
