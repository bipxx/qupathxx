"""Installed quantitative raw classifier workflows; no display conversion."""
import gc
import json
from pathlib import Path
import struct
import subprocess
import sys
import threading
from fractions import Fraction

import numpy as np
import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_tiff, reason="quantitative TIFF reader")


def tiff(path, width=8, height=6):
    data = bytearray(160 + width * height * 4)
    struct.pack_into("<2sHIH", data, 0, b"II", 42, 8, 12)
    tags = [(256, 4, 1, width), (257, 4, 1, height), (258, 3, 2, 0x00100010),
            (259, 3, 1, 1), (262, 3, 1, 1), (273, 4, 1, 160), (277, 3, 1, 2),
            (278, 4, 1, height), (279, 4, 1, width * height * 4), (284, 3, 1, 1),
            (338, 3, 1, 0), (339, 3, 2, 0x00010001)]
    for i, tag in enumerate(tags):
        struct.pack_into("<HHII", data, 10 + 12 * i, *tag)
    values = np.frombuffer(data, dtype="<u2", offset=160).reshape(height, width, 2)
    y, x = np.indices((height, width))
    for c in range(2):
        values[:, :, c] = (x + width * y + 100 * c) % 60000
    path.write_bytes(data)
    return values.copy()


@pytest.fixture
def raw(tmp_path):
    path = tmp_path / "source.tif"
    pixels = tiff(path)
    server = q.open_tiff(str(path))
    doc = q.Document(server)
    ids = [doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 3, 6)), class_name="A"),
           doc.add_detection(q.Roi.rectangle(q.Rect(4, 0, 4, 6)), class_name="B")]
    grid = q.ScientificGrid(8, 6, q.ScientificGridAxis(0, 8, 7, 2), q.ScientificGridAxis(0, 6, 5, 2))
    selection = q.ScientificRawOptions()
    selection.channels = 2
    features = q.RawPixelFeatureOptions()
    features.offsets, features.scales = [0, 0], [1, 1]
    options = q.RawPixelTrainingOptions()
    options.max_samples, options.max_samples_per_class, options.work_bytes = 12, 6, 16 * 1024**2
    tiles = q.RawPixelTileOptions()
    tiles.width, tiles.height = 2, 2
    full = q.RawPixelTrainingRequest(doc, ids, grid, selection, features, options)
    tiled = q.RawPixelTiledTrainingRequest(doc, ids, q.ScientificRawLattice(grid), selection, features, options, tiles)
    yield doc, server, pixels, full, tiled, path
    server.close()


def trained(table):
    if not q.has_classifier_training:
        pytest.skip("actual optional forest backend")
    p = q.ForestParameters()
    p.trees, p.max_depth, p.work_bytes = 5, 3, 2 * 1024**2
    if isinstance(table, q.RawPixelTiledTrainingTable):
        return q.train_raw_pixel_tiled_classifier(table, p)
    return q.train_raw_pixel_classifier(table, p)


def exact_mean(pixels, gx, gy, c):
    left, right = Fraction(8 * gx, 7), Fraction(8 * (gx + 1), 7)
    top, bottom = Fraction(6 * gy, 5), Fraction(6 * (gy + 1), 5)
    total = Fraction(0)
    for y in range(6):
        for x in range(8):
            dx = max(Fraction(0), min(right, x + 1) - max(left, x))
            dy = max(Fraction(0), min(bottom, y + 1) - max(top, y))
            total += int(pixels[y, x, c]) * dx * dy
    return np.float32(float(total / ((right - left) * (bottom - top))))


