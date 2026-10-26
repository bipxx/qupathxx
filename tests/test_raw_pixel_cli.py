"""Official native raw workflow against an independent installed Python consumer."""
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from tests.test_raw_pixel_python import tiff

pytestmark = pytest.mark.skipif(not q.has_tiff or not q.has_project,
                                reason="quantitative TIFF and project backends")
CLI = Path(os.environ.get("QUPATHXX_TEST_RAW_CLI", Path(_native.__file__).with_name(
    "qupathxx_batch.exe" if os.name == "nt" else "qupathxx_batch")))


def run(*args):
    return subprocess.run([str(CLI), *map(str, args)], capture_output=True,
                          text=True, encoding="utf-8", timeout=60)


def final(done, code=0):
    assert done.returncode == code, done.stdout + done.stderr
    return json.loads(done.stdout.splitlines()[-1])


@pytest.fixture
def source(tmp_path):
    pixels = tmp_path / "输入.tif"
    tiff(pixels)
    project = q.Project()
    image = project.add_image(str(pixels), "tiff")
    doc = project.document(image)
    ids = [doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 3, 6)), class_name="A"),
           doc.add_detection(q.Roi.rectangle(q.Rect(4, 0, 4, 6)), class_name="B")]
    path = tmp_path / "项目.qpx"
    project.save(str(path))
    yield path, image, doc, ids, pixels
    doc.server.close()


def arguments(source, tmp_path):
    path, image, *_ = source
    return ["--train-raw-pixels", path, image, tmp_path / "模型.qrm",
            "--checkpoint", tmp_path / "采样.qtc", "--archive", tmp_path / "训练.qtt",
            "--x-axis", "0,8,7,2", "--y-axis", "0,6,5,2", "--channels", 2,
            "--tile-width", 2, "--tile-height", 2, "--max-samples", 12,
            "--samples-per-class", 6, "--work-bytes", 16 * 1024**2,
            "--trees", 5, "--max-depth", 3, "--forest-work-bytes", 2 * 1024**2]


def require_training():
    if not q.has_classifier_training:
        pytest.skip("actual optional forest backend")


