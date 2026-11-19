"""Official native raw workflow against an independent installed Python consumer."""
import csv
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
from contextlib import closing

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_raw_pixel_python import tiff

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


def test_raw_classifier_commands_require_review_for_replaced_verified_image(tmp_path):
    from test_project_source import fingerprint

    require_training()
    pixels = tmp_path / "image.tif"
    tiff(pixels)
    project = q.Project()
    image = project.add_image(str(pixels), "tiff")
    doc = project.document(image)
    doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 3, 6)), class_name="A")
    doc.add_detection(q.Roi.rectangle(q.Rect(4, 0, 4, 6)), class_name="B")
    project.apply_source_review(project.review_source_verification(image, fingerprint(pixels)))
    saved = tmp_path / "verified.qpx"
    project.save(str(saved))
    del doc, project
    original = saved.read_bytes()
    training = tmp_path / "original"
    training.mkdir()
    initial = arguments((saved, image), training)
    assert final(run(*initial))["state"] == "trained"
    model = initial[3]
    model_before = model.read_bytes()

    altered = bytearray(pixels.read_bytes())
    np.frombuffer(altered, dtype="<u2", offset=160)[::2] += 25
    replacement = tmp_path / "replacement.tif"
    replacement.write_bytes(altered)
    replacement.replace(pixels)
    denied = tmp_path / "denied"
    denied.mkdir()
    export_options = ["--model-grid", "--tile-width", 2, "--tile-height", 2]
    for mode in (
        arguments((saved, image), denied),
        ["--export-raw-pixels", saved, image, model, denied / "pixels.qws", *export_options],
    ):
        refused = final(run(*mode), 1)
        assert refused["state"] == "failed" and "differs from saved source verification" in refused["error"]
    if q.has_geos:
        refused = final(run("--contour-raw-pixels", saved, image, model, denied / "contours.qpx", *export_options), 1)
        assert "differs from saved source verification" in refused["error"]
    workflow = tmp_path / "denied-workflow"
    result = run("--raw-classifier-workflow", saved, workflow, image, "--channels", 2)
    assert result.returncode == 2 and "differs from saved source verification" in result.stderr
    assert not list(denied.iterdir()) and not workflow.exists()

    project = q.Project.open(str(saved))
    review = project.review_source_verification(image, fingerprint(pixels))
    assert review.comparison == "changed_bytes"
    project.apply_source_review(review, accept_changed=True)
    adopted = tmp_path / "adopted.qpx"
    project.save(str(adopted))
    outputs = tmp_path / "adopted"
    outputs.mkdir()
    train = arguments((adopted, image), outputs)
    result = final(run(*train))
    assert result["state"] == "trained" and result["rows"] == 12
    prediction = outputs / "pixels.qws"
    streamed = final(run("--export-raw-pixels", adopted, image, train[3], prediction, *export_options))
    verified = final(run("--verify-raw-pixels", prediction))
    assert verified["state"] == "verified"
    assert verified["summary"]["sha256"] == streamed["summary"]["sha256"]
    assert saved.read_bytes() == original and model.read_bytes() == model_before


