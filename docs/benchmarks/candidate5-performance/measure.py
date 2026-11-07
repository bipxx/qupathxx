"""Run one fixed worker and observe its actual engine PID's Windows high-water RSS."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import subprocess
import time


class Counters(c.Structure):
    _fields_=[('cb', w.DWORD), ('faults', w.DWORD)]+[(name,c.c_size_t) for name in
        ['peak_rss','rss','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile']]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('output',type=Path)
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    command=args.command[1:] if args.command[:1]==['--'] else args.command
    kernel=c.WinDLL('kernel32',use_last_error=True)
    psapi=c.WinDLL('psapi',use_last_error=True)
    kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
    kernel.CloseHandle.argtypes=[w.HANDLE]
    psapi.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Counters),w.DWORD]
    psapi.GetProcessMemoryInfo.restype=w.BOOL
    started=time.perf_counter();handle=None;peak=0;private=0;samples=0;pid=None
    with Path(str(args.output)+'.log').open('w',encoding='utf-8') as log:
        process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
        try:
            while process.poll() is None:
                if time.perf_counter()-started>240:
                    process.terminate()
                    raise TimeoutError('Worker exceeded240s; retained log and outputs')
                path=args.output/'pid.txt'
                if handle is None and path.exists():
                    text=path.read_text().strip()
                    if text:
                        pid=int(text);handle=kernel.OpenProcess(0x410,False,pid)
                        if not handle:raise c.WinError(c.get_last_error())
                if handle:
                    values=Counters();values.cb=c.sizeof(values)
                    if psapi.GetProcessMemoryInfo(handle,c.byref(values),values.cb):
                        peak=max(peak,values.peak_rss);private=max(private,values.peak_pagefile);samples+=1
                time.sleep(.025)
            elapsed=time.perf_counter()-started
            resource=dict(engine_pid=pid,launcher_pid=process.pid,process_wall_ms=elapsed*1000,
                peak_working_set_bytes=peak,peak_pagefile_bytes=private,memory_samples=samples,
                exit_code=process.returncode,memory_scope=('One full workflow plus engine startup, no mask oracles' if '--resource-only' in command or 'resource' in command else 'Actual engine process lifetime, including startup/warmup/oracles')+'; OS peak counter sampled25ms')
            Path(str(args.output)+'-resources.json').write_text(json.dumps(resource,indent=2),encoding='utf-8')
            if process.returncode:raise RuntimeError('Worker failed; inspect '+str(args.output)+'.log')
            assert samples and peak
            print(json.dumps(resource))
        finally:
            if handle:kernel.CloseHandle(handle)


if __name__=='__main__':main()