def test_tables_preserve_numeric_rows_dtype_and_array_ownership(raw):
    doc, server, pixels, full, tiled, _ = raw
    a, b = q.sample_raw_pixel_training(full), q.sample_raw_pixel_tiled_training(tiled)
    for method in ("values_numpy", "labels_numpy", "coordinates_numpy", "annotation_ids_numpy"):
        np.testing.assert_array_equal(getattr(a, method)(), getattr(b, method)())
    assert b.row_count == 12 and b.classes == ["A", "B"]
    assert b.values_numpy().dtype == np.float32 and b.coordinates_numpy().dtype == np.int64
    assert b.labels_numpy().dtype == np.uint32 and b.annotation_ids_numpy().dtype == np.uint64
    for row, (gx, gy) in zip(b.values_numpy(), b.coordinates_numpy()):
        np.testing.assert_array_equal(row, [exact_mean(pixels, int(gx), int(gy), c) for c in range(2)])
    view, original = b.values_numpy(), b.values_numpy(copy=True)
    assert not view.flags.writeable and original.flags.writeable and view.flags.c_contiguous
    with pytest.raises(ValueError):
        view[0, 0] = 123
    original[0, 0] += 1
    assert view[0, 0] != original[0, 0]
    expected = view.copy()
    del a, b
    gc.collect()
    np.testing.assert_array_equal(view, expected)
    recipe = tiled.recipe
    copied_options = recipe.features
    copied_options.offsets = [100, 100]
    assert tiled.recipe.features.offsets == [0, 0]
    assert q.RawPixelRecipe.from_bytes(recipe.to_bytes()).to_bytes() == recipe.to_bytes()


def test_window_geometry_and_hwc_views(raw):
    doc, server, _, full, tiled, _ = raw
    window = tiled.lattice.window(1, 1, 3, 2)
    assert window.x.samples == 7 and window.y.samples == 5
    assert window.x.first_core_index == 1 and window.core_width == 3
    assert float(window.x.center(1)) == float(tiled.lattice.x.center(1))
    data = q.read_scientific_raw_window(server, window, full.recipe.selection)
    assert data.numpy().shape == (window.height, window.width, 2)
    assert data.numpy().dtype == np.float64 and not data.numpy().flags.writeable
    assert data.valid_numpy().dtype == np.uint8 and data.reasons_numpy().dtype == np.uint8
    digest = q.digest_scientific_raw_window(server, window, full.recipe.selection)
    assert digest.input_sha256 == data.input_sha256
    x = q.ScientificGridAxis(-1, 999983, 1000000, 1)
    y = q.ScientificGridAxis(2, 999979, 1000000, 1)
    huge = q.ScientificRawLattice(1000000, 1000000, x, y)
    assert huge.core_samples == 10**12 and huge.window(543211, 123456, 2, 2).x.samples == 1000000


def test_actual_model_full_tiled_window_prediction_and_bytes(raw, tmp_path):
    doc, server, _, full, tiled, _ = raw
    a, b = q.sample_raw_pixel_training(full), q.sample_raw_pixel_tiled_training(tiled)
    model, tiled_model = trained(a), trained(b)
    assert model.to_bytes() == tiled_model.to_bytes()
    assert q.RawPixelClassifier.from_bytes(model.to_bytes()).sha256 == model.sha256
    path = tmp_path / "model.qrm"
    q.save_raw_pixel_classifier(model, str(path))
    assert q.load_raw_pixel_classifier(str(path)).to_bytes() == model.to_bytes()
    prediction = q.predict_raw_pixel_region(q.RawPixelPredictionRequest(doc, model, full.grid))
    window = tiled.lattice.window(1, 1, 3, 2)
    part = q.predict_raw_pixel_window(q.RawPixelWindowPredictionRequest(doc, model, window))
    np.testing.assert_array_equal(part.labels_numpy(), prediction.labels_numpy()[1:3, 1:4])
    np.testing.assert_array_equal(part.vote_fractions_numpy(), prediction.vote_fractions_numpy()[1:3, 1:4])
    assert part.labels_numpy().shape == (2, 3) and part.vote_fractions_numpy().dtype == np.float32
    assert not part.valid_numpy().flags.writeable
    direct = q.classify_raw_pixel_window(q.read_scientific_raw_window(server, window, model.recipe.selection), model)
    np.testing.assert_array_equal(direct.labels_numpy(), part.labels_numpy())
    with pytest.raises((ValueError, RuntimeError)):
        q.RawPixelClassifier.from_bytes(model.to_bytes()[:-1])


