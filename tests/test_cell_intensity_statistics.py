"""Retained sampled intensity statistics, independent NumPy moments and old replay."""
import csv
import gc
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q

from test_nominal_cell_python import FIXTURE

LEGACY = Path(__file__).parent / "fixtures/cell-intensity-statistics-v1"


def scene(mode="nominal"):
    rgb = np.frombuffer((FIXTURE / "source.rgb").read_bytes(), dtype=np.uint8).reshape(192, 256, 3)
    doc = q.Document(q.Image.from_numpy(rgb))
    doc.set_calibration(.5, .5)
    obj = doc.add_rectangle(q.Rect(0, 0, 256, 192))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    if mode in ("distance", "log", "bodies", "sampled"):
        algorithm = {"distance": q.CellAlgorithm.DISTANCE_WATERSHED, "log": q.CellAlgorithm.LOG_WATERSHED,
                     "bodies": q.CellAlgorithm.LOG_WATERSHED_BODIES, "sampled": q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES}[mode]
        p = q.CellParameters(algorithm)
        p.threshold, p.sigma, p.min_nucleus_pixels, p.max_nucleus_pixels = .4, 1, 16, 512
        p.contours.work_bytes = 1024 * 1024
        request = q.CellRequest(doc, obj, transform, p)
        values = rgb.astype(float)
        channels = np.maximum(0, -np.log10(np.maximum(values, 1) / 255)).astype(np.float32).astype(float)
    else:
        p = q.GridCellParameters()
        p.threshold, p.sigma, p.min_nucleus_area, p.max_nucleus_area = .4, 1, 16, 512
        p.contours.work_bytes = 1024 * 1024
        if mode == "area":
            descriptor = q.ScientificGrid(256, 192, q.ScientificGridAxis(0, 256, 171), q.ScientificGridAxis(0, 192, 128))
            values = q.read_scientific_rgb(doc.server, descriptor).numpy()
        else:
            coordinates = q.RgbNominalSampling.AFFINE_INVERSE if mode == "affine" else q.RgbNominalSampling.INTEGER_ORIGIN
            source = q.RgbNominalRequest(0, 0, 256, 192, 1.5, coordinates)
            descriptor = q.NominalCellSampling(source, 0, 0)
            values = q.read_rgb_nominal_region(doc.server, source).numpy()[..., :3].astype(float)
        request = q.CellRequest(doc, obj, transform, descriptor, p)
        channels = np.maximum(0, -np.log10(np.maximum(values, 1) / 255))
    result = q.detect_cells(request)
    return doc, request, result, values, channels


def assert_statistics(result, values, channels):
    assert result.intensity_statistics_format == "qpx.cell-intensity-statistics/1"
    for feature in result.features:
        statistics = result.intensity_statistics(feature.label)
        nucleus = result.nuclei.labels_numpy() == feature.label
        body = result.cells.labels_numpy() == feature.label
        for name, mask in [("nucleus", nucleus), ("cell", body), ("cytoplasm", body & ~nucleus)]:
            actual = getattr(statistics, name)
            if not mask.any():
                assert actual is None
                continue
            assert actual.samples == int(mask.sum())
            for space, source in [("rgb", values), ("channels", channels)]:
                selected = source[mask].astype(np.float64)
                measured = getattr(actual, space)
                np.testing.assert_allclose(measured.sum, selected.sum(axis=0), rtol=1e-11, atol=1e-12)
                np.testing.assert_allclose(measured.minimum, selected.min(axis=0), rtol=1e-12, atol=1e-12)
                np.testing.assert_allclose(measured.maximum, selected.max(axis=0), rtol=1e-12, atol=1e-12)
                if actual.samples < 2:
                    assert measured.sample_standard_deviation is None
                else:
                    np.testing.assert_allclose(measured.sample_standard_deviation, selected.std(axis=0, ddof=1), rtol=1e-10, atol=1e-12)


@pytest.mark.parametrize("mode", ["distance", "log", "bodies", "sampled", "area", "nominal", "affine"])
def test_retained_statistics_match_independent_moments_and_reopen(tmp_path, mode):
    doc, request, result, values, channels = scene(mode)
    assert result.cell_count > 0
    assert_statistics(result, values, channels)
    record = doc.keep_cells(request, result)
    held = result.intensity_statistics(1)
    original = list(held.nucleus.rgb.sum)
    returned = held.nucleus.rgb.sum
    returned[0] = -999
    assert list(result.intensity_statistics(1).nucleus.rgb.sum) == original
    with pytest.raises(AttributeError):
        held.nucleus.samples = 0
    with pytest.raises(IndexError):
        result.intensity_statistics(0)
    project = q.Project()
    image = project.add_document(doc)
    saved = tmp_path / "stats.qpxproj"
    project.save(str(saved))
    reopened = q.Project.open(str(saved)).document(image)
    retained = reopened.cell_record(record).result
    assert_statistics(retained, values, channels)
    replay = q.CellRequest.replay(reopened, record)
    assert reopened.verify_cells(record, replay, q.detect_cells(replay))
    doc.server.close()
    del doc, request, result, project, reopened, retained
    gc.collect()
    assert list(held.nucleus.rgb.sum) == original


