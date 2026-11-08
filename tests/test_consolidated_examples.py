"""Installed thin examples consume explicit inputs; independent checks stay here."""
import csv
import json
from pathlib import Path
import subprocess
import sys

import pytest
import qupathxx as q

ROOT = Path(__file__).resolve().parents[1]


def invoke(script, *args, optimized=False, expected=0):
    command = [sys.executable, '-I', *(['-O'] if optimized else []), str(ROOT / script), *map(str, args)]
    done = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)
    assert done.returncode == expected, done.stdout + done.stderr
    return done


@pytest.fixture
def saved_scene(tmp_path):
    doc = q.Document(q.Image(32, 24, [value for _ in range(24) for x in range(32)
                                 for value in ([220, 30, 10] if x < 16 else [10, 30, 220])]))
    doc.set_calibration(.5, .5)
    left = doc.add_rectangle(q.Rect(0, 0, 16, 24))
    right = doc.add_rectangle(q.Rect(16, 0, 16, 24))
    doc.classify_objects([left], 'red'); doc.classify_objects([right], 'blue')
    target = doc.add_rectangle(q.Rect(0, 0, 32, 24))
    cells = [doc.add_cell(q.Roi.rectangle(q.Rect(x, 2, 2, 2))) for x in (2, 5, 20)]
    project = q.Project(); image = project.add_document(doc, 'Explicit labelled scene')
    path = tmp_path / 'scene.qpxproj'; project.save_new(str(path))
    return path, image, left, right, target, cells


@pytest.mark.skipif(not q.has_classifier_training, reason='native training backend')
def test_pixel_controls_train_save_reopen_predict_and_refuse_overwrite(saved_scene, tmp_path):
    source, image, left, right, target, _ = saved_scene
    before = source.read_bytes(); model = tmp_path / 'pixel.qpc'
    args = [source, model, '--image', image, '--label', left, '--label', right, '--target', target]
    trained = json.loads(invoke('examples/pixel_workflow.py', *args, optimized=True).stdout)
    predicted = json.loads(invoke('examples/pixel_classifier.py', source, model, '--image', image, '--target', target).stdout)
    assert trained['model_sha256'] == predicted['model_sha256'] == q.load_pixel_classifier(str(model)).sha256
    assert trained['counts'] == predicted['counts'] and sum(predicted['counts']) == predicted['valid_pixels'] == 32 * 24
    assert trained['input_sha256'] == predicted['input_sha256'] and set(predicted['classes']) == {'red', 'blue'}
    original = model.read_bytes()
    invoke('examples/pixel_workflow.py', *args, expected=1)
    assert model.read_bytes() == original and source.read_bytes() == before


def test_neighborhood_control_and_independent_saved_csv_oracle(saved_scene, tmp_path):
    source, image, _, _, _, cells = saved_scene
    before = source.read_bytes(); output = tmp_path / 'neighbors'
    done = invoke('examples/neighborhood_workflow.py', source, output, '--image', image,
                  '--source', cells[0], '--target', cells[1], '--target', cells[2], '--radius', 4, '--units', 'px')
    report = json.loads(done.stdout)
    assert report['sources'] == 1 and report['targets'] == 2 and report['rows'] == 1
    oracle = json.loads(invoke('tools/validate_neighborhood_workflow.py', output).stdout)
    assert oracle == dict(csv_counts_equal=True, rows=1)
    with (output / 'neighborhoods.csv').open(encoding='utf-8', newline='') as stream:
        assert next(csv.DictReader(stream))['count'] == '1'
    assert source.read_bytes() == before


def test_explicit_density_control_keeps_native_exports_and_replay(saved_scene, tmp_path):
    source, image, left, _, target, cells = saved_scene
    before = source.read_bytes(); output = tmp_path / 'density'
    report = json.loads(invoke('examples/density_workflow.py', source, output, '--image', image,
                              '--window', target, '--tissue', left, '--cell', cells[0], '--cell', cells[1]).stdout)
    assert report['saved_replay'] and report['windows'] == 1 and report['cells'] == 2
    with (output / 'density.csv').open(encoding='utf-8', newline='') as stream:
        row = next(csv.DictReader(stream))
    assert row['count'] == '2' and float(row['area_pixels']) == 384
    assert source.read_bytes() == before


@pytest.mark.skipif(not q.has_classifier_training, reason='native training backend')
@pytest.mark.parametrize('script', ['tools/demo_pixel_classifier.py', 'tools/demo_pixel_workflow.py'])
def test_retained_synthetic_oracles_run_as_development_tools(script):
    assert invoke(script).stdout
