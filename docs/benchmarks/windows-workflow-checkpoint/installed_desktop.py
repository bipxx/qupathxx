"""Exercise the selected real-slide ImGui route in the installed candidate."""
import argparse
import csv
import json
import subprocess
from pathlib import Path

import qupathxx as q


def event(frame, kind, x=.5, y=.5, wait=None):
    value = q.GuiInput()
    value.frame, value.kind, value.x, value.y = frame, kind, x, y
    if wait is not None:
        value.wait_for = wait
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('slide', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    project = q.Project()
    image = project.add_image(str(args.slide.resolve()))
    document = project.document(image)
    path = args.output / 'desktop.qpxproj'
    project.save(str(path))
    options = q.GuiOptions()
    options.project = project
    options.width, options.height = 1280, 900
    view = q.Viewport()
    # Viewport.origin is a screen translation, not an image-space top-left.
    view.zoom, view.origin = 1, q.Point(-28800, -1854)
    options.start_view = view
    options.analysis_intensity_statistics = True
    options.measurement_table_path = str(args.output / 'desktop.csv')
    options.replay_timeout_seconds = 15
    options.inputs = [
        event(4, q.GuiInputKind.RECTANGLE_KEY, wait=q.GuiInputWait.WINDOW_READY),
        event(8, q.GuiInputKind.LEFT_DOWN, .45, .45),
        event(12, q.GuiInputKind.MOVE, .55, .55),
        event(16, q.GuiInputKind.LEFT_UP, .55, .55),
        event(20, q.GuiInputKind.SELECT_ALL_KEY),
        event(24, q.GuiInputKind.ANALYZE_KEY),
        event(100, q.GuiInputKind.KEEP_ANALYSIS_KEY),
        event(104, q.GuiInputKind.DETECT_TISSUE_KEY),
        event(160, q.GuiInputKind.KEEP_TISSUE_KEY, wait=q.GuiInputWait.TISSUE_READY),
        event(164, q.GuiInputKind.MOVE, wait=q.GuiInputWait.TISSUE_ARCHIVED),
        event(180, q.GuiInputKind.EXPORT_MEASUREMENT_TABLE_KEY),
        event(200, q.GuiInputKind.SAVE_KEY, wait=q.GuiInputWait.MEASUREMENT_TABLE_SAVED),
    ]
    try:
        report = q.show(document, frame_limit=260, capture_path=str(args.output / 'desktop.ppm'), options=options)
    except Exception as error:
        project.save(str(args.output / 'failed-state.qpxproj'))
        diagnostics = dict(error=str(error), objects=document.object_count,
                           analysis_records=document.analysis_record_count,
                           tissue_records=document.tissue_record_count,
                           annotations=[str(a.roi) for a in document.annotations])
        (args.output / 'failure.json').write_text(json.dumps(diagnostics, indent=2))
        raise
    summary = {name: getattr(report, name) for name in (
        'replay_complete', 'analysis_state', 'analysis_error', 'tissue_state',
        'tissue_error', 'project_error', 'measurement_table_state',
        'measurement_table_error', 'measurement_table_rows', 'ui_overflow_items')}
    summary.update(source_revision=q.build_environment().git_revision,
                   object_count=document.object_count,
                   analysis_records=document.analysis_record_count,
                   tissue_records=document.tissue_record_count)
    (args.output / 'desktop-report.json').write_text(json.dumps(summary, indent=2))
    assert report.replay_complete, summary
    assert report.analysis_state == report.tissue_state == 'ready', summary
    assert report.measurement_table_state == 'succeeded', summary
    assert not report.project_error, summary
    assert document.object_count == document.analysis_record_count == document.tissue_record_count == 1, summary
    verify_saved(args.output, image, summary)


def verify_saved(output, image, summary):
    """Finish artifact checks without replaying an already successful GUI run."""
    path = output / 'desktop.qpxproj'
    restored = q.Project.open(str(path)).document(image)
    assert restored.object_count == restored.analysis_record_count == restored.tissue_record_count == 1
    for record in restored.analysis_records:
        request = q.AnalysisRequest.replay(restored, record.id)
        assert restored.verify_analysis(record.id, request, q.analyze(request))
        assert record.result.intensity_statistics is not None
    for record in restored.tissue_records:
        request = q.TissueRequest.replay(restored, record.id)
        result = q.detect_tissue(request)
        assert restored.verify_tissue(record.id, request, result)
        assert result.foreground_pixels > 0
    with (output / 'desktop.csv').open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 1 and rows[0]['intensity_statistics_format']
    app = Path(q.__file__).parent / 'qupathxx_app.exe'
    done = subprocess.run([str(app), '--project', str(path), '--smoke', '--compact'],
                          capture_output=True, text=True, timeout=30)
    summary.update(saved_replay=True, standalone_project_exit=done.returncode,
                   standalone_stderr=done.stderr)
    (output / 'desktop-report.json').write_text(json.dumps(summary, indent=2))
    assert done.returncode == 0, summary
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