def test_legacy_measurement_policy_remains_absent_and_exact(tmp_path):
    manifest = json.loads((LEGACY / "legacy-manifest.json").read_text())
    path = LEGACY / manifest["file"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == manifest["sha256"]
    project = q.Project.open(str(path))
    doc = project.document(manifest["image_id"])
    for saved in manifest["records"]:
        result = doc.cell_record(saved["record_id"]).result
        assert result.intensity_statistics_format is None
        if result.cell_count:
            assert result.intensity_statistics(1) is None
        replay = q.CellRequest.replay(doc, saved["record_id"])
        fresh = q.detect_cells(replay)
        assert fresh.intensity_statistics_format is None
        assert fresh.nuclei.input_sha256 == saved["input_sha256"]
        assert doc.verify_cells(saved["record_id"], replay, fresh)
    second = tmp_path / "old-again.qpxproj"
    project.save(str(second))
    assert all(record.result.intensity_statistics_format is None for record in q.Project.open(str(second)).document(manifest["image_id"]).cell_records)
    assert hashlib.sha256(path.read_bytes()).hexdigest() == manifest["sha256"]


def test_statistics_csv_and_cli_share_the_retained_values(tmp_path):
    doc, request, result, _, _ = scene()
    record = doc.keep_cells(request, result)
    options = q.CellTableOptions()
    options.include_intensity_statistics = True
    rows = list(csv.DictReader(io.StringIO(q.cell_measurements_csv(q.CellTableRequest(doc, [record], options)))))
    assert len(rows) == result.cell_count
    first = result.intensity_statistics(1)
    assert rows[0]["table_schema"] == "qpx.cell-table/13"
    assert rows[0]["intensity_statistics_format"] == result.intensity_statistics_format
    assert float(rows[0]["nucleus_minimum_r"]) == first.nucleus.rgb.minimum[0]
    assert float(rows[0]["nucleus_sample_stddev_channel_1"]) == pytest.approx(first.nucleus.channels.sample_standard_deviation[0])
    default = q.cell_measurements_csv(q.CellTableRequest(doc, [record]))
    assert "intensity_statistics_format" not in next(csv.reader(io.StringIO(default)))
    options.include_capture_calibration = True
    assert list(csv.DictReader(io.StringIO(q.cell_measurements_csv(q.CellTableRequest(doc, [record], options)))))[0]["table_schema"] == "qpx.cell-table/14"
    project = q.Project()
    project.add_document(doc)
    source = tmp_path / "source.qpxproj"
    project.save(str(source))
    destination = tmp_path / "stats.csv"
    executable = Path(q.__file__).with_name("qupathxx_batch.exe" if os.name == "nt" else "qupathxx_batch")
    command = subprocess.run([str(executable), "--export-cell-table", str(source), str(destination), "--intensity-statistics"], capture_output=True, text=True, timeout=60)
    assert command.returncode == 0, command.stderr
    exported = list(csv.DictReader(destination.open(newline="", encoding="utf-8")))
    assert exported[0]["nucleus_sum_r"] == rows[0]["nucleus_sum_r"]


@pytest.mark.parametrize("foreground", [False, True])
def test_zero_and_single_sample_availability_survives_storage(tmp_path, foreground):
    rgb = np.full((5, 5, 3), 255, dtype=np.uint8)
    if foreground:
        rgb[2, 2] = 0
    doc = q.Document(q.Image.from_numpy(rgb))
    obj = doc.add_rectangle(q.Rect(0, 0, 5, 5))
    parameters = q.GridCellParameters()
    parameters.threshold, parameters.sigma, parameters.min_nucleus_area = .5, 0, 1
    parameters.cell_expansion = 0
    descriptor = q.NominalCellSampling(q.RgbNominalRequest(0, 0, 5, 5), 0, 0)
    request = q.CellRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), descriptor, parameters)
    result = q.detect_cells(request)
    assert result.intensity_statistics_format == "qpx.cell-intensity-statistics/1"
    assert result.cell_count == int(foreground)
    if foreground:
        statistics = result.intensity_statistics(1)
        assert statistics.nucleus.samples == 1 and statistics.cytoplasm is None
        assert statistics.nucleus.rgb.minimum == [0, 0, 0]
        assert statistics.nucleus.rgb.sample_standard_deviation is None
        assert statistics.nucleus.channels.sample_standard_deviation is None
    else:
        with pytest.raises(IndexError):
            result.intensity_statistics(1)
    record = doc.keep_cells(request, result)
    options = q.CellTableOptions()
    options.include_intensity_statistics = True
    rows = list(csv.DictReader(io.StringIO(q.cell_measurements_csv(q.CellTableRequest(doc, [record], options)))))
    if foreground:
        assert rows[0]["nucleus_sample_stddev_r"] == ""
        assert rows[0]["nucleus_range_r"] == "0"
        assert rows[0]["cytoplasm_minimum_r"] == ""
    project = q.Project()
    image = project.add_document(doc)
    saved = tmp_path / "availability.qpxproj"
    project.save(str(saved))
    restored = q.Project.open(str(saved)).document(image)
    assert restored.cell_record(record).result.intensity_statistics_format == result.intensity_statistics_format
    replay = q.CellRequest.replay(restored, record)
    assert restored.verify_cells(record, replay, q.detect_cells(replay))


