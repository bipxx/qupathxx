"""NuCLS boundary/representation checks against installed candidate4."""
import csv
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

import pytest
from PIL import Image
import qupathxx as q

SCRIPT = Path(__file__).resolve().parents[1] / 'examples/nucls_import.py'
spec = importlib.util.spec_from_file_location('nucls_import', SCRIPT)
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)

HEADER = ['raw_classification', 'main_classification', 'super_classification',
          'type', 'xmin', 'ymin', 'xmax', 'ymax', 'coords_x', 'coords_y']


def rows():
    return [dict(zip(HEADER, ['tumor', '肿瘤', 'tumor_any', 'rectangle', '1', '2', '5', '6', '1,5,5,1', '2,2,6,6'])),
            dict(zip(HEADER, ['lymphocyte', 'Lymphocyte', 'immune', 'polyline', '2', '1', '8', '7', '2,8,2,2', '1,1,7,1']))]


def write_csv(path, data, header=HEADER):
    with path.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=header)
        writer.writeheader()
        writer.writerows(data)


@pytest.fixture
def scene(tmp_path):
    images, annotations = tmp_path / 'rgb', tmp_path / 'csv'
    images.mkdir(); annotations.mkdir()
    for name in ('Case A', 'Case B'):
        Image.new('RGB', (12, 10), (30, 60, 90)).save(images / (name + '.png'))
        write_csv(annotations / (name + '.csv'), rows())
    return images, annotations


def run(scene, **kwargs):
    options = dict(mpp_x=.4, mpp_y=.7, representation='outlines')
    options.update(kwargs)
    return importer.run(*scene, **options)


@pytest.mark.parametrize('representation', ['outlines', 'centroids'])
def test_preview_save_reopen_and_metadata(scene, tmp_path, representation):
    before = {p: p.read_bytes() for folder in scene for p in folder.iterdir()}
    report = run(scene, representation=representation)
    assert report['valid'] and not report['written'] and len(report['images']) == 2
    assert report['images'][0]['classes'] == {'肿瘤': 1, 'Lymphocyte': 1}
    output = tmp_path / 'new.qpxproj'
    assert run(scene, representation=representation, output=output)['written']
    project = q.Project.open_without_journal_recovery(str(output))
    assert [i.name for i in project.images] == ['Case A', 'Case B']
    for image in project.images:
        doc = project.document(image.id)
        assert (doc.pixel_width_um, doc.pixel_height_um) == (.4, .7)
        objects = doc.annotations
        assert [o.class_name for o in objects] == ['肿瘤', 'Lymphocyte']
        assert all(o.kind == q.ObjectKind.CELL and o.nucleus is not None for o in objects)
        metadata = objects[0].properties.metadata
        assert metadata['raw_classification'] == 'tumor' and metadata['source_row'] == '1'
        assert metadata['representation'] == representation
        assert project.image_metadata(image.id)['image_sha256'] == report['images'][image.id - 1]['image_sha256']
        capture = q.SpatialDistanceRequest(doc, [o.id for o in objects], [o.id for o in objects])
        assert (capture.source_object(0).centroid.x, capture.source_object(0).centroid.y) == (3, 4)
        assert capture.source_object(1).centroid.x == pytest.approx(4)
        assert capture.source_object(1).centroid.y == pytest.approx(3)
        assert objects[0].geometry.area == (16 if representation == 'outlines' else 1)
        assert objects[1].geometry.area == (18 if representation == 'outlines' else 1)
    assert all(p.read_bytes() == data for p, data in before.items())


def test_class_column_and_subset(scene):
    report = run(scene, names=['Case B'], class_column='super_classification')
    assert report['valid'] and len(report['images']) == 1
    assert report['images'][0]['classes'] == {'tumor_any': 1, 'immune': 1}


def test_missing_pairs_and_exact_stems(scene, tmp_path):
    (scene[1] / 'Case B.csv').unlink()
    Image.new('RGB', (1, 1)).save(scene[0] / 'orphan.png')
    output = tmp_path / 'blocked.qpxproj'
    report = run(scene, output=output)
    assert not report['valid'] and len(report['images']) == 3 and not output.exists()
    assert sum(i['issue_count'] for i in report['images']) == 2
    assert run(scene, names=['Case A'])['valid']
    assert not run(scene, names=['case a'])['valid']


@pytest.mark.parametrize('options', [dict(mpp_x=0), dict(mpp_y=float('nan')),
    dict(mpp_x=True), dict(representation='auto'), dict(class_column='unknown'),
    dict(names=['Case A', 'Case A']), dict(names=['../escape']), dict(names=[])])
def test_invalid_options(scene, options):
    with pytest.raises(ValueError):
        run(scene, **options)


@pytest.mark.parametrize('field,value', [('xmin', 'nan'), ('xmax', '1'),
    ('type', 'ellipse'), ('main_classification', ''), ('main_classification', ' padded ')])
