"""One staged native-runtime handoff; no downloads or pixel/biology benchmark."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('work', type=Path)
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--legacy', action='store_true')
    parser.add_argument('--desktop', action='store_true')
    args = parser.parse_args()
    if args.runtime:
        sys.path.insert(0, str(args.runtime.resolve()))
    import qupathxx as q
    work = args.work.resolve()
    work.mkdir(exist_ok=True)
    if args.legacy:
        doc = q.Document(q.Image.demo())
        a = doc.add_detection(q.Roi.points([q.Point(10, 10)]))
        b = doc.add_detection(q.Roi.points([q.Point(13, 14)]))
        request = q.SpatialDistanceRequest(doc, [a], [b])
        result = q.measure_spatial_distances(request)
        record = doc.keep_spatial(request, result)
        project = q.Project()
        image = project.add_document(doc, 'Frozen QSD1')
        path = work / 'legacy.qpxproj'
        project.save_new(str(path))
        (work / 'legacy.json').write_text(json.dumps(dict(image=image, record=record,
            result_sha256=result.sha256, csv=q.spatial_distance_csv(result),
            project_sha256=sha(path), source=result.provenance.environment.source_tree_sha256), indent=2), encoding='utf-8')
        return
    if args.desktop:
        project = q.Project.open(str(work / 'source.qpxproj'))
        config = q.GuiSpatialOptions()
        config.open = config.run = True
        config.sources = [2, 3, 4]
        config.targets = [1]
        options = q.SpatialDistanceOptions()
        options.metric = q.SpatialDistanceMetric.SIGNED_BOUNDARY
        options.units = q.SpatialDistanceUnits.MICROMETRES
        config.analysis = options
        config.csv_path = str(work / 'desktop.csv')
        gui = q.GuiOptions()
        gui.spatial = config
        gui.docked_workspace = False
        report = q.show(project, frame_limit=90, options=gui)
        assert report.spatial.state == 'ready' and report.spatial.result is not None
        assert [report.spatial.result.row(i).distance for i in range(3)] == [-5, 10, 0]
        (work / 'desktop-report.json').write_text(json.dumps(dict(
            state=report.spatial.state, error=report.spatial.error,
            record=report.spatial.record_id,
            export_state=report.spatial.export_state,
            values=[report.spatial.result.row(i).distance for i in range(3)],
            algorithm=report.spatial.result.algorithm), indent=2), encoding='utf-8')
        return
    legacy = json.loads((work / 'legacy.json').read_text(encoding='utf-8'))
    old = q.Project.open(str(work / 'legacy.qpxproj'))
    old_doc = old.document(legacy['image'])
    old_record = old_doc.spatial_record(legacy['record'])
    assert sha(work / 'legacy.qpxproj') == legacy['project_sha256']
    assert old_record.result.sha256 == legacy['result_sha256']
    assert q.spatial_distance_csv(old_record.result) == legacy['csv']
    replay = q.SpatialDistanceRequest.replay(old_doc, legacy['record'])
    assert old_doc.verify_spatial(legacy['record'], replay, q.measure_spatial_distances(replay))
    doc = q.Document(q.Image.demo())
    region = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Target annotation')
    cells = [doc.add_cell(q.Roi.rectangle(q.Rect(x-.5, y-.5, 1, 1)), name=name)
             for x, y, name in [(15, 20, 'Inside'), (35, 20, 'Outside'), (10, 20, 'Boundary')]]
    doc.set_calibration(2, .5)
    project = q.Project()
    image = project.add_document(doc, 'Signed boundary demonstration')
    project.save_new(str(work / 'source.qpxproj'))
    source_sha = sha(work / 'source.qpxproj')
    options = q.SpatialDistanceOptions()
    options.metric = q.SpatialDistanceMetric.SIGNED_BOUNDARY
    options.units = q.SpatialDistanceUnits.MICROMETRES
    request = q.SpatialDistanceRequest(doc, cells, [region], options)
    job = q.SpatialDistanceJob()
    assert job.request(request) and job.wait(doc).state == 'ready'
    result = job.result
    assert [result.row(i).distance for i in range(3)] == [-5, 10, 0]
    assert result.options.metric == q.SpatialDistanceMetric.SIGNED_BOUNDARY
    csv_text = q.spatial_distance_csv(result)
    table = list(csv.DictReader(io.StringIO(csv_text)))
    assert [float(row['distance']) for row in table] == [-5, 10, 0]
    assert all(row['geometry_policy'] == 'polygon-boundary/1' for row in table)
    record = doc.keep_spatial(request, result, 'Signed distances')
    project.save_new(str(work / 'signed.qpxproj'))
    reopened = q.Project.open(str(work / 'signed.qpxproj'))
    restored = reopened.document(image)
    retained = restored.spatial_record(record)
    assert retained.result.sha256 == result.sha256
    assert q.spatial_distance_csv(retained.result) == csv_text
    replay = q.SpatialDistanceRequest.replay(restored, record)
    assert restored.verify_spatial(record, replay, q.measure_spatial_distances(replay))
    q.save_spatial_distance_csv(retained.result, str(work / 'python.csv'))
    try:
        q.save_spatial_distance_csv(retained.result, str(work / 'python.csv'))
    except (RuntimeError, ValueError):
        pass
    else:
        raise AssertionError('CSV must not replace an existing output')
    native_dir = Path(q.__file__).parent
    batch = native_dir / 'qupathxx_batch.exe'
    cli_measure = [str(batch), '--measure-spatial', str(work / 'source.qpxproj'),
        str(work / 'cli.qpxproj'), '--image', str(image), '--operation', 'signed-boundary', '--units', 'um']
    for cell in cells:
        cli_measure += ['--source', str(cell)]
    cli_measure += ['--target', str(region)]
    measured = json.loads(subprocess.check_output(cli_measure, text=True, encoding='utf-8'))
    verified = json.loads(subprocess.check_output([str(batch), '--verify-spatial', str(work / 'cli.qpxproj'),
        '--image', str(image), '--record', str(measured['record_id'])], text=True, encoding='utf-8'))
    subprocess.run([str(batch), '--export-spatial', str(work / 'cli.qpxproj'), str(work / 'cli.csv'),
        '--image', str(image), '--record', str(measured['record_id'])], check=True, capture_output=True)
    cli_rows = list(csv.DictReader(io.StringIO((work / 'cli.csv').read_text(encoding='utf-8'))))
    assert [float(row['distance']) for row in cli_rows] == [-5, 10, 0]
    assert verified['state'] == 'verified' and sha(work / 'source.qpxproj') == source_sha
    inputs = sorted([p for directory, suffixes in [('cpp', ('.cpp', '.hpp')),
        ('cmake', ('.cmake', '.in')), ('src', ('.py',))]
        for p in (ROOT / directory).rglob('*') if p.is_file() and p.suffix in suffixes] +
        [ROOT / 'CMakeLists.txt', ROOT / 'pyproject.toml'], key=lambda p: p.relative_to(ROOT).as_posix())
    manifest = ''.join(p.relative_to(ROOT).as_posix() + '=' + sha(p) + '\n' for p in inputs).encode('utf-8')
    environment = result.provenance.environment
    assert hashlib.sha256(manifest).hexdigest() == environment.source_tree_sha256
    (work / 'source-inputs.sha256').write_bytes(manifest)
    receipt = dict(values=[-5, 10, 0], units='um', image=image, record=record,
        source_sha256=source_sha, result_sha256=result.sha256, input_sha256=result.input_sha256,
        source_tree_sha256=environment.source_tree_sha256, compiler_inputs=len(inputs),
        runtime=str(native_dir), compiler=environment.compiler, configuration=environment.configuration,
        geos_version=environment.geos_version, legacy_source=legacy['source'],
        legacy_exact=True, cli=measured, verified=verified,
        members={str(p.relative_to(native_dir)): sha(p) for p in sorted(native_dir.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts})
    (work / 'workflow.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print(json.dumps({key: receipt[key] for key in ['values', 'source_tree_sha256', 'compiler_inputs', 'legacy_exact']}))


if __name__ == '__main__':
    main()
