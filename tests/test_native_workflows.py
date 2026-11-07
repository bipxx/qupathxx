"""Bindings own captures and expose the native workflow contract."""
import json
from pathlib import Path
import threading

import pytest
import qupathxx as q
from qupathxx._workflow_control import await_report


@pytest.fixture
def saved_scene(tmp_path):
    doc = q.Document(q.Image(16, 16, [80] * 16 * 16 * 3))
    doc.set_calibration(2, .5)
    for name in ('Window', 'Tissue'):
        doc.set_class(name, 0x008800)
    doc.classify_annotation(doc.add_rectangle(q.Rect(0, 0, 12, 12)), 'Window')
    doc.classify_annotation(doc.add_rectangle(q.Rect(0, 0, 10, 10)), 'Tissue')
    doc.add_cell(q.Roi.rectangle(q.Rect(1, 1, 2, 2)))
    project = q.Project()
    project.add_document(doc, 'Case A')
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    recipe = q.ProjectDensityRecipe()
    recipe.classes = (['Window'], ['Tissue'], [], [])
    recipe.all_images = True
    return source, recipe


def test_owned_submit_and_nested_result_copies(saved_scene, tmp_path):
    source, recipe = saved_scene
    job = q.ProjectDensityJob()
    assert job.generate_manifest(str(source), recipe, str(tmp_path / 'manifest.json'))
    recipe.classes = (['Changed'], [], [], [])
    report = await_report(job, result='manifest_result')
    assert report.written and report.preview.valid
    snapshot = report.preview.recipe
    snapshot.classes = (['Changed'], [], [], [])
    assert report.preview.recipe.classes[0] == ['Window']
    manifest = q.load_project_density_manifest(report.output)
    mutable = manifest.recipe
    mutable.selections = []
    assert len(manifest.recipe.selections) == 1
    assert job.preview(str(source), report.preview.recipe)
    await_report(job)
    assert report.written and report.preview.valid


def test_owner_error_and_cross_thread_cancel(saved_scene):
    source, recipe = saved_scene
    job = q.ProjectDensityJob()
    job.preview(str(source), recipe)
    errors = []
    def foreign():
        try:
            job.poll()
        except RuntimeError as error:
            errors.append(str(error))
        job.cancel()
    thread = threading.Thread(target=foreign)
    thread.start(); thread.join()
    job.finish()
    assert len(errors) == 1
    # A fast preview may complete before cancellation; either terminal result is valid.
    try:
        job.rethrow_error()
    except q.WorkflowCancelled:
        pass
    assert not job.busy


@pytest.mark.parametrize('field,value', [('mpp_x', True), ('mpp_y', '1')])
def test_calibration_conversion_is_strict(field, value):
    with pytest.raises(TypeError):
        setattr(q.NuclsImportOptions(), field, value)


def test_ids_limits_and_failure_types(saved_scene, tmp_path):
    source, recipe = saved_scene
    with pytest.raises(TypeError):
        recipe.images = [True]
    with pytest.raises(TypeError):
        q.ProjectDensityRunOptions().csv_bytes = True
    with pytest.raises(OSError):
        q.load_project_density_manifest(str(tmp_path / 'absent.json'))
    target = tmp_path / 'manifest.json'
    generated = q.generate_project_density_manifest(str(source), recipe, str(target))
    with pytest.raises(FileExistsError):
        q.generate_project_density_manifest(str(source), recipe, str(target))
    manifest = q.load_project_density_manifest(str(target))
    assert json.loads(generated.manifest_json)['images'][0]['image_id'] == 1
    options = q.ProjectDensityRunOptions()
    options.csv_bytes = 1
    job = q.ProjectDensityJob()
    job.run(manifest, str(tmp_path / 'too-small'), options)
    with pytest.raises(q.ProjectDensityCsvBudget):
        await_report(job)
    assert not (tmp_path / 'too-small').exists()
