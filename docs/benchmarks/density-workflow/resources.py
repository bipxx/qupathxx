"""One bounded dense density/GIL observation, without a speed or RSS-cap claim."""
import argparse
import importlib.util
import json
from pathlib import Path
import threading
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    # Reuse the established platform observation helper, not another memory API.
    helper = Path(__file__).parents[1] / 'neighborhood-workflow/resources.py'
    spec = importlib.util.spec_from_file_location('neighborhood_resources', helper)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    doc = q.Document(q.Image(64, 64, [80] * 64 * 64 * 3))
    doc.set_calibration(.5, .25)
    body = q.Roi.rectangle(q.Rect(31, 31, 2, 2))
    cells = doc.execute([q.ObjectEdit.create_geometry(body, kind=q.ObjectKind.CELL)
                         for _ in range(20000)])
    window = q.Roi.rectangle(q.Rect(1, 1, 60, 60))
    windows = doc.execute([q.ObjectEdit.create_geometry(window) for _ in range(500)])
    tissue = doc.add_rectangle(q.Rect(0, 0, 64, 64))
    request = q.SpatialDensityRequest(doc, windows, [tissue], [], cells)
    ready, stop = threading.Event(), threading.Event()
    ticks = [0]

    def observer():
        ready.set()
        while not stop.wait(.001):
            ticks[0] += 1

    thread = threading.Thread(target=observer)
    thread.start()
    ready.wait()
    job = q.SpatialDensityJob()
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
    assert state.state == 'ready' and result is not None and result.row_count == len(windows)
    for start in range(0, result.row_count, 4096):
        for row in result.rows(start, 4096):
            assert row.count == len(cells) and row.area_pixels == 3600
    assert result.membership_tests == len(cells) * len(windows)
    assert result.geometry_operations == len(windows) * 2
    assert released_ticks >= 5, 'Python observer did not progress during native wait'
    summary = dict(source_revision=q.build_environment().git_revision,
                   source_tree_sha256=q.build_environment().source_tree_sha256,
                   cells=len(cells), windows=len(windows), expected_count_per_roi=len(cells),
                   expected_area_pixels_per_roi=3600, membership_tests=result.membership_tests,
                   geometry_operations=result.geometry_operations, snapshot_bytes=request.snapshot_bytes,
                   estimated_native_work_bytes=request.required_work_bytes,
                   retained_result_payload_bytes=result.payload_bytes,
                   process_peak_working_set_bytes=module.peak_working_set(),
                   python_ticks_during_native_wait=released_ticks, observed_wait_seconds=seconds,
                   input_sha256=result.input_sha256, result_sha256=result.sha256,
                   memory_scope='Whole Python process with scene/edit history and native libraries; opaque GEOS scratch is not a native-payload/RSS guarantee',
                   scope='One bounded coincident-cell/all-overlapping-ROI observation; no dense pair matrix, speed-superiority or biological acceptance claim')
    (args.output / 'resources.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
