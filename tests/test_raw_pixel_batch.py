"""Native project batches retain raw streams and verify current input reuse."""
import csv
import io
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_raw_pixel_python import tiff, trained

pytestmark = pytest.mark.skipif(not q.has_project or not q.has_tiff,
                                reason="project and quantitative TIFF required")
CLI = Path(_native.__file__).with_name("qupathxx_batch.exe" if os.name == "nt" else "qupathxx_batch")
FIXTURE = Path(__file__).parent / "data/pixel-streams/rectangle-v1-windows.qps"


def scene(tmp_path, widths=(8, 10), bad=False):
    project, docs = q.Project(), []
    for i, width in enumerate(widths):
        path = tmp_path / f"source-{i}.tif"
        tiff(path, width, 6)
        if bad and i == 1:
            path = tmp_path / "display.ppm"
            path.write_bytes(b"P6\n8 6\n255\n" + bytes([30, 60, 90]) * 48)
            width = 8
        image = project.add_image(str(path), "ppm" if path.suffix == ".ppm" else "tiff")
        doc = project.document(image)
        ids = [doc.add_detection(q.Roi.rectangle(q.Rect(0, 0, 3, 6)), class_name="A"),
               doc.add_detection(q.Roi.rectangle(q.Rect(4, 0, width - 4, 6)), class_name="B")]
        root = doc.add_roi(q.Roi.rectangle(q.Rect(0, 0, width, 6)))
        docs.append((image, doc, root, ids))
    doc = docs[0][1]
    lattice = q.ScientificRawLattice(8, 6, q.ScientificGridAxis(0, 8, 8), q.ScientificGridAxis(0, 6, 6))
    selection, features = q.ScientificRawOptions(), q.RawPixelFeatureOptions()
    selection.channels = 2
    features.offsets, features.scales = [0, 0], [1, 1]
    request = q.RawPixelTiledTrainingRequest(doc, docs[0][3], lattice, selection, features)
    model = trained(q.sample_raw_pixel_tiled_training(request))
    return project, docs, model


def options(model):
    o = q.ProjectBatchOptions()
    o.kind, o.raw_pixel_model = q.ProjectBatchKind.RAW_PIXELS, model
    v = q.RawPixelStreamOptions()
    v.tile_width = v.tile_height = 2
    v.work_bytes = 16 * 1024**2
    o.raw_pixels = v
    return o


def request(project, model, o=None):
    return q.ProjectBatchRequest(project, q.StainTransform([[1, 0, 0], [0, 1, 0]]), [], o or options(model))


def success(result):
    assert result.state == "succeeded", result.error + repr([(v.state, v.error) for v in result.images])
    return result


def close(project):
    for image in project.images:
        project.document(image.id).server.close()


def test_raw_batch_numeric_csv_reopen_and_exact_hash_only_reuse(tmp_path):
    p, docs, model = scene(tmp_path)
    try:
        o = options(model)
        capture = request(p, model, o)
        manifest = json.loads(capture.parameters_json)["raw_pixels"]
        assert manifest["model_sha256"] == model.sha256 and manifest["x"] is None
        first = success(q.run_project_batch(capture, str(tmp_path / "out")))
        assert first.saved == 2 and first.reused == first.failed == 0
        old_bytes = {}
        for result, (image, original, root, _) in zip(first.images, docs):
            restored = q.Project.open(result.path)
            try:
                record, = restored.document(image).raw_pixel_stream_records
                assert record.link.object_id == root and Path(record.link.path).name == f"object-{root}.qws"
                file = q.RawPixelStreamFile(record.link.path)
                file.verify()
                assert file.link().same_content(record.link)
                m = original.metadata
                lattice = q.ScientificRawLattice(m.width, m.height, q.ScientificGridAxis(0, m.width, m.width), q.ScientificGridAxis(0, m.height, m.height))
                expected = q.RawPixelPredictionStream(q.RawPixelTiledPredictionRequest(original, model, lattice, o.raw_pixels, object_id=root))
                summary = file.declared_summary
                assert summary.complete and result.inferred_blocks == result.verified_blocks == summary.completed_windows
                assert result.hashed_blocks == 0 and result.stream_bytes == summary.bytes
                for i in range(summary.completed_windows):
                    actual, block = file.read_block(i), expected.next()
                    for name in ("labels_numpy", "vote_fractions_numpy", "valid_numpy"):
                        np.testing.assert_array_equal(getattr(actual, name)(), getattr(block, name)())
                assert expected.next() is None and summary.input_sha256 == expected.input_sha256
                rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(result.path))))
                assert len(rows) == len(model.forest.classes)
                assert {v["class_name"] for v in rows} == set(model.forest.classes)
                assert all(v["path"] == f"object-{root}.qws" for v in rows)
                old_bytes[record.link.path] = Path(record.link.path).read_bytes()
            finally:
                close(restored)
        again = success(q.run_project_batch(capture, str(tmp_path / "out")))
        assert again.reused == 2 and again.saved == 0
        for before, after in zip(first.images, again.images):
            assert before.path == after.path and before.pixel_sha256 == after.pixel_sha256
            assert after.inferred_blocks == 0 and after.hashed_blocks == after.verified_blocks == before.inferred_blocks
        assert all(Path(path).read_bytes() == data for path, data in old_bytes.items())
        assert all(not doc.raw_pixel_stream_records for _, doc, _, _ in docs)
        assert not list((tmp_path / "out").glob(".qupathxx-batch-work-*"))
    finally:
        close(p)