def test_cli_resolves_model_grid_for_export_and_contours(source, tmp_path):
    require_training()
    path, image, doc, _, pixels = source
    train = arguments(source, tmp_path)
    model = train[3]
    assert final(run(*train))["state"] == "trained"
    options = ["--tile-width", 2, "--tile-height", 2, "--work-bytes", 16 * 1024**2]
    explicit_axes = ["--x-axis", "0,8,7,0", "--y-axis", "0,6,5,0"]
    def same_prediction(fitted, reference):
        for key in ("input_sha256", "sha256", "class_counts", "valid_samples", "completed_windows"):
            assert fitted["summary"][key] == reference["summary"][key], key

    export = ["--export-raw-pixels", path, image, model]
    reference = final(run(*export, tmp_path / "explicit.qws", *options, *explicit_axes))
    fitted = final(run(*export, tmp_path / "resolved.qws", *options, "--model-grid"))
    same_prediction(fitted, reference)
    assert fitted["summary"]["valid_samples"] == 35
    conflict = tmp_path / "conflict.qws"
    rejected = run(*export, conflict, *options, "--model-grid", *explicit_axes)
    assert rejected.returncode != 0 and "conflicts" in rejected.stdout + rejected.stderr
    assert not conflict.exists()

    if q.has_geos:
        command = ["--contour-raw-pixels", path, image, model]
        contour = final(run(*command, tmp_path / "contours.qpx", *options, "--model-grid"))
        reference = final(run(*command, tmp_path / "explicit-contours.qpx", *options, *explicit_axes))
        assert (tmp_path / "contours.qpx").is_file() and contour["object_ids"]
        for key in ("request_sha256", "input_sha256", "prediction_sha256", "vertices", "components", "class_counts"):
            assert contour[key] == reference[key], key

    # Fit at level-zero spacing, then apply to the same raw channel layout
    # with a finer, anisotropic calibration. The exact target step is 2,3.
    calibrated_project = q.Project.open(str(path))
    calibrated_project.document(image).set_calibration(0.5, 0.375)
    calibrated = tmp_path / "calibrated.qpx"
    calibrated_project.save(str(calibrated))
    physical_model = tmp_path / "physical.qrm"
    training = ["--train-raw-pixels", calibrated, image, physical_model,
                "--checkpoint", tmp_path / "physical.qtc", "--archive", tmp_path / "physical.qtt",
                "--channels", 2, "--physical-sigmas", "--trees", 5, *options]
    assert final(run(*training))["state"] == "trained"
    target_project = q.Project()
    target = target_project.add_image(str(pixels), "tiff")
    target_project.document(target).set_calibration(0.25, 0.125)
    target_roi = target_project.document(target).add_rectangle(q.Rect(0, 0, 8, 6))
    target_path = tmp_path / "target.qpx"
    target_project.save(str(target_path))
    base = ["--export-raw-pixels", target_path, target, physical_model]
    expected = final(run(*base, tmp_path / "physical-explicit.qws", *options,
                         "--x-axis", "0,8,4,0", "--y-axis", "0,6,2,0"))
    physical_output = tmp_path / "physical-resolved.qws"
    prefix = final(run(*base, physical_output, *options, "--model-grid", "--max-new-windows", 1), 1)
    assert prefix["state"] == "partial"
    actual = final(run(*base, physical_output, *options, "--model-grid",
                       "--resume", physical_output, "--replace-existing"))
    same_prediction(actual, expected)
    assert actual["summary"]["valid_samples"] == 8 and actual["summary"]["reused_windows"] == 1
    verified = final(run("--verify-raw-pixels", physical_output))
    assert verified["summary"]["content_sha256"] == actual["summary"]["content_sha256"]
    absent = tmp_path / "missing-calibration.qws"
    refused = run("--export-raw-pixels", path, image, physical_model, absent,
                  *options, "--model-grid")
    assert refused.returncode == 2 and "calibration" in json.loads(refused.stderr)["error"]
    assert not absent.exists()
    assert doc.pixel_width_um is None

    explicit_batch_axes = ["--raw-pixel-x-axis", "0,8,4,0", "--raw-pixel-y-axis", "0,6,2,0"]
    batch_options = ["--image", target, "--object", target_roi, "--raw-pixel-model", physical_model,
                     "--raw-pixel-tile-width", 2, "--raw-pixel-tile-height", 2,
                     "--raw-pixel-work-bytes", 16 * 1024**2]
    model_grid = ["--raw-pixel-model-grid"]
    for kind in ["raw-pixels", "raw-contours"] if q.has_geos else ["raw-pixels"]:
        command = ["--kind", kind, *batch_options]
        explicit = final(run(target_path, tmp_path / f"{kind}-explicit", *command, *explicit_batch_axes))
        out = tmp_path / f"{kind}-model"
        resolved = final(run(target_path, out, *command, *model_grid))
        assert explicit["saved"] == resolved["saved"] == 1
        assert explicit["parameters_sha256"] != resolved["parameters_sha256"]
        first, actual = explicit["images"][0], resolved["images"][0]
        assert first["pixel_sha256"] == actual["pixel_sha256"]
        tables = []
        streams = []
        contours = []
        for side, item in [("explicit", first), ("resolved", actual)]:
            table = tmp_path / f"{kind}-{side}.csv"
            exported = run("--export-csv", item["path"], table)
            assert exported.returncode == 0, exported.stdout + exported.stderr
            with table.open(encoding="utf-8", newline="") as stream:
                rows = list(csv.DictReader(stream))
            assert rows
            if kind == "raw-pixels":
                file, = Path(item["path"]).parent.glob("*.qws")
                summary = final(run("--verify-raw-pixels", file))["summary"]
                # Each archived stream authenticates its own captured provenance.
                assert {row.pop("content_sha256") for row in rows} == {summary["content_sha256"]}
                streams.append({"summary": summary})
            else:
                # Inspect the stored geometry bytes with the standard SQLite reader.
                snapshot = tmp_path / f"contours-{side}.sqlite"
                snapshot.write_bytes(Path(item["path"]).read_bytes())
                with closing(sqlite3.connect(snapshot.as_uri() + "?mode=ro", uri=True)) as db:
                    contours.append(db.execute(
                        "SELECT id,parent,class_name,geometry FROM objects "
                        "WHERE image_id=? AND parent=? ORDER BY id", (str(target), str(target_roi))
                    ).fetchall())
            tables.append(rows)
        assert tables[0] == tables[1]
        if streams:
            same_prediction(*streams)
        else:
            assert contours[0] and contours[0] == contours[1]
        repeat = final(run(target_path, out, *command, *model_grid))
        assert repeat["reused"] == 1 and repeat["images"][0]["inferred_blocks"] == 0
        collision = tmp_path / f"{kind}-conflict"
        failed = run(target_path, collision, *command, *model_grid, *explicit_batch_axes)
        assert failed.returncode == 2 and "conflicts with explicit axes" in failed.stderr
        assert not collision.exists()

    for calibration in [(0.5, 0.375), None]:
        added = target_project.add_image(str(pixels), "tiff")
        added_doc = target_project.document(added)
        added_doc.add_rectangle(q.Rect(0, 0, 8, 6))
        if calibration is not None:
            added_doc.set_calibration(*calibration)
    cohort_path = tmp_path / "multiple-calibrations.qpx"
    target_project.save(str(cohort_path))
    cohort_out = tmp_path / "multiple-calibrations-batch"
    cohort = final(run(cohort_path, cohort_out, "--kind", "raw-pixels",
                       "--raw-pixel-model", physical_model, "--continue-on-error", *model_grid), 1)
    assert cohort["saved"] == 2 and cohort["failed"] == 1
    failed = cohort["images"][2]
    assert "calibration" in failed["error"] and not failed["path"]
    assert not list(cohort_out.glob(f"image-{failed['image_id']}-*"))
    for item, samples in zip(cohort["images"][:2], [8, 48]):
        checkpoint = Path(item["path"])
        assert checkpoint.is_file()
        stream, = checkpoint.parent.glob("*.qws")
        assert final(run("--verify-raw-pixels", stream))["summary"]["valid_samples"] == samples


