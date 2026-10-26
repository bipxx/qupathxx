"""Actual desktop raw classifier actions and managed owner workflows."""
import os
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q
from test_raw_pixel_python import tiff
from test_gui_pixel_classifier import event, click as button_click, ready, ppm, sample_world
from test_viewer_session import completed, run

pytestmark = pytest.mark.skipif(not q.has_gui or not q.has_tiff or not q.has_project or
    not q.has_classifier_training or os.environ.get("QUPATHXX_TEST_GUI") != "1",
    reason="opt-in GUI, TIFF/project and actual classifier training")


def click(frame, target):
    # ImGui overlap-enabled tabs need the pointer over the item in the prior
    # frame. Move first, then exercise the real mouse press/release path.
    return [event(frame - 1, q.GuiInputKind.MOVE, target), *button_click(frame, target)]


def scene(tmp_path):
    source = tmp_path / "输入.tif"
    tiff(source)
    project = q.Project()
    image = project.add_image(str(source), "tiff")
    doc = project.document(image)
    a = doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 3, 6)), class_name="A")
    b = doc.add_detection(q.Roi.rectangle(q.Rect(4, 0, 4, 6)), class_name="B")
    root = doc.add_roi(q.Roi.rectangle(q.Rect(0, 0, 8, 6)))
    doc.set_class("A", 0xff0000)
    doc.set_class("B", 0x0000ff)
    settings = q.RawPixelTrainingSettings()
    settings.lattice = q.ScientificRawLattice(8, 6, q.ScientificGridAxis(0, 8, 7, 2), q.ScientificGridAxis(0, 6, 5, 2))
    selection = settings.selection
    selection.channels = 2
    settings.selection = selection
    sampling = settings.sampling
    sampling.max_samples, sampling.max_samples_per_class = 12, 6
    settings.sampling = sampling
    tiles = settings.tiles
    tiles.width = tiles.height = 2
    settings.tiles = tiles
    parameters = settings.forest
    parameters.trees, parameters.max_depth, parameters.work_bytes = 5, 3, 2 * 1024**2
    settings.forest = parameters
    return project, image, doc, [a, b], root, settings


def options(settings, root, tmp_path):
    raw = q.GuiRawPixelOptions()
    raw.open, raw.object_id, raw.training = True, root, settings
    raw.preview = settings.lattice.window(0, 0, 7, 5)
    raw.model_path, raw.stream_path = str(tmp_path / "模型.qrm"), str(tmp_path / "预测.qws")
    stream = raw.stream
    stream.tile_width = stream.tile_height = 2
    stream.work_bytes = 16 * 1024**2
    raw.stream = stream
    o = q.GuiOptions()
    o.width, o.height = 1280, 1000
    o.raw_pixel = raw
    o.replay_timeout_seconds, o.autosave_interval_seconds, o.settle_tiles = 25, 0, True
    return o


def model_for(doc, ids, settings):
    features = settings.features
    features.offsets, features.scales = [0, 0], [1, 1]
    request = q.RawPixelTiledTrainingRequest(doc, ids, settings.lattice, settings.selection,
        features, settings.sampling, settings.tiles)
    return q.train_raw_pixel_tiled_classifier(q.sample_raw_pixel_tiled_training(request), settings.forest)


