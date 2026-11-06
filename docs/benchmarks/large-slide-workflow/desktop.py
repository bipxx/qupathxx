"""Run the new whole-slide tissue controls and retained output in ImGui."""
import argparse
import csv
import json
import subprocess
from pathlib import Path

import qupathxx as q


def event(frame, kind, wait=None):
    value = q.GuiInput()
    value.frame, value.kind = frame, kind
    if wait is not None:
        value.wait_for = wait
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('slide', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--sdk-project', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    project = q.Project()
    image = project.add_image(str(args.slide.resolve()))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, doc.metadata.width, doc.metadata.height))
    path = args.output / 'desktop.qpxproj'
    project.save(str(path))
    options = q.GuiOptions()
    options.project = project
    options.width, options.height = 1280, 900
    options.tissue_canonical_pyramid = True
    options.tissue_level = 2
    options.tissue_work_bytes = 128 * 1024 * 1024
    options.overview_source_pixel_limit = 8 * 1024 * 1024
    options.measurement_table_kind = 1
    options.measurement_table_path = str(args.output / 'tissue.csv')
    options.replay_timeout_seconds = 30
    options.inputs = [
        event(4, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputWait.WINDOW_READY),
        event(8, q.GuiInputKind.DETECT_TISSUE_KEY),
        event(80, q.GuiInputKind.KEEP_TISSUE_KEY, q.GuiInputWait.TISSUE_READY),
        event(84, q.GuiInputKind.MOVE, q.GuiInputWait.TISSUE_ARCHIVED),
        event(100, q.GuiInputKind.EXPORT_MEASUREMENT_TABLE_KEY),
        event(120, q.GuiInputKind.SAVE_KEY, q.GuiInputWait.MEASUREMENT_TABLE_SAVED),
    ]
    report = q.show(doc, frame_limit=220, capture_path=str(args.output / 'desktop.ppm'), options=options)
    summary = {name: getattr(report, name) for name in (
        'replay_complete', 'tissue_state', 'tissue_error', 'tissue_samples',
        'tissue_components', 'tissue_foreground_pixels', 'tissue_input_sha256',
        'tissue_preview_ready', 'tissue_width', 'tissue_height', 'tissue_level',
        'tissue_downsample', 'project_error', 'overview_state', 'overview_error',
        'measurement_table_state', 'measurement_table_error', 'measurement_table_rows',
        'ui_overflow_items')}
    summary['source_revision'] = q.build_environment().git_revision
    (args.output / 'report.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    assert report.replay_complete and report.tissue_state == 'ready', summary
    assert not report.tissue_error and not report.project_error, summary
    assert report.tissue_preview_ready and report.tissue_samples == 5913875, summary
    assert report.tissue_components == 688 and report.tissue_foreground_pixels == 1083798, summary
    assert report.measurement_table_state == 'succeeded', summary
    restored = q.Project.open(str(path)).document(image)
    record = restored.latest_tissue(obj)
    request = q.TissueRequest.replay(restored, record.id)
    assert request.parameters.canonical_pyramid
    assert restored.verify_tissue(record.id, request, q.detect_tissue(request))
    with (args.output / 'tissue.csv').open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 688
    assert sum(int(row['component_samples']) for row in rows) == 1083798
    summary['saved_replay'] = True
    if args.sdk_project:
        app = Path(q.__file__).parent / 'qupathxx_app.exe'
        done = subprocess.run([str(app), '--project', str(args.sdk_project.resolve()), '--smoke', '--compact'],
                              capture_output=True, text=True, timeout=30)
        summary['sdk_project_standalone_exit'] = done.returncode
        assert done.returncode == 0, done.stderr
    (args.output / 'report.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