def test_cli_runs_pinned_raw_batch_recipe(source, tmp_path):
    require_training()
    training = arguments(source, tmp_path)
    model = Path(training[3])
    assert final(run(*training))["state"] == "trained"

    project = q.Project()
    selections = []
    for index in range(2):
        image = project.add_image(str(source[4]), "tiff")
        doc = project.document(image)
        if index:
            doc.add_rectangle(q.Rect(0, 0, 2, 2))
        selected = doc.add_rectangle(q.Rect(index, 0, 8 - index, 6))
        doc.add_rectangle(q.Rect(1, 1, 2, 2))
        selections.append({"image": image, "objects": [selected]})
    assert selections[0]["objects"] != selections[1]["objects"]
    path = tmp_path / "targets.qpx"
    project.save(str(path))

    def pin(file):
        return {"path": file.name, "sha256": hashlib.sha256(file.read_bytes()).hexdigest()}

    spec = {"schema": "qpx.project-raw-pixel-batch-recipe/1", "project": pin(path), "model": pin(model),
            "selection": selections, "failure": "stop", "resume": True,
            "raw": {"kind": "raw-pixels", "grid": {"mode": "model"},
                    "stream": {"tile_width": 2, "tile_height": 2, "work_bytes": 16 * 1024**2,
                               "min_vote_fraction": 0, "max_samples": 1000, "max_windows": 1000}}}
    recipe = tmp_path / "recipe.json"

    def write(value):
        recipe.write_text(json.dumps(value), encoding="utf-8")

    for kind in ["raw-pixels", "raw-contours"] if q.has_geos else ["raw-pixels"]:
        spec["raw"]["kind"] = kind
        spec["raw"]["grid"] = {"mode": "model"}
        write(spec)
        output = tmp_path / kind
        resolved = final(run("--pixel-batch-recipe", recipe, output))
        assert resolved["saved"] == 2 and [image["objects"] for image in resolved["images"]] == [1, 1]
        replay = final(run("--pixel-batch-recipe", recipe, output))
        assert replay["reused"] == 2 and all(image["inferred_blocks"] == 0 for image in replay["images"])
        spec["raw"]["grid"] = {"mode": "explicit", "x": [0, 8, 7, 0], "y": [0, 6, 5, 0]}
        write(spec)
        explicit = final(run("--pixel-batch-recipe", recipe, tmp_path / (kind + "-explicit")))
        assert [image["pixel_sha256"] for image in resolved["images"]] == [
            image["pixel_sha256"] for image in explicit["images"]]
        for selected, image in zip(selections, resolved["images"]):
            table = tmp_path / f"{kind}-{image['image_id']}.csv"
            exported = run("--export-csv", image["path"], table)
            assert exported.returncode == 0, exported.stderr
            with table.open(encoding="utf-8", newline="") as stream:
                ids = {int(row["object_id" if kind == "raw-pixels" else "parent_id"])
                       for row in csv.DictReader(stream)}
            assert ids == set(selected["objects"])

    spec["raw"]["grid"]["x"][0] = 2**63
    write(spec)
    blocked = tmp_path / "bad-axis"
    result = run("--pixel-batch-recipe", recipe, blocked)
    assert result.returncode == 2 and "signed-range integer" in result.stderr and not blocked.exists()
    spec["raw"]["grid"] = {"mode": "model"}
    expected_model = spec["model"]
    spec["model"] = {**expected_model, "sha256": "0" * 64}
    write(spec)
    blocked = tmp_path / "wrong-model"
    result = run("--pixel-batch-recipe", recipe, blocked)
    assert result.returncode == 2 and "Pixel model file SHA-256" in result.stderr and not blocked.exists()
    assert pin(path) == spec["project"] and pin(model) == expected_model
    spec["model"] = expected_model
    spec["project"]["sha256"] = "0" * 64
    write(spec)
    blocked = tmp_path / "wrong-project"
    result = run("--pixel-batch-recipe", recipe, blocked)
    assert result.returncode == 2 and "Project file SHA-256" in result.stderr and not blocked.exists()


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
    assert model.to_bytes() == reference.to_bytes()
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
    prefix_bytes = output.read_bytes()
    result = final(run(*export, "--resume", output, "--replace-existing",
                       "--output-project", tmp_path / "prediction.qpx"))
    assert result["state"] == "saved" and result["summary"]["complete"]
    assert result["summary"]["reused_windows"] == 1
    # QWE1 footer changes; the original header and committed QWB1 prefix do not.
    footer_bytes = 292 + 8 * len(prefix["summary"]["class_counts"])
    assert output.read_bytes().startswith(prefix_bytes[:-footer_bytes])
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
    # Fresh processes have distinct capture clocks/generations in provenance.
    # Their input/result chains and every numeric block must still match.
    other = tmp_path / "fresh.qws"
    fresh = export.copy()
    fresh[4] = other
    fresh_summary = final(run(*fresh))["summary"]
    assert fresh_summary["reused_windows"] == 0
    for key in ("input_sha256", "sha256", "class_counts", "completed_windows"):
        assert fresh_summary[key] == result["summary"][key]
    independent = q.RawPixelStreamFile(str(other))
    independent.verify()
    for i in range(fresh_summary["completed_windows"]):
        np.testing.assert_array_equal(independent.read_block(i).labels_numpy(), file.read_block(i).labels_numpy())
        np.testing.assert_array_equal(independent.read_block(i).vote_fractions_numpy(), file.read_block(i).vote_fractions_numpy())
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


