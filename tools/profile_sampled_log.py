"""Interleave fixed fresh-process LoG kernel observations; retain every timing."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import struct
import subprocess
import time


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def peak_working_set(process):
    if os.name!='nt':return None
    class Counters(ctypes.Structure):
        _fields_=[('cb',wintypes.DWORD),('PageFaultCount',wintypes.DWORD)]+[(name,ctypes.c_size_t) for name in
            ['PeakWorkingSetSize','WorkingSetSize','QuotaPeakPagedPoolUsage','QuotaPagedPoolUsage',
             'QuotaPeakNonPagedPoolUsage','QuotaNonPagedPoolUsage','PagefileUsage','PeakPagefileUsage']]
    query=ctypes.WinDLL('psapi',use_last_error=True).GetProcessMemoryInfo
    query.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD];query.restype=wintypes.BOOL
    counters=Counters();counters.cb=ctypes.sizeof(counters)
    if not query(wintypes.HANDLE(int(process._handle)),ctypes.byref(counters),counters.cb):raise ctypes.WinError(ctypes.get_last_error())
    if counters.PeakWorkingSetSize<=0:raise ValueError('native process peak working set unavailable')
    return counters.PeakWorkingSetSize


def main():
    import numpy as np
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output-directory',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--repetitions',type=int,default=5)
    args=parser.parse_args()
    if not 3<=args.repetitions<=20:parser.error('use 3 to 20 measured observations per size/mode')
    out=args.output_directory.resolve();out.mkdir(parents=True,exist_ok=False)
    executable=args.executable.resolve();environment=json.loads(subprocess.check_output([str(executable),'--environment']))
    if '+dirty' in environment['git_revision']:raise ValueError('compiled kernel source must be committed')
    records=[];summaries=[]
    for size in [256,512,1024]:
        yy,xx=np.mgrid[:size,:size];radius=(xx%64+.5-32)**2+(yy%64+.5-32)**2
        feature=np.where(radius<=9**2,1.5,.05).astype('<f4');domain=np.ones((size,size),dtype=np.uint8)
        input=out/(str(size)+'.qlp');input.write_bytes(b'QLP1'+struct.pack('<II',size,size)+feature.tobytes()+domain.tobytes())
        expected_count=(size//64)**2;identity={}
        for trial in range(args.repetitions+1):
            modes=[('legacy',[]),('sampled_shape',['--sampled-shape'])]
            if trial%2:modes.reverse()
            for mode,option in modes:
                output=out/f'{size}-{mode}-{trial}.qlr';began=time.perf_counter()
                child=subprocess.Popen([str(executable)]+option+[str(input),str(output)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                stdout,stderr=child.communicate(timeout=120);elapsed=(time.perf_counter()-began)*1000
                peak=peak_working_set(child)
                if child.returncode:raise RuntimeError(stderr.decode(errors='replace'))
                raw=output.read_bytes()
                if raw[:4]!=b'QLR1' or len(raw)!=16+size*size*4 or struct.unpack_from('<III',raw,4)!=(size,size,expected_count):
                    raise ValueError('profile output dimensions/count differ')
                labels=np.frombuffer(raw,'<u4',offset=16)
                if set(np.unique(labels))!=set(range(expected_count+1)):raise ValueError('profile labels are not dense')
                digest=sha(output)
                if mode in identity and identity[mode]!=digest:raise ValueError('profile outputs changed between trials')
                identity[mode]=digest
                record=dict(size=size,mode=mode,trial=trial,warmup=trial==0,process_wall_ms=elapsed,
                    process_peak_working_set_bytes=peak,input_sha256=sha(input),output_sha256=digest,cells=expected_count)
                records.append(record);print(json.dumps(record),flush=True)
                (out/f'{size}-{mode}-{trial}.json').write_bytes((json.dumps(record,indent=2)+'\n').encode())
                # Raw observations remain; identical protocol bytes need one representative.
                if trial:
                    if not output.resolve().is_relative_to(out):raise ValueError('temporary output escaped profile directory')
                    output.unlink()
        if len(set(identity.values()))!=1:raise ValueError('isolated-disk masks differ across kernel modes')
        for mode in identity:
            runs=[r for r in records if r['size']==size and r['mode']==mode and not r['warmup']]
            summaries.append(dict(size=size,mode=mode,cells=expected_count,observations=len(runs),
                median_process_wall_ms=statistics.median(r['process_wall_ms'] for r in runs),
                min_process_wall_ms=min(r['process_wall_ms'] for r in runs),max_process_wall_ms=max(r['process_wall_ms'] for r in runs),
                max_process_peak_working_set_bytes=max(r['process_peak_working_set_bytes'] for r in runs) if os.name=='nt' else None))
    report=dict(schema='qpx.sampled-log-kernel-profile/1',native_environment=environment,native_sha256=sha(executable),
        tool_sha256=sha(Path(__file__)),host=dict(platform=platform.platform(),python=platform.python_version()),
        records=records,summaries=summaries,cross_mode_masks_exact=True,performance_accepted=False,biological_validation=False,
        scope='Uncontrolled host. Fresh native children alternate order, with one retained/excluded warmup per size/mode. Wall time includes startup and protocol file I/O, excludes fixture generation and independent output checking. Windows peak working set is queried on the completed child handle. No Python-module, feature aggregation, archive, GUI or hardware-target acceptance.')
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())


if __name__=='__main__':main()