def test_archive_owner_replay_and_project_history(raw, tmp_path):
    if not q.has_project:
        pytest.skip("project persistence")
    doc, server, _, full, tiled, image = raw
    table = q.sample_raw_pixel_tiled_training(tiled)
    archive = q.archive_raw_pixel_tiled_training(table, trained(table))
    decoded = q.RawPixelArchive.from_bytes(archive.to_bytes())
    assert decoded.purpose == "tiled-training" and decoded.table is None
    np.testing.assert_array_equal(decoded.tiled_table.values_numpy(), table.values_numpy())
    with pytest.raises((ValueError, RuntimeError)):
        _ = decoded.grid
    with pytest.raises((ValueError, RuntimeError)):
        doc.keep_raw_pixel_tiled_training(tiled, decoded)
    record = doc.keep_raw_pixel_tiled_training(tiled, archive)
    replay = q.RawPixelTiledTrainingRequest.replay(doc, record)
    job = q.RawPixelTiledTrainingJob()
    assert job.sample(replay)
    assert job.wait(doc).state == "ready" and job.verify_result(doc, record)
    assert job.table is not None and job.model is None
    project = q.Project()
    image_id = project.add_document(doc, "Raw", str(image), "tiff")
    path = tmp_path / "project.qpx"
    project.save(str(path))
    reopened = q.Project.open(str(path))
    historic = reopened.document(image_id)
    try:
        assert historic.raw_pixel_record(record).archive.to_bytes() == archive.to_bytes()
        fresh = q.RawPixelTiledTrainingRequest.replay(historic, record)
        assert q.sample_raw_pixel_tiled_training(fresh).sha256 == table.sha256
    finally:
        historic.server.close()


def test_stream_files_resume_and_verified_external_link_jobs(raw, tmp_path):
    doc, _, _, full, tiled, _ = raw
    model = trained(q.sample_raw_pixel_tiled_training(tiled))
    options = q.RawPixelStreamOptions()
    options.tile_width, options.tile_height = 2, 2
    request = q.RawPixelTiledPredictionRequest(doc, model, tiled.lattice, options)
    stream = q.RawPixelPredictionStream(request)
    blocks = list(stream)
    assert len(blocks) == request.total_windows and stream.completed_windows == len(blocks)
    assert stream.next() is None and stream.sha256
    path, direct = tmp_path / "resume.qws", tmp_path / "direct.qws"
    save = q.RawPixelStreamSaveOptions()
    save.max_new_windows = 1
    assert not q.save_raw_pixel_stream(request, str(path), save).complete
    save.resume_path, save.replace_existing, save.max_new_windows = str(path), True, 2**64 - 1
    resumed = q.save_raw_pixel_stream(request, str(path), save)
    q.save_raw_pixel_stream(request, str(direct))
    assert resumed.complete and resumed.reused_windows == 1 and path.read_bytes() == direct.read_bytes()
    file = q.RawPixelStreamFile(str(path))
    assert not file.verified
    with pytest.raises((ValueError, RuntimeError)):
        file.link()
    reader = q.RawPixelStreamFileReadJob()
    assert reader.open(str(path))
    reader.await_completion()
    assert not reader.open(str(path)) and reader.wait().state == "ready"
    link = reader.take_link()
    assert link is not None and reader.take_file() is None
    file.verify()
    assert reader.read(file, 0)
    reader.await_completion()
    assert not reader.read(file, 1)
    assert reader.wait().state == "ready"
    block = reader.take_block()
    assert block is not None
    np.testing.assert_array_equal(block.labels_numpy(), blocks[0].labels_numpy())
    record = doc.keep_raw_pixel_stream(link)
    assert doc.raw_pixel_stream_record(record).link.same_content(link)
    assert doc.remove_raw_pixel_stream_record(record) and path.exists()


def test_checkpoint_jobs_native_array_and_late_cancel_delivery(raw, tmp_path):
    doc, _, _, _, tiled, _ = raw
    expected = q.sample_raw_pixel_tiled_training(tiled)
    path = tmp_path / "training.qtc"
    options = q.RawPixelTrainingCheckpointOptions()
    options.max_new_windows = 1
    job = q.RawPixelTrainingCheckpointJob()
    assert job.request(tiled, str(path), options) and job.wait(doc).state == "ready"
    prefix = job.take(doc)
    assert prefix is not None and prefix.table is None and not prefix.checkpoint.complete
    assert q.RawPixelTrainingCheckpoint.from_bytes(prefix.checkpoint.to_bytes()).completed_windows == 1
    options.resume_path, options.replace_existing, options.max_new_windows = str(path), True, 2**64 - 1
    assert job.request(tiled, str(path), options) and job.wait(doc).state == "ready"
    completed = job.take(doc)
    assert completed is not None and completed.table is not None and completed.replayed_windows == 1
    assert completed.table.sha256 == expected.sha256
    np.testing.assert_array_equal(completed.table.values_numpy(), expected.values_numpy())
    assert job.request(tiled, str(path), options)
    job.await_completion()
    job.cancel()
    assert job.poll(doc).state == "cancelled" and job.take(doc) is None
    assert q.load_raw_pixel_training_checkpoint(str(path)).complete
    assert job.request(tiled, str(path), options)
    job.await_completion()
    doc.rename_annotation(doc.annotations[0].id, "Edited before delivery")
    assert job.poll(doc).state == "stale" and job.take(doc) is None


