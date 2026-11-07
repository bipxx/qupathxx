"""Exact-class selection over the unchanged installed density authority."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

import pytest
import qupathxx as q

SCRIPT = Path(__file__).resolve().parents[1] / 'examples/density_manifest.py'
spec = importlib.util.spec_from_file_location('density_manifest', SCRIPT)
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)


@pytest.fixture
def scene(tmp_path):
    project = q.Project()
    for index in range(2):
        doc = q.Document(q.Image(24, 24, [100] * 24 * 24 * 3))
        doc.set_calibration(2, .5)
        for name in ('Window', 'Tissue', 'Excluded', '肿瘤: A', 'Other'):
            doc.set_class(name, 0x008800)
        for name, rect in [('Window', (0, 0, 12, 12)),
                           ('Tissue', (0, 0, 10, 10)), ('Excluded', (4, 0, 2, 10))]:
            obj = doc.add_rectangle(q.Rect(*rect))
            doc.classify_annotation(obj, name)
        for x in (2, 5, 8):
            obj = doc.add_cell(q.Roi.rectangle(q.Rect(x - 1, x - 1, 2, 2)))
            doc.classify_annotation(obj, '肿瘤: A')
        obj = doc.add_cell(q.Roi.rectangle(q.Rect(1, 1, 1, 1)))
        doc.classify_annotation(obj, 'Other')
        project.add_document(doc, f'Image {index}')
    path = tmp_path / 'study.qpxproj'
    project.save(str(path))
    return project, path


def generate(path, **kwargs):
    defaults = dict(window_classes=['Window'], tissue_classes=['Tissue'],
                    exclusion_classes=['Excluded'], cell_classes=['肿瘤: A'])
    defaults.update(kwargs)
    return selector.generate(path, **defaults)


def test_preview_and_roundtrip_to_runner(scene, tmp_path, monkeypatch):
    project, path = scene
    before = path.read_bytes()
    def forbidden(*args):
        raise AssertionError('preview must not run density')
    with monkeypatch.context() as patch:
        patch.setattr(q, 'SpatialDensityJob', forbidden)
        patch.setattr(q, 'measure_spatial_density', forbidden)
        report = generate(path)
    assert report['valid'] and not report['written']
    assert [i['counts'] for i in report['images']] == [dict(windows=1, tissue=1, excluded=1, cells=3)] * 2
    assert report['images'][0]['calibration_um'] == [2, .5]
    output = tmp_path / 'manifest.json'
    written = generate(path, output=output)
    assert written['written'] and json.loads(output.read_text()) == report['manifest']
    _, loaded_project, entries, _ = selector.runner.load_manifest(output)
    assert loaded_project == path and len(entries) == 2
    result = selector.runner.run(output, tmp_path / 'run')
    assert result['state'] == 'completed' and result['rows'] == 2
    import csv
    with Path(result['csv_path']).open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert [int(r['count']) for r in rows] == [2, 2]
    assert path.read_bytes() == before and not project.dirty


def test_image_selection_and_explicit_all_cells(scene):
    _, path = scene
    report = generate(path, image_ids=[2], cell_classes=[], all_cells=True,
                      exclusion_classes=[])
    assert report['valid'] and len(report['images']) == 1
    assert report['images'][0]['image_id'] == 2
    assert report['images'][0]['counts']['cells'] == 4
    assert report['manifest']['images'][0]['excluded'] == []


def test_repeated_classes_and_policy_options(scene):
    report = generate(scene[1], cell_classes=['Other', '肿瘤: A', 'Other'],
                      prefer_nucleus=False, polygonize_curves=True)
    assert report['valid'] and report['images'][0]['counts']['cells'] == 4
    assert report['manifest']['options'] == dict(prefer_nucleus=False, polygonize_curves=True)
    assert report['manifest']['images'][0]['cells'] == [4, 5, 6, 7]


@pytest.mark.parametrize('changes', [dict(image_ids=[3]), dict(image_ids=[1, 1]),
    dict(image_ids=[True]), dict(window_classes=[]), dict(cell_classes=[]),
    dict(all_cells=True), dict(window_classes='Window'), dict(prefer_nucleus=1)])
def test_bad_selectors(scene, changes):
    with pytest.raises(ValueError):
        generate(scene[1], **changes)


@pytest.mark.parametrize('changes', [dict(window_classes=['window']),
    dict(window_classes=['Window', 'Missing']), dict(cell_classes=['肿瘤']),
    dict(tissue_classes=['Tissue child'])])
def test_exact_names_and_missing_classes_block_publication(scene, tmp_path, changes):
    output = tmp_path / 'blocked.json'
    report = generate(scene[1], output=output, **changes)
    assert not report['valid'] and all(i['issues'] for i in report['images'])
    assert not output.exists()


@pytest.mark.parametrize('problem', ['calibration', 'cell_kind', 'curve'])
def test_native_capture_errors_are_visible(scene, tmp_path, problem):
    project, path = scene
    doc = project.document(2)
    if problem == 'calibration':
        doc.clear_calibration()
    elif problem == 'cell_kind':
        doc.classify_annotation(1, '肿瘤: A')
    else:
        obj = doc.add_roi(q.Roi.ellipse(q.Rect(0, 0, 10, 10)))
        doc.classify_annotation(obj, 'Window')
    project.save(str(path))
    output = tmp_path / 'blocked.json'
    report = generate(path, output=output)
    assert not report['valid'] and not report['images'][0]['issues']
    assert report['images'][1]['issues'] and not output.exists()
    if problem == 'curve':
        assert generate(path, polygonize_curves=True)['valid']


def test_zero_cells_with_explicit_all_cells(scene):
    project, path = scene
    doc = project.document(1)
    # Existing deletion API retains the class definitions, but no target objects.
    for obj in doc.annotations:
        if obj.kind == q.ObjectKind.CELL:
            doc.remove_annotation(obj.id)
    project.save(str(path))
    report = generate(path, cell_classes=[], all_cells=True)
    assert report['valid'] and report['images'][0]['counts']['cells'] == 0
    assert not generate(path)['valid']


def test_existing_output_and_scratch_are_preserved(scene, tmp_path):
    path = scene[1]
    with pytest.raises(FileExistsError):
        generate(path, output=path)
    output = tmp_path / 'manifest.json'
    scratch = tmp_path / '.manifest.json.part'
    scratch.write_bytes(b'preserve')
    with pytest.raises(FileExistsError):
        generate(path, output=output)
    assert scratch.read_bytes() == b'preserve' and not output.exists()


def test_changed_source_blocks_publication(scene, tmp_path, monkeypatch):
    path = scene[1]
    original = q.SpatialDensityRequest
    def change(*args):
        request = original(*args)
        with path.open('ab') as stream:
            stream.write(b'changed')
        return request
    monkeypatch.setattr(q, 'SpatialDensityRequest', change)
    output = tmp_path / 'blocked.json'
    with pytest.raises(selector.runner.InputChanged):
        generate(path, output=output)
    assert not output.exists()


def test_isolated_cli_preview_and_generation(scene, tmp_path):
    path = scene[1]
    command = [sys.executable, '-I', str(SCRIPT), str(path), '--window-class',
               'Window', '--tissue-class', 'Tissue', '--all-cells']
    preview = subprocess.run(command, capture_output=True, text=True, check=True)
    assert json.loads(preview.stdout)['valid']
    output = tmp_path / 'manifest.json'
    written = subprocess.run(command + ['--output', str(output)], capture_output=True,
                             text=True, check=True)
    assert json.loads(written.stdout)['written'] and output.is_file()
    invalid = subprocess.run(command + ['--image', '9'], capture_output=True, text=True)
    assert invalid.returncode == 2 and 'unknown image' in json.loads(invalid.stdout)['error']
