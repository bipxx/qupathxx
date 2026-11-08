"""Check the selected real-slide tissue partition and cancellation risks."""
import argparse
import hashlib
import json
from pathlib import Path
import time
import os

import numpy as np
import qupathxx as q


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--canonical', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    project = q.Project()
    image = project.add_image(str(args.image.resolve()))
    doc = project.document(image)
    meta = doc.metadata
    obj = doc.add_rectangle(q.Rect(0, 0, meta.width, meta.height), 'Whole slide tissue')
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
    results = []
    runs = []
    for tile in [256, 512]:
        parameters = q.TissueParameters()
        parameters.level = len(meta.levels) - 1
        parameters.tile_size = tile
        if args.canonical:
            parameters.canonical_pyramid = True
            parameters.work_bytes = 128 * 1024 * 1024
        request = q.TissueRequest(doc, obj, transform, parameters)
        started = time.monotonic()
        result = q.detect_tissue(request)
        labels = np.frombuffer(result.labels, dtype=np.uint32).reshape(result.height, result.width)
        results.append(labels.copy())
        runs.append(dict(tile_size=tile, level=result.level, width=result.width,
                         height=result.height, samples=result.samples,
                         foreground=result.foreground_pixels, components=result.component_count,
                         native_work_bytes=request.estimated_work_bytes,
                         algorithm=result.algorithm, input_format=result.input_format,
                         input_sha256=result.input_sha256,
                         labels_sha256=hashlib.sha256(result.labels).hexdigest(),
                         elapsed_seconds=time.monotonic()-started))
        print(json.dumps(runs[-1]), flush=True)
    difference = results[0] != results[1]
    mask_difference = (results[0] != 0) != (results[1] != 0)
    independent_difference = None
    if args.canonical:
        level = meta.levels[len(meta.levels) - 1]
        pixels = doc.server.read_region(q.RegionRequest(0, 0, len(meta.levels) - 1, level.width, level.height))
        rgba = pixels.numpy()
        table = np.maximum(0, np.log10(255 / np.maximum(np.arange(256, dtype=float), 1))).astype(np.float32)
        score = np.zeros(rgba.shape[:2], dtype=np.float64)
        for channel in range(3):
            score += table[rgba[..., channel]]
        expected = (rgba[..., 3] == 255) & (score >= .3)
        independent_difference = int(np.count_nonzero((results[0] != 0) != expected))
        del score, expected, rgba, pixels
    job = q.TissueJob()
    assert job.request(request)
    job.cancel()
    cancelled = job.wait(doc, obj)
    peak = None
    if os.name == 'nt':
        import ctypes
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [
                (name, ctypes.c_size_t) for name in ('PeakWorkingSetSize', 'WorkingSetSize',
                    'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage',
                    'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage')]
        info = Counters()
        info.cb = ctypes.sizeof(info)
        kernel, psapi = ctypes.WinDLL('kernel32'), ctypes.WinDLL('psapi')
        kernel.GetCurrentProcess.restype = wintypes.HANDLE
        psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(info), ctypes.sizeof(info)):
            raise OSError('cannot observe process working set')
        peak = info.PeakWorkingSetSize
    summary = dict(source=q.build_environment().git_revision, runs=runs,
                   label_differences=int(difference.sum()),
                   foreground_differences=int(mask_difference.sum()),
                   first_foreground_differences=np.argwhere(mask_difference)[:12].tolist(),
                   cancellation_state=cancelled.state,
                   cancelled_result_absent=job.result is None,
                   retained_records_after_cancel=doc.tissue_record_count)
    summary.update(canonical=args.canonical, independent_threshold_differences=independent_difference,
                   peak_working_set_bytes=peak,
                   memory_scope='observer process including Python oracle/decoder caches, not just native work allowance')
    (args.output/'probe.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2), flush=True)
    assert cancelled.state == 'cancelled' and job.result is None
    assert doc.tissue_record_count == 0
    assert not mask_difference.any(), 'Tile partition changes the tissue mask'
    assert not difference.any(), 'Tile partition changes component labels'
    if args.canonical:
        assert independent_difference == 0
        assert runs[0]['input_sha256'] == runs[1]['input_sha256']


if __name__ == '__main__':
    main()