def test_bad_rows_are_reported_without_saving(scene, tmp_path, field, value):
    data = rows(); data[0][field] = value
    write_csv(scene[1] / 'Case B.csv', data)
    output = tmp_path / 'blocked.qpxproj'
    report = run(scene, output=output)
    assert not report['valid'] and not output.exists()
    assert report['images'][1]['issues'][0].startswith('annotation 1 (CSV line 2):')


def test_outside_outline_is_not_silently_clipped(scene, tmp_path):
    data = rows(); data[0]['xmax'] = '13'
    write_csv(scene[1] / 'Case A.csv', data)
    assert not run(scene)['valid']
    output = tmp_path / 'markers.qpxproj'
    report = run(scene, representation='centroids', output=output)
    assert report['written'] and report['images'][0]['source_bounds_outside'] == 1
    doc = q.Project.open(str(output)).document(1)
    assert doc.annotations[0].roi.x == 6.5  # Original midpoint7, not clipped midpoint6.5.
    data[0].update(xmin='-.4', xmax='.6')
    write_csv(scene[1] / 'Case A.csv', data)
    assert not run(scene, representation='centroids')['valid']


@pytest.mark.parametrize('kind', ['self_intersection', 'mismatched_xy', 'wrong_bounds'])
def test_polygon_validation(scene, kind):
    data = rows()
    if kind == 'self_intersection':
        data[1].update(coords_x='2,8,2,8,2', coords_y='1,7,7,1,1')
    elif kind == 'mismatched_xy':
        data[1]['coords_y'] = '1,1,7'
    else:
        data[1]['xmax'] = '9'
    write_csv(scene[1] / 'Case A.csv', data)
    assert not run(scene, representation='centroids')['valid']


@pytest.mark.parametrize('problem', ['duplicate_header', 'row_width', 'non_rgb', 'wrong_format'])
def test_format_errors(scene, problem):
    path = scene[1] / 'Case A.csv'
    if problem == 'duplicate_header':
        path.write_text(','.join(HEADER + ['type']) + '\n', encoding='utf-8')
    elif problem == 'row_width':
        path.write_text(','.join(HEADER) + '\nshort,row\n', encoding='utf-8')
    elif problem == 'non_rgb':
        Image.new('L', (12, 10)).save(scene[0] / 'Case A.png')
    else:
        Image.new('RGB', (12, 10)).save(scene[0] / 'Case A.png', format='JPEG')
    assert not run(scene)['valid']


def test_existing_output_and_changed_input(scene, tmp_path, monkeypatch):
    output = tmp_path / 'existing.qpxproj'; output.write_bytes(b'preserve')
    with pytest.raises(FileExistsError):
        run(scene, output=output)
    assert output.read_bytes() == b'preserve'
    original = importer._geometry
    def mutate(row):
        value = original(row)
        with (scene[1] / 'Case A.csv').open('ab') as stream:
            stream.write(b'\n')
        return value
    monkeypatch.setattr(importer, '_geometry', mutate)
    output = tmp_path / 'blocked.qpxproj'
    with pytest.raises(importer.runner.InputChanged):
        run(scene, output=output)
    assert not output.exists()


def test_decoder_size_refusal_is_a_preview_issue(scene, monkeypatch):
    # Exercise Pillow's real header-level refusal without allocating a giant image.
    monkeypatch.setattr(Image, 'MAX_IMAGE_PIXELS', 1)
    report = run(scene)
    assert not report['valid'] and all(row['issue_count'] for row in report['images'])


def test_limits_and_bounded_diagnostics(scene, monkeypatch):
    with monkeypatch.context() as patch:
        patch.setattr(importer, 'MAX_PIXELS', 1)
        report = run(scene)
        assert not report['valid'] and report['inspected_pairs'] == 1
    with monkeypatch.context() as patch:
        patch.setattr(importer, 'MAX_VERTICES', 3)
        assert not run(scene)['valid']
    with monkeypatch.context() as patch:
        patch.setattr(importer, 'MAX_ROWS', 1)
        assert not run(scene)['valid']
    bad = rows()[0]; bad['xmin'] = 'nan'
    write_csv(scene[1] / 'Case A.csv', [bad] * 60)
    report = run(scene)
    assert report['images'][0]['issue_count'] == 60 and len(report['images'][0]['issues']) == 50


def test_isolated_cli(scene, tmp_path):
    command = [sys.executable, '-I', str(SCRIPT), *map(str, scene), '--mpp-x', '.4',
               '--mpp-y', '.7', '--representation', 'outlines']
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    assert json.loads(result.stdout)['valid'] and not json.loads(result.stdout)['written']
    output = tmp_path / 'cli.qpxproj'
    written = subprocess.run(command + ['--output', str(output)], capture_output=True, text=True, check=True)
    assert json.loads(written.stdout)['written']
    bad = subprocess.run(command + ['--name', 'missing'], capture_output=True, text=True)
    assert bad.returncode == 1 and not json.loads(bad.stdout)['valid']
