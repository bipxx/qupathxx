"""Windows-only before/after measurement source retained with the raw records.

Only the GLFW window owned by this worker PID is controlled. Measured intervals
exclude loading, the first 30 rendered frames, and 100 ms after state changes.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import threading
import time
from ctypes import wintypes


def worker(args):
    import qupathxx as q
    from qupathxx import _native
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.FindWindowW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR]
    user32.FindWindowW.restype = wintypes.HWND
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.IsIconic.argtypes = [wintypes.HWND]
    session = q.ViewerSession(str(args.project))
    errors, measured = [], {}
    def completed(ticket):
        event = session.wait(ticket, 30)
        assert event.state == 'succeeded', (event.state, event.error)
        return session.take(ticket)
    def producer():
        try:
            completed(session.catalog())
            end = time.monotonic()+30
            warm_frame = session.stats.frame+30
            while session.stats.frame < warm_frame:
                assert time.monotonic() < end, 'initial frames did not finish'
                time.sleep(.01)
            hwnd = user32.FindWindowW(None, 'qupathxx | Bioimage analysis')
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            assert hwnd and pid.value == os.getpid(), 'window must belong to this worker'
            if args.minimized:
                user32.ShowWindow(hwnd, 6)  # SW_MINIMIZE, on this process's own window
            time.sleep(.1)
            assert bool(user32.IsIconic(hwnd)) == args.minimized
            start_frame = session.stats.frame
            wall, cpu = time.perf_counter(), time.process_time()
            time.sleep(args.seconds)
            wall, cpu = time.perf_counter()-wall, time.process_time()-cpu
            measured.update(wall_seconds=wall, process_cpu_seconds=cpu, cpu_core_fraction=cpu/wall,
                            rendered_frame_delta=session.stats.frame-start_frame)
            assert bool(user32.IsIconic(hwnd)) == args.minimized
            if args.minimized:
                user32.ShowWindow(hwnd, 9)  # SW_RESTORE
            completed(session.close(True))
        except BaseException as error:
            errors.append(error)
            try:
                session.close(True)
            except RuntimeError:
                pass
    options = q.GuiOptions()
    options.autosave_interval_seconds = 0
    select = q.GuiInput()
    select.frame, select.kind = 3, q.GuiInputKind.SELECT_ALL_KEY
    select.wait_for = q.GuiInputWait.STARTUP_FINISHED
    options.inputs = [select]
    thread = threading.Thread(target=producer, daemon=True)
    thread.start()
    try:
        report = q.show(session, frame_limit=100000, options=options)
    finally:
        thread.join(35)
    assert not thread.is_alive()
    if errors:
        raise errors[0]
    assert len(report.selected_ids) == args.objects
    measured.update(minimized=args.minimized, gl_renderer=report.gl_renderer,
        python=sys.version, source_tree_sha256=q.build_environment().source_tree_sha256,
        build_git_revision=q.build_environment().git_revision,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
        suspended_iterations=getattr(report, 'suspended_iterations', None))
    print(json.dumps(measured))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline-python', type=Path)
    parser.add_argument('--current-python', type=Path, default=Path(sys.executable))
    parser.add_argument('--objects', type=int, default=100000)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--seconds', type=float, default=2)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--worker', action='store_true')
    parser.add_argument('--project', type=Path)
    parser.add_argument('--minimized', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'win32' or not 1 <= args.objects <= 100000 or not 1 <= args.repetitions <= 10 or not .5 <= args.seconds <= 30:
        parser.error('Windows only; objects 1..100000, repetitions 1..10, seconds .5..30')
    if args.worker:
        worker(args)
        return
    if not args.baseline_python or not args.output:
        parser.error('--baseline-python and --output are required')
    import qupathxx as q
    args.output.parent.mkdir(parents=True, exist_ok=True)
    project_path = args.output.with_suffix('.qpxproj').resolve()
    doc = q.Document(q.Image(1024, 1024, [35, 60, 90]*1024*1024))
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect((i % 500)*2, (i//500)*2, 1, 1)) for i in range(args.objects)])
    project = q.Project()
    project.add_document(doc)
    project.save(str(project_path))
    data = dict(schema='qpx.minimized-cpu-comparison/1', platform=platform.platform(), objects=args.objects,
                interval_seconds=args.seconds, repetitions=args.repetitions,
                capture_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), runs=[])
    for repetition in range(args.repetitions):
        versions = [('baseline', args.baseline_python), ('current', args.current_python)]
        if repetition % 2:
            versions.reverse()
        for label, python in versions:
            for minimized in ([False, True] if repetition % 2 == 0 else [True, False]):
                command = [str(python.resolve()), '-I', str(Path(__file__).resolve()), '--worker', '--project', str(project_path), '--seconds', str(args.seconds), '--objects', str(args.objects)]
                if minimized:
                    command.append('--minimized')
                run = json.loads(subprocess.check_output(command, timeout=90, text=True))
                run.update(version=label, repetition=repetition)
                data['runs'].append(run)
    data['median_cpu_core_fraction'] = {version: {state: statistics.median(r['cpu_core_fraction'] for r in data['runs'] if r['version'] == version and r['minimized'] == minimized)
                                                  for state, minimized in [('active', False), ('minimized', True)]}
                                        for version in ['baseline', 'current']}
    data['scope'] = ('100000-object synthetic static selected scene by default. Fresh processes, same source project and software renderer. '
                     'Per-process CPU sums all threads over a fixed wall interval after project adoption plus 30 rendered frames and 100 ms transition delay; '
                     'excludes loading, startup, context teardown and capture. Windows ShowWindow is used for both versions. '
                     'No hardware GPU, first-render or dynamic interaction performance acceptance.')
    args.output.write_text(json.dumps(data, indent=2)+'\n', encoding='utf8')
    print(json.dumps(data, indent=2))


if __name__ == '__main__':
    main()
