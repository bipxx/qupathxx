"""Exercise the selected neighborhood workflow on existing captured cell ROIs.

Reuses the pinned 506-cell QuPath capture; never reruns segmentation. The crop
and geometry are translated together into the captured ROI's local coordinates.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys

import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]


def prepare(output):
    import numpy as np
    from PIL import Image
    capture = ROOT / 'tests/fixtures/qupath-ndpi-cells-0.7.0/capture'
    manifest = json.loads((capture / 'capture-manifest.json').read_text(encoding='utf-8'))
    for name in ['contract.json', 'input.png', 'cells.geojson']:
        data = (capture / name).read_bytes()
        assert len(data) == manifest['files'][name]['bytes']
        assert hashlib.sha256(data).hexdigest() == manifest['files'][name]['sha256']
    contract = json.loads((capture / 'contract.json').read_text(encoding='utf-8'))
    x, y, width, height = contract['roi']
    image = np.asarray(Image.open(capture / 'input.png').convert('RGB'))
    assert image.shape == (height, width, 3)
    features = json.loads((capture / 'cells.geojson').read_text(encoding='utf-8'))

    def local(coords):
        if isinstance(coords[0], (float, int)):
            return [coords[0] - x, coords[1] - y]
        return [local(part) for part in coords]

    for feature in features['features']:
        for key in ['geometry', 'nucleusGeometry']:
            if feature.get(key):
                feature[key]['coordinates'] = local(feature[key]['coordinates'])
    doc = q.Document(q.Image.from_numpy(image))
    doc.set_calibration(*contract['pixel_size_um'])
    imported = q.import_geojson(doc, json.dumps(features), assume_pixels=True)
    assert not imported.issues and len(imported.imported_ids) == 506
    assert all(obj.kind == q.ObjectKind.CELL for obj in doc.annotations)
    project = q.Project()
    image_id = project.add_document(doc, 'Imported NDPI reference cells, local crop')
    path = output / 'input.qpxproj'
    project.save(str(path))
    receipt = dict(source_revision=q.build_environment().git_revision,
                   original_roi=contract['roi'], pixel_size_um=contract['pixel_size_um'],
                   translation=[-x, -y], imported_cells=506, image_id=image_id,
                   used_capture_files={name: manifest['files'][name] for name in ['contract.json', 'input.png', 'cells.geojson']},
                   scope='Existing reference geometry and crop; no new segmentation or biological acceptance')
    (output / 'scene.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    return path, image_id


def event(frame, kind, target=q.GuiInputTarget.CANVAS, wait=q.GuiInputWait.IMMEDIATE):
    value = q.GuiInput()
    value.frame, value.kind, value.target, value.wait_for = frame, kind, target, wait
    return value


def click(frame, target):
    return [event(frame, q.GuiInputKind.LEFT_DOWN, target, q.GuiInputWait.TARGET_READY),
            event(frame + 1, q.GuiInputKind.LEFT_UP, target)]


def run(command):
    completed = subprocess.run([str(part) for part in command], capture_output=True, text=True, timeout=60)
    assert completed.returncode == 0, completed.stdout + completed.stderr
    return completed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--input-project', type=Path)
    parser.add_argument('--image', type=int, default=1)
    parser.add_argument('--expected', type=Path, help='reuse a passing count oracle receipt for a metadata-only final build')
    parser.add_argument('--gui', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, image = (args.input_project.resolve(), args.image) if args.input_project else prepare(args.output)
    project = q.Project.open(str(source))
    doc = project.document(image)
    ids = [obj.id for obj in doc.annotations if obj.kind == q.ObjectKind.CELL]
    options = q.SpatialNeighborhoodOptions()
    options.radius, options.units = 10, q.SpatialDistanceUnits.MICROMETRES
    request = q.SpatialNeighborhoodRequest(doc, ids, ids, options)
    if args.expected:
        expected = json.loads(args.expected.read_text(encoding='utf-8'))
        assert expected['input_sha256'] == request.input_sha256
        wanted = expected['counts']
    else:
        # Existing centroid geometry authority is reused. This exhaustive count
        # calculation is independent of the native radius-index traversal.
        points = [request.source_object(i).centroid for i in range(request.source_count)]
        wanted = [sum(j != i and math.hypot((a.x - b.x) * request.x_scale,
                                         (a.y - b.y) * request.y_scale) <= options.radius
                      for j, b in enumerate(points)) for i, a in enumerate(points)]
    sdk = args.output / 'sdk'
    command = [sys.executable, '-I', '-B', ROOT / 'examples/neighborhood_workflow.py', source, sdk,
               '--image', str(image), '--radius', '10', '--units', 'um']
    for obj in ids:
        command += ['--source', str(obj), '--target', str(obj)]
    run(command)
    summary = json.loads((sdk / 'workflow.json').read_text(encoding='utf-8'))
    with (sdk / 'neighborhoods.csv').open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    assert [int(row['source_id']) for row in rows] == ids
    assert [int(row['count']) for row in rows] == wanted
    assert summary['input_sha256'] == request.input_sha256
    summary.update(counts=wanted, oracle='reused passing receipt' if args.expected else 'independent exhaustive count over captured centroids',
                   actual_imported_cells=len(ids), source_project=str(source))
    ids_file = args.output / 'ids.txt'
    ids_file.write_text('\n'.join(map(str, ids)), encoding='utf-8')
    package = Path(q.__file__).parent
    cli = package / ('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    cli_output = args.output / 'cli.qpxproj'
    measured = run([cli, '--measure-spatial', source, cli_output, '--image', image,
                    '--source-ids', ids_file, '--target-ids', ids_file,
                    '--operation', 'radius-count', '--radius', '10', '--units', 'um'])
    cli_report = json.loads(measured.stdout)
    assert cli_report['result_sha256'] == summary['result_sha256']
    run([cli, '--verify-spatial', cli_output, '--image', image, '--record', cli_report['record_id'], '--operation', 'radius-count'])
    summary['cli_verified'] = True
    if args.gui:
        desktop = q.Project.open(str(source))
        path = args.output / 'desktop.qpxproj'
        desktop.save(str(path))
        gui = q.GuiOptions()
        gui.project, gui.width, gui.height = desktop, 1280, 1000
        gui.autosave_interval_seconds, gui.replay_timeout_seconds = 0, 20
        spatial = q.GuiSpatialOptions()
        spatial.open, spatial.sources, spatial.targets = True, ids, ids
        spatial.operation, spatial.radius = q.SpatialOperation.RADIUS_COUNT, 10
        spatial.analysis = options
        spatial.csv_path = str(args.output / 'desktop.csv')
        gui.spatial = spatial
        scroll = event(48, q.GuiInputKind.WHEEL, q.GuiInputTarget.SPATIAL_EXPORT)
        scroll.wheel = -4
        gui.inputs = [*click(4, q.GuiInputTarget.SPATIAL_RUN),
                      event(12, q.GuiInputKind.MOVE, wait=q.GuiInputWait.SPATIAL_READY),
                      *click(16, q.GuiInputTarget.SPATIAL_KEEP),
                      event(20, q.GuiInputKind.MOVE, wait=q.GuiInputWait.SPATIAL_KEPT),
                      *click(24, q.GuiInputTarget.SPATIAL_VERIFY),
                      event(28, q.GuiInputKind.MOVE, wait=q.GuiInputWait.SPATIAL_VERIFIED),
                      *click(32, q.GuiInputTarget.SPATIAL_EXPORT),
                      event(40, q.GuiInputKind.SAVE_KEY, wait=q.GuiInputWait.SPATIAL_EXPORTED), scroll]
        report = q.show(desktop.document(image), frame_limit=72, capture_path=str(args.output / 'desktop.ppm'), options=gui)
        state = report.spatial
        summary['gui'] = dict(replay_complete=report.replay_complete, state=state.state, error=state.error,
                              record_status=state.record_status, rows=state.rows, export_state=state.export_state,
                              export_error=state.export_error, project_error=report.project_error,
                              ui_overflow_items=report.ui_overflow_items)
        assert report.replay_complete and state.state == 'verified' and not state.error
        assert state.neighborhood_result and state.result is None and state.rows == len(ids)
        assert state.export_state == 'saved' and not state.export_error and not report.project_error
        assert (args.output / 'desktop.csv').read_bytes() == (sdk / 'neighborhoods.csv').read_bytes()
        reopened = q.Project.open(str(path)).document(image)
        assert reopened.neighborhood_record(state.record_id).result.sha256 == summary['result_sha256']
        app = package / ('qupathxx_app.exe' if os.name == 'nt' else 'qupathxx_app')
        run([app, '--project', sdk / 'study.qpxproj', '--smoke', '--compact'])
        summary['standalone_reopen'] = True
    (args.output / 'workflow.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in summary.items() if key != 'counts'}, indent=2))


if __name__ == '__main__':
    main()