def test_desktop_train_keep_preview_save_and_export_retained_file(tmp_path):
    p, image, doc, ids, root, settings = scene(tmp_path)
    try:
        o = options(settings, root, tmp_path)
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_TRAIN), ready(8, q.GuiInputWait.RAW_PIXEL_TRAINED),
            *click(12, q.GuiInputTarget.RAW_PIXEL_KEEP_TRAINING), ready(16, q.GuiInputWait.RAW_PIXEL_KEPT),
            *click(20, q.GuiInputTarget.RAW_PIXEL_FILES_TAB), *click(24, q.GuiInputTarget.RAW_PIXEL_PREDICT),
            ready(28, q.GuiInputWait.RAW_PIXEL_PREVIEW_READY), *click(32, q.GuiInputTarget.RAW_PIXEL_SAVE_MODEL),
            ready(36, q.GuiInputWait.RAW_PIXEL_MODEL_SAVED), *click(40, q.GuiInputTarget.RAW_PIXEL_EXPORT),
            ready(44, q.GuiInputWait.RAW_PIXEL_FILE_SAVED), *click(48, q.GuiInputTarget.RAW_PIXEL_RECORDS_TAB),
            *click(52, q.GuiInputTarget.RAW_PIXEL_KEEP_LINK)]
        report = q.show(p, frame_limit=85, options=o)
        r = report.raw_pixel
        assert report.replay_complete and not r.error, r.error
        assert r.training_record and r.stream_record and r.model and r.training
        assert r.model.to_bytes() == model_for(doc, ids, settings).to_bytes()
        assert doc.raw_pixel_record_count == doc.raw_pixel_stream_record_count == 1
        assert r.link.summary.complete and q.load_raw_pixel_classifier(str(tmp_path / "模型.qrm")).sha256 == r.model.sha256
        saved = tmp_path / "retained.qpx"
        p.save(str(saved))
        again = q.Project.open(str(saved))
        try:
            record, = again.document(image).raw_pixel_stream_records
            file = q.RawPixelStreamFile(record.link.path)
            file.verify()
            assert file.link().same_content(record.link)
        finally:
            again.document(image).server.close()
    finally:
        doc.server.close()


def test_desktop_checkpoint_resume_without_duplicate_sampling(tmp_path):
    p, _, doc, _, root, settings = scene(tmp_path)
    try:
        checkpoint = tmp_path / "training.qtc"
        settings.checkpoint_path = str(checkpoint)
        save = settings.checkpoint
        save.max_new_windows = 1
        settings.checkpoint = save
        o = options(settings, root, tmp_path)
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_TRAIN), ready(9, q.GuiInputWait.RAW_PIXEL_FINISHED)]
        first = q.show(p, frame_limit=35, options=o).raw_pixel
        assert first.state == "partial" and first.checkpoint and first.training is None and first.model is None
        save.resume_path, save.replace_existing, save.max_new_windows = str(checkpoint), True, 2**64 - 1
        settings.checkpoint = save
        o = options(settings, root, tmp_path)
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_TRAIN), ready(9, q.GuiInputWait.RAW_PIXEL_TRAINED)]
        result = q.show(p, frame_limit=40, options=o).raw_pixel
        assert not result.error and result.training and result.model and result.checkpoint.complete
    finally:
        doc.server.close()


def test_desktop_historical_open_keep_remove_preserves_external_file(tmp_path):
    p, _, doc, ids, root, settings = scene(tmp_path)
    try:
        model = model_for(doc, ids, settings)
        output = tmp_path / "历史.qws"
        stream = q.RawPixelStreamOptions()
        stream.tile_width = stream.tile_height = 2
        q.save_raw_pixel_stream(q.RawPixelTiledPredictionRequest(doc, model, settings.lattice, stream, object_id=root), str(output))
        original = output.read_bytes()
        o = options(settings, root, tmp_path)
        raw = o.raw_pixel
        raw.stream_path = str(output)
        o.raw_pixel = raw
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_FILES_TAB), *click(8, q.GuiInputTarget.RAW_PIXEL_OPEN_FILE),
            ready(12, q.GuiInputWait.RAW_PIXEL_FILE_READ), *click(16, q.GuiInputTarget.RAW_PIXEL_RECORDS_TAB),
            *click(20, q.GuiInputTarget.RAW_PIXEL_KEEP_LINK), *click(24, q.GuiInputTarget.RAW_PIXEL_OPEN_RECORD),
            ready(28, q.GuiInputWait.RAW_PIXEL_FILE_READ), *click(32, q.GuiInputTarget.RAW_PIXEL_REMOVE_RECORD)]
        report = q.show(p, frame_limit=60, options=o)
        assert report.replay_complete and not report.raw_pixel.error
        assert report.raw_pixel.historical and report.raw_pixel.prediction is None
        assert doc.raw_pixel_stream_record_count == 0 and output.read_bytes() == original
    finally:
        doc.server.close()


