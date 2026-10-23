"""Project-wide pixel output, verified hash-only reuse, ownership and native CLI."""
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_pixel_input_digest import setup

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage required')


def scene(images=2, targets=2, size=64):
    project = q.Project()
    docs = []
    for i in range(images):
        doc, root, model = setup(size)
        roots = [root]
        if not targets:
            doc.remove_annotation(root)
            roots = []
        elif targets > 1:
            roots.append(doc.add_roi(q.Roi.rectangle(q.Rect(0, 0, size//2, size//2))))
        image = project.add_document(doc, f'Image {i}, "组织"\nquoted')
        docs.append((image, doc, roots))
    return project, docs, model


def options(model):
    o = q.ProjectBatchOptions()
    o.kind = q.ProjectBatchKind.PIXELS
    o.pixel_model = model
    stream = q.PixelStreamOptions()
    stream.block_size = 16
    prediction = q.PixelPredictionOptions()
    prediction.tile_size = 16
    prediction.work_bytes = 8*1024*1024
    stream.prediction = prediction
    o.pixels = stream
    return o


def request(project, model, o=None, selection=None):
    # Pixel model owns its transform, independent of the legacy stain argument.
    return q.ProjectBatchRequest(project, q.StainTransform([[1, 0, 0], [0, 1, 0]]), selection or [], o or options(model))


def run(r, path):
    result = q.run_project_batch(r, str(path))
    assert result.state == 'succeeded', result.error + repr([(i.state, i.error) for i in result.images])
    return result


@pytest.mark.parametrize('block_size', [16, 256])
def test_fresh_batch_all_numeric_blocks_csv_and_hash_only_resume(tmp_path, block_size):
    p, docs, model = scene()
    o = options(model)
    stream_options = o.pixels
    stream_options.block_size = block_size
    o.pixels = stream_options
    captured = request(p, model, o)
    revisions = [doc.revision for _, doc, _ in docs]
    first = run(captured, tmp_path/'out')
    assert first.saved == 2 and not first.reused
    hashes = {}
    for image_result, (image, original, roots) in zip(first.images, docs):
        saved = q.Project.open(image_result.path).document(image)
        assert image_result.objects == image_result.records == 2
        records = saved.pixel_stream_records
        assert len(records) == 2 and not original.pixel_stream_records
        blocks = total = 0
        for record, root in zip(records, roots):
            path = Path(record.link.path)
            assert path.parent == Path(image_result.path).parent
            assert path.name == f'object-{root}.qps'
            hashes[path] = hashlib.sha256(path.read_bytes()).hexdigest()
            file = q.PixelStreamFile(str(path))
            file.verify()
            expected = q.PixelPredictionStream(original, root, model, o.pixels)
            assert file.input_sha256() == expected.input_sha256()
            assert file.link().same_content(record.link)
            total += file.declared_summary.bytes
            blocks += file.declared_summary.blocks
            for i in range(file.declared_summary.blocks):
                block = file.read_block(i)
                direct = q.predict_pixel_region(q.PixelPredictionRequest(original, root, model, o.pixels.prediction, block.bounds))
                for method in ['labels_numpy', 'valid_numpy', 'vote_fractions_numpy']:
                    np.testing.assert_array_equal(getattr(block, method)(), getattr(direct, method)())
        assert image_result.inferred_blocks == image_result.verified_blocks == blocks
        assert image_result.hashed_blocks == 0 and image_result.stream_bytes == total
        rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(image_result.path))))
        assert len(rows) == len(roots)*len(model.classes)
        for row in rows:
            assert row['batch_image_name'] == image_result.image_name
            assert row['class_name'] in model.classes
            assert row['path'] == f"object-{row['object_id']}.qps"
            assert row['model_sha256'] == model.sha256
    second = run(captured, tmp_path/'out')
    assert second.reused == 2 and not second.saved
    assert [i.path for i in second.images] == [i.path for i in first.images]
    for before, after in zip(first.images, second.images):
        assert not after.inferred_blocks
        assert after.hashed_blocks == after.verified_blocks == before.inferred_blocks
        assert after.pixel_sha256 == before.pixel_sha256 and after.stream_bytes == before.stream_bytes
    assert [doc.revision for _, doc, _ in docs] == revisions
    assert all(hashlib.sha256(path.read_bytes()).hexdigest() == digest for path, digest in hashes.items())
    assert not list((tmp_path/'out').glob('.qupathxx-batch-work-*'))


@pytest.mark.parametrize('fault', ['missing', 'footer', 'block', 'checkpoint'])
def test_resume_rejects_changed_completed_output_and_no_resume_preserves_it(tmp_path, fault):
    p, docs, model = scene(1)
    r = request(p, model)
    first = run(r, tmp_path/'out')
    checkpoint = Path(first.images[0].path)
    saved = q.Project.open(str(checkpoint)).document(docs[0][0])
    path = Path(saved.pixel_stream_records[0].link.path)
    if fault == 'missing':
        path.unlink()
    elif fault == 'checkpoint':
        import sqlite3
        with sqlite3.connect(checkpoint) as db:
            db.execute("UPDATE objects SET name='tampered'")
    else:
        data = bytearray(path.read_bytes())
        data[-1 if fault == 'footer' else len(data)//2] ^= 1
        path.write_bytes(data)
    result = q.run_project_batch(r, str(tmp_path/'out'))
    assert result.state == 'failed' and result.failed == 1 and not result.reused
    assert checkpoint.exists()
    o = options(model)
    o.resume = False
    recovered = run(request(p, model, o), tmp_path/'out')
    assert recovered.saved == 1 and recovered.images[0].path != str(checkpoint)
    assert checkpoint.exists()


@pytest.mark.parametrize('field', ['result_budget', 'csv_limit', 'checkpoint_limit', 'pixel_file_limit', 'pixel_total_limit'])
def test_failed_image_leaves_no_completed_bundle_or_pixel_files(tmp_path, field):
    p, _, model = scene(1)
    o = options(model)
    setattr(o, field, 4096 if field in ['checkpoint_limit', 'pixel_file_limit', 'pixel_total_limit'] else 1)
    result = q.run_project_batch(request(p, model, o), str(tmp_path/'out'))
    assert result.state == 'failed' and result.failed == 1
    assert not list((tmp_path/'out').rglob('*.qps'))
    assert not list((tmp_path/'out').rglob('result.qpxproj'))
    assert not list((tmp_path/'out').rglob('pixels-*'))


def test_existing_historical_catalog_preserved_and_excluded_from_reuse_outputs(tmp_path):
    p, docs, model = scene(1)
    _, doc, roots = docs[0]
    prior = tmp_path/'prior.qps'
    q.save_pixel_stream(q.PixelPredictionStream(doc, roots[0], model, options(model).pixels), str(prior))
    old = doc.keep_pixel_stream(q.prepare_pixel_stream_link(str(prior)), 'Existing unrelated output')
    r = request(p, model)
    first = run(r, tmp_path/'out')
    saved = q.Project.open(first.images[0].path).document(docs[0][0])
    assert len(saved.pixel_stream_records) == 3 and len(doc.pixel_stream_records) == 1
    assert saved.pixel_stream_record(old).name == 'Existing unrelated output'
    assert run(r, tmp_path/'out').reused == 1 and prior.exists()
    prior.unlink()
    assert run(r, tmp_path/'out').reused == 1


def test_owned_partial_and_foreign_directories_are_ignored_and_preserved(tmp_path):
    p, _, model = scene(1)
    r = request(p, model)
    first = run(r, tmp_path/'out')
    folder = Path(first.images[0].path).parent.parent
    for token, owner in [('a'*32, True), ('b'*32, False)]:
        path = folder/f'pixels-{token}'
        path.mkdir()
        (path/'owner').write_text('qupathxx pixel batch 1\n'+token if owner else 'not owned', encoding='utf-8')
        (path/'unrelated.txt').write_text('keep', encoding='utf-8')
    assert run(r, tmp_path/'out').reused == 1
    assert len(list(folder.rglob('unrelated.txt'))) == 2


def test_bundle_relative_links_survive_batch_directory_relocation(tmp_path):
    p, docs, model = scene(1)
    r = request(p, model)
    first = run(r, tmp_path/'before')
    before = Path(first.images[0].path)
    shutil.move(str(tmp_path/'before'), str(tmp_path/'after'))
    second = run(r, tmp_path/'after')
    assert second.reused == 1 and second.images[0].path != str(before)
    doc = q.Project.open(second.images[0].path).document(docs[0][0])
    assert all(Path(record.link.path).is_file() for record in doc.pixel_stream_records)


@pytest.mark.parametrize('change', ['vote', 'block', 'tile', 'roi', 'model'])
def test_capture_or_prediction_changes_create_fresh_checkpoints(tmp_path, change):
    p, docs, model = scene(1)
    first = run(request(p, model), tmp_path/'out')
    o = options(model)
    if change == 'model':
        features = model.options
        features.work_bytes += 1024*1024
        o.pixel_model = q.PixelClassifier(model.forest, features, model.transform, model.resolution)
    elif change == 'vote':
        v = o.pixels
        prediction = v.prediction
        prediction.min_vote_fraction = .9
        v.prediction = prediction
        o.pixels = v
    elif change in ['block', 'tile']:
        v = o.pixels
        if change == 'block':
            v.block_size = 32
        else:
            prediction = v.prediction
            prediction.tile_size = 32
            v.prediction = prediction
        o.pixels = v
    else:
        docs[0][1].update_roi(docs[0][2][0], q.Roi.rectangle(q.Rect(1, 1, 20, 20)))
    second = run(request(p, model, o), tmp_path/'out')
    assert second.saved == 1 and not second.reused and second.images[0].path != first.images[0].path


def test_empty_target_image_has_reusable_completed_checkpoint(tmp_path):
    p, _, model = scene(1, targets=0)
    r = request(p, model)
    first = run(r, tmp_path/'out')
    assert first.saved == 1 and first.images[0].records == first.images[0].inferred_blocks == 0
    assert not list((tmp_path/'out').rglob('*.qps'))
    assert run(r, tmp_path/'out').reused == 1


def test_options_validation_and_captured_model_ownership(tmp_path):
    p, _, model = scene(1)
    o = options(model)
    r = request(p, model, o)
    o.pixel_model = None
    with pytest.raises(ValueError):
        o.validate()
    assert run(r, tmp_path/'out').saved == 1
    for field in ['nucleus', 'publish_cells']:
        o = options(model)
        setattr(o, field, True)
        with pytest.raises(ValueError):
            o.validate()


def test_native_cli_fresh_resume_and_model_arguments(tmp_path):
    p, _, model = scene(1)
    source = tmp_path/'source.qpxproj'
    p.save(str(source))
    model_path = tmp_path/'model.qpc'
    q.save_pixel_classifier(model, str(model_path))
    exe = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    args = [str(exe), str(source), str(tmp_path/'out'), '--kind', 'pixels', '--pixel-model', str(model_path), '--block-size', '16', '--tile-size', '16', '--pixel-work-bytes', str(8*1024*1024)]
    before = source.read_bytes()
    for expected in ['saved', 'reused']:
        result = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', timeout=30)
        assert result.returncode == 0, result.stderr + result.stdout
        lines = [json.loads(line) for line in result.stdout.splitlines()]
        assert lines[-1]['state'] == 'succeeded' and lines[-1][expected] == 1
        assert lines[0]['inferred_blocks'] > 0 if expected == 'saved' else lines[0]['inferred_blocks'] == 0 and lines[0]['hashed_blocks'] > 0
    api_path = tmp_path/'shared-api-cli'
    api = run(request(p, model), api_path)
    shared_args = args.copy()
    shared_args[2] = str(api_path)
    shared = subprocess.run(shared_args, capture_output=True, text=True, encoding='utf-8', timeout=30)
    assert shared.returncode == 0, shared.stdout + shared.stderr
    summary = json.loads(shared.stdout.splitlines()[-1])
    assert summary['reused'] == 1 and summary['parameters_sha256'] == api.parameters_sha256
    assert source.read_bytes() == before
    for flags in [['--kind', 'pixels'], ['--pixel-model', str(model_path)], ['--kind', 'pixels', '--pixel-model', str(model_path), '--min-vote', 'nan']]:
        result = subprocess.run([str(exe), str(source), str(tmp_path/'bad'), *flags], capture_output=True, timeout=30)
        assert result.returncode == 2


def test_job_cancellation_keeps_first_image_and_resume_finishes_remaining_image(tmp_path):
    import time
    p, docs, model = scene(1)
    large, root, _ = setup(2048)
    p.add_document(large, 'second large image')
    r = request(p, model)
    job = q.ProjectBatchJob()
    assert job.request(r, str(tmp_path/'out'))
    deadline = time.monotonic()+30
    while job.busy:
        stats = job.poll()
        if stats.completed_images:
            job.cancel()
            break
        assert time.monotonic() < deadline
        time.sleep(.001)
    assert job.wait().state == 'cancelled'
    assert job.result.saved == 1 and len(job.result.images) == 1
    assert not list((tmp_path/'out').glob('.qupathxx-batch-work-*'))
    assert len(list((tmp_path/'out').rglob('result.qpxproj'))) == 1
    resumed = run(r, tmp_path/'out')
    assert resumed.reused == resumed.saved == 1
    assert resumed.images[0].inferred_blocks == 0 and resumed.images[1].inferred_blocks > 0
    assert not large.pixel_stream_records and not docs[0][1].pixel_stream_records


def test_continue_policy_commits_valid_image_and_reports_failed_image(tmp_path):
    p, _, model = scene(1)
    large, _, _ = setup(128)
    p.add_document(large, 'too large')
    o = options(model)
    stream = o.pixels
    stream.max_pixels = 4096
    o.pixels = stream
    o.failure = q.ProjectBatchFailure.CONTINUE
    r = request(p, model, o)
    result = q.run_project_batch(r, str(tmp_path/'out'))
    assert result.state == 'partial' and result.saved == result.failed == 1
    resumed = q.run_project_batch(r, str(tmp_path/'out'))
    assert resumed.state == 'partial' and resumed.reused == resumed.failed == 1
    assert len(list((tmp_path/'out').rglob('result.qpxproj'))) == 1


def test_linked_source_pixels_change_invalidates_reuse_with_same_input_metadata(tmp_path):
    _, _, model = scene(1)
    source = tmp_path/'source.ppm'
    source.write_bytes(b'P6\n64 64\n255\n'+bytes([16])*64*64*3)
    p = q.Project()
    image = p.add_image(str(source))
    doc = p.document(image)
    resolution = model.resolution
    if resolution.pixel_width_um is not None:
        doc.set_calibration(resolution.pixel_width_um, resolution.pixel_height_um)
    doc.add_rectangle(q.Rect(0, 0, 64, 64))
    r = request(p, model)
    first = run(r, tmp_path/'out')
    assert run(r, tmp_path/'out').reused == 1
    source.write_bytes(b'P6\n64 64\n255\n'+bytes([32])*64*64*3)
    second = run(r, tmp_path/'out')
    assert second.saved == 1 and second.images[0].input_sha256 == first.images[0].input_sha256
    assert second.images[0].pixel_sha256 != first.images[0].pixel_sha256 and second.images[0].path != first.images[0].path
    assert run(r, tmp_path/'out').reused == 1
