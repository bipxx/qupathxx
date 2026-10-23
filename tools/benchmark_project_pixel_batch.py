"""Installed project pixel batches: fresh/reuse timing and every-block oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import time
import uuid

import numpy as np
import qupathxx as q
from qupathxx import _native


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as file:
        for block in iter(lambda: file.read(65536), b''):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--size', type=int, default=1024)
    parser.add_argument('--work', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 64 <= args.size <= 4096:
        parser.error('size must be 64..4096')
    root = args.work or Path('build/project-pixel-batch')/uuid.uuid4().hex
    root.mkdir(parents=True, exist_ok=False)
    fixture = Path(__file__).resolve().parents[1]/'tests/data/pixel-streams/rectangle-v1-windows.qps'
    model = q.PixelStreamFile(str(fixture)).read_block(0).model
    n = args.size
    project = q.Project()
    sources = {}
    for index in range(2):
        rgb = np.empty((n, n, 3), np.uint8)
        rgb[:, :n//2] = [220-index, 30, 10]
        rgb[:, n//2:] = [10, 30, 220-index]
        doc = q.Document(q.Image.from_numpy(rgb))
        del rgb
        if model.resolution.pixel_width_um is not None:
            doc.set_calibration(model.resolution.pixel_width_um, model.resolution.pixel_height_um)
        target = doc.add_roi(q.Roi.rectangle(q.Rect(0, 0, n, n)))
        image = project.add_document(doc, f'Image {index}')
        sources[image] = (doc, target, doc.revision)
    source = root/'source.qpxproj'
    project.save(str(source))
    model_path = root/'model.qpc'
    q.save_pixel_classifier(model, str(model_path))
    source_hash = sha(source)
    o = q.ProjectBatchOptions()
    o.kind = q.ProjectBatchKind.PIXELS
    o.pixel_model = model
    stream = q.PixelStreamOptions()
    stream.block_size = 128
    prediction = q.PixelPredictionOptions()
    prediction.tile_size = 64
    prediction.work_bytes = 8*1024*1024
    stream.prediction = prediction
    o.pixels = stream
    began = time.perf_counter()
    request = q.ProjectBatchRequest(project, model.transform, options=o)
    capture_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    fresh = q.run_project_batch(request, str(root/'output'))
    fresh_ms = (time.perf_counter()-began)*1000
    assert fresh.state == 'succeeded' and fresh.saved == 2 and not fresh.reused, fresh.error
    before = {path: sha(path) for path in (root/'output').rglob('*.qps')}
    assert len(before) == 2
    began = time.perf_counter()
    reused = q.run_project_batch(request, str(root/'output'))
    resume_ms = (time.perf_counter()-began)*1000
    assert reused.state == 'succeeded' and reused.reused == 2 and not reused.saved, reused.error
    verified = []
    max_arrays = 0
    for a, b in zip(fresh.images, reused.images):
        assert a.path == b.path and a.pixel_sha256 == b.pixel_sha256
        assert a.inferred_blocks == a.verified_blocks == b.hashed_blocks == b.verified_blocks and not b.inferred_blocks
        saved = q.Project.open(b.path).document(b.image_id)
        assert len(saved.pixel_stream_records) == 1
        record = saved.pixel_stream_records[0]
        file = q.PixelStreamFile(record.link.path)
        file.verify()
        original, target, revision = sources[b.image_id]
        assert file.input_sha256() == q.PixelPredictionStream(original, target, model, stream).input_sha256()
        assert b.stream_bytes == file.declared_summary.bytes
        for index in range(file.declared_summary.blocks):
            block = file.read_block(index)
            oracle = q.predict_pixel_region(q.PixelPredictionRequest(original, target, model, prediction, block.bounds))
            assert block.input_sha256 == oracle.input_sha256
            for method in ['labels_numpy', 'valid_numpy', 'vote_fractions_numpy']:
                np.testing.assert_array_equal(getattr(block, method)(), getattr(oracle, method)())
            max_arrays = max(max_arrays, block.width*block.height*9)
        assert original.revision == revision and not original.pixel_stream_records
        verified.append(dict(image_id=b.image_id, blocks=b.verified_blocks, inferred_on_resume=b.inferred_blocks,
                             stream_bytes=b.stream_bytes, input_sha256=b.pixel_sha256,
                             file_sha256=sha(record.link.path), content_sha256=record.link.summary.sha256))
    assert all(sha(path) == digest for path, digest in before.items()) and sha(source) == source_hash
    env = q.build_environment()
    out = dict(schema='qpx.project-pixel-batch-benchmark/1', size=n, images=2,
               capture_ms=capture_ms, fresh_ms=fresh_ms, resume_ms=resume_ms,
               observed_speedup=fresh_ms/resume_ms, snapshot_bytes=request.snapshot_bytes,
               snapshot_budget=o.snapshot_budget, producer_budget=prediction.work_bytes,
               estimated_producer_work_bytes=q.PixelPredictionStream(original, target, model, stream).estimated_work_bytes,
               max_block_array_bytes=max_arrays, verified=verified, parameters_sha256=request.parameters_sha256,
               model_sha256=model.sha256, source_project_sha256=source_hash,
               native_sha256=sha(_native.__file__),
               environment={name: getattr(env, name) for name in
                            ['source_tree_sha256', 'git_revision', 'compiler', 'configuration', 'platform']},
               scope='Two synthetic embedded RGB images, one target each, portable fixture model, shared workstation. '
                     'Fresh includes inference, full QPS verification and atomic image checkpoints. Resume includes '
                     'input snapshot serialization, hash-only source scan, checkpoint checksum and full saved-output '
                     'verification; zero inferred blocks. Every block compared with direct window inference and '
                     'source/model/output bytes retained. No measured RSS, vendor/network storage throughput, '
                     'hardware/frame budget or biological acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(out, indent=2))


if __name__ == '__main__':
    main()