def test_cli_train_resume_predict_retain_and_verify(source, tmp_path):
    require_training()
    path, image, doc, ids, _ = source
    before = path.read_bytes()
    args = arguments(source, tmp_path)
    partial = final(run(*args, "--max-new-windows", 1), 1)
    assert partial["state"] == "partial" and partial["model_sha256"] is None
    assert not (tmp_path / "模型.qrm").exists() and not (tmp_path / "训练.qtt").exists()
    trained = final(run(*args, "--resume", tmp_path / "采样.qtc", "--replace-existing",
                        "--output-project", tmp_path / "trained.qpx"))
    assert trained["state"] == "trained" and trained["replayed_windows"] == 1
    lattice = q.ScientificRawLattice(8, 6, q.ScientificGridAxis(0, 8, 7, 2),
                                    q.ScientificGridAxis(0, 6, 5, 2))
    selection, features = q.ScientificRawOptions(), q.RawPixelFeatureOptions()
    selection.channels = 2
    selection.work_bytes = features.work_bytes = 16 * 1024**2
    features.offsets, features.scales = [0, 0], [1, 1]
    options, tiles = q.RawPixelTrainingOptions(), q.RawPixelTileOptions()
    options.max_samples, options.max_samples_per_class, options.work_bytes = 12, 6, 16 * 1024**2
    tiles.width = tiles.height = 2
    request = q.RawPixelTiledTrainingRequest(doc, ids, lattice, selection, features, options, tiles)
    table = q.sample_raw_pixel_tiled_training(request)
    parameters = q.ForestParameters()
    parameters.trees, parameters.max_depth, parameters.work_bytes = 5, 3, 2 * 1024**2
    reference = q.train_raw_pixel_tiled_classifier(table, parameters)
    model = q.load_raw_pixel_classifier(str(tmp_path / "模型.qrm"))
    assert model.bytes() == reference.bytes()
    assert trained["rows"] == table.row_count and trained["table_sha256"] == table.sha256
    archive = q.load_raw_pixel_archive(str(tmp_path / "训练.qtt"))
    np.testing.assert_array_equal(archive.tiled_table.values_numpy(), table.values_numpy())
    saved = q.Project.open(str(tmp_path / "trained.qpx"))
    assert saved.document(image).raw_pixel_record_count == 1
    saved.document(image).server.close()
    output = tmp_path / "输出.qws"
    export = ["--export-raw-pixels", path, image, tmp_path / "模型.qrm", output,
              "--x-axis", "0,8,7,2", "--y-axis", "0,6,5,2", "--tile-width", 2,
              "--tile-height", 2, "--work-bytes", 16 * 1024**2]
    prefix = final(run(*export, "--max-new-windows", 1), 1)
    assert prefix["state"] == "partial" and prefix["summary"]["completed_windows"] == 1
    result = final(run(*export, "--resume", output, "--replace-existing",
                       "--output-project", tmp_path / "prediction.qpx"))
    assert result["state"] == "saved" and result["summary"]["complete"]
    assert result["summary"]["reused_windows"] == 1
    stream_options = q.RawPixelStreamOptions()
    stream_options.tile_width = stream_options.tile_height = 2
    stream_options.work_bytes = 16 * 1024**2
    prediction = q.RawPixelTiledPredictionRequest(doc, model, lattice, stream_options)
    stream = q.RawPixelPredictionStream(prediction)
    file = q.RawPixelStreamFile(str(output))
    file.verify()
    for i in range(file.declared_summary.completed_windows):
        actual, expected = file.read_block(i), stream.next()
        np.testing.assert_array_equal(actual.labels_numpy(), expected.labels_numpy())
        np.testing.assert_array_equal(actual.vote_fractions_numpy(), expected.vote_fractions_numpy())
        np.testing.assert_array_equal(actual.valid_numpy(), expected.valid_numpy())
    assert stream.next() is None
    verified = final(run("--verify-raw-pixels", output))
    assert verified["state"] == "verified" and verified["summary"]["sha256"] == result["summary"]["sha256"]
    reopened = q.Project.open(str(tmp_path / "prediction.qpx"))
    assert reopened.document(image).raw_pixel_stream_record_count == 1
    reopened.document(image).server.close()
    assert path.read_bytes() == before
    # A complete fresh export and a replayed export have identical persisted bytes.
    other = tmp_path / "fresh.qws"
    fresh = export.copy()
    fresh[5] = other
    assert final(run(*fresh))["summary"]["reused_windows"] == 0
    assert output.read_bytes() == other.read_bytes()
    corrupt = bytearray(output.read_bytes())
    corrupt[len(corrupt) // 2] ^= 1
    (tmp_path / "corrupt.qws").write_bytes(corrupt)
    assert final(run("--verify-raw-pixels", tmp_path / "corrupt.qws"), 1)["state"] == "failed"


@pytest.mark.parametrize("extra", [
    ["--x-axis", "0,8,7,2"], ["--channels", "4294967296"],
    ["--x-axis", "0,8,7,2", "--y-axis", "0,6,5,2", "--x-axis", "0,8,7,2"],
    ["--scales", "nan"], ["--unknown"], ["--tile-width", "-2"],
])
def test_cli_arguments_reject_before_output(source, tmp_path, extra):
    path, image, *_ = source
    result = run("--train-raw-pixels", path, image, tmp_path / "model.qrm",
                 "--checkpoint", tmp_path / "sampling.qtc", "--archive", tmp_path / "rows.qtt", *extra)
    assert result.returncode == 2, result.stdout + result.stderr
    assert not (tmp_path / "sampling.qtc").exists()


@pytest.mark.parametrize("target", ["project", "pixels", "hardlink", "same-products"])
def test_cli_training_refuses_source_and_product_aliases(source, tmp_path, target):
    require_training()
    path, _, _, _, pixels = source
    args = arguments(source, tmp_path)
    old_project, old_pixels = path.read_bytes(), pixels.read_bytes()
    if target == "hardlink":
        alias = tmp_path / "alias.qrm"
        os.link(pixels, alias)
        args[3] = alias
    elif target == "same-products":
        args[3] = tmp_path / "采样.qtc"
    else:
        args[3] = path if target == "project" else pixels
    result = run(*args, "--replace-existing")
    assert result.returncode == 2, result.stdout + result.stderr
    assert path.read_bytes() == old_project and pixels.read_bytes() == old_pixels
    assert not (tmp_path / "采样.qtc").exists()


def test_cli_model_lattice_mismatch_refuses_prediction_output(source, tmp_path):
    require_training()
    path, image, *_ = source
    final(run(*arguments(source, tmp_path)))
    output = tmp_path / "wrong.qws"
    result = run("--export-raw-pixels", path, image, tmp_path / "模型.qrm", output)
    assert result.returncode in (1, 2), result.stdout + result.stderr
    assert not output.exists()
