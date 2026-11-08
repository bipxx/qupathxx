"""Compare stable buckets and the original expansion heap in fresh native processes."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import statistics
import struct
import subprocess
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_bytes((json.dumps(value, indent=2, allow_nan=False)+'\n').encode())


def main():
    import numpy as np
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--baseline-executable',type=Path,help='Compare an older native build instead of forcing the current heap')
    parser.add_argument('--radius',type=int,default=5,choices=range(9))
    parser.add_argument('--output-directory',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--repetitions',type=int,default=5)
    args=parser.parse_args()
    if not __debug__ or not 1<=args.repetitions<=10:
        parser.error('require assertions enabled and 1..10 repetitions')
    executable=args.executable.resolve()
    environment=json.loads(subprocess.check_output([str(executable),'--environment']))
    if '+dirty' in environment['git_revision']:
        raise ValueError('profile requires committed native source')
    baseline=args.baseline_executable.resolve() if args.baseline_executable else None
    baseline_environment=None
    if baseline:
        if sha(baseline)==sha(executable):parser.error('baseline and candidate native hashes must differ')
        baseline_environment=json.loads(subprocess.check_output([str(baseline),'--environment']))
        if '+dirty' in baseline_environment['git_revision']:raise ValueError('baseline source must be committed')
    out=args.output_directory.resolve()
    out.mkdir(parents=True,exist_ok=False)
    spec=importlib.util.spec_from_file_location('qpx_peak_profile',Path(__file__).with_name('profile_sampled_log.py'))
    helper=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    records=[]
    for size in [256,512,1024]:
        yy,xx=np.mgrid[:size,:size]
        disk=(xx%64+.5-32)**2+(yy%64+.5-32)**2<=81
        labels=np.where(disk,yy//64*(size//64)+xx//64+1,0).astype('<u4')
        domain=np.ones((size,size),dtype=np.uint8)
        source=out/f'{size}.qbe'
        source.write_bytes(b'QBE1'+struct.pack('<III',size,size,args.radius)+labels.tobytes()+domain.tobytes())
        expected=labels.copy()
        for dy in range(1-args.radius,args.radius):
            for dx in range(1-args.radius,args.radius):
                if dx*dx+dy*dy>=args.radius**2:
                    continue
                sy,sx=slice(max(0,-dy),min(size,size-dy)),slice(max(0,-dx),min(size,size-dx))
                ty,tx=slice(max(0,dy),min(size,size+dy)),slice(max(0,dx),min(size,size+dx))
                np.maximum(expected[ty,tx],labels[sy,sx],out=expected[ty,tx])
        identity=None
        for trial in range(args.repetitions+1):
            modes=([('baseline',baseline,[]),('candidate',executable,[])] if baseline else
                   [('heap',executable,['--heap']),('buckets',executable,[])])
            if trial%2:
                modes.reverse()
            for mode,program,flags in modes:
                output=out/f'{size}-{mode}-{trial}.qbr'
                began=time.perf_counter()
                child=subprocess.Popen([str(program),'--timed']+flags+[str(source),str(output)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                try:
                    stdout,stderr=child.communicate(timeout=120)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.communicate()
                    raise
                elapsed=(time.perf_counter()-began)*1000
                peak=helper.peak_working_set(child)
                if child.returncode:
                    output.with_suffix('.stderr').write_bytes(stderr)
                    raise RuntimeError('expansion probe failed: '+str(output))
                timing=json.loads(stdout)
                assert 0<timing['kernel_ms']<=elapsed
                if args.radius>1:assert timing['queue']==('heap' if mode=='heap' else 'buckets')
                else:assert timing['queued_pixels']==timing['peak_queue']==0
                raw=output.read_bytes()
                assert raw[:4]==b'QBR1' and struct.unpack_from('<II',raw,4)==(size,size) and len(raw)==12+size*size*4
                actual=np.frombuffer(raw,dtype='<u4',offset=12).reshape(size,size)
                np.testing.assert_array_equal(actual,expected)
                digest=sha(output)
                assert identity is None or digest==identity
                identity=digest
                record=dict(size=size,radius=args.radius,mode=mode,trial=trial,warmup=trial==0,native_sha256=sha(program),
                    process_wall_ms=elapsed,process_peak_working_set_bytes=peak,
                    native_kernel_ms=timing['kernel_ms'],queued_pixels=timing['queued_pixels'],peak_queue=timing['peak_queue'],
                    input_sha256=sha(source),output_sha256=digest,independent_strict_expansion_exact=True)
                records.append(record)
                write(output.with_suffix('.json'),record)
                print(json.dumps(record),flush=True)
                if trial:
                    assert output.resolve().is_relative_to(out)
                    output.unlink()
    summaries=[]
    for size in [256,512,1024]:
        for mode in (['baseline','candidate'] if baseline else ['heap','buckets']):
            rows=[r for r in records if r['size']==size and r['mode']==mode and not r['warmup']]
            summaries.append(dict(size=size,mode=mode,observations=len(rows),
                median_native_kernel_ms=statistics.median(r['native_kernel_ms'] for r in rows),
                min_native_kernel_ms=min(r['native_kernel_ms'] for r in rows),
                max_native_kernel_ms=max(r['native_kernel_ms'] for r in rows),
                median_process_wall_ms=statistics.median(r['process_wall_ms'] for r in rows),
                min_process_wall_ms=min(r['process_wall_ms'] for r in rows),
                max_process_wall_ms=max(r['process_wall_ms'] for r in rows),
                max_process_peak_working_set_bytes=max(r['process_peak_working_set_bytes'] for r in rows) if os.name=='nt' else None))
    args.report.parent.mkdir(parents=True,exist_ok=True)
    write(args.report,dict(schema='qpx.expansion-frontier-profile/2',environment=environment,
        baseline_environment=baseline_environment,baseline_native_sha256=sha(baseline) if baseline else None,radius=args.radius,
        native_sha256=sha(executable),tool_sha256=sha(Path(__file__)),platform=platform.platform(),
        peak_helper_sha256=sha(Path(__file__).with_name('profile_sampled_log.py')),records=records,summaries=summaries,
        performance_accepted=False,scope='Uncontrolled fresh native children alternate mode/build order; one retained warmup per mode/size is excluded from summaries. All output labels match independent strict-radius dilation (with the defined unchanged-seed zero-radius case) and both queues/builds. Native kernel timing excludes protocol I/O. Process wall includes startup and binary protocol I/O. Peak memory is the child process peak. One representative output per size/mode remains; every raw timing and digest is retained. No end-to-end Python/archive/GUI/hardware acceptance.'))


if __name__=='__main__':
    main()
