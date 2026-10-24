"""Compare fresh native CLI file loading; retain all timings and process metrics.

Windows read-transfer counters include all process I/O, not physical disk reads.
The native TMA test separately measures the oversized call's read-transfer delta.
"""
import argparse
import csv
import ctypes
from ctypes import wintypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import statistics
import subprocess
import time

import qupathxx as q


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def io_counters(process):
    class Counters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_ulonglong) for name in
                    ['read_operations', 'write_operations', 'other_operations',
                     'read_transfer_bytes', 'write_transfer_bytes', 'other_transfer_bytes']]
    query = ctypes.WinDLL('kernel32', use_last_error=True).GetProcessIoCounters
    query.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters)]
    query.restype = wintypes.BOOL
    counters = Counters()
    if not query(wintypes.HANDLE(int(process._handle)), ctypes.byref(counters)):
        raise ctypes.WinError(ctypes.get_last_error())
    return {name: getattr(counters, name) for name, _ in counters._fields_}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--baseline-executable', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--baseline-source', required=True)
    parser.add_argument('--output-directory', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--repetitions', type=int, default=3)
    args = parser.parse_args()
    if not __debug__ or os.name != 'nt' or not 3 <= args.repetitions <= 10:
        parser.error('require Windows, enabled assertions and 3..10 measured repetitions')
    executables = dict(baseline=args.baseline_executable.resolve(), candidate=args.executable.resolve())
    sources = dict(baseline=args.baseline_source, candidate=args.source)
    for mode, executable in executables.items():
        source = sources[mode]
        if len(source) != 40 or any(c not in '0123456789abcdef' for c in source) or source.encode() not in executable.read_bytes():
            parser.error('expected compiled source revision is not embedded in '+mode+' executable')
    if sha(executables['baseline']) == sha(executables['candidate']):
        parser.error('require distinct baseline and candidate executables')
    spec = importlib.util.spec_from_file_location('qpx_process_metrics', Path(__file__).with_name('profile_sampled_log.py'))
    helper = importlib.util.module_from_spec(spec); spec.loader.exec_module(helper)
    out = args.output_directory.resolve(); out.mkdir(parents=True, exist_ok=False)
    doc = q.Document(q.Image(256, 192, [32]*256*192*3))
    options = q.TmaGridOptions(); options.rows, options.columns = 2, 3
    options.origin = q.Point(30, 30); options.row_step, options.column_step = q.Point(0, 60), q.Point(60, 0)
    options.width, options.height = 30, 24
    ids = doc.create_tma_grid(options); doc.set_tma_case_ids({ids[0]: 'profile'})
    core = doc.tma_grid().core(0, 0)
    project = q.Project(); project.add_document(doc, 'input profile'); project_path = out/'source.qpxproj'; project.save(str(project_path))
    project_sha = sha(project_path)
    header = b'source_object_id,source_object_revision,value,weight\r\n'
    row = f'{core.id},{core.revision},1.25,2\r\n'.encode()
    cases = []
    for count in [100000, 500000, 2000000]:
        path = out/f'input-{count}.csv'
        with path.open('wb') as stream:
            stream.write(header)
            for at in range(0, count, 10000):
                stream.write(row*min(10000, count-at))
        cases.append(dict(name=str(count), rows=count, input=path, input_sha256=sha(path), limit=64*1024*1024, expected_exit=0))
    cases.append(dict(cases[-1], name='oversized', limit=1024*1024, expected_exit=1))
    records = []; output_hashes = {}
    for case in cases:
        for trial in range(args.repetitions+1):
            for mode in (['baseline', 'candidate'] if trial % 2 == 0 else ['candidate', 'baseline']):
                output = out/f'{case["name"]}-{mode}-{trial}.csv'
                command = [str(executables[mode]), '--export-tma-cases', str(project_path), str(output),
                           '--measurements', str(case['input']), '--value-column', 'value', '--weight-column', 'weight',
                           '--csv-bytes', str(case['limit'])]
                start = time.perf_counter()
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    stdout, stderr = process.communicate(timeout=120)
                except subprocess.TimeoutExpired:
                    process.kill(); process.communicate(); raise
                elapsed = (time.perf_counter()-start)*1000
                assert process.returncode == case['expected_exit'], (command, stdout, stderr)
                metrics = io_counters(process); peak = helper.peak_working_set(process)
                if case['expected_exit']:
                    report = json.loads(stderr); assert 'exceeds byte limit' in report['error'] and not output.exists()
                    digest = None
                else:
                    report = json.loads(stdout); assert report['bytes'] == output.stat().st_size
                    with output.open(encoding='utf8', newline='') as stream:
                        rows = list(csv.DictReader(stream))
                    measured = next(r for r in rows if r['case_id'] == 'profile'); count = case['rows']
                    for column, value in [('value_sum', count*1.25), ('weight_sum', count*2), ('weighted_sum', count*2.5),
                                          ('mean', 1.25), ('weighted_mean', 1.25), ('minimum', 1.25), ('maximum', 1.25)]:
                        assert float(measured[column]) == value
                    assert measured['measurement_rows'] == str(count) and measured['input_table_sha256'] == case['input_sha256']
                    assert len(rows) == 6 and all(r['mean'] == '' for r in rows if r['case_id'] != 'profile')
                    digest = sha(output); output_hashes.setdefault(case['name'], digest); assert output_hashes[case['name']] == digest
                records.append(dict(case=case['name'], mode=mode, trial=trial, warmup=trial == 0,
                                    milliseconds=elapsed, peak_working_set_bytes=peak, **metrics,
                                    exit_code=process.returncode, output_sha256=digest, report=report))
        assert sha(case['input']) == case['input_sha256'] and sha(project_path) == project_sha
        assert not list(out.glob('*.qpx-tmp-*'))
    summaries = []
    for case in cases:
        summary = dict(case=case['name'], input_bytes=case['input'].stat().st_size, byte_limit=case['limit'])
        for mode in executables:
            selected = [r for r in records if r['case'] == case['name'] and r['mode'] == mode and not r['warmup']]
            summary[mode] = {key: statistics.median(r[key] for r in selected) for key in
                             ['milliseconds', 'peak_working_set_bytes', 'read_transfer_bytes', 'read_operations']}
        summaries.append(summary)
    report = dict(schema='qpx.tma-file-input-profile/1', sources=sources,
                  executable_sha256={mode: sha(path) for mode, path in executables.items()},
                  profiler_sha256=sha(Path(__file__)), project_sha256=project_sha, summaries=summaries, records=records,
                  valid_output_hashes_identical=True, independent_statistics_verified=True,
                  input_and_project_unchanged=True, performance_accepted=False, release_ready=False,
                  scope='Fresh native headless/project CLI processes on a shared Windows workstation. Alternating order, one explicit warmup per case/build, all observations retained. Process read-transfer counts include all process reads and are not physical storage traffic. Wall time includes startup, project loading, CSV parsing/hash/format and atomic publication. OS cache is uncontrolled; no hardware-wide speedup or release acceptance.')
    args.report.write_bytes((json.dumps(report, indent=2)+'\n').encode())
    print(json.dumps(summaries, indent=2))


if __name__ == '__main__':
    main()