@pytest.mark.skipif(not q.has_geos, reason="requires published cell features")
def test_classifier_preserves_mixed_known_and_unknown_statistics(tmp_path):
    from test_object_classifier_archives import model_for

    manifest = json.loads((LEGACY / "legacy-manifest.json").read_text())
    project = q.Project.open(str(LEGACY / manifest["file"]))
    doc = project.document(manifest["image_id"])
    record = manifest["records"][0]["record_id"]
    legacy = doc.cell_record(record).result
    replay = q.CellRequest.replay(doc, record)
    assert doc.verify_cells(record, replay, q.detect_cells(replay))
    old_ids = doc.publish_cell_record(record)
    transform = q.StainTransform.from_normalized(legacy.nuclei.stains, legacy.nuclei.white)
    request = q.CellRequest(doc, manifest["object_id"], transform, legacy.parameters)
    fresh = q.detect_cells(request)
    assert fresh.intensity_statistics_format == "qpx.cell-intensity-statistics/1"
    with pytest.raises(ValueError, match="parameters"):
        doc.verify_cells(record, request, fresh)
    new_record = doc.keep_cells(request, fresh)
    new_ids = doc.publish_cell_record(new_record)
    options = q.ObjectFeatureOptions()
    options.cell_rgb = True
    features = q.ObjectFeatureRequest(doc, old_ids + new_ids, options)
    table = q.extract_object_features(features)
    assert table.cell_intensity_statistics(0) is None
    expected = table.cell_intensity_statistics(len(old_ids))
    assert expected.nucleus.rgb.sum == fresh.intensity_statistics(1).nucleus.rgb.sum
    job = q.ObjectClassifierJob()
    assert job.predict(features, model_for(table.features))
    assert job.wait(doc).state == "ready"
    job.apply(doc)
    saved = tmp_path / "classifier.qpxproj"
    project.save(str(saved))
    restored = q.Project.open(str(saved)).document(manifest["image_id"]).classifier_records[-1].archive.table
    assert restored.cell_intensity_statistics(0) is None
    assert restored.cell_intensity_statistics(len(old_ids)).nucleus.rgb.sum == expected.nucleus.rgb.sum
    doc.server.close()
    del doc, project, features, table, job
    gc.collect()
    assert expected.nucleus.rgb.sum == fresh.intensity_statistics(1).nucleus.rgb.sum


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in measured cell export desktop")
def test_desktop_exports_retained_statistics(tmp_path):
    from test_gui_replay import event

    doc, _, _, _, _ = scene()
    options = q.GuiOptions()
    options.cell_nominal = q.NominalCellRoiSampling()
    parameters = q.GridCellParameters()
    parameters.threshold, parameters.sigma, parameters.min_nucleus_area, parameters.max_nucleus_area = .4, 1, 16, 512
    parameters.contours.work_bytes = 1024 * 1024
    options.grid_cell_parameters = parameters
    options.cell_table_intensity_statistics = True
    options.cell_table_path = str(tmp_path / "desktop.csv")
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10, q.GuiInputKind.KEEP_CELL_RUN_KEY, wait=q.GuiInputWait.CELLS_READY),
                      event(12, q.GuiInputKind.EXPORT_CELL_TABLE_KEY, wait=q.GuiInputWait.CELLS_ARCHIVED),
                      event(14, q.GuiInputKind.MOVE, wait=q.GuiInputWait.CELL_TABLE_SAVED)]
    report = q.show(doc, frame_limit=40, options=options)
    assert report.saved_cell_status == "current"
    rows = list(csv.DictReader(Path(options.cell_table_path).open(newline="", encoding="utf-8")))
    assert len(rows) == report.cell_result.cell_count
    assert rows[0]["intensity_statistics_format"] == "qpx.cell-intensity-statistics/1"
    assert float(rows[0]["nucleus_sum_r"]) == report.cell_result.intensity_statistics(1).nucleus.rgb.sum[0]



def test_legacy_exact_grid_budget_still_reopens_and_replays():
    manifest = json.loads((LEGACY / "legacy-tight-manifest.json").read_text())
    source = LEGACY / manifest["file"]
    assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["sha256"]
    doc = q.Project.open(str(source)).document(manifest["image_id"])
    record = manifest["record_id"]
    request = q.CellRequest.replay(doc, record)
    assert request.estimated_work_bytes == manifest["work_bytes"]
    result = q.detect_cells(request)
    assert result.intensity_statistics_format is None
    assert result.nuclei.input_sha256 == manifest["input_sha256"]
    assert doc.verify_cells(record, request, result)
