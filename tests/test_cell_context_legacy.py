"""Known-wrong old cell outputs are compatibility witnesses, not science targets."""
import hashlib
import json
from pathlib import Path

import pytest
import qupathxx as q

FIXTURE = Path(__file__).parent / "fixtures/cell-context-legacy-v1"
MANIFEST = json.loads((FIXTURE / "manifest.json").read_text(encoding="utf-8"))


@pytest.mark.parametrize("case", MANIFEST["cases"], ids=lambda case: case["file"])
def test_signed_boundary_legacy_recipe_replays_exactly(case, tmp_path):
    source = FIXTURE / case["file"]
    assert hashlib.sha256(source.read_bytes()).hexdigest() == case["sha256"]
    project = q.Project.open(str(source))
    doc = project.document(case["image_id"])
    result = doc.cell_record(case["record_id"]).result
    assert result.algorithm == case["algorithm"]
    assert result.nuclei.input_format == case["input_format"]
    assert result.nuclei.input_sha256 == case["input_sha256"]
    assert result.nuclei.samples == case["legacy_samples"]
    assert result.cell_count == case["legacy_cells"]
    assert list(result.nuclei.labels) == case["nucleus_labels"]
    assert list(result.cells.labels) == case["body_labels"]
    assert list(result.features[0].nucleus_rgb) == case["legacy_nucleus_rgb"]
    request = q.CellRequest.replay(doc, case["record_id"])
    replay = q.detect_cells(request)
    assert replay.nuclei.input_sha256 == case["input_sha256"]
    assert doc.verify_cells(case["record_id"], request, replay)
    saved = tmp_path / case["file"]
    project.save(str(saved))
    reopened = q.Project.open(str(saved)).document(case["image_id"])
    request = q.CellRequest.replay(reopened, case["record_id"])
    assert reopened.verify_cells(case["record_id"], request, q.detect_cells(request))
    assert hashlib.sha256(source.read_bytes()).hexdigest() == case["sha256"]


@pytest.mark.parametrize("context", [0, 2])
@pytest.mark.parametrize("step,origin,samples", [(1.6, -4, 20), (.4, -1, 380)])
def test_new_signed_boundary_has_no_padding_cells(context, step, origin, samples, tmp_path):
    doc = q.Document(q.Image(8, 8, [255] * (8 * 8 * 3)))
    obj = doc.add_rectangle(q.Rect(0, 0, 8, 8))
    transform = q.StainTransform([[.65, .7, .29], [.07, .99, .11]])
    parameters = q.GridCellParameters()
    parameters.threshold = .1
    parameters.sigma = .1
    parameters.min_nucleus_area = .1
    parameters.cell_expansion = 0
    descriptor = q.NominalCellSampling(q.RgbNominalRequest(origin, 0, 8-origin, 8, step), context, context)
    request = q.CellRequest(doc, obj, transform, descriptor, parameters)
    result = q.detect_cells(request)
    assert result.nuclei.input_format == "qpx.nominal-cell-rgb/2"
    assert result.nuclei.samples == samples and result.cell_count == 0
    record = doc.keep_cells(request, result)
    project = q.Project()
    image = project.add_document(doc)
    saved = tmp_path / "corrected.qpxproj"
    project.save(str(saved))
    reopened = q.Project.open(str(saved)).document(image)
    replay = q.CellRequest.replay(reopened, record)
    restored = q.detect_cells(replay)
    assert restored.nuclei.input_sha256 == result.nuclei.input_sha256
    assert reopened.verify_cells(record, replay, restored)


@pytest.mark.skipif(not q.has_geos or not q.has_project, reason="requires cell publication and project storage")
def test_classifier_mixed_legacy_and_corrected_origins_roundtrip(tmp_path):
    from test_object_classifier_archives import model_for

    manifest = json.loads((FIXTURE / "patterned-manifest.json").read_text())
    source = FIXTURE / manifest["file"]
    assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["sha256"]
    project = q.Project.open(str(source))
    doc = project.document(manifest["image_id"])
    legacy = doc.cell_record(manifest["record_id"]).result
    old_request = q.CellRequest.replay(doc, manifest["record_id"])
    assert doc.verify_cells(manifest["record_id"], old_request, q.detect_cells(old_request))
    old_ids = doc.publish_cell_record(manifest["record_id"])
    transform = q.StainTransform.from_normalized(legacy.nuclei.stains, legacy.nuclei.white)
    request = q.CellRequest(doc, manifest["object_id"], transform, legacy.nominal_sampling, legacy.grid_parameters)
    corrected = q.detect_cells(request)
    assert corrected.cell_count == legacy.cell_count == manifest["cell_count"]
    assert corrected.nuclei.input_format == "qpx.nominal-cell-rgb/2"
    assert legacy.nuclei.input_format == manifest["input_format"] == "qpx.nominal-cell-rgb/1"
    assert corrected.nuclei.input_sha256 != manifest["input_sha256"]
    record = doc.keep_cells(request, corrected)
    new_ids = doc.publish_cell_record(record)
    options = q.ObjectFeatureOptions()
    options.cell_rgb = True
    features = q.ObjectFeatureRequest(doc, old_ids + new_ids, options)
    table = q.extract_object_features(features)
    job = q.ObjectClassifierJob()
    assert job.predict(features, model_for(table.features))
    assert job.wait(doc).state == "ready"
    expected = [table.cell_origin(i).source.input_sha256 for i in range(len(old_ids + new_ids))]
    assert set(expected) == {manifest["input_sha256"], corrected.nuclei.input_sha256}
    job.apply(doc)
    saved = tmp_path / "mixed-classifier.qpxproj"
    project.save(str(saved))
    restored_project = q.Project.open(str(saved))
    reopened = restored_project.document(manifest["image_id"])
    archive = reopened.classifier_records[-1].archive
    assert [archive.table.cell_origin(i).source.input_sha256 for i in range(len(expected))] == expected
    digest = archive.sha256
    second = tmp_path / "mixed-again.qpxproj"
    restored_project.save(str(second))
    assert q.Project.open(str(second)).document(manifest["image_id"]).classifier_records[-1].archive.sha256 == digest
    assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["sha256"]