def test_raw_overlay_toggle_invalidates_cached_canvas_at_fractional_edges(tmp_path):
    captures = []
    for hidden in (False, True):
        directory = tmp_path / str(hidden)
        directory.mkdir()
        p, _, doc, ids, root, settings = scene(directory)
        try:
            o = options(settings, root, directory)
            raw = o.raw_pixel
            raw.model = model_for(doc, ids, settings)
            o.raw_pixel = raw
            o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_FILES_TAB), *click(8, q.GuiInputTarget.RAW_PIXEL_PREDICT),
                ready(12, q.GuiInputWait.RAW_PIXEL_PREVIEW_READY)]
            if hidden:
                o.inputs += click(16, q.GuiInputTarget.RAW_PIXEL_TOGGLE_OVERLAY)
            o.inputs += click(20, q.GuiInputTarget.RAW_PIXEL_HIDE)
            path = directory / "canvas.ppm"
            report = q.show(p, frame_limit=50, capture_path=str(path), options=o)
            assert report.raw_pixel.preview_ready and not report.raw_pixel.error and report.overlay_cache_hits > 0
            captures.append((ppm(path), report))
        finally:
            doc.server.close()
    visible, shown = captures[0]
    baseline, other = captures[1]
    checked = 0
    for y in (1.4, 2.4, 3.4, 4.4):
        for x in (1.4, 2.4, 5.4, 6.4):
            raw = sample_world(baseline, other, x, y)
            # Exclude tools, annotation strokes and panel pixels.
            if np.max(raw) > 2:
                continue
            label = shown.raw_pixel.prediction.labels_numpy()[int(y * 5 / 6), int(x * 7 / 8)]
            color = np.array([255, 0, 0] if label == 1 else [0, 0, 255])
            expected = np.rint(raw * (1 - 115 / 255) + color * 115 / 255) if label else raw
            np.testing.assert_allclose(sample_world(visible, shown, x, y), expected, atol=3)
            checked += 1
    assert checked >= 8


def test_managed_raw_training_prediction_resume_read_and_saved_project(tmp_path):
    p, image, doc, ids, root, settings = scene(tmp_path)
    source = tmp_path / "managed.qpx"
    p.save(str(source))
    doc.server.close()
    session = q.ViewerSession(str(source))
    output, saved = tmp_path / "managed.qws", tmp_path / "managed-result.qpx"
    observations = {}
    def producer():
        trained = completed(session, session.train_raw_pixels(image, ids, settings))
        assert trained.raw_pixel_training and trained.record_id
        model = trained.raw_pixel_training.model
        result = completed(session, session.predict_raw_pixels(image, model, settings.lattice.window(1, 1, 2, 2), root))
        observations["prediction"] = result.raw_pixel_prediction
        stream, save = q.RawPixelStreamOptions(), q.RawPixelStreamSaveOptions()
        stream.tile_width = stream.tile_height = 2
        save.max_new_windows = 1
        ticket = session.export_raw_pixels(image, model, settings.lattice, str(output), stream, save, root, False)
        partial = session.wait(ticket, 20)
        assert partial.state == "partial" and partial.raw_pixel_link and not partial.raw_pixel_link.summary.complete
        session.take(ticket)
        save.resume_path, save.replace_existing, save.max_new_windows = str(output), True, 2**64 - 1
        done = completed(session, session.export_raw_pixels(image, model, settings.lattice, str(output), stream, save, root))
        historical = completed(session, session.read_raw_pixel_stream(image, "", record_id=done.record_id))
        assert historical.verified and historical.raw_pixel_prediction and not historical.committed
        completed(session, session.save(str(saved)))
        completed(session, session.close(True))
    report = run(session, producer, frames=1000)
    assert report.frames > 0 and observations["prediction"].labels_numpy().shape == (2, 2)
    result = q.Project.open(str(saved))
    try:
        assert result.document(image).raw_pixel_record_count == result.document(image).raw_pixel_stream_record_count == 1
    finally:
        result.document(image).server.close()