def test_stream_owner_backpressure_and_cross_thread_refusal(raw):
    doc, _, _, full, tiled, _ = raw
    model = trained(q.sample_raw_pixel_tiled_training(tiled))
    options = q.RawPixelStreamOptions()
    options.tile_width, options.tile_height = 2, 2
    request = q.RawPixelTiledPredictionRequest(doc, model, tiled.lattice, options)
    job = q.RawPixelPredictionStreamJob()
    assert job.request(request)
    delivered = []
    while len(delivered) < request.total_windows:
        stats = job.wait(doc)
        assert stats.state == "block_ready" and stats.produced_windows == len(delivered) + 1
        delivered.append(job.take(doc))
    assert job.poll(doc).state == "complete" and job.sha256
    errors = []
    def wrong_thread():
        try:
            job.poll(doc)
        except RuntimeError as error:
            errors.append(str(error))
    thread = threading.Thread(target=wrong_thread)
    thread.start()
    thread.join()
    assert errors


def test_native_stream_releases_gil_and_accepts_cross_thread_cancel(tmp_path):
    if not q.has_classifier_training:
        pytest.skip("actual forest backend")
    path = tmp_path / "large.tif"
    tiff(path, 384, 384)
    server = q.open_tiff(str(path))
    try:
        doc = q.Document(server)
        lattice = q.ScientificRawLattice(384, 384, q.ScientificGridAxis(0, 384, 384, 10), q.ScientificGridAxis(0, 384, 384, 10))
        selection = q.ScientificRawOptions()
        selection.channels = 2
        features = q.RawPixelFeatureOptions()
        features.offsets, features.scales = [0, 0], [1, 1]
        features.sigmas = [q.RawPixelFeatureScale(0, 0), q.RawPixelFeatureScale(2, 3)]
        features.gradient_magnitude = True
        recipe = q.RawPixelRecipe(server.raw_metadata, lattice, selection, features)
        cols = len(recipe.feature_schema)
        values = np.array([[0] * cols, [1] * cols, [10] * cols, [11] * cols], dtype=np.float32)
        labels = np.array([0, 0, 1, 1], dtype=np.uint32)
        p = q.ForestParameters()
        p.trees, p.max_depth = 2, 2
        forest = q.train_forest(values, labels, recipe.feature_schema, ["A", "B"], p)
        stream = q.RawPixelPredictionStream(q.RawPixelTiledPredictionRequest(doc, q.RawPixelClassifier(forest, recipe), lattice))
        observed, stop, ready = threading.Event(), threading.Event(), threading.Event()
        def observer():
            ready.set()
            while not stop.wait(.0005):
                if stream.busy:
                    observed.set()
                    stream.cancel()
                    return
        thread = threading.Thread(target=observer)
        thread.start()
        ready.wait()
        try:
            try:
                stream.next()
            except RuntimeError:
                assert observed.is_set()
        finally:
            stop.set()
            thread.join()
        assert observed.is_set(), "observer must execute while native next is producing, not merely before/after"
        with pytest.raises(RuntimeError):
            stream.next()
    finally:
        server.close()


