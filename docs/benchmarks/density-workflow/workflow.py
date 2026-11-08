"""Reuse the imported506-cell crop; verify density through SDK/CLI/desktop.

The independently evaluated denominator uses an exact axis-aligned coordinate
arrangement for this declared fixture, never a pixel raster or native clipping.
"""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import subprocess

import qupathxx as q


def inside(point, rect, strict=False):
    x, y, width, height = rect
    if strict:
        return x < point[0] < x + width and y < point[1] < y + height
    return x <= point[0] <= x + width and y <= point[1] <= y + height


def independent_oracle(window, points):
    t1, t2, hole = (16, 16, 160, 200), (96, 32, 144, 208), (192, 96, 32, 32)
    exclusions = [(64, 64, 32, 128), (80, 96, 32, 96)]

    def valid(point):
        tissue = inside(point, t1) or (inside(point, t2) and not inside(point, hole, strict=True))
        return inside(point, window) and tissue and not any(inside(point, r) for r in exclusions)

    rectangles = [window, (0, 0, 256, 256), t1, t2, hole, *exclusions]
    xs = sorted({v for x, _, w, _ in rectangles for v in (x, x + w)})
    ys = sorted({v for _, y, _, h in rectangles for v in (y, y + h)})
    area = sum((b - a) * (d - c)
               for a, b in zip(xs, xs[1:]) for c, d in zip(ys, ys[1:])
               if valid(((a + b) / 2, (c + d) / 2)))
    return area, sum(valid(p) for p in points) if area else 0


def event(frame, kind, target=q.GuiInputTarget.CANVAS, wait=q.GuiInputWait.IMMEDIATE):
    value = q.GuiInput()
    value.frame, value.kind, value.target, value.wait_for = frame, kind, target, wait
    return value


def click(frame, target):
    return [event(frame, q.GuiInputKind.LEFT_DOWN, target, q.GuiInputWait.TARGET_READY),
            event(frame + 1, q.GuiInputKind.LEFT_UP, target)]