def test_desktop_prediction_prefix_resume_preserves_committed_bytes(tmp_path):
    p, _, doc, ids, root, settings = scene(tmp_path)
    try:
        model = model_for(doc, ids, settings)
        o = options(settings, root, tmp_path)
        raw = o.raw_pixel
        raw.model = model
        save = raw.save
        save.max_new_windows = 1
        raw.save = save
        o.raw_pixel = raw
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_FILES_TAB),
            *click(8, q.GuiInputTarget.RAW_PIXEL_EXPORT), ready(12, q.GuiInputWait.RAW_PIXEL_FILE_SAVED)]
        first = q.show(p, frame_limit=35, options=o).raw_pixel
        output = Path(raw.stream_path)
        prefix = output.read_bytes()
        assert first.state == "partial" and not first.error and first.link.summary.completed_windows == 1
        assert not first.link.summary.complete
        save.resume_path, save.replace_existing, save.max_new_windows = str(output), True, 2**64 - 1
        raw.save = save
        o.raw_pixel = raw
        second = q.show(p, frame_limit=40, options=o).raw_pixel
        assert not second.error and second.link.summary.complete
        # QWS1 replaces the terminal summary; its header and committed blocks
        # must survive byte-for-byte (footer grammar: 292 + 8 per class).
        footer_bytes = 292 + 8 * len(first.link.summary.class_counts)
        assert output.read_bytes().startswith(prefix[:-footer_bytes])
        file = q.RawPixelStreamFile(str(output))
        file.verify()
        assert file.link().same_content(second.link)
        assert doc.raw_pixel_stream_record_count == 0
    finally:
        doc.server.close()


def test_desktop_invalid_model_preserves_current_model_and_can_predict(tmp_path):
    p, _, doc, ids, root, settings = scene(tmp_path)
    try:
        model = model_for(doc, ids, settings)
        o = options(settings, root, tmp_path)
        raw = o.raw_pixel
        raw.model = model
        Path(raw.model_path).write_bytes(b"invalid model")
        o.raw_pixel = raw
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_FILES_TAB),
            *click(8, q.GuiInputTarget.RAW_PIXEL_LOAD_MODEL), ready(12, q.GuiInputWait.RAW_PIXEL_FINISHED)]
        failed = q.show(p, frame_limit=35, options=o).raw_pixel
        assert failed.state == "failed" and failed.error and failed.model.sha256 == model.sha256
        assert Path(raw.model_path).read_bytes() == b"invalid model"
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_FILES_TAB),
            *click(8, q.GuiInputTarget.RAW_PIXEL_LOAD_MODEL), ready(12, q.GuiInputWait.RAW_PIXEL_FINISHED),
            *click(16, q.GuiInputTarget.RAW_PIXEL_PREDICT), ready(20, q.GuiInputWait.RAW_PIXEL_PREVIEW_READY)]
        retried = q.show(p, frame_limit=45, options=o).raw_pixel
        assert not retried.error and retried.preview_ready and retried.model.sha256 == model.sha256
        assert retried.prediction.labels_numpy().shape == (5, 7)
    finally:
        doc.server.close()


def test_desktop_image_switch_retires_raw_prediction_and_gpu_preview(tmp_path):
    p, image, doc, ids, root, settings = scene(tmp_path)
    other = q.Document(q.Image(8, 6, [0] * 8 * 6 * 3))
    p.add_document(other, "Other")
    p.active_image = image
    try:
        o = options(settings, root, tmp_path)
        raw = o.raw_pixel
        raw.model = model_for(doc, ids, settings)
        o.raw_pixel = raw
        o.inputs = [*click(4, q.GuiInputTarget.RAW_PIXEL_FILES_TAB),
            *click(8, q.GuiInputTarget.RAW_PIXEL_PREDICT), ready(12, q.GuiInputWait.RAW_PIXEL_PREVIEW_READY),
            event(20, q.GuiInputKind.NEXT_IMAGE_KEY)]
        report = q.show(p, frame_limit=45, options=o)
        assert report.replay_complete and report.image_switches == 1
        assert report.raw_pixel.prediction is None and not report.raw_pixel.preview_ready
        assert report.raw_pixel.preview_bytes == 0
    finally:
        doc.server.close()
