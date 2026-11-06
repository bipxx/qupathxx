"""One dense count/GIL observation; timings do not establish speed superiority."""
import argparse
import json
import os
from pathlib import Path
import threading
import time

import qupathxx as q


def peak_working_set():
    if os.name != 'nt':
        return None
    import ctypes
    from ctypes import wintypes
    class Counters(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [
            (name, ctypes.c_size_t) for name in (
                'PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage',
                'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage',
                'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage')]
    info = Counters()
    info.cb = ctypes.sizeof(info)
    kernel, psapi = ctypes.WinDLL('kernel32'), ctypes.WinDLL('psapi')
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
    if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(info), ctypes.sizeof(info)):
        raise OSError('cannot observe process working set')
    return info.PeakWorkingSetSize


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    count = 20000
    doc = q.Document(q.Image(64, 64, [80] * 64 * 64 * 3))
    body = q.Roi.rectangle(q.Rect(31, 31, 2, 2))
    ids = doc.execute([q.ObjectEdit.create_geometry(body, kind=q.ObjectKind.CELL) for _ in range(count)])
    options = q.SpatialNeighborhoodOptions()
    options.radius, options.max_distance_tests = 1, 1 << 40
    request = q.SpatialNeighborhoodRequest(doc, ids, ids, options)
    ready, stop = threading.Event(), threading.Event()
    ticks = [0]

    def observer():
        ready.set()
        while not stop.wait(.001):
            ticks[0] += 1

    thread = threading.Thread(target=observer)
    thread.start()
    ready.wait()
    job = q.SpatialNeighborhoodJob()
    try:
        assert job.request(request)
        before = ticks[0]
        started = time.perf_counter()
        state = job.wait(doc)
        seconds = time.perf_counter() - started
        released_ticks = ticks[0] - before
    finally:
        stop.set()
        thread.join()
    result = job.result
    assert state.state == 'ready' and result is not None
    assert result.row_count == count
    assert all(row.count == count - 1 for start in range(0, count, 4096) for row in result.rows(start, 4096))
    assert result.distance_tests == count * (count - 1)
    assert released_ticks >= 5, 'Python observer did not progress while native wait was blocked'
    summary = dict(source_revision=q.build_environment().git_revision,
                   source_tree_sha256=q.build_environment().source_tree_sha256,
                   cohort_cells=count, expected_count_per_cell=count - 1,
                   distance_tests=result.distance_tests, snapshot_bytes=request.snapshot_bytes,
                   estimated_native_work_bytes=request.required_work_bytes,
                   retained_result_payload_bytes=result.payload_bytes,
                   process_peak_working_set_bytes=peak_working_set(),
                   memory_scope='whole observer process including Python, scene/edit history and libraries; not a native work allowance',
                   python_ticks_during_native_wait=released_ticks,
                   observed_wait_seconds=seconds, capture_ms=request.capture_ms,
                   input_sha256=result.input_sha256, result_sha256=result.sha256,
                   scope='Worst-case coincident centroid workload, no pruning benefit; one local observation, no speed or general RSS cap claim')
    (args.output / 'resources.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