def run(command):
    completed = subprocess.run([str(p) for p in command], capture_output=True, text=True, timeout=60)
    assert completed.returncode == 0, completed.stdout + completed.stderr
    return completed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input_project', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--image', type=int, default=1)
    parser.add_argument('--gui', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    project = q.Project.open(str(args.input_project.resolve()))
    doc = project.document(args.image)
    cells = sorted(obj.id for obj in doc.annotations if obj.kind == q.ObjectKind.CELL)
    assert len(cells) == 506
    assert doc.metadata.width == doc.metadata.height == 256
    window_rects = [(0, 0, 256, 256), (0, 0, 128, 256),
                    (128, 0, 128, 256), (240, 240, 8, 8)]
    windows = [doc.add_rectangle(q.Rect(*r), 'Density window') for r in window_rects]
    t1 = doc.add_rectangle(q.Rect(16, 16, 160, 200), 'Tissue rectangle')

    def ring(rect):
        x, y, w, h = rect
        return [q.Point(x, y), q.Point(x + w, y), q.Point(x + w, y + h), q.Point(x, y + h)]

    t2 = doc.add_roi(q.Roi.polygon(ring((96, 32, 144, 208)),
                                  [ring((192, 96, 32, 32))]), 'Tissue with a hole')
    tissue = [t1, t2]
    excluded = [doc.add_rectangle(q.Rect(*r), 'Density exclusion')
                for r in [(64, 64, 32, 128), (80, 96, 32, 96)]]
    for name, color, ids in [('Density windows', 0x3080ff, windows),
                             ('Density tissue', 0x40d070, tissue),
                             ('Density exclusion', 0xff4040, excluded)]:
        doc.set_class(name, color)
        doc.classify_objects(ids, name)
    source = args.output / 'scene.qpxproj'
    project.save(str(source))
    request = q.SpatialDensityRequest(doc, windows, tissue, excluded, cells)
    points = [(request.cell_object(i).centroid.x, request.cell_object(i).centroid.y)
              for i in range(request.cell_count)]
    expected = [independent_oracle(w, points) for w in window_rects]
    assert expected[0][0] == 40576 and expected[-1] == (0, 0)
    result = q.measure_spatial_density(request)
    for row, (area, count) in zip(result.rows(), expected):
        assert row.count == count and row.area_pixels == area
        if area:
            area_mm2 = area * request.x_scale * request.y_scale / 1e6
            assert math.isclose(row.area_mm2, area_mm2, rel_tol=1e-12)
            assert math.isclose(row.density_per_mm2, count / area_mm2, rel_tol=1e-12)
        else:
            assert row.density_per_mm2 is row.footprint is None
    package = Path(q.__file__).parent
    cli = package / ('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [cli, '--measure-spatial', source, args.output / 'cli.qpxproj',
               '--image', args.image, '--operation', 'tissue-density']
    for name, ids in [('source', windows), ('target', cells),
                      ('tissue', tissue), ('exclude', excluded)]:
        path = args.output / (name + '.ids')
        path.write_text('\n'.join(map(str, ids)), encoding='utf-8')
        command += ['--' + name + '-ids', path]
    cli_report = json.loads(run(command).stdout)
    assert cli_report['result_sha256'] == result.sha256
    common = ['--image', args.image, '--record', cli_report['record_id'], '--operation', 'tissue-density']
    run([cli, '--verify-spatial', args.output / 'cli.qpxproj', *common])
    run([cli, '--export-spatial', args.output / 'cli.qpxproj', args.output / 'cli.csv', *common])
    sdk = args.output / 'sdk'
    sdk.mkdir()
    record_id = doc.keep_density(request, result)
    q.save_spatial_density_csv(result, str(sdk / 'density.csv'))
    project.save(str(sdk / 'study.qpxproj'))
    reopened = q.Project.open(str(sdk / 'study.qpxproj')).document(args.image)
    replay = q.SpatialDensityRequest.replay(reopened, record_id)
    assert reopened.verify_density(record_id, replay, q.measure_spatial_density(replay))
    assert (sdk / 'density.csv').read_bytes() == (args.output / 'cli.csv').read_bytes()
    summary = dict(source_revision=q.build_environment().git_revision,
                   source_tree_sha256=q.build_environment().source_tree_sha256,
                   source_project=str(args.input_project.resolve()),
                   source_project_sha256=hashlib.sha256(args.input_project.read_bytes()).hexdigest(),
                   imported_cells=len(cells), windows=windows, tissue=tissue, excluded=excluded,
                   expected_area_count=expected, input_sha256=result.input_sha256,
                   result_sha256=result.sha256, cli_verified=True, sdk_reopened_verified=True,
                   pixel_size_um=[result.x_scale, result.y_scale],
                   oracle='Independent axis-aligned coordinate arrangement and closed centroid predicates; native captured centroid authority reused',
                   scope='Existing imported NDPI reference ROIs/crop; declared synthetic tissue/exclusions; no segmentation, speed-superiority or biological acceptance')
    if args.gui:
        desktop = q.Project.open(str(source))
        path = args.output / 'desktop.qpxproj'
        desktop.save(str(path))
        gui = q.GuiOptions()
        gui.project, gui.width, gui.height = desktop, 1280, 1000
        gui.autosave_interval_seconds, gui.replay_timeout_seconds = 0, 20
        spatial = q.GuiSpatialOptions()
        spatial.open, spatial.operation = True, q.SpatialOperation.TISSUE_DENSITY
        spatial.sources, spatial.targets, spatial.tissue, spatial.excluded = windows, cells, tissue, excluded
        spatial.csv_path = str(args.output / 'desktop.csv')
        gui.spatial = spatial
        scroll = event(52, q.GuiInputKind.WHEEL, q.GuiInputTarget.SPATIAL_EXPORT)
        scroll.wheel = -4
        gui.inputs = [*click(4, q.GuiInputTarget.SPATIAL_RUN),
                      event(12, q.GuiInputKind.MOVE, wait=q.GuiInputWait.SPATIAL_READY),
                      *click(16, q.GuiInputTarget.SPATIAL_KEEP),
                      event(20, q.GuiInputKind.MOVE, wait=q.GuiInputWait.SPATIAL_KEPT),
                      *click(24, q.GuiInputTarget.SPATIAL_INSPECT),
                      *click(28, q.GuiInputTarget.SPATIAL_VERIFY),
                      event(32, q.GuiInputKind.MOVE, wait=q.GuiInputWait.SPATIAL_VERIFIED),
                      *click(36, q.GuiInputTarget.SPATIAL_EXPORT),
                      event(44, q.GuiInputKind.SAVE_KEY, wait=q.GuiInputWait.SPATIAL_EXPORTED), scroll]
        report = q.show(desktop.document(args.image), frame_limit=72,
                        capture_path=str(args.output / 'desktop.ppm'), options=gui)
        state = report.spatial
        summary['gui'] = dict(replay_complete=report.replay_complete, state=state.state,
                              error=state.error, record_status=state.record_status, rows=state.rows,
                              export_state=state.export_state, export_error=state.export_error,
                              project_error=report.project_error, ui_overflow_items=report.ui_overflow_items)
        (args.output / 'gui-observation.json').write_text(json.dumps(summary['gui'], indent=2), encoding='utf-8')
        assert report.replay_complete and state.state == 'verified' and not state.error
        assert state.density_result and state.result is state.neighborhood_result is None and state.rows == len(windows)
        assert state.export_state == 'saved' and not state.export_error and not report.project_error
        assert (args.output / 'desktop.csv').read_bytes() == (sdk / 'density.csv').read_bytes()
        saved = q.Project.open(str(path)).document(args.image)
        assert saved.density_record(state.record_id).result.sha256 == result.sha256
        app = package / ('qupathxx_app.exe' if os.name == 'nt' else 'qupathxx_app')
        run([app, '--project', sdk / 'study.qpxproj', '--smoke', '--compact'])
        summary['standalone_reopen'] = True
    (args.output / 'workflow.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
