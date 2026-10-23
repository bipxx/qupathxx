"""Compare bounded source hashing with inference; verify every emitted block."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time

import numpy as np
import qupathxx as q
from qupathxx import _native


def timed(call):
    start = time.perf_counter()
    value = call()
    return value, (time.perf_counter() - start) * 1000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--size', type=int, default=1024)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 64 <= args.size <= 4096:
        parser.error('size must be 64..4096')
    fixture = Path(__file__).resolve().parents[1] / 'tests/data/pixel-streams/rectangle-v1-windows.qps'
    model = q.PixelStreamFile(str(fixture)).read_block(0).model
    n = args.size
    rgb = np.empty((n, n, 3), np.uint8)
    rgb[:, :n//2] = [220, 30, 10]
    rgb[:, n//2:] = [10, 30, 220]
    doc = q.Document(q.Image.from_numpy(rgb))
    del rgb
    resolution = model.resolution
    if resolution.pixel_width_um is not None:
        doc.set_calibration(resolution.pixel_width_um, resolution.pixel_height_um)
    root = doc.add_roi(q.Roi.ellipse(q.Rect(0, 0, n, n)))
    revision = doc.revision
    options = q.PixelStreamOptions()
    options.block_size = 128
    prediction = q.PixelPredictionOptions()
    prediction.tile_size = 64
    prediction.work_bytes = 8 * 1024 * 1024
    options.prediction = prediction
    stream = q.PixelPredictionStream(doc, root, model, options)
    digest, hash_ms = timed(stream.input_sha256)
    assert stream.state == 'ready' and stream.completed_blocks == 0
    framed = hashlib.sha256(b'qpx.pixel-stream-input/1')
    framed.update(model.sha256.encode())
    framed.update(struct.pack('<Q', stream.total_blocks))
    blocks = valid = maximum = 0
    start = time.perf_counter()
    while (block := stream.next()) is not None:
        framed.update(struct.pack('<Q', blocks))
        framed.update(block.input_sha256.encode())
        blocks += 1
        valid += block.valid_pixels
        maximum = max(maximum, block.width * block.height * 9)
    inference_ms = (time.perf_counter() - start) * 1000
    assert digest == framed.hexdigest() and stream.state == 'complete'
    assert blocks == stream.total_blocks and doc.revision == revision
    env = q.build_environment()
    out = dict(schema='qpx.pixel-input-digest-benchmark/1', size=n,
               blocks=blocks, valid_pixels=valid, input_sha256=digest,
               hash_only_ms=hash_ms, inference_ms=inference_ms,
               observed_speedup=inference_ms/hash_ms,
               estimated_producer_work_bytes=stream.estimated_work_bytes,
               budget_bytes=prediction.work_bytes, max_block_array_bytes=maximum,
               model_sha256=model.sha256,
               native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
               environment={name: getattr(env, name) for name in
                            ['source_tree_sha256', 'git_revision', 'compiler', 'configuration', 'platform']},
               scope='Single synthetic embedded RGB source on a shared workstation. '
                     'Hash-only scan compared with independently framed input hashes of every inferred block. '
                     'No output block collection retained; transient Python ownership is outside the producer estimate. Producer estimate is conservative for inference; '
                     'not measured RSS, vendor-slide throughput, project-batch resume or biological acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(out, indent=2))


if __name__ == '__main__':
    main()