def test_cli_cancellation_joins_before_reader_release(tmp_path):
    require_training()
    import signal
    pixels = tmp_path / "large.tif"
    tiff(pixels, 2048, 2048)
    project = q.Project()
    image = project.add_image(str(pixels), "tiff")
    doc = project.document(image)
    for x, name in [(0, "A"), (1024, "B")]:
        doc.add_detection(q.Roi.rectangle(q.Rect(x, 0, 1024, 2048)), class_name=name)
    path = tmp_path / "large.qpx"
    project.save(str(path))
    doc.server.close()
    model, checkpoint, archive = [tmp_path / n for n in ("model.qrm", "sampling.qtc", "rows.qtt")]
    kwargs = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {}
    child = subprocess.Popen([str(CLI), "--train-raw-pixels", str(path), str(image), str(model),
                              "--checkpoint", str(checkpoint), "--archive", str(archive)],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             encoding="utf-8", **kwargs)
    try:
        first = json.loads(child.stdout.readline())
        assert first["state"] == "sampling" and child.poll() is None
        child.send_signal(signal.CTRL_BREAK_EVENT if os.name == "nt" else signal.SIGINT)
        stdout, stderr = child.communicate(timeout=40)
        assert child.returncode == 130, stdout + stderr
        assert json.loads(stdout.splitlines()[-1])["state"] == "cancelled"
        assert not model.exists() and not archive.exists()
        pixels.rename(tmp_path / "closed.tif")
    finally:
        if child.poll() is None:
            child.kill()
            child.communicate(timeout=20)