@pytest.mark.parametrize("policy", [q.ProjectBatchFailure.STOP, q.ProjectBatchFailure.CONTINUE])
def test_raw_batch_stop_continue_with_incompatible_reader(tmp_path, policy):
    p, docs, model = scene(tmp_path, (8, 8, 10), bad=True)
    try:
        o = options(model)
        o.failure = policy
        result = q.run_project_batch(request(p, model, o), str(tmp_path / "out"))
        assert result.failed == 1 and result.images[1].state == "failed"
        assert result.saved == (1 if policy == q.ProjectBatchFailure.STOP else 2)
        assert result.state == ("failed" if policy == q.ProjectBatchFailure.STOP else "partial")
    finally:
        close(p)


@pytest.mark.parametrize("fault", ["missing", "block", "checkpoint"])
def test_raw_batch_tampered_outputs_refuse_reuse_and_no_resume_preserves_them(tmp_path, fault):
    p, docs, model = scene(tmp_path, (8,))
    try:
        r = request(p, model)
        first = success(q.run_project_batch(r, str(tmp_path / "out")))
        checkpoint = Path(first.images[0].path)
        saved = q.Project.open(str(checkpoint))
        stream = Path(saved.document(docs[0][0]).raw_pixel_stream_records[0].link.path)
        close(saved)
        if fault == "missing":
            stream.unlink()
        elif fault == "block":
            data = bytearray(stream.read_bytes())
            data[len(data) // 2] ^= 1
            stream.write_bytes(data)
        else:
            with sqlite3.connect(checkpoint) as db:
                db.execute("UPDATE objects SET name='tampered'")
        snapshot = {v: v.read_bytes() for v in checkpoint.parent.iterdir() if v.is_file()}
        assert q.run_project_batch(r, str(tmp_path / "out")).state == "failed"
        o = options(model)
        o.resume = False
        replacement = success(q.run_project_batch(request(p, model, o), str(tmp_path / "out")))
        assert replacement.saved == 1 and replacement.images[0].path != str(checkpoint)
        assert all(v.read_bytes() == data for v, data in snapshot.items())
    finally:
        close(p)


@pytest.mark.parametrize("field,value", [("pixel_total_limit", 4096), ("result_budget", 1), ("materialize_budget", 1)])
def test_raw_batch_quotas_cleanup_only_owned_work(tmp_path, field, value):
    p, _, model = scene(tmp_path, (8,))
    try:
        o = options(model)
        setattr(o, field, value)
        directory = tmp_path / "out"
        directory.mkdir()
        unrelated = directory / "keep.txt"
        unrelated.write_bytes(b"preserve")
        result = q.run_project_batch(request(p, model, o), str(directory))
        assert result.state == "failed" and result.saved == 0
        assert unrelated.read_bytes() == b"preserve"
        assert not list(directory.rglob("*.qws")) and not list(directory.rglob("result.qpxproj"))
    finally:
        close(p)


def test_raw_batch_explicit_fraction_mismatch_is_per_image_failure(tmp_path):
    p, _, model = scene(tmp_path)
    try:
        o = options(model)
        o.raw_pixel_x, o.raw_pixel_y = q.ScientificGridAxis(0, 8, 7, 2), q.ScientificGridAxis(0, 6, 5, 2)
        o.failure = q.ProjectBatchFailure.CONTINUE
        report = q.run_project_batch(request(p, model, o), str(tmp_path / "out"))
        assert report.state == "partial" and report.failed == 2 and report.saved == 0
        assert not list((tmp_path / "out").rglob("*.qws"))
        o.raw_pixel_y = None
        with pytest.raises(ValueError):
            request(p, model, o)
    finally:
        close(p)


def test_raw_batch_mixed_rgb_raw_links_and_both_typed_reuse_paths(tmp_path):
    p, docs, model = scene(tmp_path, (8,))
    try:
        image, doc, root, _ = docs[0]
        lattice = q.ScientificRawLattice(8, 6, q.ScientificGridAxis(0, 8, 8), q.ScientificGridAxis(0, 6, 6))
        native = q.RawPixelTiledPredictionRequest(doc, model, lattice, object_id=root)
        raw_path = tmp_path / "prior.qws"
        q.save_raw_pixel_stream(native, str(raw_path))
        file = q.RawPixelStreamFile(str(raw_path))
        file.verify()
        doc.keep_raw_pixel_stream(file.link())
        rgb_model = q.PixelStreamFile(str(FIXTURE)).read_block(0).model
        rgb_path = tmp_path / "prior.qps"
        q.save_pixel_stream(q.PixelPredictionStream(doc, root, rgb_model), str(rgb_path))
        rgb = q.PixelStreamFile(str(rgb_path))
        rgb.verify()
        doc.keep_pixel_stream(rgb.link())
        for kind in ("raw", "rgb"):
            o = options(model) if kind == "raw" else q.ProjectBatchOptions()
            if kind == "rgb":
                o.kind, o.pixel_model = q.ProjectBatchKind.PIXELS, rgb_model
            r = request(p, model, o)
            first = success(q.run_project_batch(r, str(tmp_path / kind)))
            second = success(q.run_project_batch(r, str(tmp_path / kind)))
            assert first.saved == second.reused == 1
            saved = q.Project.open(first.images[0].path)
            assert saved.document(image).raw_pixel_stream_record_count == (2 if kind == "raw" else 1)
            assert saved.document(image).pixel_stream_record_count == (1 if kind == "raw" else 2)
            close(saved)
        if os.name=="nt":
            source=tmp_path/"linked-source.qpxproj";p.save(str(source))
            first=q.Project.open(str(source));second=q.Project.open(str(source))
            try:
                folder=tmp_path/"snapshots";folder.mkdir()
                plain=folder/"plain.qpxproj";extended=folder/"extended.qpxproj"
                text=str(extended)
                argument="\\\\?\\UNC\\"+text[2:] if text.startswith("\\\\") else "\\\\?\\"+text
                first.save_new(str(plain));second.save_new(argument)
                # Images, QSL1 and QWL1 locators all contribute to the input seal.
                assert plain.read_bytes()==extended.read_bytes()
            finally:
                close(first);close(second)
    finally:
        close(p)


def test_raw_batch_cli_moved_bundle_and_unchanged_source_project(tmp_path):
    p, docs, model = scene(tmp_path)
    try:
        source, model_path = tmp_path / "项目.qpx", tmp_path / "模型.qrm"
        p.save(str(source))
        q.save_raw_pixel_classifier(model, str(model_path))
        before = source.read_bytes()
        output = tmp_path / "out"
        cmd = [str(CLI), str(source), str(output), "--kind", "raw-pixels", "--raw-pixel-model", str(model_path),
               "--raw-pixel-tile-width", "2", "--raw-pixel-tile-height", "2", "--raw-pixel-work-bytes", str(16 * 1024**2)]
        done = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", timeout=40)
        assert done.returncode == 0, done.stdout + done.stderr
        result = json.loads(done.stdout.splitlines()[-1])
        assert result["saved"] == 2
        done = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", timeout=40)
        assert done.returncode == 0, done.stdout + done.stderr
        assert json.loads(done.stdout.splitlines()[-1])["reused"] == 2
        moved = tmp_path / "moved"
        def copy_path(path):
            text=str(path.resolve())
            if os.name!='nt':return text
            return "\\\\?\\UNC\\"+text[2:] if text.startswith("\\\\") else "\\\\?\\"+text
        # The fixture copy must support the same long paths as native project
        # opening; CopyFile2 otherwise fails before the moved-bundle assertions.
        shutil.copytree(copy_path(output), copy_path(moved))
        for entry in result["images"]:
            original = Path(entry["path"])
            relative = next(v.relative_to(output) for v in output.rglob("result.qpxproj") if v.samefile(original))
            checkpoint = moved / relative
            saved = q.Project.open(str(checkpoint))
            record, = saved.document(entry["image_id"]).raw_pixel_stream_records
            assert Path(record.link.path).parent == checkpoint.parent
            file = q.RawPixelStreamFile(record.link.path)
            file.verify()
            close(saved)
        assert source.read_bytes() == before
    finally:
        close(p)


def test_raw_batch_reopened_changed_source_is_hashed_and_recomputed(tmp_path):
    p, _, model = scene(tmp_path, (8,))
    source = tmp_path / "source.qpx"
    p.save(str(source))
    first = success(q.run_project_batch(request(p, model), str(tmp_path / "out")))
    close(p)
    pixels = tmp_path / "source-0.tif"
    data = bytearray(pixels.read_bytes())
    data[160] ^= 64
    pixels.write_bytes(data)
    current = q.Project.open(str(source))
    try:
        after = success(q.run_project_batch(request(current, model), str(tmp_path / "out")))
        assert after.saved == 1 and after.reused == 0
        assert after.images[0].pixel_sha256 != first.images[0].pixel_sha256
        assert after.images[0].path != first.images[0].path
        assert after.images[0].hashed_blocks > 0
    finally:
        close(current)


def test_raw_batch_fractional_axes_actual_training_and_async_snapshot(tmp_path):
    p, docs, _ = scene(tmp_path, (8,))
    try:
        image, doc, root, ids = docs[0]
        lattice = q.ScientificRawLattice(8, 6, q.ScientificGridAxis(0, 8, 7, 2), q.ScientificGridAxis(0, 6, 5, 2))
        selection, features = q.ScientificRawOptions(), q.RawPixelFeatureOptions()
        selection.channels = 2
        features.offsets, features.scales = [0, 0], [1, 1]
        table = q.sample_raw_pixel_tiled_training(q.RawPixelTiledTrainingRequest(doc, ids, lattice, selection, features))
        model = trained(table)
        o = options(model)
        o.raw_pixel_x, o.raw_pixel_y = lattice.x, lattice.y
        captured = request(p, model, o)
        original_name = doc.annotation(root).name
        doc.rename_annotation(root, "later live edit")
        job = q.ProjectBatchJob()
        assert job.request(captured, str(tmp_path / "out"))
        assert job.wait().state == "succeeded"
        report = success(job.result)
        restored = q.Project.open(report.images[0].path)
        try:
            assert restored.document(image).annotation(root).name == original_name
            record, = restored.document(image).raw_pixel_stream_records
            assert record.link.lattice.x.samples == 7 and record.link.lattice.x.extent == 8
            file = q.RawPixelStreamFile(record.link.path)
            file.verify()
            expected = q.RawPixelPredictionStream(q.RawPixelTiledPredictionRequest(restored.document(image), model, lattice, o.raw_pixels, object_id=root))
            for i in range(file.declared_summary.completed_windows):
                block = expected.next()
                np.testing.assert_array_equal(file.read_block(i).labels_numpy(), block.labels_numpy())
                np.testing.assert_array_equal(file.read_block(i).vote_fractions_numpy(), block.vote_fractions_numpy())
        finally:
            close(restored)
    finally:
        close(p)
