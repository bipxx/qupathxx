"""Fresh-process observations of production LoG cell recipes and saved replay."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time

if not __debug__:
    raise RuntimeError('workflow profiling requires independent assertions enabled')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, data):
    path.write_bytes((json.dumps(data, indent=2, allow_nan=False)+'\n').encode())


def profile_helper():
    spec=importlib.util.spec_from_file_location('qpx_kernel_profile',Path(__file__).with_name('profile_sampled_log.py'))
    helper=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    return helper


def worker(size, mode, output, affinity=None):
    import numpy as np
    import qupathxx as q
    from qupathxx import _native
    yy, xx = np.mgrid[:size, :size]
    disks = (xx % 64+.5-32)**2+(yy % 64+.5-32)**2 <= 81
    rgb = np.where(disks[..., None], [30, 40, 60], [255, 255, 255]).astype(np.uint8)
    doc = q.Document(q.Image.from_numpy(rgb))
    obj = doc.add_rectangle(q.Rect(0, 0, size, size))
    algorithm = (q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES if mode == 'sampled'
                 else q.CellAlgorithm.LOG_WATERSHED_BODIES)
    p = q.CellParameters(algorithm)
    p.sigma = 1
    p.cell_expansion = 5
    p.tile_size = 64
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    began = time.perf_counter()
    request = q.CellRequest(doc, obj, transform, p)
    capture_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    result = q.detect_cells(request)
    detection_wall_ms = (time.perf_counter()-began)*1000
    nuclei = result.nuclei.labels_numpy()
    bodies = result.cells.labels_numpy()
    count = (size//64)**2
    assert result.cell_count == count
    assert set(np.unique(nuclei)) == set(range(count+1))
    # Independent spatial ledger: exactly one dense nucleus per isolated disk.
    expected_labels = yy//64*(size//64)+xx//64+1
    np.testing.assert_array_equal(nuclei[nuclei != 0], expected_labels[nuclei != 0])
    assert all(nuclei[y, x] == y//64*(size//64)+x//64+1
               for y in range(32, size, 64) for x in range(32, size, 64))
    # Strict sampled-radius expansion oracle. Disks are separated sufficiently
    # that their expanded compartments cannot compete with another label.
    expanded = np.zeros_like(nuclei)
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            if dx*dx+dy*dy >= 25:
                continue
            sy, sx = slice(max(0, -dy), min(size, size-dy)), slice(max(0, -dx), min(size, size-dx))
            ty, tx = slice(max(0, dy), min(size, size+dy)), slice(max(0, dx), min(size, size+dx))
            np.maximum(expanded[ty, tx], nuclei[sy, sx], out=expanded[ty, tx])
    np.testing.assert_array_equal(bodies, expanded)
    for feature in result.features:
        for labels, prefix in [(nuclei, 'nucleus'), (bodies, 'cell')]:
            selected = rgb[labels == feature.label]
            assert len(selected) == getattr(feature, prefix+'_pixels')
            np.testing.assert_allclose(selected.mean(axis=0), getattr(feature, prefix+'_rgb'), atol=1e-10, rtol=0)
    assert result.retained_bytes <= request.estimated_work_bytes <= p.work_bytes
    began = time.perf_counter()
    record = doc.keep_cells(request, result)
    project = q.Project()
    image = project.add_document(doc)
    project_path = output.with_suffix('.qpxproj')
    project.save(str(project_path))
    save_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    reopened = q.Project.open(str(project_path)).document(image)
    restored = reopened.cell_record(record).result
    assert restored.nuclei.labels == result.nuclei.labels and restored.cells.labels == result.cells.labels
    reopen_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    replay = q.CellRequest.replay(reopened, record)
    assert reopened.verify_cells(record, replay, q.detect_cells(replay))
    replay_ms = (time.perf_counter()-began)*1000
    environment = q.build_environment()
    write(output, dict(mode=mode, size=size, cells=count, capture_ms=capture_ms,
        detection_wall_ms=detection_wall_ms, native_detection_ms=result.elapsed_ms,
        keep_save_ms=save_ms, reopen_validate_ms=reopen_ms, replay_verify_ms=replay_ms,
        retained_bytes=result.retained_bytes, estimated_work_bytes=request.estimated_work_bytes,
        project_bytes=project_path.stat().st_size, source_rgb_sha256=hashlib.sha256(rgb.tobytes()).hexdigest(),
        masks_sha256={name: hashlib.sha256(labels.astype('<u4', copy=False).tobytes()).hexdigest()
                      for name, labels in [('nucleus', nuclei), ('body', bodies)]},
        independent_compartment_checks=True, exact_saved_replay=True,
        build_git_revision=environment.git_revision, source_tree_sha256=environment.source_tree_sha256,
        native_sha256=sha(Path(_native.__file__)), python_version=platform.python_version(),cpu_affinity=affinity))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python', type=Path)
    parser.add_argument('--baseline-python', type=Path, help='Optionally interleave a second installed build on the identical fixture')
    parser.add_argument('--cpu-affinity-mask',type=lambda text:int(text,0),help='Windows worker mask, e.g. 0x1; verified before NumPy/native imports')
    parser.add_argument('--output-directory', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--worker-mode', choices=['legacy', 'sampled'])
    parser.add_argument('--size', type=int, choices=[256, 512, 1024])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.cpu_affinity_mask is not None and (os.name!='nt' or not 0<args.cpu_affinity_mask<2**64):
        parser.error('affinity requires Windows and a nonzero 64-bit mask')
    if args.worker_mode:
        if args.size is None or args.output is None:
            parser.error('worker requires size and output')
        affinity=profile_helper().set_current_process_affinity(args.cpu_affinity_mask)
        worker(args.size, args.worker_mode, args.output, affinity)
        return
    if args.python is None or args.output_directory is None or args.report is None or not 1 <= args.repetitions <= 10:
        parser.error('require python, output-directory, report and 1..10 repetitions')
    if args.baseline_python and args.baseline_python.resolve()==args.python.resolve():
        parser.error('baseline and candidate must use different installed environments')
    directory = args.output_directory.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    helper=profile_helper()
    records = []
    builds=[('candidate',args.python)]
    if args.baseline_python:
        builds.insert(0,('baseline',args.baseline_python))
    for size in [256, 512, 1024]:
        for trial in range(args.repetitions+1):
            tasks=[(build,python,mode) for build,python in builds for mode in ['legacy','sampled']]
            if trial%2:tasks.reverse()
            for build,python,mode in tasks:
                output = directory/f'{size}-{build}-{mode}-{trial}.json'
                began = time.perf_counter()
                child = subprocess.Popen([str(python.resolve()), '-I', str(Path(__file__).resolve()),
                    '--worker-mode', mode, '--size', str(size), '--output', str(output)]+
                    (['--cpu-affinity-mask',hex(args.cpu_affinity_mask)] if args.cpu_affinity_mask is not None else []),
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    stdout, stderr = child.communicate(timeout=180)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.communicate()
                    raise
                elapsed = (time.perf_counter()-began)*1000
                peak = helper.peak_working_set(child)
                output.with_suffix('.stdout').write_bytes(stdout)
                output.with_suffix('.stderr').write_bytes(stderr)
                if child.returncode:
                    raise RuntimeError('workflow child failed; see '+str(output.with_suffix('.stderr')))
                record = json.loads(output.read_text())
                assert '+dirty' not in record['build_git_revision']
                if args.cpu_affinity_mask is not None:assert record['cpu_affinity']['verified_mask']==hex(args.cpu_affinity_mask)
                record.update(build=build,trial=trial, warmup=trial == 0, process_wall_ms=elapsed,
                              process_peak_working_set_bytes=peak)
                write(output, record)
                records.append(record)
                print(json.dumps(dict(size=size,build=build,mode=mode,trial=trial,native_detection_ms=record['native_detection_ms'])),flush=True)
        selected = [r for r in records if r['size'] == size]
        assert all(r['masks_sha256'] == selected[0]['masks_sha256'] for r in selected)
    for build,_ in builds:
        assert len({(r['native_sha256'],r['source_tree_sha256'],r['build_git_revision']) for r in records if r['build']==build})==1
    if args.baseline_python:assert len({r['native_sha256'] for r in records})==2
    timings = ['capture_ms', 'detection_wall_ms', 'native_detection_ms', 'keep_save_ms',
               'reopen_validate_ms', 'replay_verify_ms', 'process_wall_ms']
    summaries = []
    for size in [256, 512, 1024]:
        for build,mode in [(build,mode) for build,_ in builds for mode in ['legacy','sampled']]:
            rows = [r for r in records if r['size']==size and r['build']==build and r['mode']==mode and not r['warmup']]
            summaries.append(dict(size=size,build=build,mode=mode,observations=len(rows),
                median_ms={key: statistics.median(r[key] for r in rows) for key in timings},
                max_process_peak_working_set_bytes=max(r['process_peak_working_set_bytes'] for r in rows) if os.name == 'nt' else None))
    args.report.parent.mkdir(parents=True, exist_ok=True)
    write(args.report, dict(schema='qpx.sampled-cell-workflow-profile/2',
        build_executables={label:str(python.resolve()) for label,python in builds},
        requested_cpu_affinity_mask=hex(args.cpu_affinity_mask) if args.cpu_affinity_mask is not None else None,
        tool_sha256=sha(Path(__file__)), peak_helper_sha256=sha(Path(__file__).with_name('profile_sampled_log.py')),
        records=records, summaries=summaries, platform=platform.platform(), performance_accepted=False,
        biological_validation=False, scope='Uncontrolled fresh processes alternate build/recipe order, with one retained/excluded warmup per build/size/recipe. Detection includes native compartment/contour work. Save, load validation and replay are measured separately. All label identities, compartment RGB/counts, strict expansion and saved replay are checked across every build and recipe. Nucleus boundaries are compared across recipes, not independently accepted as biological truth. Process peak includes fixture generation, verification and persistence. No GUI or hardware-target acceptance.'))


if __name__ == '__main__':
    main()