def test_cli_training_backend_off_refuses_before_output(source, tmp_path):
    if q.has_classifier_training:
        pytest.skip("backend-OFF profile only")
    done = run(*arguments(source, tmp_path))
    assert final(done, 1)["state"] == "failed"
    assert "OpenCV" in done.stdout and not (tmp_path / "采样.qtc").exists()


def test_cli_prediction_refuses_aliases_and_changed_resume_request(source, tmp_path):
    require_training()
    path, image, _, _, pixels = source
    final(run(*arguments(source, tmp_path)))
    model = tmp_path / "模型.qrm"
    source_bytes, model_bytes, pixel_bytes = path.read_bytes(), model.read_bytes(), pixels.read_bytes()
    base = ["--export-raw-pixels", path, image, model]
    options = ["--x-axis", "0,8,7,2", "--y-axis", "0,6,5,2",
               "--tile-width", 2, "--tile-height", 2]
    for dest in (path, model, pixels):
        done = run(*base, dest, *options, "--replace-existing")
        assert done.returncode == 2, done.stdout + done.stderr
    assert path.read_bytes() == source_bytes and model.read_bytes() == model_bytes and pixels.read_bytes() == pixel_bytes
    output = tmp_path / "prefix.qws"
    final(run(*base, output, *options, "--max-new-windows", 1), 1)
    before = output.read_bytes()
    refused = final(run(*base, output, *options, "--resume", output,
                        "--replace-existing", "--min-vote", ".75"), 1)
    assert refused["state"] == "failed" and output.read_bytes() == before
    # A conflicting new-project destination is refused before replacing a prefix.
    done = run(*base, output, *options, "--resume", output,
               "--replace-existing", "--output-project", path)
    assert done.returncode == 2 and output.read_bytes() == before
