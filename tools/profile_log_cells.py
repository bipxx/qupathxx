"""Isolated-process production LoG scaling, memory and tile-invariance observations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time


def peak_rss():
    if os.name == 'nt':
        import ctypes as c
        from ctypes import wintypes as w

        class Counters(c.Structure):
            _fields_ = [('cb', w.DWORD), ('PageFaultCount', w.DWORD)] + [
                (name, c.c_size_t) for name in ['PeakWorkingSetSize', 'WorkingSetSize',
                'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage',
                'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage']]
        kernel = c.WinDLL('kernel32', use_last_error=True)
        psapi = c.WinDLL('psapi', use_last_error=True)
        kernel.GetCurrentProcess.restype = w.HANDLE
        psapi.GetProcessMemoryInfo.argtypes = [w.HANDLE, c.POINTER(Counters), w.DWORD]
        counters = Counters(); counters.cb = c.sizeof(counters)
        if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), c.byref(counters), counters.cb):
            raise c.WinError(c.get_last_error())
        return counters.PeakWorkingSetSize
    import resource
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == 'darwin' else 1024)


def child(args):
    import numpy as np
    import qupathxx as q
    from qupathxx import _native

    size = args.child_size
    yy, xx = np.indices((size, size))
    inside = (xx % 64 - 32)**2 + (yy % 64 - 32)**2 <= 100
    rgb = np.where(inside[..., None], [30, 40, 60], [255, 255, 255]).astype(np.uint8)
    doc = q.Document(q.Image.from_numpy(rgb))
    obj = doc.add_rectangle(q.Rect(0, 0, size, size))
    p = q.CellParameters(); p.algorithm = q.CellAlgorithm.LOG_WATERSHED
    p.tile_size = args.tile; p.threshold = .4; p.max_nucleus_pixels = 1024
    p.cell_expansion = 5
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    setup_peak = peak_rss()
    start = time.perf_counter_ns()
    request = q.CellRequest(doc, obj, transform, p)
    capture_ms = (time.perf_counter_ns() - start) / 1e6
    start = time.perf_counter_ns(); result = q.detect_cells(request)
    wall_ms = (time.perf_counter_ns() - start) / 1e6
    pipeline_peak = peak_rss()
    assert result.cell_count == (size // 64)**2
    assert result.retained_bytes <= request.estimated_work_bytes <= p.work_bytes
    assert q.input_sha256(request) == result.nuclei.input_sha256
    # Independently check that each expected disk yields one nucleus and that
    # every reported compartment count and RGB mean follows the returned mask.
    nuclei, bodies = result.nuclei.labels_numpy(), result.cells.labels_numpy()
    centers = set(zip(*np.nonzero(inside & (xx % 64 == 32) & (yy % 64 == 32))))
    assert len(centers) == result.cell_count
    assert len({int(nuclei[y, x]) for y, x in centers}) == result.cell_count and all(nuclei[y, x] for y, x in centers)
    features = []
    for f in result.features:
        n, b = nuclei == f.label, bodies == f.label
        assert f.nucleus_pixels == int(n.sum()) and f.cell_pixels == int(b.sum())
        np.testing.assert_allclose(f.nucleus_rgb, rgb[n].mean(axis=0), atol=1e-12)
        np.testing.assert_allclose(f.cell_rgb, rgb[b].mean(axis=0), atol=1e-12)
        features.append([f.label, f.nucleus_pixels, f.cell_pixels, list(f.nucleus_rgb), list(f.cell_rgb)])
    hashes = dict(nuclei=hashlib.sha256(nuclei.astype('<u4', copy=False).tobytes()).hexdigest(),
                  bodies=hashlib.sha256(bodies.astype('<u4', copy=False).tobytes()).hexdigest(),
                  features=hashlib.sha256(json.dumps(features, separators=(',', ':')).encode()).hexdigest())
    environment = q.build_environment()
    output = dict(size=size, tile_size=args.tile, cells=result.cell_count, capture_ms=capture_ms,
                  detection_wall_ms=wall_ms, worker_ms=result.elapsed_ms, algorithm=result.algorithm,
                  estimated_work_bytes=request.estimated_work_bytes, retained_bytes=result.retained_bytes,
                  process_peak_after_setup_bytes=setup_peak, process_peak_after_detection_bytes=pipeline_peak,
                  label_and_feature_sha256=hashes, input_sha256=result.nuclei.input_sha256,
                  native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
                  source_tree_sha256=environment.source_tree_sha256, build_git_revision=environment.git_revision,
                  script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  platform=platform.platform(), python=sys.version)
    args.output.write_bytes((json.dumps(output, indent=2) + '\n').encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--child-size', type=int, choices=[256, 512, 1024])
    parser.add_argument('--tile', type=int, choices=[64, 256], default=64)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--output-directory', type=Path)
    args = parser.parse_args()
    if args.child_size:
        child(args); return
    if args.output_directory is None:
        parser.error('--output-directory is required for the parent experiment')
    args.output_directory.mkdir(parents=True, exist_ok=False)
    cases = []
    for size in [256, 512, 1024]:
        samples = []
        for repetition in range(4):
            for tile in ([64, 256] if repetition % 2 == 0 else [256, 64]):
                path = args.output_directory / f'{size}-{repetition}-{tile}.json'
                subprocess.run([sys.executable, '-I', str(Path(__file__).resolve()), '--child-size', str(size),
                                '--tile', str(tile), '--output', str(path.resolve())], check=True, timeout=180)
                observation = json.loads(path.read_text(encoding='utf-8'))
                if repetition:
                    samples.append(observation)
        assert len({json.dumps(s['label_and_feature_sha256'], sort_keys=True) for s in samples}) == 1
        summaries = {}
        for tile in [64, 256]:
            arm = [s for s in samples if s['tile_size'] == tile]
            summaries[str(tile)] = {key: dict(median=statistics.median(s[key] for s in arm),
                minimum=min(s[key] for s in arm), maximum=max(s[key] for s in arm)) for key in
                ['capture_ms', 'detection_wall_ms', 'worker_ms', 'process_peak_after_detection_bytes']}
        cases.append(dict(size=size, cells=samples[0]['cells'], summary=summaries, samples=samples))
    assert len({s['native_sha256'] for c in cases for s in c['samples']}) == 1
    report = dict(schema='qpx.log-cells-profile/1', performance_accepted=False, cases=cases,
        protocol='Fresh child per observation; one excluded pair followed by three retained pairs per size, alternating tile order. Timers exclude image setup and independent output checks. Native synchronous production detector.',
        limitations=['Synthetic isolated disks only; this does not establish biological throughput.',
            'Whole-process peak RSS includes Python, NumPy, input generation, imports and native allocations; it is not the native work estimate.',
            'Shared workstation with uncontrolled other workloads; no hardware, frame-budget or universal speed claim.'])
    args.output.write_bytes((json.dumps(report, indent=2) + '\n').encode())
    print(json.dumps(dict(cases=len(cases), performance_accepted=False)))


if __name__ == '__main__':
    main()