def test_job_shutdown_releases_python_owned_files_on_other_threads(raw, tmp_path):
    doc, _, _, _, tiled, _ = raw
    model = trained(q.sample_raw_pixel_tiled_training(tiled))
    options = q.RawPixelStreamOptions()
    options.tile_width, options.tile_height = 2, 2
    path = tmp_path / "shutdown.qws"
    q.save_raw_pixel_stream(q.RawPixelTiledPredictionRequest(doc, model, tiled.lattice, options), str(path))
    # Native jobs may own a shared_ptr caster that retains the Python file. The
    # finalizer must release the GIL while cancelling/joining, including when
    # the last Python job reference is dropped on another thread.
    code = '''
import gc, os, sys, threading
import qupathxx as q
for i in range(12):
    file = q.RawPixelStreamFile(sys.argv[1])
    job = q.RawPixelStreamFileReadJob()
    assert job.read(file, 0)
    del file
    if i % 2:
        refs = [job]
        del job
        thread = threading.Thread(target=refs.clear, daemon=True)
        thread.start()
        thread.join(5)
        if thread.is_alive(): os._exit(2)
    else:
        del job
    gc.collect()
print("Native raw job shutdown released Python-owned file captures")
'''
    child = subprocess.run([sys.executable, "-I", "-c", code, str(path)], capture_output=True, text=True, timeout=20)
    assert child.returncode == 0, child.stdout + child.stderr
    assert "shutdown released Python-owned file captures" in child.stdout


def test_installed_project_to_project_example(raw, tmp_path):
    if not q.has_project or not q.has_classifier_training:
        pytest.skip("actual training and project persistence")
    doc, _, _, _, _, image = raw
    project = q.Project()
    image_id = project.add_document(doc, "Labelled raw TIFF", str(image), "tiff")
    source = tmp_path / "input.qpx"
    project.save(str(source))
    example = Path(__file__).resolve().parents[1] / "examples/raw_classifier_workflow.py"
    result = subprocess.run([sys.executable, "-I", str(example), str(source), str(tmp_path / "output"),
                             "--image-id", str(image_id), "--channels", "2"], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    summary = json.loads(result.stdout)
    assert summary["rows"] >= 4 and summary["windows"] >= 1
    reopened = q.Project.open(summary["project"])
    output = reopened.document(image_id)
    try:
        assert output.raw_pixel_record_count == 1 and output.raw_pixel_stream_record_count == 1
        model = output.raw_pixel_records[0].archive.model
        link = output.raw_pixel_stream_records[0].link
        assert model is not None and model.sha256 == summary["model_sha256"]
        reader = q.RawPixelStreamFileReadJob()
        assert reader.open_link(link) and reader.wait().state == "ready"
        verified = reader.take_link()
        assert verified is not None and verified.summary.sha256 == summary["stream_sha256"]
    finally:
        output.server.close()


@pytest.mark.skipif(not q.has_project or not q.has_classifier_training, reason="example needs project/training")
@pytest.mark.parametrize("optimized", [False, True], ids=["normal", "optimized"])
def test_raw_classifier_example_resumes_and_retains(optimized, tmp_path):
    pixels = tmp_path / "example-input.tif"
    tiff(pixels)
    project = q.Project()
    image = project.add_image(str(pixels), "tiff")
    doc = project.document(image)
    doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 3, 6)), class_name="A")
    doc.add_detection(q.Roi.rectangle(q.Rect(4, 0, 4, 6)), class_name="B")
    source = tmp_path / "source.qpx"
    project.save(str(source))
    doc.server.close()
    before = source.read_bytes()
    output = tmp_path / "analysis"
    example = Path(__file__).resolve().parents[1] / "examples/raw_classifier_workflow.py"
    command = [sys.executable, "-I", *(["-O"] if optimized else []), str(example), str(source), str(output), "--image-id", str(image), "--channels", "2"]
    done = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=60)
    assert done.returncode == 0, done.stdout + done.stderr
    report = json.loads(done.stdout)
    assert report["rows"] > 0 and report["windows"] > 1 and source.read_bytes() == before
    assert q.load_raw_pixel_training_checkpoint(str(output / "training.qtc")).complete
    assert q.load_raw_pixel_classifier(str(output / "model.qrm")).sha256 == report["model_sha256"]
    again = q.Project.open(report["project"])
    try:
        restored = again.document(image)
        assert restored.raw_pixel_record_count == restored.raw_pixel_stream_record_count == 1
        record, = restored.raw_pixel_stream_records
        file = q.RawPixelStreamFile(record.link.path)
        file.verify()
        assert file.link().same_content(record.link) and file.declared_summary.complete
    finally:
        again.document(image).server.close()
