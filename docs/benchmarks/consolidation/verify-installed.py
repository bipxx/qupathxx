"""Selected native CLI/thin installed Python agreement; independent development checks."""
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]
BUILD = ROOT.parents[1]


def load(name):
    path = ROOT / 'examples' / (name + '.py')
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(report):
    with Path(report['csv_path']).open(encoding='utf-8', newline='') as stream:
        return list(csv.DictReader(stream))


def main():
    output = Path(sys.argv[1]).resolve()
    resume = len(sys.argv) > 2 and sys.argv[2] == '--resume-checks'
    output.mkdir(exist_ok=resume)
    executable = Path(q.__file__).parent / 'qupathxx_batch.exe'
    calls = []
    def native(*args):
        if resume and args[0] == '--project-density':
            report = Path(args[2]) / 'report.json'
            if report.is_file():
                calls.append(dict(args=list(map(str, args)), retained_report=str(report)))
                return json.loads(report.read_text(encoding='utf-8'))
        if resume and args[0] == '--import-nucls' and '--output' in args:
            path = Path(args[args.index('--output') + 1])
            if path.is_file():
                calls.append(dict(args=list(map(str, args)), retained_project=str(path)))
                return dict(written=True)
        completed = subprocess.run([str(executable), *map(str, args)], capture_output=True,
                                   text=True, encoding='utf-8', timeout=60)
        calls.append(dict(args=list(map(str, args)), exit=completed.returncode))
        assert completed.returncode == 0, completed.stdout + completed.stderr
        return json.loads(completed.stdout)
    density, importer = load('project_density'), load('nucls_import')
    manifest = BUILD / 'project-density-runner/workflow/selections.json'
    source = manifest.parent / 'study.qpxproj'
    source_hashes = {str(p): sha(p) for p in (manifest, source)}
    cpp = native('--project-density', manifest, output / 'native-density')
    python = (json.loads((output / 'python-density/report.json').read_text(encoding='utf-8'))
              if resume else density.run(manifest, output / 'python-density'))
    cpp_rows, python_rows = rows(cpp), rows(python)
    assert len(cpp_rows) == len(python_rows) == 8
    assert [int(row['count']) for row in cpp_rows] == [289, 121, 168, 0] * 2
    assert [float(row['area_pixels']) for row in cpp_rows] == [40576, 17536, 23040, 0] * 2
    assert [{k: v for k, v in row.items() if k != 'checkpoint'} for row in cpp_rows] == [
        {k: v for k, v in row.items() if k != 'checkpoint'} for row in python_rows]
    reused_python = (json.loads((output / 'python-resume/report.json').read_text(encoding='utf-8'))
                     if resume else density.run(manifest, output / 'python-resume', resume_from=output / 'native-density'))
    reused_cpp = native('--project-density', manifest, output / 'native-resume', '--resume-from', output / 'python-density')
    assert reused_cpp['reused'] == reused_python['reused'] == 2
    assert reused_cpp['computed'] == reused_python['computed'] == 0
    assert not list((output / 'native-resume').glob('*.qpxproj'))
    assert not list((output / 'python-resume').glob('*.qpxproj'))
    for entry in cpp['images']:
        checkpoint = q.Project.open_without_journal_recovery(entry['checkpoint'])
        doc = checkpoint.document(entry['checkpoint_image_id'])
        request = q.SpatialDensityRequest.replay(doc, entry['record_id'])
        assert doc.verify_density(entry['record_id'], request, q.measure_spatial_density(request))
    sources = json.loads((ROOT / 'docs/benchmarks/research-density-pilot/sources.json').read_text(encoding='utf-8'))
    fixtures = BUILD / 'research-density-pilot/nucls'
    for item in sources['files']:
        path = fixtures / item['name']
        assert sha(path) == item['sha256']
        source_hashes[str(path)] = item['sha256']
    names = [case['name'] for case in sources['cases']]
    args = ['--import-nucls', fixtures, fixtures, '--mpp-x', '.2', '--mpp-y', '.2', '--representation', 'centroids']
    for name in names:
        args += ['--name', name]
    cpp_preview = native(*args)
    python_preview = importer.run(fixtures, fixtures, mpp_x=.2, mpp_y=.2, representation='centroids', names=names)
    assert cpp_preview == python_preview
    assert [row['imported'] for row in cpp_preview['images']] == [32, 28]
    native(*args, '--output', output / 'native-import.qpxproj')
    if not resume:
        importer.run(fixtures, fixtures, mpp_x=.2, mpp_y=.2, representation='centroids', names=names,
                     output=output / 'python-import.qpxproj')
    original = q.Project.open_without_journal_recovery(str(BUILD / 'nucls-import/workflow/imported.qpxproj'))
    baseline = []
    for project_path in (output / 'native-import.qpxproj', output / 'python-import.qpxproj'):
        project = q.Project.open_without_journal_recovery(str(project_path))
        observed = []
        for image, old_image in zip(project.images, original.images, strict=True):
            doc, old = project.document(image.id), original.document(old_image.id)
            assert image.name == old_image.name and doc.image.numpy().tobytes() == old.image.numpy().tobytes()
            assert doc.pixel_width_um == old.pixel_width_um == .2
            assert doc.pixel_height_um == old.pixel_height_um == .2
            for obj, old_obj in zip(doc.annotations, old.annotations, strict=True):
                assert obj.id == old_obj.id and obj.revision == old_obj.revision
                assert obj.class_name == old_obj.class_name and obj.geometry.wkt == old_obj.geometry.wkt
                assert obj.nucleus.wkt == old_obj.nucleus.wkt
                assert obj.properties.metadata == old_obj.properties.metadata
            observed.append(dict(name=image.name, objects=len(doc.annotations), rgb_sha256=sha_bytes(doc.image.numpy().tobytes())))
        baseline.append(observed)
    assert baseline[0] == baseline[1]
    assert all(sha(Path(path)) == value for path, value in source_hashes.items())
    receipt = dict(source_tree=q.build_environment().source_tree_sha256, module=str(q.__file__),
                   density_rows=8, density_counts_areas_exact=True, density_csv_equal=True,
                   bidirectional_resume_reuses=2, checkpoint_replay=True,
                   nucls_objects=[32, 28], nucls_preview_equal=True,
                   nucls_pixels_geometry_calibration_properties_equal_legacy=True,
                   source_hashes=source_hashes, native_calls=calls, binary_release=False)
    (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(receipt, indent=2))


def sha_bytes(value):
    return hashlib.sha256(value).hexdigest()


if __name__ == '__main__':
    main()
